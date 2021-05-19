// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/transferred_frame_queue_underlying_source.h"

#include "media/base/video_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame_serialization_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

template <typename NativeFrameType>
TransferredFrameQueueUnderlyingSource<NativeFrameType>::
    TransferredFrameQueueUnderlyingSource(
        ScriptState* script_state,
        FrameQueueHost* host,
        scoped_refptr<base::SequencedTaskRunner> host_runner,
        wtf_size_t max_queue_size)
    : FrameQueueUnderlyingSource<NativeFrameType>(script_state, max_queue_size),
      host_runner_(host_runner),
      host_(host) {}

template <typename NativeFrameType>
bool TransferredFrameQueueUnderlyingSource<
    NativeFrameType>::StartFrameDelivery() {
  // PostCrossThreadTask needs a closure, so we have to ignore
  // StartFrameDelivery()'s return type.
  auto start_frame_delivery_cb = [](FrameQueueHost* host) {
    host->StartFrameDelivery();
  };

  PostCrossThreadTask(*host_runner_.get(), FROM_HERE,
                      CrossThreadBindOnce(start_frame_delivery_cb, host_));

  // This could fail on the host side, but for now we don't do an async check.
  return true;
}

template <typename NativeFrameType>
void TransferredFrameQueueUnderlyingSource<
    NativeFrameType>::StopFrameDelivery() {
  PostCrossThreadTask(*host_runner_.get(), FROM_HERE,
                      CrossThreadBindOnce(&FrameQueueHost::Close, host_));
}

template <typename NativeFrameType>
void TransferredFrameQueueUnderlyingSource<NativeFrameType>::Trace(
    Visitor* visitor) const {
  FrameQueueUnderlyingSource<NativeFrameType>::Trace(visitor);
}

template class MODULES_TEMPLATE_EXPORT TransferredFrameQueueUnderlyingSource<
    std::unique_ptr<AudioFrameSerializationData>>;
template class MODULES_TEMPLATE_EXPORT
    TransferredFrameQueueUnderlyingSource<scoped_refptr<media::VideoFrame>>;

}  // namespace blink
