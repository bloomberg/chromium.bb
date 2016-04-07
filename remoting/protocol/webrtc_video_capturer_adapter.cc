// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_capturer_adapter.h"

#include <utility>

#include "remoting/base/constants.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/webrtc/media/engine/webrtcvideoframe.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {
namespace protocol {

// Number of frames to be captured per second.
const int kFramesPerSec = 30;

WebrtcVideoCapturerAdapter::WebrtcVideoCapturerAdapter(
    std::unique_ptr<webrtc::DesktopCapturer> capturer)
    : desktop_capturer_(std::move(capturer)), weak_factory_(this) {
  DCHECK(desktop_capturer_);

  // Disable video adaptation since we don't intend to use it.
  set_enable_video_adapter(false);
}

WebrtcVideoCapturerAdapter::~WebrtcVideoCapturerAdapter() {
  DCHECK(!capture_timer_);
}

void WebrtcVideoCapturerAdapter::SetSizeCallback(
    const VideoStream::SizeCallback& size_callback) {
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

bool WebrtcVideoCapturerAdapter::PauseCapturer(bool pause) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (pause) {
    if (paused_)
      return true;

    bool running = capture_state() == cricket::CS_STARTING ||
                   capture_state() == cricket::CS_RUNNING;

    DCHECK_EQ(running, IsRunning());

    if (!running) {
      LOG(ERROR)
          << "Cannot pause WebrtcVideoCapturerAdapter.";
      return false;
    }

    capture_timer_->Stop();
    paused_ = true;

    VLOG(1) << "WebrtcVideoCapturerAdapter paused.";

    return true;
  } else {  // Unpausing.
    if (!paused_ || !GetCaptureFormat() || !capture_timer_) {
      LOG(ERROR) << "Cannot unpause WebrtcVideoCapturerAdapter.";
      return false;
    }

    capture_timer_->Start(FROM_HERE,
                          base::TimeDelta::FromMicroseconds(
                              GetCaptureFormat()->interval /
                              (base::Time::kNanosecondsPerMicrosecond)),
                          this,
                          &WebrtcVideoCapturerAdapter::CaptureNextFrame);
    paused_ = false;

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

  if (!frame)
    return;

  std::unique_ptr<webrtc::DesktopFrame> owned_frame(frame);

  // TODO(sergeyu): Currently the adapter keeps generating frames even when
  // nothing is changing on the screen. This is necessary because the video
  // sender drops frames. Obviously this is a suboptimal. The sending code in
  // WebRTC needs to have some mechanism to notify when the bandwidth is
  // exceeded, so the capturer can adapt frame rate.

  webrtc::DesktopVector dpi =
      frame->dpi().is_zero() ? webrtc::DesktopVector(kDefaultDpi, kDefaultDpi)
                             : frame->dpi();
  if (!frame_size_.equals(frame->size()) || !frame_dpi_.equals(dpi)) {
    frame_size_ = frame->size();
    frame_dpi_ = dpi;
    if (!size_callback_.is_null())
      size_callback_.Run(frame_size_, frame_dpi_);
  }

  if (!yuv_frame_ ||
      !frame_size_.equals(
          webrtc::DesktopSize(yuv_frame_->width(), yuv_frame_->height()))) {
    yuv_frame_ = new rtc::RefCountedObject<webrtc::I420Buffer>(
        frame_size_.width(), frame_size_.height());
    // Set updated_region so the whole frame is converted to YUV below.
    frame->mutable_updated_region()->SetRect(
        webrtc::DesktopRect::MakeSize(frame_size_));
  }

  if (!yuv_frame_->HasOneRef()) {
    // Frame is still used, typically by the encoder. We have to make
    // a copy before modifying it.
    // TODO(sergeyu): This will copy the buffer if it's being used.
    // Optimize it by keeping a queue of frames.
    yuv_frame_ = webrtc::I420Buffer::Copy(yuv_frame_);
  }

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

    int y_stride = yuv_frame_->stride(webrtc::kYPlane);
    int u_stride = yuv_frame_->stride(webrtc::kUPlane);
    int v_stride = yuv_frame_->stride(webrtc::kVPlane);
    libyuv::ARGBToI420(
        frame->data() + frame->stride() * top +
            left * webrtc::DesktopFrame::kBytesPerPixel,
        frame->stride(),
        yuv_frame_->MutableData(webrtc::kYPlane) + y_stride * top + left,
        y_stride, yuv_frame_->MutableData(webrtc::kUPlane) +
                      u_stride * top / 2 + left / 2,
        u_stride, yuv_frame_->MutableData(webrtc::kVPlane) +
                      v_stride * top / 2 + left / 2,
        v_stride, width, height);
  }

  cricket::WebRtcVideoFrame video_frame(
      yuv_frame_, (base::TimeTicks::Now() - base::TimeTicks()) /
                      base::TimeDelta::FromMicroseconds(1) * 1000,
      webrtc::kVideoRotation_0);

  OnFrame(this, &video_frame);
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
