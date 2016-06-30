// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_GL_DISPLAY_HANDLER_H_
#define REMOTING_CLIENT_JNI_JNI_GL_DISPLAY_HANDLER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/jni/display_updater_factory.h"

namespace remoting {

class ChromotingJniRuntime;
class JniVideoRenderer;

// Handles OpenGL display operations. Draws desktop and cursor on the OpenGL
// surface.
class JniGlDisplayHandler : public DisplayUpdaterFactory {
 public:
  JniGlDisplayHandler();
  ~JniGlDisplayHandler() override;

  // DisplayUpdaterFactory overrides.
  std::unique_ptr<protocol::CursorShapeStub> CreateCursorShapeStub() override;
  std::unique_ptr<protocol::VideoRenderer> CreateVideoRenderer() override;

  static bool RegisterJni(JNIEnv* env);

  void OnSurfaceCreated(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& caller);

  void OnSurfaceChanged(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& caller,
                        int width,
                        int height);

  void OnDrawFrame(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& caller);

 private:
  DISALLOW_COPY_AND_ASSIGN(JniGlDisplayHandler);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_GL_DISPLAY_HANDLER_H_
