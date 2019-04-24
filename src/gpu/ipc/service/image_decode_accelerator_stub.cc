// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_decode_accelerator_stub.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/image_decode_accelerator_worker.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/color_space.h"

namespace gpu {
class Buffer;

ImageDecodeAcceleratorStub::ImageDecodeAcceleratorStub(
    ImageDecodeAcceleratorWorker* worker,
    GpuChannel* channel,
    int32_t route_id)
    : worker_(worker),
      channel_(channel),
      sequence_(channel->scheduler()->CreateSequence(SchedulingPriority::kLow)),
      sync_point_client_state_(
          channel->sync_point_manager()->CreateSyncPointClientState(
              CommandBufferNamespace::GPU_IO,
              CommandBufferIdFromChannelAndRoute(channel->client_id(),
                                                 route_id),
              sequence_)),
      main_task_runner_(channel->task_runner()),
      io_task_runner_(channel->io_task_runner()) {
  // We need the sequence to be initially disabled so that when we schedule a
  // task to release the decode sync token, it doesn't run immediately (we want
  // it to run when the decode is done).
  channel_->scheduler()->DisableSequence(sequence_);
}

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
  if (!channel_ || destroying_channel_) {
    // The channel is no longer available, so don't do anything.
    return;
  }

  // Make sure the decode sync token is ordered with respect to the last decode
  // request.
  if (release_count <= last_release_count_) {
    DLOG(ERROR) << "Out-of-order decode sync token";
    OnError();
    return;
  }
  last_release_count_ = release_count;

  // Make sure the output dimensions are not too small.
  if (decode_params.output_size.IsEmpty()) {
    DLOG(ERROR) << "Output dimensions are too small";
    OnError();
    return;
  }

  // Start the actual decode.
  worker_->Decode(
      std::move(decode_params.encoded_data), decode_params.output_size,
      base::BindOnce(&ImageDecodeAcceleratorStub::OnDecodeCompleted,
                     base::WrapRefCounted(this), decode_params.output_size));

  // Schedule a task to eventually release the decode sync token. Note that this
  // task won't run until the sequence is re-enabled when a decode completes.
  const SyncToken discardable_handle_sync_token = SyncToken(
      CommandBufferNamespace::GPU_IO,
      CommandBufferIdFromChannelAndRoute(channel_->client_id(),
                                         decode_params.raster_decoder_route_id),
      decode_params.discardable_handle_release_count);
  channel_->scheduler()->ScheduleTask(Scheduler::Task(
      sequence_,
      base::BindOnce(&ImageDecodeAcceleratorStub::ProcessCompletedDecode,
                     base::WrapRefCounted(this), std::move(decode_params),
                     release_count),
      {discardable_handle_sync_token} /* sync_token_fences */));
}

void ImageDecodeAcceleratorStub::ProcessCompletedDecode(
    GpuChannelMsg_ScheduleImageDecode_Params params,
    uint64_t decode_release_count) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  if (!channel_ || destroying_channel_) {
    // The channel is no longer available, so don't do anything.
    return;
  }

  DCHECK(!pending_completed_decodes_.empty());
  std::unique_ptr<CompletedDecode> completed_decode =
      std::move(pending_completed_decodes_.front());

  // Gain access to the transfer cache through the GpuChannelManager's
  // SharedContextState. We will also use that to get a GrContext that will be
  // used for uploading the image.
  ContextResult context_result;
  scoped_refptr<SharedContextState> shared_context_state =
      channel_->gpu_channel_manager()->GetSharedContextState(&context_result);
  if (context_result != ContextResult::kSuccess) {
    DLOG(ERROR) << "Unable to obtain the SharedContextState";
    OnError();
    return;
  }
  DCHECK(shared_context_state);
  if (!shared_context_state->gr_context()) {
    DLOG(ERROR) << "Could not get the GrContext";
    OnError();
    return;
  }
  if (!shared_context_state->MakeCurrent(nullptr /* surface */)) {
    DLOG(ERROR) << "Could not MakeCurrent the shared context";
    OnError();
    return;
  }

  // Insert the cache entry in the transfer cache. Note that this section
  // validates several of the IPC parameters: |params.raster_decoder_route_id|,
  // |params.transfer_cache_entry_id|, |params.discardable_handle_shm_id|, and
  // |params.discardable_handle_shm_offset|.
  CommandBufferStub* command_buffer =
      channel_->LookupCommandBuffer(params.raster_decoder_route_id);
  if (!command_buffer) {
    DLOG(ERROR) << "Could not find the command buffer";
    OnError();
    return;
  }
  scoped_refptr<Buffer> handle_buffer =
      command_buffer->GetTransferBuffer(params.discardable_handle_shm_id);
  if (!DiscardableHandleBase::ValidateParameters(
          handle_buffer.get(), params.discardable_handle_shm_offset)) {
    DLOG(ERROR) << "Could not validate the discardable handle parameters";
    OnError();
    return;
  }
  DCHECK(command_buffer->decoder_context());
  if (command_buffer->decoder_context()->GetRasterDecoderId() < 0) {
    DLOG(ERROR) << "Could not get the raster decoder ID";
    OnError();
    return;
  }
  DCHECK(shared_context_state->transfer_cache());
  if (!shared_context_state->transfer_cache()->CreateLockedImageEntry(
          command_buffer->decoder_context()->GetRasterDecoderId(),
          params.transfer_cache_entry_id,
          ServiceDiscardableHandle(std::move(handle_buffer),
                                   params.discardable_handle_shm_offset,
                                   params.discardable_handle_shm_id),
          shared_context_state->gr_context(),
          base::make_span(completed_decode->output),
          completed_decode->row_bytes, completed_decode->image_info,
          params.needs_mips, params.target_color_space.ToSkColorSpace())) {
    DLOG(ERROR) << "Could not create and insert the transfer cache entry";
    OnError();
    return;
  }
  shared_context_state->set_need_context_state_reset(true);

  // All done! The decoded image can now be used for rasterization, so we can
  // release the decode sync token.
  sync_point_client_state_->ReleaseFenceSync(decode_release_count);

  // If there are no more completed decodes to be processed, we can disable the
  // sequence: when the next decode is completed, the sequence will be
  // re-enabled.
  pending_completed_decodes_.pop();
  if (pending_completed_decodes_.empty())
    channel_->scheduler()->DisableSequence(sequence_);
}

ImageDecodeAcceleratorStub::CompletedDecode::CompletedDecode(
    std::vector<uint8_t> output,
    size_t row_bytes,
    SkImageInfo image_info)
    : output(std::move(output)), row_bytes(row_bytes), image_info(image_info) {}

ImageDecodeAcceleratorStub::CompletedDecode::~CompletedDecode() = default;

void ImageDecodeAcceleratorStub::OnDecodeCompleted(
    gfx::Size expected_output_size,
    std::vector<uint8_t> output,
    size_t row_bytes,
    SkImageInfo image_info) {
  base::AutoLock lock(lock_);
  if (!channel_ || destroying_channel_) {
    // The channel is no longer available, so don't do anything.
    return;
  }

  if (output.empty()) {
    DLOG(ERROR) << "The decode failed";
    OnError();
    return;
  }

  // Some sanity checks on the output of the decoder.
  DCHECK_EQ(expected_output_size.width(), image_info.width());
  DCHECK_EQ(expected_output_size.height(), image_info.height());
  DCHECK_NE(0u, image_info.minRowBytes());
  DCHECK_GE(row_bytes, image_info.minRowBytes());
  DCHECK_EQ(output.size(), image_info.computeByteSize(row_bytes));

  // The decode is ready to be processed: add it to |pending_completed_decodes_|
  // so that ProcessCompletedDecode() can pick it up.
  pending_completed_decodes_.push(std::make_unique<CompletedDecode>(
      std::move(output), row_bytes, image_info));

  // We only need to enable the sequence when the number of pending completed
  // decodes is 1. If there are more, the sequence should already be enabled.
  if (pending_completed_decodes_.size() == 1u)
    channel_->scheduler()->EnableSequence(sequence_);
}

void ImageDecodeAcceleratorStub::OnError() {
  lock_.AssertAcquired();
  DCHECK(channel_);

  // Trigger the destruction of the channel and stop processing further
  // completed decodes, even if they're successful. We can't call
  // GpuChannel::OnChannelError() directly because that will end up calling
  // ImageDecodeAcceleratorStub::Shutdown() while |lock_| is still acquired. So,
  // we post a task to the main thread instead.
  destroying_channel_ = true;
  channel_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuChannel::OnChannelError, channel_->AsWeakPtr()));
}

}  // namespace gpu
