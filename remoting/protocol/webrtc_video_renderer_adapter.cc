// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_renderer_adapter.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "remoting/protocol/frame_consumer.h"
#include "third_party/libyuv/include/libyuv/video_common.h"
#include "third_party/webrtc/media/base/videoframe.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {
namespace protocol {

WebrtcVideoRendererAdapter::WebrtcVideoRendererAdapter(
    scoped_refptr<webrtc::MediaStreamInterface> media_stream,
    FrameConsumer* frame_consumer)
    : media_stream_(std::move(media_stream)),
      frame_consumer_(frame_consumer),
      output_format_fourcc_(frame_consumer_->GetPixelFormat() ==
                                    FrameConsumer::FORMAT_BGRA
                                ? libyuv::FOURCC_ARGB
                                : libyuv::FOURCC_ABGR),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
  webrtc::VideoTrackVector video_tracks = media_stream_->GetVideoTracks();
  if (video_tracks.empty()) {
    LOG(ERROR) << "Received media stream with no video tracks.";
    return;
  }

  if (video_tracks.size() > 1U) {
    LOG(WARNING) << "Received media stream with multiple video tracks.";
  }

  video_tracks[0]->AddRenderer(this);
}

WebrtcVideoRendererAdapter::~WebrtcVideoRendererAdapter() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void WebrtcVideoRendererAdapter::RenderFrame(const cricket::VideoFrame* frame) {
  // TODO(sergeyu): WebRTC calls RenderFrame on a separate thread it creates.
  // FrameConsumer normally expects to be called on the network thread, so we
  // cannot call FrameConsumer::AllocateFrame() here and instead
  // BasicDesktopFrame is created directly. This will not work correctly with
  // all FrameConsumer implementations. Fix this somehow.
  scoped_ptr<webrtc::DesktopFrame> rgb_frame(new webrtc::BasicDesktopFrame(
      webrtc::DesktopSize(frame->GetWidth(), frame->GetHeight())));

  frame->ConvertToRgbBuffer(
      output_format_fourcc_, rgb_frame->data(),
      std::abs(rgb_frame->stride()) * rgb_frame->size().height(),
      rgb_frame->stride());
  rgb_frame->mutable_updated_region()->AddRect(
      webrtc::DesktopRect::MakeSize(rgb_frame->size()));
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WebrtcVideoRendererAdapter::DrawFrame,
                 weak_factory_.GetWeakPtr(), base::Passed(&rgb_frame)));
}

void WebrtcVideoRendererAdapter::DrawFrame(
    scoped_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  frame_consumer_->DrawFrame(std::move(frame), base::Closure());
}

}  // namespace remoting
}  // namespace protocol
