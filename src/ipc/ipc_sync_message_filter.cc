// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_sync_message_filter.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_sync_message.h"
#include "mojo/public/cpp/bindings/sync_handle_registry.h"

namespace IPC {

namespace {

// A generic callback used when watching handles synchronously. Sets |*signal|
// to true.
void OnEventReady(bool* signal) {
  *signal = true;
}

}  // namespace

bool SyncMessageFilter::Send(Message* message) {
  if (!message->is_sync()) {
    {
      base::AutoLock auto_lock(lock_);
      if (!io_task_runner_.get()) {
        pending_messages_.emplace_back(base::WrapUnique(message));
        return true;
      }
    }
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SyncMessageFilter::SendOnIOThread, this, message));
    return true;
  }

  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  PendingSyncMsg pending_message(
      SyncMessage::GetMessageId(*message),
      static_cast<SyncMessage*>(message)->GetReplyDeserializer(),
      &done_event);

  {
    base::AutoLock auto_lock(lock_);
    // Can't use this class on the main thread or else it can lead to deadlocks.
    // Also by definition, can't use this on IO thread since we're blocking it.
    if (base::ThreadTaskRunnerHandle::IsSet()) {
      DCHECK(base::ThreadTaskRunnerHandle::Get() != listener_task_runner_);
      DCHECK(base::ThreadTaskRunnerHandle::Get() != io_task_runner_);
    }
    pending_sync_messages_.insert(&pending_message);

    if (io_task_runner_.get()) {
      io_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&SyncMessageFilter::SendOnIOThread, this, message));
    } else {
      pending_messages_.emplace_back(base::WrapUnique(message));
    }
  }

  bool done = false;
  bool shutdown = false;
  scoped_refptr<mojo::SyncHandleRegistry> registry =
      mojo::SyncHandleRegistry::current();
  auto on_shutdown_callback = base::BindRepeating(&OnEventReady, &shutdown);
  auto on_done_callback = base::BindRepeating(&OnEventReady, &done);
  registry->RegisterEvent(shutdown_event_, on_shutdown_callback);
  registry->RegisterEvent(&done_event, on_done_callback);

  const bool* stop_flags[] = { &done, &shutdown };
  registry->Wait(stop_flags, 2);
  if (done) {
    TRACE_EVENT_FLOW_END0(TRACE_DISABLED_BY_DEFAULT("toplevel.flow"),
                          "SyncMessageFilter::Send", &done_event);
  }

  registry->UnregisterEvent(shutdown_event_, on_shutdown_callback);
  registry->UnregisterEvent(&done_event, on_done_callback);

  {
    base::AutoLock auto_lock(lock_);
    delete pending_message.deserializer;
    pending_sync_messages_.erase(&pending_message);
  }

  return pending_message.send_result;
}

void SyncMessageFilter::OnFilterAdded(Channel* channel) {
  std::vector<std::unique_ptr<Message>> pending_messages;
  {
    base::AutoLock auto_lock(lock_);
    channel_ = channel;

    io_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    std::swap(pending_messages_, pending_messages);
  }
  for (auto& msg : pending_messages)
    SendOnIOThread(msg.release());
}

void SyncMessageFilter::OnChannelError() {
  base::AutoLock auto_lock(lock_);
  channel_ = nullptr;
  SignalAllEvents();
}

void SyncMessageFilter::OnChannelClosing() {
  base::AutoLock auto_lock(lock_);
  channel_ = nullptr;
  SignalAllEvents();
}

bool SyncMessageFilter::OnMessageReceived(const Message& message) {
  base::AutoLock auto_lock(lock_);
  for (PendingSyncMessages::iterator iter = pending_sync_messages_.begin();
       iter != pending_sync_messages_.end(); ++iter) {
    if (SyncMessage::IsMessageReplyTo(message, (*iter)->id)) {
      if (!message.is_reply_error()) {
        (*iter)->send_result =
            (*iter)->deserializer->SerializeOutputParameters(message);
      }
      TRACE_EVENT_FLOW_BEGIN0(TRACE_DISABLED_BY_DEFAULT("toplevel.flow"),
                              "SyncMessageFilter::OnMessageReceived",
                              (*iter)->done_event);
      (*iter)->done_event->Signal();
      return true;
    }
  }

  return false;
}

SyncMessageFilter::SyncMessageFilter(base::WaitableEvent* shutdown_event)
    : channel_(nullptr),
      listener_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      shutdown_event_(shutdown_event) {}

SyncMessageFilter::~SyncMessageFilter() = default;

void SyncMessageFilter::SendOnIOThread(Message* message) {
  if (channel_) {
    channel_->Send(message);
    return;
  }

  if (message->is_sync()) {
    // We don't know which thread sent it, but it doesn't matter, just signal
    // them all.
    base::AutoLock auto_lock(lock_);
    SignalAllEvents();
  }

  delete message;
}

void SyncMessageFilter::SignalAllEvents() {
  lock_.AssertAcquired();
  for (PendingSyncMessages::iterator iter = pending_sync_messages_.begin();
       iter != pending_sync_messages_.end(); ++iter) {
    TRACE_EVENT_FLOW_BEGIN0(TRACE_DISABLED_BY_DEFAULT("toplevel.flow"),
                            "SyncMessageFilter::SignalAllEvents",
                            (*iter)->done_event);
    (*iter)->done_event->Signal();
  }
}

void SyncMessageFilter::GetGenericRemoteAssociatedInterface(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  base::AutoLock auto_lock(lock_);
  DCHECK(io_task_runner_ && io_task_runner_->BelongsToCurrentThread());
  if (!channel_) {
    // Attach the associated interface to a disconnected pipe, so that the
    // associated interface pointer can be used to make calls (which are
    // dropped).
    mojo::AssociateWithDisconnectedPipe(std::move(handle));
    return;
  }

  Channel::AssociatedInterfaceSupport* support =
      channel_->GetAssociatedInterfaceSupport();
  support->GetGenericRemoteAssociatedInterface(
      interface_name, std::move(handle));
}

}  // namespace IPC
