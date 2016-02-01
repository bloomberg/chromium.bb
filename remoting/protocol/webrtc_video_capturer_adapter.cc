// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_capturer_adapter.h"

#include <utility>

#include "third_party/libjingle/source/talk/media/webrtc/webrtcvideoframe.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {
namespace protocol {

// Number of frames to be captured per second.
const int kFramesPerSec = 30;

WebrtcVideoCapturerAdapter::WebrtcVideoCapturerAdapter(
    scoped_ptr<webrtc::DesktopCapturer> capturer)
    : desktop_capturer_(std::move(capturer)), weak_factory_(this) {
  DCHECK(desktop_capturer_);

  // Disable video adaptation since we don't intend to use it.
  set_enable_video_adapter(false);
}

WebrtcVideoCapturerAdapter::~WebrtcVideoCapturerAdapter() {
  DCHECK(!capture_timer_);
}

void WebrtcVideoCapturerAdapter::SetSizeCallback(
    const SizeCallback& size_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  size_callback_ = size_callback;
}

base::WeakPtr<WebrtcVideoCapturerAdapter>
WebrtcVideoCapturerAdapter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool WebrtcVideoCapturerAdapter::GetBestCaptureFormat(
    const cricket::VideoFormat& desired,
    cricket::VideoFormat* best_format) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // The |capture_format| passed to Start() is always ignored, so copy
  // |best_format| to |desired_format|.
  *best_format = desired;
  return true;
}

cricket::CaptureState WebrtcVideoCapturerAdapter::Start(
    const cricket::VideoFormat& capture_format) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!capture_timer_);

  if (!desktop_capturer_) {
    VLOG(1) << "WebrtcVideoCapturerAdapter failed to start.";
    return cricket::CS_FAILED;
  }

  desktop_capturer_->Start(this);

  capture_timer_.reset(new base::RepeatingTimer());
  capture_timer_->Start(FROM_HERE,
                        base::TimeDelta::FromSeconds(1) / kFramesPerSec, this,
                        &WebrtcVideoCapturerAdapter::CaptureNextFrame);

  return cricket::CS_RUNNING;
}

// Similar to the base class implementation with some important differences:
// 1. Does not call either Stop() or Start(), as those would affect the state of
// |desktop_capturer_|.
// 2. Does not support unpausing after stopping the capturer. It is unclear
// if that flow needs to be supported.
bool WebrtcVideoCapturerAdapter::Pause(bool pause) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (pause) {
    if (capture_state() == cricket::CS_PAUSED)
      return true;

    bool running = capture_state() == cricket::CS_STARTING ||
                   capture_state() == cricket::CS_RUNNING;

    DCHECK_EQ(running, IsRunning());

    if (!running) {
      LOG(ERROR)
          << "Cannot pause WebrtcVideoCapturerAdapter.";
      return false;
    }

    // Stop |capture_timer_| and set capture state to cricket::CS_PAUSED.
    capture_timer_->Stop();
    SetCaptureState(cricket::CS_PAUSED);

    VLOG(1) << "WebrtcVideoCapturerAdapter paused.";

    return true;
  } else {  // Unpausing.
    if (capture_state() != cricket::CS_PAUSED || !GetCaptureFormat() ||
        !capture_timer_) {
      LOG(ERROR) << "Cannot unpause WebrtcVideoCapturerAdapter.";
      return false;
    }

    // Restart |capture_timer_| and set capture state to cricket::CS_RUNNING;
    capture_timer_->Start(FROM_HERE,
                          base::TimeDelta::FromMicroseconds(
                              GetCaptureFormat()->interval /
                              (base::Time::kNanosecondsPerMicrosecond)),
                          this,
                          &WebrtcVideoCapturerAdapter::CaptureNextFrame);
    SetCaptureState(cricket::CS_RUNNING);

    VLOG(1) << "WebrtcVideoCapturerAdapter unpaused.";
  }
  return true;
}

void WebrtcVideoCapturerAdapter::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(capture_state(), cricket::CS_STOPPED);

  capture_timer_.reset();
  desktop_capturer_.reset();

  SetCaptureFormat(nullptr);
  SetCaptureState(cricket::CS_STOPPED);

  VLOG(1) << "WebrtcVideoCapturerAdapter stopped.";
}

bool WebrtcVideoCapturerAdapter::IsRunning() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return capture_timer_->IsRunning();
}

bool WebrtcVideoCapturerAdapter::IsScreencast() const {
  return true;
}

bool WebrtcVideoCapturerAdapter::GetPreferredFourccs(
    std::vector<uint32_t>* fourccs) {
  return false;
}

webrtc::SharedMemory* WebrtcVideoCapturerAdapter::CreateSharedMemory(
    size_t size) {
  return nullptr;
}

void WebrtcVideoCapturerAdapter::OnCaptureCompleted(
    webrtc::DesktopFrame* frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(capture_pending_);
  capture_pending_ = false;

  scoped_ptr<webrtc::DesktopFrame> owned_frame(frame);

  // Drop the frame if there were no changes.
  if (!owned_frame || owned_frame->updated_region().is_empty())
    return;

  size_t width = frame->size().width();
  size_t height = frame->size().height();
  if (!yuv_frame_ || yuv_frame_->GetWidth() != width ||
      yuv_frame_->GetHeight() != height) {
    if (!size_callback_.is_null())
      size_callback_.Run(frame->size());

    scoped_ptr<cricket::WebRtcVideoFrame> webrtc_frame(
        new cricket::WebRtcVideoFrame());
    webrtc_frame->InitToEmptyBuffer(width, height, 0);
    yuv_frame_ = std::move(webrtc_frame);

    // Set updated_region so the whole frame is converted to YUV below.
    frame->mutable_updated_region()->SetRect(
        webrtc::DesktopRect::MakeWH(width, height));
  }

  // TODO(sergeyu): This will copy the buffer if it's being used. Optimize it by
  // keeping a queue of frames.
  CHECK(yuv_frame_->MakeExclusive());

  yuv_frame_->SetTimeStamp(base::TimeTicks::Now().ToInternalValue() *
                           base::Time::kNanosecondsPerMicrosecond);

  for (webrtc::DesktopRegion::Iterator i(frame->updated_region()); !i.IsAtEnd();
       i.Advance()) {
    int left = i.rect().left();
    int top = i.rect().top();
    int width = i.rect().width();
    int height = i.rect().height();

    if (left % 2 == 1) {
      --left;
      ++width;
    }
    if (top % 2 == 1) {
      --top;
      ++height;
    }
    libyuv::ARGBToI420(
        frame->data() + frame->stride() * top +
            left * webrtc::DesktopFrame::kBytesPerPixel,
        frame->stride(),
        yuv_frame_->GetYPlane() + yuv_frame_->GetYPitch() * top + left,
        yuv_frame_->GetYPitch(),
        yuv_frame_->GetUPlane() + yuv_frame_->GetUPitch() * top / 2 + left / 2,
        yuv_frame_->GetUPitch(),
        yuv_frame_->GetVPlane() + yuv_frame_->GetVPitch() * top / 2 + left / 2,
        yuv_frame_->GetVPitch(), width, height);
  }

  SignalVideoFrame(this, yuv_frame_.get());
}

void WebrtcVideoCapturerAdapter::CaptureNextFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (capture_pending_)
    return;
  capture_pending_ = true;
  desktop_capturer_->Capture(webrtc::DesktopRegion());
}

}  // namespace protocol
}  // namespace remoting
