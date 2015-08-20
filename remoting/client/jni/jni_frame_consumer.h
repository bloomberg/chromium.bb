// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_FRAME_CONSUMER_H_
#define REMOTING_CLIENT_JNI_JNI_FRAME_CONSUMER_H_

#include <list>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/frame_consumer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

class ChromotingJniRuntime;

// FrameConsumer implementation that draws onto a JNI direct byte buffer.
class JniFrameConsumer : public FrameConsumer {
 public:
  // Does not take ownership of |jni_runtime|.
  explicit JniFrameConsumer(ChromotingJniRuntime* jni_runtime);

  ~JniFrameConsumer() override;

  // FrameConsumer implementation.
  scoped_ptr<webrtc::DesktopFrame> AllocateFrame(
      const webrtc::DesktopSize& size) override;
  void DrawFrame(scoped_ptr<webrtc::DesktopFrame> frame,
                 const base::Closure& done) override;
  PixelFormat GetPixelFormat() override;

 private:
  class Renderer;

  void OnFrameRendered(const base::Closure& done);

  // Used to obtain task runner references and make calls to Java methods.
  ChromotingJniRuntime* jni_runtime_;

  // Renderer object used to render the frames on the display thread.
  scoped_ptr<Renderer> renderer_;

  base::WeakPtrFactory<JniFrameConsumer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(JniFrameConsumer);
};

}  // namespace remoting

#endif
