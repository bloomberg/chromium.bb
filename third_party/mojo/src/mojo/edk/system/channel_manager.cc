// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/task_runner.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/channel_endpoint.h"
#include "mojo/edk/system/message_pipe_dispatcher.h"

namespace mojo {
namespace system {

namespace {

void ShutdownChannelHelper(const ChannelInfo& channel_info) {
  if (base::MessageLoopProxy::current() ==
      channel_info.channel_thread_task_runner) {
    channel_info.channel->Shutdown();
  } else {
    channel_info.channel->WillShutdownSoon();
    channel_info.channel_thread_task_runner->PostTask(
        FROM_HERE, base::Bind(&Channel::Shutdown, channel_info.channel));
  }
}

}  // namespace

ChannelManager::ChannelManager(embedder::PlatformSupport* platform_support)
    : platform_support_(platform_support) {
}

ChannelManager::~ChannelManager() {
  // No need to take the lock.
  for (const auto& map_elem : channel_infos_)
    ShutdownChannelHelper(map_elem.second);
}

scoped_refptr<MessagePipeDispatcher> ChannelManager::CreateChannelOnIOThread(
    ChannelId channel_id,
    embedder::ScopedPlatformHandle platform_handle) {
  scoped_refptr<system::ChannelEndpoint> bootstrap_channel_endpoint;
  scoped_refptr<system::MessagePipeDispatcher> dispatcher =
      system::MessagePipeDispatcher::CreateRemoteMessagePipe(
          &bootstrap_channel_endpoint);
  CreateChannelOnIOThreadHelper(channel_id, platform_handle.Pass(),
                                bootstrap_channel_endpoint);
  return dispatcher;
}

scoped_refptr<MessagePipeDispatcher> ChannelManager::CreateChannel(
    ChannelId channel_id,
    embedder::ScopedPlatformHandle platform_handle,
    scoped_refptr<base::TaskRunner> io_thread_task_runner,
    base::Closure callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  DCHECK(io_thread_task_runner);
  DCHECK(!callback.is_null());
  // (|callback_thread_task_runner| may be null.)

  scoped_refptr<system::ChannelEndpoint> bootstrap_channel_endpoint;
  scoped_refptr<system::MessagePipeDispatcher> dispatcher =
      system::MessagePipeDispatcher::CreateRemoteMessagePipe(
          &bootstrap_channel_endpoint);
  io_thread_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&ChannelManager::CreateChannelHelper, base::Unretained(this),
                 channel_id, base::Passed(&platform_handle),
                 bootstrap_channel_endpoint, callback,
                 callback_thread_task_runner));
  return dispatcher;
}

scoped_refptr<Channel> ChannelManager::GetChannel(ChannelId channel_id) const {
  base::AutoLock locker(lock_);
  auto it = channel_infos_.find(channel_id);
  DCHECK(it != channel_infos_.end());
  return it->second.channel;
}

void ChannelManager::WillShutdownChannel(ChannelId channel_id) {
  GetChannel(channel_id)->WillShutdownSoon();
}

void ChannelManager::ShutdownChannel(ChannelId channel_id) {
  ChannelInfo channel_info;
  {
    base::AutoLock locker(lock_);
    auto it = channel_infos_.find(channel_id);
    DCHECK(it != channel_infos_.end());
    channel_info.Swap(&it->second);
    channel_infos_.erase(it);
  }
  ShutdownChannelHelper(channel_info);
}

void ChannelManager::CreateChannelOnIOThreadHelper(
    ChannelId channel_id,
    embedder::ScopedPlatformHandle platform_handle,
    scoped_refptr<system::ChannelEndpoint> bootstrap_channel_endpoint) {
  DCHECK_NE(channel_id, kInvalidChannelId);
  DCHECK(platform_handle.is_valid());
  DCHECK(bootstrap_channel_endpoint);

  // Create and initialize a |system::Channel|.
  scoped_refptr<system::Channel> channel =
      new system::Channel(platform_support_);
  channel->Init(system::RawChannel::Create(platform_handle.Pass()));
  channel->SetBootstrapEndpoint(bootstrap_channel_endpoint);

  {
    base::AutoLock locker(lock_);
    CHECK(channel_infos_.find(channel_id) == channel_infos_.end());
    channel_infos_[channel_id] =
        ChannelInfo(channel, base::MessageLoopProxy::current());
  }
  channel->SetChannelManager(this);
}

void ChannelManager::CreateChannelHelper(
    ChannelId channel_id,
    embedder::ScopedPlatformHandle platform_handle,
    scoped_refptr<system::ChannelEndpoint> bootstrap_channel_endpoint,
    base::Closure callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  CreateChannelOnIOThreadHelper(channel_id, platform_handle.Pass(),
                                bootstrap_channel_endpoint);
  if (callback_thread_task_runner)
    callback_thread_task_runner->PostTask(FROM_HERE, callback);
  else
    callback.Run();
}

}  // namespace system
}  // namespace mojo
