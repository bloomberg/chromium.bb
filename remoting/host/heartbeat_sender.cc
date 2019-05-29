// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_sender.h"

#include <math.h>

#include <cstdint>
#include <utility>

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/time/time.h"
#include "remoting/base/constants.h"
#include "remoting/base/grpc_support/grpc_async_unary_request.h"
#include "remoting/base/grpc_support/grpc_authenticated_executor.h"
#include "remoting/base/grpc_support/grpc_channel.h"
#include "remoting/base/grpc_support/grpc_util.h"
#include "remoting/base/logging.h"
#include "remoting/base/service_urls.h"
#include "remoting/host/host_details.h"
#include "remoting/host/server_log_entry_host.h"
#include "remoting/proto/remoting/v1/directory_service.grpc.pb.h"
#include "remoting/signaling/ftl_signal_strategy.h"
#include "remoting/signaling/log_to_server.h"
#include "remoting/signaling/server_log_entry.h"
#include "remoting/signaling/signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/xmpp_signal_strategy.h"

namespace remoting {

namespace {

constexpr base::TimeDelta kMinimumHeartbeatInterval =
    base::TimeDelta::FromMinutes(3);
constexpr base::TimeDelta kHeartbeatResponseTimeout =
    base::TimeDelta::FromSeconds(30);
constexpr base::TimeDelta kResendDelay = base::TimeDelta::FromSeconds(10);
constexpr base::TimeDelta kResendDelayOnHostNotFound =
    base::TimeDelta::FromSeconds(10);

const int kMaxResendOnHostNotFoundCount = 12;  // 2 minutes (12 x 10 seconds).

using HeartbeatResponseCallback =
    base::OnceCallback<void(const grpc::Status&,
                            const apis::v1::HeartbeatResponse&)>;

}  // namespace

class HeartbeatSender::HeartbeatClient final {
 public:
  HeartbeatClient(OAuthTokenGetter* oauth_token_getter);
  ~HeartbeatClient();

  void Heartbeat(const apis::v1::HeartbeatRequest& request,
                 HeartbeatResponseCallback callback);
  void CancelPendingRequests();
  void SetChannelForTest(GrpcChannelSharedPtr channel);

 private:
  using DirectoryService = apis::v1::RemotingDirectoryService;
  std::unique_ptr<DirectoryService::Stub> directory_;
  GrpcAuthenticatedExecutor executor_;
  DISALLOW_COPY_AND_ASSIGN(HeartbeatClient);
};

HeartbeatSender::HeartbeatClient::HeartbeatClient(
    OAuthTokenGetter* oauth_token_getter)
    : executor_(oauth_token_getter) {
  directory_ = DirectoryService::NewStub(CreateSslChannelForEndpoint(
      ServiceUrls::GetInstance()->remoting_server_endpoint()));
}

HeartbeatSender::HeartbeatClient::~HeartbeatClient() = default;

void HeartbeatSender::HeartbeatClient::Heartbeat(
    const apis::v1::HeartbeatRequest& request,
    HeartbeatResponseCallback callback) {
  HOST_LOG << "Sending outgoing heartbeat:\n"
           << "signature: " << request.signature() << "\n"
           << "host_id: " << request.host_id() << "\n"
           << "jabber_id: " << request.jabber_id() << "\n"
           << "tachyon_id: " << request.tachyon_id() << "\n"
           << "sequence_id: " << request.sequence_id() << "\n"
           << "host_version: " << request.host_version() << "\n"
           << "host_offline_reason: " << request.host_offline_reason() << "\n"
           << "host_os_name: " << request.host_os_name() << "\n"
           << "host_os_version: " << request.host_os_version() << "\n"
           << "=========================================================";

  auto client_context = std::make_unique<grpc::ClientContext>();
  auto async_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&DirectoryService::Stub::AsyncHeartbeat,
                     base::Unretained(directory_.get())),
      request, std::move(callback));
  SetDeadline(async_request->context(),
              base::Time::Now() + kHeartbeatResponseTimeout);
  executor_.ExecuteRpc(std::move(async_request));
}

void HeartbeatSender::HeartbeatClient::CancelPendingRequests() {
  executor_.CancelPendingRequests();
}

void HeartbeatSender::HeartbeatClient::SetChannelForTest(
    GrpcChannelSharedPtr channel) {
  directory_ = DirectoryService::NewStub(channel);
}

// end of HeartbeatSender::HeartbeatClient

HeartbeatSender::HeartbeatSender(
    base::OnceClosure on_heartbeat_successful_callback,
    base::OnceClosure on_unknown_host_id_error,
    const std::string& host_id,
    MuxingSignalStrategy* signal_strategy,
    const scoped_refptr<const RsaKeyPair>& host_key_pair,
    OAuthTokenGetter* oauth_token_getter,
    LogToServer* log_to_server)
    : on_heartbeat_successful_callback_(
          std::move(on_heartbeat_successful_callback)),
      on_unknown_host_id_error_(std::move(on_unknown_host_id_error)),
      host_id_(host_id),
      signal_strategy_(signal_strategy),
      host_key_pair_(host_key_pair),
      client_(std::make_unique<HeartbeatClient>(oauth_token_getter)),
      log_to_server_(log_to_server) {
  DCHECK(host_key_pair_.get());
  DCHECK(log_to_server_);

  signal_strategy_->AddListener(this);
  OnSignalStrategyStateChange(signal_strategy_->GetState());
}

HeartbeatSender::~HeartbeatSender() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  signal_strategy_->RemoveListener(this);
}

void HeartbeatSender::SetHostOfflineReason(
    const std::string& host_offline_reason,
    const base::TimeDelta& timeout,
    base::OnceCallback<void(bool success)> ack_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!host_offline_reason_ack_callback_);

  host_offline_reason_ = host_offline_reason;
  host_offline_reason_ack_callback_ = std::move(ack_callback);
  host_offline_reason_timeout_timer_.Start(
      FROM_HERE, timeout, this, &HeartbeatSender::OnHostOfflineReasonTimeout);
  if (signal_strategy_->GetState() == SignalStrategy::State::CONNECTED) {
    SendHeartbeat();
  }
}

void HeartbeatSender::SetGrpcChannelForTest(GrpcChannelSharedPtr channel) {
  client_->SetChannelForTest(channel);
}

void HeartbeatSender::OnSignalStrategyStateChange(SignalStrategy::State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state) {
    case SignalStrategy::State::CONNECTED:
      SendHeartbeat();
      break;
    case SignalStrategy::State::DISCONNECTED:
      client_->CancelPendingRequests();
      heartbeat_timer_.AbandonAndStop();
      break;
    default:
      // Do nothing
      break;
  }
}

bool HeartbeatSender::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  return false;
}

void HeartbeatSender::OnHostOfflineReasonTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host_offline_reason_ack_callback_);

  std::move(host_offline_reason_ack_callback_).Run(false);
}

void HeartbeatSender::OnHostOfflineReasonAck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!host_offline_reason_ack_callback_) {
    DCHECK(!host_offline_reason_timeout_timer_.IsRunning());
    return;
  }

  DCHECK(host_offline_reason_timeout_timer_.IsRunning());
  host_offline_reason_timeout_timer_.AbandonAndStop();

  std::move(host_offline_reason_ack_callback_).Run(true);
}

void HeartbeatSender::SendHeartbeat() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (signal_strategy_->GetState() == SignalStrategy::State::DISCONNECTED) {
    LOG(WARNING)
        << "Not sending heartbeat because all strategies are disconnected.";
    return;
  }

  // Drop previous heartbeat and timer so that it doesn't interfere with the
  // current one.
  client_->CancelPendingRequests();
  heartbeat_timer_.AbandonAndStop();

  client_->Heartbeat(
      CreateHeartbeatRequest(),
      base::BindOnce(&HeartbeatSender::OnResponse, base::Unretained(this)));
  ++sequence_id_;

  // Send a heartbeat log
  std::unique_ptr<ServerLogEntry> log_entry = MakeLogEntryForHeartbeat();
  AddHostFieldsToLogEntry(log_entry.get());
  log_to_server_->Log(*log_entry);
}

void HeartbeatSender::OnResponse(const grpc::Status& status,
                                 const apis::v1::HeartbeatResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status.ok()) {
    heartbeat_succeeded_ = true;
    failed_heartbeat_count_ = 0;

    // Notify listener of the first successful heartbeat.
    if (on_heartbeat_successful_callback_) {
      std::move(on_heartbeat_successful_callback_).Run();
    }

    // Notify caller of SetHostOfflineReason that we got an ack and don't
    // schedule another heartbeat.
    if (!host_offline_reason_.empty()) {
      OnHostOfflineReasonAck();
      return;
    }
  } else {
    failed_heartbeat_count_++;
  }

  if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
    LOG(ERROR) << "Heartbeat timed out.";
  }

  // If the host was registered immediately before it sends a heartbeat,
  // then server-side latency may prevent the server recognizing the
  // host ID in the heartbeat. So even if all of the first few heartbeats
  // get a "host ID not found" error, that's not a good enough reason to
  // exit.
  if (status.error_code() == grpc::StatusCode::NOT_FOUND &&
      (heartbeat_succeeded_ ||
       (failed_heartbeat_count_ > kMaxResendOnHostNotFoundCount))) {
    if (on_unknown_host_id_error_) {
      std::move(on_unknown_host_id_error_).Run();
    }
    return;
  }

  // Calculate delay before sending the next message.
  base::TimeDelta delay;
  // See CoreErrorDomainTranslator.java for the mapping
  switch (status.error_code()) {
    case grpc::StatusCode::OK:
      delay = base::TimeDelta::FromSeconds(response.set_interval_seconds());
      if (delay < kMinimumHeartbeatInterval) {
        LOG(WARNING) << "Received suspicious set_interval_seconds: " << delay
                     << ". Using minimum interval: "
                     << kMinimumHeartbeatInterval;
        delay = kMinimumHeartbeatInterval;
      }
      break;
    case grpc::StatusCode::NOT_FOUND:
      delay = kResendDelayOnHostNotFound;
      break;
    // TODO(yuweih): Handle sequence ID mismatch case. This is not implemented
    // in the server yet.
    default:
      delay = pow(2.0, failed_heartbeat_count_) * (1 + base::RandDouble()) *
              kResendDelay;
      break;
  }

  heartbeat_timer_.Start(FROM_HERE, delay, this,
                         &HeartbeatSender::SendHeartbeat);
}

apis::v1::HeartbeatRequest HeartbeatSender::CreateHeartbeatRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  apis::v1::HeartbeatRequest heartbeat;
  std::string signaling_id;
  if (signal_strategy_->ftl_signal_strategy()->GetState() ==
      SignalStrategy::State::CONNECTED) {
    heartbeat.set_tachyon_id(
        signal_strategy_->ftl_signal_strategy()->GetLocalAddress().jid());
    signaling_id =
        signal_strategy_->ftl_signal_strategy()->GetLocalAddress().jid();
  }
  if (signal_strategy_->xmpp_signal_strategy()->GetState() ==
      SignalStrategy::State::CONNECTED) {
    heartbeat.set_jabber_id(
        signal_strategy_->xmpp_signal_strategy()->GetLocalAddress().jid());
    if (signaling_id.empty()) {
      signaling_id =
          signal_strategy_->xmpp_signal_strategy()->GetLocalAddress().jid();
    }
  }
  heartbeat.set_host_id(host_id_);
  heartbeat.set_sequence_id(sequence_id_);
  if (!host_offline_reason_.empty()) {
    heartbeat.set_host_offline_reason(host_offline_reason_);
  }
  heartbeat.set_signature(CreateSignature(signaling_id));
  // Append host version.
  heartbeat.set_host_version(STRINGIZE(VERSION));
  // If we have not recorded a heartbeat success, continue sending host OS info.
  if (!heartbeat_succeeded_) {
    // Append host OS name.
    heartbeat.set_host_os_name(GetHostOperatingSystemName());
    // Append host OS version.
    heartbeat.set_host_os_version(GetHostOperatingSystemVersion());
  }
  return heartbeat;
}

std::string HeartbeatSender::CreateSignature(const std::string& signaling_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string message = signaling_id + ' ' + base::NumberToString(sequence_id_);
  return host_key_pair_->SignMessage(message);
}

}  // namespace remoting
