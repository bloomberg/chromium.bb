// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_decode_accelerator_stub.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"

namespace gpu {

ImageDecodeAcceleratorStub::ImageDecodeAcceleratorStub(GpuChannel* channel,
                                                       int32_t route_id)
    : channel_(channel),
      sequence_(channel->scheduler()->CreateSequence(SchedulingPriority::kLow)),
      sync_point_client_state_(
          channel->sync_point_manager()->CreateSyncPointClientState(
              CommandBufferNamespace::GPU_IO,
              CommandBufferIdFromChannelAndRoute(channel->client_id(),
                                                 route_id),
              sequence_)),
      main_task_runner_(channel->task_runner()),
      io_task_runner_(channel->io_task_runner()) {}

bool ImageDecodeAcceleratorStub::OnMessageReceived(const IPC::Message& msg) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!base::FeatureList::IsEnabled(
          features::kVaapiJpegImageDecodeAcceleration)) {
    return false;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ImageDecodeAcceleratorStub, msg)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_ScheduleImageDecode,
                        OnScheduleImageDecode)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ImageDecodeAcceleratorStub::Shutdown() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  sync_point_client_state_->Destroy();
  channel_->scheduler()->DestroySequence(sequence_);
  channel_ = nullptr;
}

ImageDecodeAcceleratorStub::~ImageDecodeAcceleratorStub() {
  DCHECK(!channel_);
}

void ImageDecodeAcceleratorStub::OnScheduleImageDecode(
    const GpuChannelMsg_ScheduleImageDecode_Params& decode_params,
    uint64_t release_count) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  if (!channel_) {
    // The channel is no longer available, so don't schedule a decode.
    return;
  }
  // TODO(andrescj): schedule the release of the decode sync token and start the
  // decode.
}

}  // namespace gpu
