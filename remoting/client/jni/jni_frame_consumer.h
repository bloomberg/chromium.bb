// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_FRAME_CONSUMER_H_
#define REMOTING_CLIENT_JNI_JNI_FRAME_CONSUMER_H_

#include <list>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/client/frame_consumer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace gfx {
class JavaBitmap;
}  // namespace gfx

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {
class ChromotingJniInstance;
class ChromotingJniRuntime;
class FrameProducer;

// FrameConsumer implementation that draws onto a JNI direct byte buffer.
class JniFrameConsumer : public FrameConsumer {
 public:
  // The instance does not take ownership of |jni_runtime|.
  explicit JniFrameConsumer(ChromotingJniRuntime* jni_runtime,
                            scoped_refptr<ChromotingJniInstance> jni_instance);

  virtual ~JniFrameConsumer();

  // This must be called once before the producer's source size is set.
  void set_frame_producer(FrameProducer* producer);

  // FrameConsumer implementation.
  virtual void ApplyBuffer(const webrtc::DesktopSize& view_size,
                           const webrtc::DesktopRect& clip_area,
                           webrtc::DesktopFrame* buffer,
                           const webrtc::DesktopRegion& region,
                           const webrtc::DesktopRegion& shape) OVERRIDE;
  virtual void ReturnBuffer(webrtc::DesktopFrame* buffer) OVERRIDE;
  virtual void SetSourceSize(const webrtc::DesktopSize& source_size,
                             const webrtc::DesktopVector& dpi) OVERRIDE;
  virtual PixelFormat GetPixelFormat() OVERRIDE;

 private:
  // Allocates a new buffer of |source_size|, informs Java about it, and tells
  // the producer to draw onto it.
  void AllocateBuffer(const webrtc::DesktopSize& source_size);

  // Frees a frame buffer previously allocated by AllocateBuffer.
  void FreeBuffer(webrtc::DesktopFrame* buffer);

  // Variables are to be used from the display thread.

  // Used to obtain task runner references and make calls to Java methods.
  ChromotingJniRuntime* jni_runtime_;

  // Used to record statistics.
  scoped_refptr<ChromotingJniInstance> jni_instance_;

  FrameProducer* frame_producer_;
  webrtc::DesktopRect clip_area_;

  // List of allocated image buffers.
  std::list<webrtc::DesktopFrame*> buffers_;

  // This global reference is required, instead of a local reference, so it
  // remains valid for the lifetime of |bitmap_| - gfx::JavaBitmap does not
  // create its own global reference internally. And this global ref must be
  // destroyed (released) after |bitmap_| is destroyed.
  base::android::ScopedJavaGlobalRef<jobject> bitmap_global_ref_;

  // Reference to the frame bitmap that is passed to Java when the frame is
  // allocated. This provides easy access to the underlying pixels.
  scoped_ptr<gfx::JavaBitmap> bitmap_;

  DISALLOW_COPY_AND_ASSIGN(JniFrameConsumer);
};

}  // namespace remoting

#endif
