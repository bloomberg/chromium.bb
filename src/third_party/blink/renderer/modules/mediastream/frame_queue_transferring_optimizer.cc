// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/frame_queue_transferring_optimizer.h"

#include "media/base/video_frame.h"
#include "third_party/blink/renderer/modules/mediastream/transferred_frame_queue_underlying_source.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame_serialization_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

template <typename NativeFrameType>
FrameQueueTransferringOptimizer<NativeFrameType>::
    FrameQueueTransferringOptimizer(
        FrameQueueHost* host,
        scoped_refptr<base::SequencedTaskRunner> host_runner,
        wtf_size_t max_queue_size,
        ConnectHostCallback connect_host_callback)
    : host_(host),
      host_runner_(std::move(host_runner)),
      connect_host_callback_(std::move(connect_host_callback)),
      max_queue_size_(max_queue_size) {}

template <typename NativeFrameType>
UnderlyingSourceBase*
FrameQueueTransferringOptimizer<NativeFrameType>::PerformInProcessOptimization(
    ScriptState* script_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  scoped_refptr<base::SingleThreadTaskRunner> current_runner =
      context->GetTaskRunner(TaskType::kInternalMediaRealTime);

  auto host = host_.Lock();
  if (!host)
    return nullptr;

  auto* source = MakeGarbageCollected<
      TransferredFrameQueueUnderlyingSource<NativeFrameType>>(
      script_state, host, host_runner_, max_queue_size_);

  PostCrossThreadTask(
      *host_runner_, FROM_HERE,
      CrossThreadBindOnce(std::move(connect_host_callback_), current_runner,
                          WrapCrossThreadPersistent(source)));
  return source;
}

template class MODULES_TEMPLATE_EXPORT FrameQueueTransferringOptimizer<
    std::unique_ptr<AudioFrameSerializationData>>;
template class MODULES_TEMPLATE_EXPORT
    FrameQueueTransferringOptimizer<scoped_refptr<media::VideoFrame>>;

}  // namespace blink
