// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/data_pipe_bytes_consumer.h"

#include <algorithm>

#include "base/location.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

DataPipeBytesConsumer::DataPipeBytesConsumer(
    ExecutionContext* execution_context,
    mojo::ScopedDataPipeConsumerHandle data_pipe)
    : execution_context_(execution_context),
      data_pipe_(std::move(data_pipe)),
      watcher_(FROM_HERE,
               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
               execution_context->GetTaskRunner(TaskType::kNetworking)) {}

DataPipeBytesConsumer::~DataPipeBytesConsumer() {}

BytesConsumer::Result DataPipeBytesConsumer::BeginRead(const char** buffer,
                                                       size_t* available) {
  DCHECK(!is_in_two_phase_read_);
  *buffer = nullptr;
  *available = 0;
  if (state_ == InternalState::kClosed)
    return Result::kDone;
  if (state_ == InternalState::kErrored)
    return Result::kError;

  uint32_t pipe_available = 0;
  MojoResult rv =
      data_pipe_->BeginReadData(reinterpret_cast<const void**>(buffer),
                                &pipe_available, MOJO_READ_DATA_FLAG_NONE);

  switch (rv) {
    case MOJO_RESULT_OK:
      is_in_two_phase_read_ = true;
      *available = pipe_available;
      return Result::kOk;
    case MOJO_RESULT_SHOULD_WAIT:
      MaybeStartWatching();
      watcher_.ArmOrNotify();
      return Result::kShouldWait;
    case MOJO_RESULT_FAILED_PRECONDITION:
      Close();
      return Result::kDone;
    default:
      SetError();
      return Result::kError;
  }

  NOTREACHED();
}

BytesConsumer::Result DataPipeBytesConsumer::EndRead(size_t read) {
  DCHECK(is_in_two_phase_read_);
  is_in_two_phase_read_ = false;
  DCHECK(state_ == InternalState::kReadable ||
         state_ == InternalState::kWaiting);
  MojoResult rv = data_pipe_->EndReadData(read);
  if (rv != MOJO_RESULT_OK) {
    SetError();
    return Result::kError;
  }
  total_read_ += read;
  if (has_pending_notification_) {
    has_pending_notification_ = false;
    execution_context_->GetTaskRunner(TaskType::kNetworking)
        ->PostTask(FROM_HERE, WTF::Bind(&DataPipeBytesConsumer::Notify,
                                        WrapPersistent(this), MOJO_RESULT_OK));
  }
  return Result::kOk;
}

mojo::ScopedDataPipeConsumerHandle DataPipeBytesConsumer::DrainAsDataPipe() {
  DCHECK(!is_in_two_phase_read_);
  mojo::ScopedDataPipeConsumerHandle data_pipe = std::move(data_pipe_);
  Cancel();
  return data_pipe;
}

void DataPipeBytesConsumer::SetClient(BytesConsumer::Client* client) {
  DCHECK(!client_);
  DCHECK(client);
  if (state_ == InternalState::kReadable || state_ == InternalState::kWaiting)
    client_ = client;
}

void DataPipeBytesConsumer::ClearClient() {
  client_ = nullptr;
}

void DataPipeBytesConsumer::Cancel() {
  if (state_ == InternalState::kReadable || state_ == InternalState::kWaiting) {
    // We don't want the client to be notified in this case.
    BytesConsumer::Client* client = client_;
    client_ = nullptr;
    Close();
    client_ = client;
  }
}

BytesConsumer::PublicState DataPipeBytesConsumer::GetPublicState() const {
  return GetPublicStateFromInternalState(state_);
}

void DataPipeBytesConsumer::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
  visitor->Trace(client_);
  BytesConsumer::Trace(visitor);
}

void DataPipeBytesConsumer::Close() {
  DCHECK(!is_in_two_phase_read_);
  if (state_ == InternalState::kClosed)
    return;
  DCHECK(state_ == InternalState::kReadable ||
         state_ == InternalState::kWaiting);
  state_ = InternalState::kClosed;
  data_pipe_ = mojo::ScopedDataPipeConsumerHandle();
  watcher_.Cancel();
  ClearClient();
}

void DataPipeBytesConsumer::SetError() {
  DCHECK(!is_in_two_phase_read_);
  if (state_ == InternalState::kErrored)
    return;
  DCHECK(state_ == InternalState::kReadable ||
         state_ == InternalState::kWaiting);
  state_ = InternalState::kErrored;
  data_pipe_ = mojo::ScopedDataPipeConsumerHandle();
  watcher_.Cancel();
  error_ = Error("error");
  ClearClient();
}

void DataPipeBytesConsumer::Notify(MojoResult) {
  if (state_ == InternalState::kClosed || state_ == InternalState::kErrored) {
    return;
  }
  if (is_in_two_phase_read_) {
    has_pending_notification_ = true;
    return;
  }
  uint32_t read_size = 0;
  MojoResult rv =
      data_pipe_->ReadData(nullptr, &read_size, MOJO_READ_DATA_FLAG_NONE);
  BytesConsumer::Client* client = client_;
  switch (rv) {
    case MOJO_RESULT_OK:
    case MOJO_RESULT_FAILED_PRECONDITION:
      break;
    case MOJO_RESULT_SHOULD_WAIT:
      watcher_.ArmOrNotify();
      return;
    default:
      SetError();
      break;
  }
  if (client)
    client->OnStateChange();
}

void DataPipeBytesConsumer::MaybeStartWatching() {
  if (watcher_.IsWatching())
    return;

  watcher_.Watch(
      data_pipe_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      WTF::BindRepeating(&DataPipeBytesConsumer::Notify, WrapPersistent(this)));
}

}  // namespace blink
