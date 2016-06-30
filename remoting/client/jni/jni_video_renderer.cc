// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_video_renderer.h"

#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "remoting/base/util.h"
#include "remoting/client/jni/chromoting_jni_instance.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "remoting/client/jni/jni_client.h"
#include "remoting/client/jni/jni_display_handler.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"
#include "ui/gfx/android/java_bitmap.h"

namespace remoting {

class JniVideoRenderer::Renderer {
 public:
  Renderer(ChromotingJniRuntime* jni_runtime,
           base::WeakPtr<JniDisplayHandler> display)
      : jni_runtime_(jni_runtime), display_handler_(display) {}
  ~Renderer() {
    DCHECK(jni_runtime_->display_task_runner()->BelongsToCurrentThread());
  }

  void RenderFrame(std::unique_ptr<webrtc::DesktopFrame> frame);

 private:
  // Used to obtain task runner references and make calls to Java methods.
  ChromotingJniRuntime* jni_runtime_;

  base::WeakPtr<JniDisplayHandler> display_handler_;

  // This global reference is required, instead of a local reference, so it
  // remains valid for the lifetime of |bitmap_| - gfx::JavaBitmap does not
  // create its own global reference internally. And this global ref must be
  // destroyed (released) after |bitmap_| is destroyed.
  base::android::ScopedJavaGlobalRef<jobject> bitmap_global_ref_;

  // Reference to the frame bitmap that is passed to Java when the frame is
  // allocated. This provides easy access to the underlying pixels.
  std::unique_ptr<gfx::JavaBitmap> bitmap_;
};

// Function called on the display thread to render the frame.
void JniVideoRenderer::Renderer::RenderFrame(
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(jni_runtime_->display_task_runner()->BelongsToCurrentThread());
  if (!display_handler_) {
    return;
  }

  if (!bitmap_ || bitmap_->size().width() != frame->size().width() ||
      bitmap_->size().height() != frame->size().height()) {
    // Allocate a new Bitmap, store references here, and pass it to Java.
    JNIEnv* env = base::android::AttachCurrentThread();

    // |bitmap_| must be deleted before |bitmap_global_ref_| is released.
    bitmap_.reset();
    bitmap_global_ref_.Reset(
        env, display_handler_
                 ->NewBitmap(frame->size().width(), frame->size().height())
                 .obj());
    bitmap_.reset(new gfx::JavaBitmap(bitmap_global_ref_.obj()));
    display_handler_->UpdateFrameBitmap(bitmap_global_ref_);
  }

  // Copy pixels from |frame| into the Java Bitmap.
  // TODO(lambroslambrou): Optimize away this copy by having the VideoDecoder
  // decode directly into the Bitmap's pixel memory. This currently doesn't
  // work very well because the VideoDecoder writes the decoded data in BGRA,
  // and then the R/B channels are swapped in place (on the decoding thread).
  // If a repaint is triggered from a Java event handler, the unswapped pixels
  // can sometimes appear on the display.
  uint8_t* dest_buffer = static_cast<uint8_t*>(bitmap_->pixels());
  webrtc::DesktopRect buffer_rect =
      webrtc::DesktopRect::MakeSize(frame->size());
  for (webrtc::DesktopRegion::Iterator i(frame->updated_region()); !i.IsAtEnd();
       i.Advance()) {
    CopyRGB32Rect(frame->data(), frame->stride(), buffer_rect, dest_buffer,
                  bitmap_->stride(), buffer_rect, i.rect());
  }

  display_handler_->RedrawCanvas();
}

JniVideoRenderer::JniVideoRenderer(
    ChromotingJniRuntime* jni_runtime,
    base::WeakPtr<JniDisplayHandler> display)
    : jni_runtime_(jni_runtime),
      software_video_renderer_(this),
      renderer_(new Renderer(jni_runtime, display)),
      weak_factory_(this) {}

JniVideoRenderer::~JniVideoRenderer() {
  jni_runtime_->display_task_runner()->DeleteSoon(FROM_HERE,
                                                  renderer_.release());
}

std::unique_ptr<webrtc::DesktopFrame> JniVideoRenderer::AllocateFrame(
    const webrtc::DesktopSize& size) {
  return base::WrapUnique(new webrtc::BasicDesktopFrame(size));
}

void JniVideoRenderer::DrawFrame(std::unique_ptr<webrtc::DesktopFrame> frame,
                                 const base::Closure& done) {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  jni_runtime_->display_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&Renderer::RenderFrame, base::Unretained(renderer_.get()),
                 base::Passed(&frame)),
      base::Bind(&JniVideoRenderer::OnFrameRendered, weak_factory_.GetWeakPtr(),
                 done));
}

void JniVideoRenderer::OnFrameRendered(const base::Closure& done) {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  if (!done.is_null())
    done.Run();
}

protocol::FrameConsumer::PixelFormat JniVideoRenderer::GetPixelFormat() {
  return FORMAT_RGBA;
}

void JniVideoRenderer::OnSessionConfig(const protocol::SessionConfig& config) {
  return software_video_renderer_.OnSessionConfig(config);
}

protocol::VideoStub* JniVideoRenderer::GetVideoStub() {
  return software_video_renderer_.GetVideoStub();
}

protocol::FrameConsumer* JniVideoRenderer::GetFrameConsumer() {
  return software_video_renderer_.GetFrameConsumer();
}

bool JniVideoRenderer::Initialize(const ClientContext& context,
                                  protocol::PerformanceTracker* perf_tracker) {
  return software_video_renderer_.Initialize(context, perf_tracker);
}

}  // namespace remoting
