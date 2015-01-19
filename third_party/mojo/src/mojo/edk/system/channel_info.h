// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_CHANNEL_INFO_H_
#define MOJO_EDK_SYSTEM_CHANNEL_INFO_H_

#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace system {

struct MOJO_SYSTEM_IMPL_EXPORT ChannelInfo {
  ChannelInfo();
  ChannelInfo(scoped_refptr<Channel> channel,
              scoped_refptr<base::TaskRunner> channel_thread_task_runner);
  ~ChannelInfo();

  void Swap(ChannelInfo* other);

  scoped_refptr<Channel> channel;
  // The task runner for |channel|'s creation thread (a.k.a. its I/O thread), on
  // which it must, e.g., be shut down.
  scoped_refptr<base::TaskRunner> channel_thread_task_runner;
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_CHANNEL_INFO_H_
