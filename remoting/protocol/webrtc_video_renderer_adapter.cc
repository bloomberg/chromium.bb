// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_renderer_adapter.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/threading/worker_pool.h"
#include "remoting/protocol/frame_consumer.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"
#include "third_party/libyuv/include/libyuv/video_common.h"
#include "third_party/webrtc/media/base/videoframe.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {
namespace protocol {

namespace {

std::unique_ptr<webrtc::DesktopFrame> ConvertYuvToRgb(
    scoped_refptr<webrtc::VideoFrameBuffer> yuv_frame,
    std::unique_ptr<webrtc::DesktopFrame> rgb_frame,
    uint32_t format) {
  DCHECK(rgb_frame->size().equals(
      webrtc::DesktopSize(yuv_frame->width(), yuv_frame->height())));
  libyuv::I420ToARGB(yuv_frame->DataY(), yuv_frame->StrideY(),
                     yuv_frame->DataU(), yuv_frame->StrideU(),
                     yuv_frame->DataV(), yuv_frame->StrideV(),
                     rgb_frame->data(), rgb_frame->stride(), yuv_frame->width(),
                     yuv_frame->height());

  rgb_frame->mutable_updated_region()->AddRect(
      webrtc::DesktopRect::MakeSize(rgb_frame->size()));
  return rgb_frame;
}

}  // namespace

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

  video_tracks[0]->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

WebrtcVideoRendererAdapter::~WebrtcVideoRendererAdapter() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  webrtc::VideoTrackVector video_tracks = media_stream_->GetVideoTracks();
  DCHECK(!video_tracks.empty());
  video_tracks[0]->RemoveSink(this);
}

void WebrtcVideoRendererAdapter::OnFrame(const cricket::VideoFrame& frame) {
  if (static_cast<uint64_t>(frame.timestamp_us()) >= rtc::TimeMicros()) {
    // The host sets playout delay to 0, so all incoming frames are expected to
    // be rendered as so as they are received.
    LOG(WARNING) << "Received frame with playout delay greater than 0.";
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WebrtcVideoRendererAdapter::HandleFrameOnMainThread,
                 weak_factory_.GetWeakPtr(),
                 scoped_refptr<webrtc::VideoFrameBuffer>(
                     frame.video_frame_buffer().get())));
}

void WebrtcVideoRendererAdapter::HandleFrameOnMainThread(
    scoped_refptr<webrtc::VideoFrameBuffer> frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  std::unique_ptr<webrtc::DesktopFrame> rgb_frame =
      frame_consumer_->AllocateFrame(
          webrtc::DesktopSize(frame->width(), frame->height()));

  base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(false).get(), FROM_HERE,
      base::Bind(&ConvertYuvToRgb, base::Passed(&frame),
                 base::Passed(&rgb_frame), output_format_fourcc_),
      base::Bind(&WebrtcVideoRendererAdapter::DrawFrame,
                 weak_factory_.GetWeakPtr()));
}

void WebrtcVideoRendererAdapter::DrawFrame(
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  frame_consumer_->DrawFrame(std::move(frame), base::Closure());
}

}  // namespace protocol
}  // namespace remoting
