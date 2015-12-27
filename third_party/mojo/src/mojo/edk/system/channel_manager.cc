// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/mojo/src/mojo/edk/system/channel_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/task_runner.h"
#include "third_party/mojo/src/mojo/edk/system/channel.h"
#include "third_party/mojo/src/mojo/edk/system/channel_endpoint.h"
#include "third_party/mojo/src/mojo/edk/system/message_pipe_dispatcher.h"

namespace mojo {
namespace system {

ChannelManager::ChannelManager(
    embedder::PlatformSupport* platform_support,
    scoped_refptr<base::TaskRunner> io_thread_task_runner,
    ConnectionManager* connection_manager)
    : platform_support_(platform_support),
      io_thread_task_runner_(io_thread_task_runner),
      connection_manager_(connection_manager),
      weak_factory_(this) {
  DCHECK(platform_support_);
  DCHECK(io_thread_task_runner_);
  // (|connection_manager_| may be null.)
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

ChannelManager::~ChannelManager() {
  // |Shutdown()| must be called before destruction and have been completed.
  // TODO(vtl): This doesn't verify the above condition very strictly at all
  // (e.g., we may never have had any channels, or we may have manually shut all
  // the channels down).
  DCHECK(channels_.empty());
  DCHECK(!weak_factory_.HasWeakPtrs());
}

void ChannelManager::ShutdownOnIOThread() {
  // Taking this lock really shouldn't be necessary, but we do it for
  // consistency.
  ChannelIdToChannelMap channels;
  {
    MutexLocker locker(&mutex_);
    channels.swap(channels_);
  }

  for (auto& channel : channels)
    channel.second->Shutdown();

  weak_factory_.InvalidateWeakPtrs();
}

void ChannelManager::Shutdown(
    const base::Closure& callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  bool ok = io_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ChannelManager::ShutdownHelper, base::Unretained(this),
                 callback, callback_thread_task_runner));
  DCHECK(ok);
}

scoped_refptr<MessagePipeDispatcher> ChannelManager::CreateChannelOnIOThread(
    ChannelId channel_id,
    embedder::ScopedPlatformHandle platform_handle) {
  scoped_refptr<system::ChannelEndpoint> bootstrap_channel_endpoint;
  scoped_refptr<system::MessagePipeDispatcher> dispatcher =
      system::MessagePipeDispatcher::CreateRemoteMessagePipe(
          &bootstrap_channel_endpoint);
  CreateChannelOnIOThreadHelper(channel_id, std::move(platform_handle),
                                bootstrap_channel_endpoint);
  return dispatcher;
}

scoped_refptr<Channel> ChannelManager::CreateChannelWithoutBootstrapOnIOThread(
    ChannelId channel_id,
    embedder::ScopedPlatformHandle platform_handle) {
  return CreateChannelOnIOThreadHelper(channel_id, std::move(platform_handle),
                                       nullptr);
}

scoped_refptr<MessagePipeDispatcher> ChannelManager::CreateChannel(
    ChannelId channel_id,
    embedder::ScopedPlatformHandle platform_handle,
    const base::Closure& callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  DCHECK(!callback.is_null());
  // (|callback_thread_task_runner| may be null.)

  scoped_refptr<system::ChannelEndpoint> bootstrap_channel_endpoint;
  scoped_refptr<system::MessagePipeDispatcher> dispatcher =
      system::MessagePipeDispatcher::CreateRemoteMessagePipe(
          &bootstrap_channel_endpoint);
  bool ok = io_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ChannelManager::CreateChannelHelper, weak_ptr_,
                 channel_id, base::Passed(&platform_handle),
                 bootstrap_channel_endpoint, callback,
                 callback_thread_task_runner));
  DCHECK(ok);
  return dispatcher;
}

scoped_refptr<Channel> ChannelManager::GetChannel(ChannelId channel_id) const {
  MutexLocker locker(&mutex_);
  auto it = channels_.find(channel_id);
  DCHECK(it != channels_.end());
  return it->second;
}

void ChannelManager::WillShutdownChannel(ChannelId channel_id) {
  GetChannel(channel_id)->WillShutdownSoon();
}

void ChannelManager::ShutdownChannelOnIOThread(ChannelId channel_id) {
  scoped_refptr<Channel> channel;
  {
    MutexLocker locker(&mutex_);
    auto it = channels_.find(channel_id);
    DCHECK(it != channels_.end());
    channel.swap(it->second);
    channels_.erase(it);
  }
  channel->Shutdown();
}

void ChannelManager::ShutdownChannel(
    ChannelId channel_id,
    const base::Closure& callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  WillShutdownChannel(channel_id);
  bool ok = io_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(
          &ChannelManager::ShutdownChannelHelper, weak_ptr_,
          channel_id, callback, callback_thread_task_runner));
  DCHECK(ok);
}

void ChannelManager::ShutdownChannelHelper(
    ChannelId channel_id,
    const base::Closure& callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  ShutdownChannelOnIOThread(channel_id);
  if (callback_thread_task_runner) {
    bool ok = callback_thread_task_runner->PostTask(FROM_HERE, callback);
    DCHECK(ok);
  } else {
    callback.Run();
  }
}

void ChannelManager::ShutdownHelper(
    const base::Closure& callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  ShutdownOnIOThread();
  if (callback_thread_task_runner) {
    bool ok = callback_thread_task_runner->PostTask(FROM_HERE, callback);
    DCHECK(ok);
  } else {
    callback.Run();
  }
}

scoped_refptr<Channel> ChannelManager::CreateChannelOnIOThreadHelper(
    ChannelId channel_id,
    embedder::ScopedPlatformHandle platform_handle,
    scoped_refptr<system::ChannelEndpoint> bootstrap_channel_endpoint) {
  DCHECK_NE(channel_id, kInvalidChannelId);
  DCHECK(platform_handle.is_valid());

  // Create and initialize a |system::Channel|.
  scoped_refptr<system::Channel> channel =
      new system::Channel(platform_support_);
  channel->Init(system::RawChannel::Create(std::move(platform_handle)));
  if (bootstrap_channel_endpoint)
    channel->SetBootstrapEndpoint(bootstrap_channel_endpoint);

  {
    MutexLocker locker(&mutex_);
    CHECK(channels_.find(channel_id) == channels_.end());
    channels_[channel_id] = channel;
  }
  channel->SetChannelManager(this);
  return channel;
}

// static
void ChannelManager::CreateChannelHelper(
    base::WeakPtr<ChannelManager> channel_manager,
    ChannelId channel_id,
    embedder::ScopedPlatformHandle platform_handle,
    scoped_refptr<system::ChannelEndpoint> bootstrap_channel_endpoint,
    const base::Closure& callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  // TODO(amistry): Handle this gracefully after determining exactly what cases
  // can cause this. There appear to be crashes caused by ChannelManager being
  // destroyed before this point, which shouldn't be possible in the current
  // uses of ChannelManager.
  CHECK(channel_manager);
  channel_manager->CreateChannelOnIOThreadHelper(
      channel_id, std::move(platform_handle), bootstrap_channel_endpoint);
  if (callback_thread_task_runner) {
    bool ok = callback_thread_task_runner->PostTask(FROM_HERE, callback);
    DCHECK(ok);
  } else {
    callback.Run();
  }
}

}  // namespace system
}  // namespace mojo
