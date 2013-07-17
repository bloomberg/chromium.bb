// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_frame_consumer.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "remoting/client/frame_producer.h"
#include "remoting/client/jni/chromoting_jni.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace {

// Allocates its buffer within a Java direct byte buffer, where it can be
// accessed by both native and managed code.
class DirectDesktopFrame : public webrtc::BasicDesktopFrame {
 public:
  DirectDesktopFrame(int width, int height);

  virtual ~DirectDesktopFrame();

  jobject buffer() const {
    return buffer_;
  }

 private:
  jobject buffer_;
};

DirectDesktopFrame::DirectDesktopFrame(int width, int height)
    : webrtc::BasicDesktopFrame(webrtc::DesktopSize(width, height)) {
  JNIEnv* env = base::android::AttachCurrentThread();
  buffer_ = env->NewDirectByteBuffer(data(), stride()*height);
}

DirectDesktopFrame::~DirectDesktopFrame() {}

}  // namespace

namespace remoting {

JniFrameConsumer::JniFrameConsumer()
    : provide_buffer_(true),
      frame_producer_(NULL) {
}

JniFrameConsumer::~JniFrameConsumer() {
  // Stop giving the producer a buffer to work with.
  provide_buffer_ = false;

  // Don't destroy the object until we've deleted the buffer.
  base::WaitableEvent done_event(true, false);
  frame_producer_->RequestReturnBuffers(
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done_event)));
  done_event.Wait();
}

void JniFrameConsumer::set_frame_producer(FrameProducer* producer) {
  frame_producer_ = producer;
}

void JniFrameConsumer::ApplyBuffer(const SkISize& view_size,
                                   const SkIRect& clip_area,
                                   webrtc::DesktopFrame* buffer,
                                   const SkRegion& region) {
  DCHECK(ChromotingJni::GetInstance()->
      display_task_runner()->BelongsToCurrentThread());

  ChromotingJni::GetInstance()->RedrawCanvas();

  if (view_size.width() > view_size_.width() ||
      view_size.height() > view_size_.height()) {
    LOG(INFO) << "Existing buffer is too small";
    view_size_ = view_size;
    delete buffer;
    AllocateBuffer();
  }

  // Supply |frame_producer_| with a buffer to render the next frame into.
  if (provide_buffer_)
    frame_producer_->DrawBuffer(buffer);
}

void JniFrameConsumer::ReturnBuffer(webrtc::DesktopFrame* buffer) {
  DCHECK(ChromotingJni::GetInstance()->
      display_task_runner()->BelongsToCurrentThread());
  LOG(INFO) << "Returning image buffer";
  delete buffer;
}

void JniFrameConsumer::SetSourceSize(const SkISize& source_size,
                                     const SkIPoint& dpi) {
  DCHECK(ChromotingJni::GetInstance()->
      display_task_runner()->BelongsToCurrentThread());

  // We currently render the desktop 1:1 and perform pan/zoom scaling
  // and cropping on the managed canvas.
  view_size_ = source_size;
  clip_area_ = SkIRect::MakeSize(view_size_);
  frame_producer_->SetOutputSizeAndClip(view_size_, clip_area_);

  // Unless being destructed, allocate buffer and start drawing frames onto it.
  frame_producer_->RequestReturnBuffers(base::Bind(
      &JniFrameConsumer::AllocateBuffer, base::Unretained(this)));
}

void JniFrameConsumer::AllocateBuffer() {
  // Only do anything if we're not being destructed.
  if (provide_buffer_) {
    if (!ChromotingJni::GetInstance()->
        display_task_runner()->BelongsToCurrentThread()) {
      ChromotingJni::GetInstance()->display_task_runner()->PostTask(FROM_HERE,
          base::Bind(&JniFrameConsumer::AllocateBuffer,
                     base::Unretained(this)));
      return;
    }

    DirectDesktopFrame* buffer = new DirectDesktopFrame(view_size_.width(),
                                                        view_size_.height());

    // Update Java's reference to the buffer and record of its dimensions.
    ChromotingJni::GetInstance()->UpdateImageBuffer(
        view_size_.width(),
        view_size_.height(),
        buffer->buffer());

    frame_producer_->DrawBuffer(buffer);
  }
}

}  // namespace remoting
