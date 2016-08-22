// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/dual_buffer_frame_consumer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"

namespace remoting {

DualBufferFrameConsumer::DualBufferFrameConsumer(
    const RenderCallback& callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    protocol::FrameConsumer::PixelFormat format)
    : callback_(callback),
      task_runner_(task_runner),
      pixel_format_(format),
      weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();
}

DualBufferFrameConsumer::~DualBufferFrameConsumer() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void DualBufferFrameConsumer::RequestFullDesktopFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!buffers_[0]) {
    return;
  }
  DCHECK(buffers_[0]->size().equals(buffers_[1]->size()));
  // This creates a copy of buffers_[0] and merges area defined in
  // |buffer_1_mask_| from buffers_[1] into the copy.
  std::unique_ptr<webrtc::DesktopFrame> full_frame(
      webrtc::BasicDesktopFrame::CopyOf(*buffers_[0]));
  webrtc::DesktopRect desktop_rect =
        webrtc::DesktopRect::MakeSize(buffers_[0]->size());
  for (webrtc::DesktopRegion::Iterator i(buffer_1_mask_); !i.IsAtEnd();
       i.Advance()) {
    full_frame->CopyPixelsFrom(*buffers_[1], i.rect().top_left(),
                               i.rect());
  }
  full_frame->mutable_updated_region()->SetRect(desktop_rect);

  RunRenderCallback(std::move(full_frame), base::Bind(&base::DoNothing));
}

std::unique_ptr<webrtc::DesktopFrame> DualBufferFrameConsumer::AllocateFrame(
    const webrtc::DesktopSize& size) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Both buffers are reallocated whenever screen size changes.
  if (!buffers_[0] || !buffers_[0]->size().equals(size)) {
    buffers_[0] = webrtc::SharedDesktopFrame::Wrap(
        base::MakeUnique<webrtc::BasicDesktopFrame>(size));
    buffers_[1] = webrtc::SharedDesktopFrame::Wrap(
        base::MakeUnique<webrtc::BasicDesktopFrame>(size));
    buffer_1_mask_.Clear();
    current_buffer_ = 0;
  } else {
    current_buffer_ = (current_buffer_ + 1) % 2;
  }
  return buffers_[current_buffer_]->Share();
}

void DualBufferFrameConsumer::DrawFrame(
    std::unique_ptr<webrtc::DesktopFrame> frame,
    const base::Closure& done) {
  DCHECK(thread_checker_.CalledOnValidThread());
  webrtc::SharedDesktopFrame* shared_frame =
      reinterpret_cast<webrtc::SharedDesktopFrame*> (frame.get());
  if (shared_frame->GetUnderlyingFrame() == buffers_[1]->GetUnderlyingFrame()) {
    buffer_1_mask_.AddRegion(frame->updated_region());
  } else if (shared_frame->GetUnderlyingFrame() ==
      buffers_[0]->GetUnderlyingFrame()) {
    buffer_1_mask_.Subtract(frame->updated_region());
  }
  RunRenderCallback(std::move(frame), done);
}

protocol::FrameConsumer::PixelFormat
DualBufferFrameConsumer::GetPixelFormat() {
  return pixel_format_;
}

base::WeakPtr<DualBufferFrameConsumer> DualBufferFrameConsumer::GetWeakPtr() {
  return weak_ptr_;
}

void DualBufferFrameConsumer::RunRenderCallback(
    std::unique_ptr<webrtc::DesktopFrame> frame,
    const base::Closure& done) {
  if (!task_runner_) {
    callback_.Run(std::move(frame), done);
    return;
  }

  task_runner_->PostTask(
      FROM_HERE, base::Bind(callback_, base::Passed(&frame), base::Bind(
          base::IgnoreResult(&base::TaskRunner::PostTask),
          base::ThreadTaskRunnerHandle::Get(), FROM_HERE, done)));
}

}  // namespace remoting
