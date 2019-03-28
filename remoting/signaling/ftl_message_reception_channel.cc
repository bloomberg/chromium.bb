// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_message_reception_channel.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "remoting/signaling/ftl_grpc_context.h"
#include "remoting/signaling/ftl_services.grpc.pb.h"
#include "remoting/signaling/grpc_support/scoped_grpc_server_stream.h"

namespace remoting {

namespace {

const net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    FtlMessageReceptionChannel::kBackoffInitialDelay.InMilliseconds(),

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.5,

    // Maximum amount of time we are willing to delay our request in ms.
    FtlMessageReceptionChannel::kBackoffMaxDelay.InMilliseconds(),

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Starts with initial delay.
    false,
};

}  // namespace

constexpr base::TimeDelta FtlMessageReceptionChannel::kPongTimeout;
constexpr base::TimeDelta FtlMessageReceptionChannel::kStreamLifetime;
constexpr base::TimeDelta FtlMessageReceptionChannel::kBackoffInitialDelay;
constexpr base::TimeDelta FtlMessageReceptionChannel::kBackoffMaxDelay;

FtlMessageReceptionChannel::FtlMessageReceptionChannel()
    : reconnect_retry_backoff_(&kBackoffPolicy), weak_factory_(this) {}

FtlMessageReceptionChannel::~FtlMessageReceptionChannel() = default;

void FtlMessageReceptionChannel::Initialize(
    const StreamOpener& stream_opener,
    const MessageCallback& on_incoming_msg) {
  DCHECK(stream_opener);
  DCHECK(on_incoming_msg);
  DCHECK(!stream_opener_);
  DCHECK(!on_incoming_msg_);
  stream_opener_ = stream_opener;
  on_incoming_msg_ = on_incoming_msg;
}

void FtlMessageReceptionChannel::StartReceivingMessages(DoneCallback on_done) {
  if (state_ == State::STARTED) {
    std::move(on_done).Run(grpc::Status::OK);
    return;
  }
  start_receiving_messages_callbacks_.push_back(std::move(on_done));
  if (state_ == State::STARTING) {
    return;
  }
  StartReceivingMessagesInternal();
}

void FtlMessageReceptionChannel::StopReceivingMessages() {
  if (state_ == State::STOPPED) {
    return;
  }
  StopReceivingMessagesInternal();
  RunStartReceivingMessagesCallbacks(grpc::Status::CANCELLED);
}

const net::BackoffEntry&
FtlMessageReceptionChannel::GetReconnectRetryBackoffEntryForTesting() const {
  return reconnect_retry_backoff_;
}

void FtlMessageReceptionChannel::OnReceiveMessagesStreamClosed(
    const grpc::Status& status) {
  if (state_ == State::STOPPED) {
    // Previously closed by the caller.
    return;
  }
  if (status.error_code() == grpc::StatusCode::ABORTED ||
      status.error_code() == grpc::StatusCode::UNAVAILABLE) {
    // These are 'soft' connection errors that should be retried.
    // Other errors should be ignored.  See this file for more info:
    // third_party/grpc/src/include/grpcpp/impl/codegen/status_code_enum.h
    RetryStartReceivingMessagesWithBackoff();
    return;
  }
  StopReceivingMessagesInternal();
  RunStartReceivingMessagesCallbacks(status);
}

void FtlMessageReceptionChannel::OnMessageReceived(
    const ftl::ReceiveMessagesResponse& response) {
  switch (response.body_case()) {
    case ftl::ReceiveMessagesResponse::BodyCase::kInboxMessage: {
      VLOG(0) << "Received message";
      on_incoming_msg_.Run(response.inbox_message());
      break;
    }
    case ftl::ReceiveMessagesResponse::BodyCase::kPong:
      VLOG(0) << "Received pong";
      stream_pong_timer_->Reset();
      break;
    case ftl::ReceiveMessagesResponse::BodyCase::kStartOfBatch:
      state_ = State::STARTED;
      RunStartReceivingMessagesCallbacks(grpc::Status::OK);
      BeginStreamTimers();
      break;
    case ftl::ReceiveMessagesResponse::BodyCase::kEndOfBatch:
      VLOG(0) << "Received end of batch";
      break;
    default:
      LOG(WARNING) << "Received unknown message type: " << response.body_case();
      break;
  }
}

void FtlMessageReceptionChannel::RunStartReceivingMessagesCallbacks(
    const grpc::Status& status) {
  if (start_receiving_messages_callbacks_.empty()) {
    return;
  }
  for (DoneCallback& callback : start_receiving_messages_callbacks_) {
    std::move(callback).Run(status);
  }
  start_receiving_messages_callbacks_.clear();
}

void FtlMessageReceptionChannel::RetryStartReceivingMessagesWithBackoff() {
  reconnect_retry_backoff_.InformOfRequest(false);
  VLOG(0) << "RetryStartReceivingMessages will be called with backoff: "
          << reconnect_retry_backoff_.GetTimeUntilRelease();
  reconnect_retry_timer_.Start(
      FROM_HERE, reconnect_retry_backoff_.GetTimeUntilRelease(),
      base::BindOnce(&FtlMessageReceptionChannel::RetryStartReceivingMessages,
                     base::Unretained(this)));
}

void FtlMessageReceptionChannel::RetryStartReceivingMessages() {
  VLOG(0) << "RetryStartReceivingMessages called";
  StopReceivingMessagesInternal();
  StartReceivingMessagesInternal();
}

void FtlMessageReceptionChannel::StartReceivingMessagesInternal() {
  DCHECK_EQ(State::STOPPED, state_);
  state_ = State::STARTING;
  receive_messages_stream_ = stream_opener_.Run(
      base::BindRepeating(&FtlMessageReceptionChannel::OnMessageReceived,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(&FtlMessageReceptionChannel::OnReceiveMessagesStreamClosed,
                     weak_factory_.GetWeakPtr()));
}

void FtlMessageReceptionChannel::StopReceivingMessagesInternal() {
  DCHECK_NE(State::STOPPED, state_);
  state_ = State::STOPPED;
  receive_messages_stream_.reset();
  reconnect_retry_timer_.Stop();
  stream_lifetime_timer_.Stop();
  stream_pong_timer_.reset();
}

void FtlMessageReceptionChannel::BeginStreamTimers() {
  reconnect_retry_backoff_.Reset();
  stream_pong_timer_ = std::make_unique<base::DelayTimer>(
      FROM_HERE, kPongTimeout, this,
      &FtlMessageReceptionChannel::OnPongTimeout);
  stream_pong_timer_->Reset();
  stream_lifetime_timer_.Start(
      FROM_HERE, kStreamLifetime,
      base::BindOnce(&FtlMessageReceptionChannel::OnStreamLifetimeExceeded,
                     base::Unretained(this)));
}

void FtlMessageReceptionChannel::OnPongTimeout() {
  LOG(WARNING) << "Timed out waiting for PONG message from server.";
  RetryStartReceivingMessagesWithBackoff();
}

void FtlMessageReceptionChannel::OnStreamLifetimeExceeded() {
  VLOG(0) << "Reached maximum lifetime for current stream.";
  reconnect_retry_backoff_.Reset();
  RetryStartReceivingMessages();
}

}  // namespace remoting
