// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_frame_consumer.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "remoting/base/util.h"
#include "remoting/client/jni/chromoting_jni_instance.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"
#include "ui/gfx/android/java_bitmap.h"

namespace remoting {

class JniFrameConsumer::Renderer {
 public:
  Renderer(ChromotingJniRuntime* jni_runtime) : jni_runtime_(jni_runtime) {}
  ~Renderer() {
    DCHECK(jni_runtime_->display_task_runner()->BelongsToCurrentThread());
  }

  void RenderFrame(scoped_ptr<webrtc::DesktopFrame> frame);

 private:
  // Used to obtain task runner references and make calls to Java methods.
  ChromotingJniRuntime* jni_runtime_;

  // This global reference is required, instead of a local reference, so it
  // remains valid for the lifetime of |bitmap_| - gfx::JavaBitmap does not
  // create its own global reference internally. And this global ref must be
  // destroyed (released) after |bitmap_| is destroyed.
  base::android::ScopedJavaGlobalRef<jobject> bitmap_global_ref_;

  // Reference to the frame bitmap that is passed to Java when the frame is
  // allocated. This provides easy access to the underlying pixels.
  scoped_ptr<gfx::JavaBitmap> bitmap_;
};

// Function called on the display thread to render the frame.
void JniFrameConsumer::Renderer::RenderFrame(
    scoped_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(jni_runtime_->display_task_runner()->BelongsToCurrentThread());

  if (!bitmap_ || bitmap_->size().width() != frame->size().width() ||
      bitmap_->size().height() != frame->size().height()) {
    // Allocate a new Bitmap, store references here, and pass it to Java.
    JNIEnv* env = base::android::AttachCurrentThread();

    // |bitmap_| must be deleted before |bitmap_global_ref_| is released.
    bitmap_.reset();
    bitmap_global_ref_.Reset(
        env,
        jni_runtime_->NewBitmap(frame->size().width(), frame->size().height())
            .obj());
    bitmap_.reset(new gfx::JavaBitmap(bitmap_global_ref_.obj()));
    jni_runtime_->UpdateFrameBitmap(bitmap_global_ref_.obj());
  }

  // Copy pixels from |frame| into the Java Bitmap.
  // TODO(lambroslambrou): Optimize away this copy by having the VideoDecoder
  // decode directly into the Bitmap's pixel memory. This currently doesn't
  // work very well because the VideoDecoder writes the decoded data in BGRA,
  // and then the R/B channels are swapped in place (on the decoding thread).
  // If a repaint is triggered from a Java event handler, the unswapped pixels
  // can sometimes appear on the display.
  uint8* dest_buffer = static_cast<uint8*>(bitmap_->pixels());
  webrtc::DesktopRect buffer_rect =
      webrtc::DesktopRect::MakeSize(frame->size());
  for (webrtc::DesktopRegion::Iterator i(frame->updated_region()); !i.IsAtEnd();
       i.Advance()) {
    CopyRGB32Rect(frame->data(), frame->stride(), buffer_rect, dest_buffer,
                  bitmap_->stride(), buffer_rect, i.rect());
  }

  jni_runtime_->RedrawCanvas();
}

JniFrameConsumer::JniFrameConsumer(ChromotingJniRuntime* jni_runtime)
    : jni_runtime_(jni_runtime),
      renderer_(new Renderer(jni_runtime)),
      weak_factory_(this) {}

JniFrameConsumer::~JniFrameConsumer() {
  jni_runtime_->display_task_runner()->DeleteSoon(FROM_HERE,
                                                  renderer_.release());
}

scoped_ptr<webrtc::DesktopFrame> JniFrameConsumer::AllocateFrame(
    const webrtc::DesktopSize& size) {
  return make_scoped_ptr(new webrtc::BasicDesktopFrame(size));
}

void JniFrameConsumer::DrawFrame(scoped_ptr<webrtc::DesktopFrame> frame,
                                 const base::Closure& done) {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  jni_runtime_->display_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&Renderer::RenderFrame, base::Unretained(renderer_.get()),
                 base::Passed(&frame)),
      base::Bind(&JniFrameConsumer::OnFrameRendered, weak_factory_.GetWeakPtr(),
                 done));
}

void JniFrameConsumer::OnFrameRendered(const base::Closure& done) {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  if (!done.is_null())
    done.Run();
}

FrameConsumer::PixelFormat JniFrameConsumer::GetPixelFormat() {
  return FORMAT_RGBA;
}

}  // namespace remoting
