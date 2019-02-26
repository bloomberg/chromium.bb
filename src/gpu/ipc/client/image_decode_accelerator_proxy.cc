// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/image_decode_accelerator_proxy.h"

#include <vector>

#include "gpu/command_buffer/common/constants.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

ImageDecodeAcceleratorProxy::ImageDecodeAcceleratorProxy(GpuChannelHost* host,
                                                         int32_t route_id)
    : host_(host), route_id_(route_id) {}

ImageDecodeAcceleratorProxy::~ImageDecodeAcceleratorProxy() {}

SyncToken ImageDecodeAcceleratorProxy::ScheduleImageDecode(
    base::span<const uint8_t> encoded_data,
    const gfx::Size& output_size,
    CommandBufferId raster_decoder_command_buffer_id,
    uint32_t transfer_cache_entry_id,
    int32_t discardable_handle_shm_id,
    uint32_t discardable_handle_shm_offset,
    const gfx::ColorSpace& target_color_space,
    bool needs_mips) {
  DCHECK(host_);
  DCHECK_EQ(host_->channel_id(),
            ChannelIdFromCommandBufferId(raster_decoder_command_buffer_id));

  GpuChannelMsg_ScheduleImageDecode_Params params;
  params.encoded_data =
      std::vector<uint8_t>(encoded_data.cbegin(), encoded_data.cend());
  params.output_size = output_size;
  params.raster_decoder_route_id =
      RouteIdFromCommandBufferId(raster_decoder_command_buffer_id);
  params.transfer_cache_entry_id = transfer_cache_entry_id;
  params.discardable_handle_shm_id = discardable_handle_shm_id;
  params.discardable_handle_shm_offset = discardable_handle_shm_offset;
  params.target_color_space = target_color_space;
  params.needs_mips = needs_mips;

  base::AutoLock lock(lock_);
  uint64_t release_count = ++next_release_count_;
  // Note: we send the message under the lock to guarantee monotonicity of the
  // release counts as seen by the service.
  host_->Send(new GpuChannelMsg_ScheduleImageDecode(
      route_id_, std::move(params), release_count));
  return SyncToken(
      CommandBufferNamespace::GPU_IO,
      CommandBufferIdFromChannelAndRoute(host_->channel_id(), route_id_),
      release_count);
}

}  // namespace gpu
