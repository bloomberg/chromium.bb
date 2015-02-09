// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel_info.h"

#include <algorithm>

#include "base/task_runner.h"
#include "mojo/edk/system/channel.h"

namespace mojo {
namespace system {

ChannelInfo::ChannelInfo() {
}

ChannelInfo::ChannelInfo(
    scoped_refptr<Channel> channel,
    scoped_refptr<base::TaskRunner> channel_thread_task_runner)
    : channel(channel), channel_thread_task_runner(channel_thread_task_runner) {
}

ChannelInfo::~ChannelInfo() {
}

void ChannelInfo::Swap(ChannelInfo* other) {
  // Note: Swapping avoids refcount churn.
  std::swap(channel, other->channel);
  std::swap(channel_thread_task_runner, other->channel_thread_task_runner);
}

}  // namespace system
}  // namespace mojo
