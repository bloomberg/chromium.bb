// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/embedder.h"

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_support.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/channel_endpoint.h"
#include "mojo/edk/system/channel_manager.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/core.h"
#include "mojo/edk/system/message_pipe_dispatcher.h"
#include "mojo/edk/system/platform_handle_dispatcher.h"
#include "mojo/edk/system/raw_channel.h"

namespace mojo {
namespace embedder {

namespace {

// TODO(vtl): For now, we need this to be thread-safe (since theoretically we
// currently support multiple channel creation threads -- possibly one per
// channel). Eventually, we won't need it to be thread-safe (we'll require a
// single I/O thread), and eventually we won't need it at all. Remember to
// remove the base/atomicops.h include.
system::ChannelId MakeChannelId() {
  // Note that |AtomicWord| is signed.
  static base::subtle::AtomicWord counter = 0;

  base::subtle::AtomicWord new_counter_value =
      base::subtle::NoBarrier_AtomicIncrement(&counter, 1);
  // Don't allow the counter to wrap. Note that any (strictly) positive value is
  // a valid |ChannelId| (and |NoBarrier_AtomicIncrement()| returns the value
  // post-increment).
  CHECK_GT(new_counter_value, 0);
  // Use "negative" values for these IDs, so that we'll also be able to use
  // "positive" "process identifiers" (see connection_manager.h) as IDs (and
  // they won't conflict).
  return static_cast<system::ChannelId>(-new_counter_value);
}

}  // namespace

namespace internal {

// Declared in embedder_internal.h.
PlatformSupport* g_platform_support = nullptr;
system::Core* g_core = nullptr;
system::ChannelManager* g_channel_manager = nullptr;

}  // namespace internal

void Init(scoped_ptr<PlatformSupport> platform_support) {
  DCHECK(platform_support);

  DCHECK(!internal::g_platform_support);
  internal::g_platform_support = platform_support.release();

  DCHECK(!internal::g_core);
  internal::g_core = new system::Core(internal::g_platform_support);

  DCHECK(!internal::g_channel_manager);
  internal::g_channel_manager =
      new system::ChannelManager(internal::g_platform_support);
}

Configuration* GetConfiguration() {
  return system::GetMutableConfiguration();
}

// TODO(vtl): Write tests for this.
ScopedMessagePipeHandle CreateChannelOnIOThread(
    ScopedPlatformHandle platform_handle,
    ChannelInfo** channel_info) {
  DCHECK(platform_handle.is_valid());
  DCHECK(channel_info);

  scoped_refptr<system::ChannelEndpoint> channel_endpoint;
  scoped_refptr<system::MessagePipeDispatcher> dispatcher =
      system::MessagePipeDispatcher::CreateRemoteMessagePipe(&channel_endpoint);

  DCHECK(internal::g_core);
  ScopedMessagePipeHandle rv(
      MessagePipeHandle(internal::g_core->AddDispatcher(dispatcher)));

  *channel_info = new ChannelInfo(MakeChannelId());
  internal::g_channel_manager->CreateChannelOnIOThread(
      (*channel_info)->channel_id, platform_handle.Pass(), channel_endpoint);

  return rv.Pass();
}

ScopedMessagePipeHandle CreateChannel(
    ScopedPlatformHandle platform_handle,
    scoped_refptr<base::TaskRunner> io_thread_task_runner,
    DidCreateChannelCallback callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  DCHECK(platform_handle.is_valid());
  DCHECK(io_thread_task_runner);
  DCHECK(!callback.is_null());

  scoped_refptr<system::ChannelEndpoint> channel_endpoint;
  scoped_refptr<system::MessagePipeDispatcher> dispatcher =
      system::MessagePipeDispatcher::CreateRemoteMessagePipe(&channel_endpoint);

  DCHECK(internal::g_core);
  ScopedMessagePipeHandle rv(
      MessagePipeHandle(internal::g_core->AddDispatcher(dispatcher)));

  // We'll have to set |channel_info->channel_id| on the I/O thread.
  scoped_ptr<ChannelInfo> channel_info(new ChannelInfo());

  if (rv.is_valid()) {
    system::ChannelId channel_id = MakeChannelId();
    channel_info->channel_id = channel_id;
    internal::g_channel_manager->CreateChannel(
        channel_id, platform_handle.Pass(), channel_endpoint,
        io_thread_task_runner,
        base::Bind(callback, base::Unretained(channel_info.release())),
        callback_thread_task_runner);
  } else {
    (callback_thread_task_runner ? callback_thread_task_runner
                                 : io_thread_task_runner)
        ->PostTask(FROM_HERE, base::Bind(callback, channel_info.release()));
  }

  return rv.Pass();
}

// TODO(vtl): Write tests for this.
void DestroyChannel(ChannelInfo* channel_info) {
  DCHECK(channel_info);
  DCHECK(channel_info->channel_id);
  DCHECK(internal::g_channel_manager);
  // This will destroy the channel synchronously if called from the channel
  // thread.
  internal::g_channel_manager->ShutdownChannel(channel_info->channel_id);
  delete channel_info;
}

void WillDestroyChannelSoon(ChannelInfo* channel_info) {
  DCHECK(channel_info);
  DCHECK(internal::g_channel_manager);
  internal::g_channel_manager->WillShutdownChannel(channel_info->channel_id);
}

MojoResult CreatePlatformHandleWrapper(
    ScopedPlatformHandle platform_handle,
    MojoHandle* platform_handle_wrapper_handle) {
  DCHECK(platform_handle_wrapper_handle);

  scoped_refptr<system::Dispatcher> dispatcher(
      new system::PlatformHandleDispatcher(platform_handle.Pass()));

  DCHECK(internal::g_core);
  MojoHandle h = internal::g_core->AddDispatcher(dispatcher);
  if (h == MOJO_HANDLE_INVALID) {
    LOG(ERROR) << "Handle table full";
    dispatcher->Close();
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  *platform_handle_wrapper_handle = h;
  return MOJO_RESULT_OK;
}

MojoResult PassWrappedPlatformHandle(MojoHandle platform_handle_wrapper_handle,
                                     ScopedPlatformHandle* platform_handle) {
  DCHECK(platform_handle);

  DCHECK(internal::g_core);
  scoped_refptr<system::Dispatcher> dispatcher(
      internal::g_core->GetDispatcher(platform_handle_wrapper_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (dispatcher->GetType() != system::Dispatcher::kTypePlatformHandle)
    return MOJO_RESULT_INVALID_ARGUMENT;

  *platform_handle =
      static_cast<system::PlatformHandleDispatcher*>(dispatcher.get())
          ->PassPlatformHandle()
          .Pass();
  return MOJO_RESULT_OK;
}

MojoResult AsyncWait(MojoHandle handle,
                     MojoHandleSignals signals,
                     base::Callback<void(MojoResult)> callback) {
  return internal::g_core->AsyncWait(handle, signals, callback);
}

}  // namespace embedder
}  // namespace mojo
