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
#include "remoting/proto/remoting/v1/directory_service.grpc.pb.h"
#include "remoting/signaling/signal_strategy.h"
#include "remoting/signaling/signaling_address.h"

namespace remoting {

namespace {

constexpr base::TimeDelta kDefaultHeartbeatInterval =
    base::TimeDelta::FromMinutes(5);
constexpr base::TimeDelta kHeartbeatResponseTimeout =
    base::TimeDelta::FromSeconds(30);
constexpr base::TimeDelta kWaitForAllStrategiesConnectedTimeout =
    base::TimeDelta::FromSeconds(5);
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
  SetDeadline(client_context.get(),
              base::Time::Now() + kHeartbeatResponseTimeout);
  auto async_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&DirectoryService::Stub::AsyncHeartbeat,
                     base::Unretained(directory_.get())),
      std::move(client_context), request, std::move(callback));
  executor_.ExecuteRpc(std::move(async_request));
}

void HeartbeatSender::HeartbeatClient::CancelPendingRequests() {
  executor_.CancelPendingRequests();
}

// end of HeartbeatSender::HeartbeatClient

HeartbeatSender::HeartbeatSender(
    base::OnceClosure on_heartbeat_successful_callback,
    base::OnceClosure on_unknown_host_id_error,
    const std::string& host_id,
    SignalStrategy* xmpp_signal_strategy,
    SignalStrategy* ftl_signal_strategy,
    const scoped_refptr<const RsaKeyPair>& host_key_pair,
    OAuthTokenGetter* oauth_token_getter)
    : on_heartbeat_successful_callback_(
          std::move(on_heartbeat_successful_callback)),
      on_unknown_host_id_error_(std::move(on_unknown_host_id_error)),
      host_id_(host_id),
      xmpp_signal_strategy_(xmpp_signal_strategy),
      ftl_signal_strategy_(ftl_signal_strategy),
      host_key_pair_(host_key_pair),
      client_(std::make_unique<HeartbeatClient>(oauth_token_getter)) {
  DCHECK(xmpp_signal_strategy_);
  DCHECK(ftl_signal_strategy_);
  DCHECK(host_key_pair_.get());

  xmpp_signal_strategy_->AddListener(this);
  ftl_signal_strategy_->AddListener(this);

  // Start heartbeats if any signaling strategy is already connected. We only
  // need to call OnSignalStrategyStateChange() once and it will figure out the
  // state of both strategies.
  OnSignalStrategyStateChange(ftl_signal_strategy_->GetState());
}

HeartbeatSender::~HeartbeatSender() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  xmpp_signal_strategy_->RemoveListener(this);
  ftl_signal_strategy_->RemoveListener(this);
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
  if (IsAnyStrategyConnected()) {
    // Drop refresh timer and pending heartbeat (which doesn't have the offline
    // reason) and send a new heartbeat immediately with the offline reason.
    client_->CancelPendingRequests();
    heartbeat_timer_.AbandonAndStop();

    SendHeartbeat();
  }
}

void HeartbeatSender::OnSignalStrategyStateChange(SignalStrategy::State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsEveryStrategyConnected()) {
    wait_for_all_strategies_connected_timeout_timer_.AbandonAndStop();
    heartbeat_timer_.AbandonAndStop();
    SendHeartbeat();
  } else if (IsAnyStrategyConnected()) {
    wait_for_all_strategies_connected_timeout_timer_.Start(
        FROM_HERE, kWaitForAllStrategiesConnectedTimeout, this,
        &HeartbeatSender::OnWaitForAllStrategiesConnectedTimeout);
  } else if (IsEveryStrategyDisconnected()) {
    wait_for_all_strategies_connected_timeout_timer_.AbandonAndStop();
    client_->CancelPendingRequests();
    heartbeat_timer_.AbandonAndStop();
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

void HeartbeatSender::OnWaitForAllStrategiesConnectedTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsEveryStrategyConnected()) {
    LOG(WARNING) << "Timeout waiting for all strategies to be connected.";
    SendHeartbeat();
  }
}

void HeartbeatSender::SendHeartbeat() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsEveryStrategyDisconnected()) {
    LOG(WARNING)
        << "Not sending heartbeat because all strategies are disconnected.";
    return;
  }

  client_->Heartbeat(
      CreateHeartbeatRequest(),
      base::BindOnce(&HeartbeatSender::OnResponse, base::Unretained(this)));
  ++sequence_id_;
}

void HeartbeatSender::OnResponse(const grpc::Status& status,
                                 const apis::v1::HeartbeatResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status.ok() && response.result() == apis::v1::HEARTBEATRESULT_SUCCESS) {
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
      if (delay.is_zero()) {
        LOG(WARNING)
            << "set_interval_seconds is not set. Using default interval.";
        delay = kDefaultHeartbeatInterval;
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
  if (ftl_signal_strategy_->GetState() == SignalStrategy::State::CONNECTED) {
    heartbeat.set_tachyon_id(ftl_signal_strategy_->GetLocalAddress().jid());
  }
  if (xmpp_signal_strategy_->GetState() == SignalStrategy::State::CONNECTED) {
    heartbeat.set_jabber_id(xmpp_signal_strategy_->GetLocalAddress().jid());
  }
  heartbeat.set_host_id(host_id_);
  heartbeat.set_sequence_id(sequence_id_);
  if (!host_offline_reason_.empty()) {
    heartbeat.set_host_offline_reason(host_offline_reason_);
  }
  heartbeat.set_signature(CreateSignature());
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

std::string HeartbeatSender::CreateSignature() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(yuweih): Rename JID to something FTL specific.
  std::string jid = ftl_signal_strategy_->GetLocalAddress().jid();
  if (jid.empty()) {
    jid = xmpp_signal_strategy_->GetLocalAddress().jid();
  }
  std::string message = jid + ' ' + base::NumberToString(sequence_id_);
  return host_key_pair_->SignMessage(message);
}

bool HeartbeatSender::IsAnyStrategyConnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return xmpp_signal_strategy_->GetState() ==
             SignalStrategy::State::CONNECTED ||
         ftl_signal_strategy_->GetState() == SignalStrategy::State::CONNECTED;
}

bool HeartbeatSender::IsEveryStrategyConnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return xmpp_signal_strategy_->GetState() ==
             SignalStrategy::State::CONNECTED &&
         ftl_signal_strategy_->GetState() == SignalStrategy::State::CONNECTED;
}

bool HeartbeatSender::IsEveryStrategyDisconnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return xmpp_signal_strategy_->GetState() ==
             SignalStrategy::State::DISCONNECTED &&
         ftl_signal_strategy_->GetState() ==
             SignalStrategy::State::DISCONNECTED;
}

}  // namespace remoting
