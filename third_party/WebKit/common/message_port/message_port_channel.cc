// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/WebKit/common/message_port/message_port_channel.h"

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/WebKit/common/message_port/message_port.mojom.h"
#include "third_party/WebKit/common/message_port/message_port_message_struct_traits.h"

namespace blink {

MessagePortChannel::~MessagePortChannel() {}

MessagePortChannel::MessagePortChannel() : state_(new State()) {}

MessagePortChannel::MessagePortChannel(const MessagePortChannel& other)
    : state_(other.state_) {}

MessagePortChannel& MessagePortChannel::operator=(
    const MessagePortChannel& other) {
  state_ = other.state_;
  return *this;
}

MessagePortChannel::MessagePortChannel(mojo::ScopedMessagePipeHandle handle)
    : state_(new State(std::move(handle))) {}

const mojo::ScopedMessagePipeHandle& MessagePortChannel::GetHandle() const {
  return state_->handle();
}

mojo::ScopedMessagePipeHandle MessagePortChannel::ReleaseHandle() const {
  state_->StopWatching();
  return state_->TakeHandle();
}

// static
std::vector<mojo::ScopedMessagePipeHandle> MessagePortChannel::ReleaseHandles(
    const std::vector<MessagePortChannel>& ports) {
  std::vector<mojo::ScopedMessagePipeHandle> handles(ports.size());
  for (size_t i = 0; i < ports.size(); ++i)
    handles[i] = ports[i].ReleaseHandle();
  return handles;
}

// static
std::vector<MessagePortChannel> MessagePortChannel::CreateFromHandles(
    std::vector<mojo::ScopedMessagePipeHandle> handles) {
  std::vector<MessagePortChannel> ports(handles.size());
  for (size_t i = 0; i < handles.size(); ++i)
    ports[i] = MessagePortChannel(std::move(handles[i]));
  return ports;
}

void MessagePortChannel::PostMessage(const uint8_t* encoded_message,
                                     size_t encoded_message_size,
                                     std::vector<MessagePortChannel> ports) {
  DCHECK(state_->handle().is_valid());

  // NOTE: It is OK to ignore the return value of mojo::WriteMessageNew here.
  // HTML MessagePorts have no way of reporting when the peer is gone.

  MessagePortMessage msg;
  msg.encoded_message = base::make_span(encoded_message, encoded_message_size);
  msg.ports = ReleaseHandles(ports);
  mojo::Message mojo_message =
      mojom::MessagePortMessage::SerializeAsMessage(&msg);
  mojo::WriteMessageNew(state_->handle().get(), mojo_message.TakeMojoMessage(),
                        MOJO_WRITE_MESSAGE_FLAG_NONE);
}

bool MessagePortChannel::GetMessage(std::vector<uint8_t>* encoded_message,
                                    std::vector<MessagePortChannel>* ports) {
  DCHECK(state_->handle().is_valid());
  mojo::ScopedMessageHandle message_handle;
  MojoResult rv = mojo::ReadMessageNew(state_->handle().get(), &message_handle,
                                       MOJO_READ_MESSAGE_FLAG_NONE);
  if (rv != MOJO_RESULT_OK)
    return false;

  mojo::Message message(std::move(message_handle));
  MessagePortMessage msg;
  bool success = mojom::MessagePortMessage::DeserializeFromMessage(
      std::move(message), &msg);
  if (!success)
    return false;

  *encoded_message = std::move(msg.owned_encoded_message);
  *ports = CreateFromHandles(std::move(msg.ports));

  return true;
}

void MessagePortChannel::SetCallback(const base::Closure& callback) {
  state_->StopWatching();
  state_->StartWatching(callback);
}

void MessagePortChannel::ClearCallback() {
  state_->StopWatching();
}

MessagePortChannel::State::State() {}

MessagePortChannel::State::State(mojo::ScopedMessagePipeHandle handle)
    : handle_(std::move(handle)) {}

void MessagePortChannel::State::StartWatching(const base::Closure& callback) {
  base::AutoLock lock(lock_);
  DCHECK(!callback_);
  DCHECK(handle_.is_valid());
  callback_ = callback;

  DCHECK(!watcher_handle_.is_valid());
  MojoResult rv = CreateWatcher(&State::CallOnHandleReady, &watcher_handle_);
  DCHECK_EQ(MOJO_RESULT_OK, rv);

  // Balanced in CallOnHandleReady when MOJO_RESULT_CANCELLED is received.
  AddRef();

  // NOTE: An HTML MessagePort does not receive an event to tell it when the
  // peer has gone away, so we only watch for readability here.
  rv = MojoWatch(watcher_handle_.get().value(), handle_.get().value(),
                 MOJO_HANDLE_SIGNAL_READABLE, MOJO_WATCH_CONDITION_SATISFIED,
                 reinterpret_cast<uintptr_t>(this));
  DCHECK_EQ(MOJO_RESULT_OK, rv);

  ArmWatcher();
}

void MessagePortChannel::State::StopWatching() {
  mojo::ScopedWatcherHandle watcher_handle;

  {
    // NOTE: Resetting the watcher handle may synchronously invoke
    // OnHandleReady(), so we don't hold |lock_| while doing that.
    base::AutoLock lock(lock_);
    watcher_handle = std::move(watcher_handle_);
    callback_.Reset();
  }
}

mojo::ScopedMessagePipeHandle MessagePortChannel::State::TakeHandle() {
  base::AutoLock lock(lock_);
  DCHECK(!watcher_handle_.is_valid());
  return std::move(handle_);
}

MessagePortChannel::State::~State() = default;

void MessagePortChannel::State::ArmWatcher() {
  lock_.AssertAcquired();

  if (!watcher_handle_.is_valid())
    return;

  uint32_t num_ready_contexts = 1;
  uintptr_t ready_context;
  MojoResult ready_result;
  MojoHandleSignalsState ready_state;
  MojoResult rv =
      MojoArmWatcher(watcher_handle_.get().value(), &num_ready_contexts,
                     &ready_context, &ready_result, &ready_state);
  if (rv == MOJO_RESULT_OK)
    return;

  // The watcher could not be armed because it would notify immediately.
  DCHECK_EQ(MOJO_RESULT_FAILED_PRECONDITION, rv);
  DCHECK_EQ(1u, num_ready_contexts);
  DCHECK_EQ(reinterpret_cast<uintptr_t>(this), ready_context);

  if (ready_result == MOJO_RESULT_OK) {
    // The handle is already signaled, so we trigger a callback now.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&State::OnHandleReady, this, MOJO_RESULT_OK));
    return;
  }

  if (ready_result == MOJO_RESULT_FAILED_PRECONDITION) {
    DVLOG(1) << this << " MojoArmWatcher failed because of a broken pipe.";
    return;
  }

  NOTREACHED();
}

void MessagePortChannel::State::OnHandleReady(MojoResult result) {
  base::AutoLock lock(lock_);
  if (result == MOJO_RESULT_OK && callback_) {
    callback_.Run();
    ArmWatcher();
  } else {
    // And now his watch is ended.
  }
}

// static
void MessagePortChannel::State::CallOnHandleReady(
    uintptr_t context,
    MojoResult result,
    MojoHandleSignalsState signals_state,
    MojoWatcherNotificationFlags flags) {
  auto* state = reinterpret_cast<State*>(context);
  if (result == MOJO_RESULT_CANCELLED) {
    // Balanced in MessagePortChannel::State::StartWatching().
    state->Release();
  } else {
    state->OnHandleReady(result);
  }
}

}  // namespace blink
