// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_frame_consumer.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "remoting/base/util.h"
#include "remoting/client/frame_producer.h"
#include "remoting/client/jni/chromoting_jni_instance.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"
#include "ui/gfx/android/java_bitmap.h"

namespace remoting {

JniFrameConsumer::JniFrameConsumer(
    ChromotingJniRuntime* jni_runtime,
    scoped_refptr<ChromotingJniInstance> jni_instance)
    : jni_runtime_(jni_runtime),
      jni_instance_(jni_instance),
      frame_producer_(NULL) {
}

JniFrameConsumer::~JniFrameConsumer() {
  // The producer should now return any pending buffers. At this point, however,
  // ReturnBuffer() tasks scheduled by the producer will not be delivered,
  // so we free all the buffers once the producer's queue is empty.
  base::WaitableEvent done_event(true, false);
  frame_producer_->RequestReturnBuffers(
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done_event)));
  done_event.Wait();

  STLDeleteElements(&buffers_);
}

void JniFrameConsumer::set_frame_producer(FrameProducer* producer) {
  frame_producer_ = producer;
}

void JniFrameConsumer::ApplyBuffer(const webrtc::DesktopSize& view_size,
                                   const webrtc::DesktopRect& clip_area,
                                   webrtc::DesktopFrame* buffer,
                                   const webrtc::DesktopRegion& region,
                                   const webrtc::DesktopRegion& shape) {
  DCHECK(jni_runtime_->display_task_runner()->BelongsToCurrentThread());

  if (bitmap_->size().width() != buffer->size().width() ||
      bitmap_->size().height() != buffer->size().height()) {
    // Drop the frame, since the data belongs to the previous generation,
    // before SetSourceSize() called SetOutputSizeAndClip().
    FreeBuffer(buffer);
    return;
  }

  // Copy pixels from |buffer| into the Java Bitmap.
  // TODO(lambroslambrou): Optimize away this copy by having the VideoDecoder
  // decode directly into the Bitmap's pixel memory. This currently doesn't
  // work very well because the VideoDecoder writes the decoded data in BGRA,
  // and then the R/B channels are swapped in place (on the decoding thread).
  // If a repaint is triggered from a Java event handler, the unswapped pixels
  // can sometimes appear on the display.
  uint8* dest_buffer = static_cast<uint8*>(bitmap_->pixels());
  webrtc::DesktopRect buffer_rect = webrtc::DesktopRect::MakeSize(view_size);

  for (webrtc::DesktopRegion::Iterator i(region); !i.IsAtEnd(); i.Advance()) {
    const webrtc::DesktopRect& rect(i.rect());
    CopyRGB32Rect(buffer->data(), buffer->stride(), buffer_rect, dest_buffer,
                  bitmap_->stride(), buffer_rect, rect);
  }

  // TODO(lambroslambrou): Optimize this by only repainting the changed pixels.
  base::TimeTicks start_time = base::TimeTicks::Now();
  jni_runtime_->RedrawCanvas();
  jni_instance_->RecordPaintTime(
      (base::TimeTicks::Now() - start_time).InMilliseconds());

  // Supply |frame_producer_| with a buffer to render the next frame into.
  frame_producer_->DrawBuffer(buffer);
}

void JniFrameConsumer::ReturnBuffer(webrtc::DesktopFrame* buffer) {
  DCHECK(jni_runtime_->display_task_runner()->BelongsToCurrentThread());
  FreeBuffer(buffer);
}

void JniFrameConsumer::SetSourceSize(const webrtc::DesktopSize& source_size,
                                     const webrtc::DesktopVector& dpi) {
  DCHECK(jni_runtime_->display_task_runner()->BelongsToCurrentThread());

  // We currently render the desktop 1:1 and perform pan/zoom scaling
  // and cropping on the managed canvas.
  clip_area_ = webrtc::DesktopRect::MakeSize(source_size);
  frame_producer_->SetOutputSizeAndClip(source_size, clip_area_);

  // Allocate buffer and start drawing frames onto it.
  AllocateBuffer(source_size);
}

FrameConsumer::PixelFormat JniFrameConsumer::GetPixelFormat() {
  return FORMAT_RGBA;
}

void JniFrameConsumer::AllocateBuffer(const webrtc::DesktopSize& source_size) {
  DCHECK(jni_runtime_->display_task_runner()->BelongsToCurrentThread());

  webrtc::DesktopSize size(source_size.width(), source_size.height());

  // Allocate a new Bitmap, store references here, and pass it to Java.
  JNIEnv* env = base::android::AttachCurrentThread();

  // |bitmap_| must be deleted before |bitmap_global_ref_| is released.
  bitmap_.reset();
  bitmap_global_ref_.Reset(env, jni_runtime_->NewBitmap(size).obj());
  bitmap_.reset(new gfx::JavaBitmap(bitmap_global_ref_.obj()));
  jni_runtime_->UpdateFrameBitmap(bitmap_global_ref_.obj());

  webrtc::DesktopFrame* buffer = new webrtc::BasicDesktopFrame(size);
  buffers_.push_back(buffer);
  frame_producer_->DrawBuffer(buffer);
}

void JniFrameConsumer::FreeBuffer(webrtc::DesktopFrame* buffer) {
  DCHECK(std::find(buffers_.begin(), buffers_.end(), buffer) != buffers_.end());

  buffers_.remove(buffer);
  delete buffer;
}

}  // namespace remoting
