// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_signal_strategy.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "remoting/base/logging.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/signaling/ftl_device_id_provider.h"
#include "remoting/signaling/ftl_messaging_client.h"
#include "remoting/signaling/ftl_registration_manager.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/xmpp_stream_parser.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"

namespace remoting {

namespace {

constexpr base::TimeDelta kDestroyMessagingClientDelay =
    base::TimeDelta::FromSeconds(5);

SignalStrategy::Error GrpcStatusToSignalingError(const grpc::Status& status) {
  switch (status.error_code()) {
    case grpc::StatusCode::OK:
      return SignalStrategy::Error::OK;
    case grpc::StatusCode::PERMISSION_DENIED:
    case grpc::StatusCode::UNAUTHENTICATED:
      return SignalStrategy::Error::AUTHENTICATION_FAILED;
    default:
      // There are so many errors that gRPC can report, so just generalize them
      // as NETWORK_ERROR.
      return SignalStrategy::Error::NETWORK_ERROR;
  }
}

}  // namespace

class FtlSignalStrategy::Core {
 public:
  Core(std::unique_ptr<OAuthTokenGetter> oauth_token_getter,
       std::unique_ptr<FtlDeviceIdProvider> device_id_provider);
  ~Core();

  void Connect();
  void Disconnect();
  State GetState() const;
  Error GetError() const;
  const SignalingAddress& GetLocalAddress() const;
  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);
  bool SendStanza(std::unique_ptr<jingle_xmpp::XmlElement> stanza);
  bool IsSignInError() const;

 private:
  // Methods are called in the order below when Connect() is called.
  void OnGetOAuthTokenResponse(OAuthTokenGetter::Status status,
                               const std::string& user_email,
                               const std::string& access_token);
  void OnSignInGaiaResponse(const grpc::Status& status);
  void StartReceivingMessages();
  void OnReceiveMessagesStreamStarted();
  void OnReceiveMessagesStreamClosed(const grpc::Status& status);
  void OnMessageReceived(const std::string& sender_id,
                         const std::string& sender_registration_id,
                         const ftl::ChromotingMessage& message);

  void SendMessage(const SignalingAddress& receiver,
                   const std::string& stanza_id,
                   const std::string& message);
  void OnSendMessageResponse(const SignalingAddress& receiver,
                             const std::string& stanza_id,
                             const grpc::Status& status);

  // Returns true if the status is handled.
  void HandleGrpcStatusError(const base::Location& location,
                             const grpc::Status& status);

  // Event handlers for XmppStreamParser.
  void OnStanza(const std::unique_ptr<jingle_xmpp::XmlElement> stanza);
  void OnParserError();

  std::unique_ptr<OAuthTokenGetter> oauth_token_getter_;

  std::unique_ptr<RegistrationManager> registration_manager_;
  std::unique_ptr<FtlMessagingClient> messaging_client_;

  std::string user_email_;
  SignalingAddress local_address_;

  std::unique_ptr<FtlMessagingClient::MessageCallbackSubscription>
      receive_message_subscription_;
  std::unique_ptr<XmppStreamParser> stream_parser_;

  std::string message_sender_override_;

  Error error_ = OK;
  bool is_sign_in_error_ = false;

  base::ObserverList<Listener, true> listeners_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<Core> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(Core);
};

FtlSignalStrategy::Core::Core(
    std::unique_ptr<OAuthTokenGetter> oauth_token_getter,
    std::unique_ptr<FtlDeviceIdProvider> device_id_provider)
    : weak_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(oauth_token_getter);
  DCHECK(device_id_provider);
  oauth_token_getter_ = std::move(oauth_token_getter);
  // TODO(yuweih): Just make FtlMessagingClient own FtlRegistrationManager and
  // call SignInGaia() transparently.
  registration_manager_ = std::make_unique<FtlRegistrationManager>(
      oauth_token_getter_.get(), std::move(device_id_provider));
  messaging_client_ = std::make_unique<FtlMessagingClient>(
      oauth_token_getter_.get(), registration_manager_.get());
}

FtlSignalStrategy::Core::~Core() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Disconnect();

  registration_manager_->SignOut();
  // Dirty hack to make sure session-terminate message is sent before the
  // |messaging_client_| gets deleted.
  // TODO(yuweih): Either improve this by waiting for request queue to drain or
  // make FtlMessagingClient a singleton if we decide to reuse it for host
  // status discovery.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<RegistrationManager> registration_manager,
             std::unique_ptr<FtlMessagingClient> messaging_client,
             std::unique_ptr<OAuthTokenGetter> oauth_token_getter) {
            messaging_client.reset();
            registration_manager.reset();
            oauth_token_getter.reset();
          },
          std::move(registration_manager_), std::move(messaging_client_),
          std::move(oauth_token_getter_)),
      kDestroyMessagingClientDelay);
}

void FtlSignalStrategy::Core::Connect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetState() != DISCONNECTED) {
    LOG(WARNING) << "Signaling is not disconnected. State: " << GetState();
    return;
  }

  error_ = OK;
  is_sign_in_error_ = false;

  receive_message_subscription_ =
      messaging_client_->RegisterMessageCallback(base::BindRepeating(
          &Core::OnMessageReceived, weak_factory_.GetWeakPtr()));

  for (auto& observer : listeners_)
    observer.OnSignalStrategyStateChange(CONNECTING);

  StartReceivingMessages();
}

void FtlSignalStrategy::Core::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (receive_message_subscription_) {
    stream_parser_.reset();
    receive_message_subscription_.reset();
    messaging_client_->StopReceivingMessages();

    for (auto& observer : listeners_)
      observer.OnSignalStrategyStateChange(DISCONNECTED);
  }
}

SignalStrategy::State FtlSignalStrategy::Core::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (stream_parser_) {
    DCHECK(receive_message_subscription_);
    return CONNECTED;
  } else if (receive_message_subscription_) {
    return CONNECTING;
  } else {
    return DISCONNECTED;
  }
}

SignalStrategy::Error FtlSignalStrategy::Core::GetError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return error_;
}

const SignalingAddress& FtlSignalStrategy::Core::GetLocalAddress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return local_address_;
}

void FtlSignalStrategy::Core::AddListener(Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.AddObserver(listener);
}

void FtlSignalStrategy::Core::RemoveListener(Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.RemoveObserver(listener);
}

bool FtlSignalStrategy::Core::SendStanza(
    std::unique_ptr<jingle_xmpp::XmlElement> stanza) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!stream_parser_) {
    HOST_LOG << "Dropping signaling message because FTL is not connected.";
    return false;
  }

  std::string to_error;
  SignalingAddress to =
      SignalingAddress::Parse(stanza.get(), SignalingAddress::TO, &to_error);
  DCHECK(to_error.empty());

  // Remove from and to fields from the stanza to prevent confusion to the peer.
  stanza->ClearAttr(jingle_xmpp::QN_FROM);
  stanza->ClearAttr(jingle_xmpp::QN_TO);

  std::string stanza_id = stanza->Attr(jingle_xmpp::QN_ID);
  SendMessage(to, stanza_id, stanza->Str());

  // Return false if the SendMessage() call above resulted in the SignalStrategy
  // being disconnected.
  return stream_parser_ != nullptr;
}

bool FtlSignalStrategy::Core::IsSignInError() const {
  return is_sign_in_error_;
}

void FtlSignalStrategy::Core::OnGetOAuthTokenResponse(
    OAuthTokenGetter::Status status,
    const std::string& user_email,
    const std::string& access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status != OAuthTokenGetter::Status::SUCCESS) {
    switch (status) {
      case OAuthTokenGetter::Status::NETWORK_ERROR:
        error_ = SignalStrategy::Error::NETWORK_ERROR;
        break;
      case OAuthTokenGetter::Status::AUTH_ERROR:
        error_ = SignalStrategy::Error::AUTHENTICATION_FAILED;
        break;
      default:
        NOTREACHED();
        break;
    }
    is_sign_in_error_ = true;
    Disconnect();
    return;
  }

  user_email_ = user_email;
  StartReceivingMessages();
}

void FtlSignalStrategy::Core::OnSignInGaiaResponse(const grpc::Status& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!status.ok()) {
    is_sign_in_error_ = true;
    HandleGrpcStatusError(FROM_HERE, status);
    return;
  }
  StartReceivingMessages();
}

void FtlSignalStrategy::Core::StartReceivingMessages() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(CONNECTING, GetState());
  DCHECK(!messaging_client_->IsReceivingMessages());

  if (user_email_.empty()) {
    oauth_token_getter_->CallWithToken(base::BindOnce(
        &Core::OnGetOAuthTokenResponse, weak_factory_.GetWeakPtr()));
    return;
  }

  if (!registration_manager_->IsSignedIn()) {
    registration_manager_->SignInGaia(base::BindOnce(
        &Core::OnSignInGaiaResponse, weak_factory_.GetWeakPtr()));
    return;
  }

  messaging_client_->StartReceivingMessages(
      base::BindOnce(&Core::OnReceiveMessagesStreamStarted,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&Core::OnReceiveMessagesStreamClosed,
                     weak_factory_.GetWeakPtr()));
}

void FtlSignalStrategy::Core::OnReceiveMessagesStreamStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_address_ = SignalingAddress::CreateFtlSignalingAddress(
      user_email_, registration_manager_->GetRegistrationId());

  stream_parser_ = std::make_unique<XmppStreamParser>();
  stream_parser_->SetCallbacks(
      base::BindRepeating(&Core::OnStanza, weak_factory_.GetWeakPtr()),
      base::BindRepeating(&Core::OnParserError, weak_factory_.GetWeakPtr()));

  // XMPP receiver stream starts with an opened <stream> tag.
  stream_parser_->AppendData("<stream>");

  for (auto& observer : listeners_)
    observer.OnSignalStrategyStateChange(CONNECTED);
}

void FtlSignalStrategy::Core::OnReceiveMessagesStreamClosed(
    const grpc::Status& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status.error_code() == grpc::StatusCode::CANCELLED) {
    // Stream is canceled by calling Disconnect().
    return;
  }
  if (!status.ok()) {
    HandleGrpcStatusError(FROM_HERE, status);
    return;
  }
}

void FtlSignalStrategy::Core::OnMessageReceived(
    const std::string& sender_id,
    const std::string& sender_registration_id,
    const ftl::ChromotingMessage& message) {
  auto sender_address = SignalingAddress::CreateFtlSignalingAddress(
      sender_id, sender_registration_id);
  DCHECK(message_sender_override_.empty());
  if (!message_sender_override_.empty() &&
      sender_address.jid() != message_sender_override_) {
    LOG(ERROR) << "Received message from a different sender before the current "
               << "stanza is parsed.";
    return;
  }
  message_sender_override_ = sender_address.jid();
  stream_parser_->AppendData(message.xmpp().stanza());
  DCHECK(message_sender_override_.empty());
}

void FtlSignalStrategy::Core::SendMessage(const SignalingAddress& receiver,
                                          const std::string& stanza_id,
                                          const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string receiver_username;
  std::string receiver_registration_id;
  bool get_info_result =
      receiver.GetFtlInfo(&receiver_username, &receiver_registration_id);
  if (!get_info_result) {
    LOG(DFATAL) << "Receiver is not in FTL address: " << receiver.jid();
    return;
  }

  HOST_LOG << "Sending outgoing stanza:\n"
           << "Receiver: " << receiver_username << "\n"
           << "Receiver registration ID: " << receiver_registration_id << "\n"
           << message
           << "\n=========================================================";
  ftl::ChromotingMessage crd_message;
  crd_message.mutable_xmpp()->set_stanza(message);
  messaging_client_->SendMessage(
      receiver_username, receiver_registration_id, crd_message,
      base::BindOnce(&Core::OnSendMessageResponse, weak_factory_.GetWeakPtr(),
                     receiver, stanza_id));
}

void FtlSignalStrategy::Core::OnSendMessageResponse(
    const SignalingAddress& receiver,
    const std::string& stanza_id,
    const grpc::Status& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status.ok()) {
    return;
  }

  if (status.error_code() == grpc::StatusCode::UNAUTHENTICATED) {
    HandleGrpcStatusError(FROM_HERE, status);
    return;
  }

  // Fake an error message so that JingleSession will take it as
  // PEER_IS_OFFLINE.
  DCHECK(message_sender_override_.empty());
  message_sender_override_ = receiver.jid();
  LOG(ERROR) << "Failed to send message to peer. Error code: "
             << status.error_code() << ", message: " << status.error_message();
  auto error_iq = std::make_unique<jingle_xmpp::XmlElement>(jingle_xmpp::QN_IQ);
  error_iq->SetAttr(jingle_xmpp::QN_TYPE, jingle_xmpp::STR_ERROR);
  error_iq->SetAttr(jingle_xmpp::QN_ID, stanza_id);
  OnStanza(std::move(error_iq));
  DCHECK(message_sender_override_.empty());
}

void FtlSignalStrategy::Core::HandleGrpcStatusError(
    const base::Location& location,
    const grpc::Status& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!status.ok());
  error_ = GrpcStatusToSignalingError(status);
  LOG(ERROR) << "Received gRPC error. Error code: " << status.error_code()
             << ", message: " << status.error_message()
             << ", location: " << location.ToString();
  if (status.error_code() == grpc::StatusCode::UNAUTHENTICATED) {
    registration_manager_->SignOut();
    oauth_token_getter_->InvalidateCache();
  }
  Disconnect();
}

void FtlSignalStrategy::Core::OnStanza(
    const std::unique_ptr<jingle_xmpp::XmlElement> stanza) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Don't trust the sender/receiver fields coming from the sender.
  stanza->SetAttr(jingle_xmpp::QN_FROM, message_sender_override_);
  stanza->SetAttr(jingle_xmpp::QN_TO, local_address_.jid());

  message_sender_override_.clear();

  HOST_LOG << "Received incoming stanza:\n"
           << stanza->Str()
           << "\n=========================================================";

  for (auto& listener : listeners_) {
    if (listener.OnSignalStrategyIncomingStanza(stanza.get()))
      return;
  }
}

void FtlSignalStrategy::Core::OnParserError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  error_ = NETWORK_ERROR;
  Disconnect();
}

FtlSignalStrategy::FtlSignalStrategy(
    std::unique_ptr<OAuthTokenGetter> oauth_token_getter,
    std::unique_ptr<FtlDeviceIdProvider> device_id_provider)
    : core_(std::make_unique<Core>(std::move(oauth_token_getter),
                                   std::move(device_id_provider))) {}

FtlSignalStrategy::~FtlSignalStrategy() {
  // All listeners should be removed at this point, so it's safe to detach
  // |core_|.
  base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                     core_.release());
}

void FtlSignalStrategy::Connect() {
  core_->Connect();
}

void FtlSignalStrategy::Disconnect() {
  core_->Disconnect();
}

SignalStrategy::State FtlSignalStrategy::GetState() const {
  return core_->GetState();
}

SignalStrategy::Error FtlSignalStrategy::GetError() const {
  return core_->GetError();
}

const SignalingAddress& FtlSignalStrategy::GetLocalAddress() const {
  return core_->GetLocalAddress();
}

void FtlSignalStrategy::AddListener(Listener* listener) {
  core_->AddListener(listener);
}

void FtlSignalStrategy::RemoveListener(Listener* listener) {
  core_->RemoveListener(listener);
}
bool FtlSignalStrategy::SendStanza(
    std::unique_ptr<jingle_xmpp::XmlElement> stanza) {
  return core_->SendStanza(std::move(stanza));
}

std::string FtlSignalStrategy::GetNextId() {
  return base::NumberToString(base::RandUint64());
}

bool FtlSignalStrategy::IsSignInError() const {
  return core_->IsSignInError();
}

}  // namespace remoting
