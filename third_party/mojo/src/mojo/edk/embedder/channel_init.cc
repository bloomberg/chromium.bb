// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/channel_init.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "mojo/edk/embedder/embedder.h"

namespace mojo {
namespace embedder {

ChannelInit::ChannelInit() : channel_info_(nullptr), weak_factory_(this) {
}

ChannelInit::~ChannelInit() {
  // TODO(vtl): This is likely leaky in common scenarios (we're on the main
  // thread, which outlives the I/O thread, and we're destroyed after the I/O
  // thread is destroyed.
  if (channel_info_)
    DestroyChannel(channel_info_);
}

ScopedMessagePipeHandle ChannelInit::Init(
    base::PlatformFile file,
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  ScopedMessagePipeHandle message_pipe =
      CreateChannel(ScopedPlatformHandle(PlatformHandle(file)),
                    io_thread_task_runner,
                    base::Bind(&ChannelInit::OnCreatedChannel,
                               weak_factory_.GetWeakPtr()),
                    base::MessageLoop::current()->task_runner()).Pass();
  return message_pipe.Pass();
}

void ChannelInit::WillDestroySoon() {
  if (channel_info_)
    WillDestroyChannelSoon(channel_info_);
}

// static
void ChannelInit::OnCreatedChannel(base::WeakPtr<ChannelInit> self,
                                   ChannelInfo* channel) {
  // If |self| was already destroyed, shut the channel down.
  if (!self) {
    DestroyChannel(channel);
    return;
  }

  DCHECK(!self->channel_info_);
  self->channel_info_ = channel;
}

}  // namespace embedder
}  // namespace mojo
