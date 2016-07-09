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

// Handles OpenGL display operations. Draws desktop and cursor on the OpenGL
// surface.
// JNI functions should all be called on the UI thread. The display handler
// itself should be deleted on the display thread.
// Please see GlDisplay.java for documentations.
class JniGlDisplayHandler : public DisplayUpdaterFactory {
 public:
  JniGlDisplayHandler(ChromotingJniRuntime* runtime);
  ~JniGlDisplayHandler() override;

  base::android::ScopedJavaLocalRef<jobject> CreateDesktopViewFactory();

  // DisplayUpdaterFactory overrides.
  std::unique_ptr<protocol::CursorShapeStub> CreateCursorShapeStub() override;
  std::unique_ptr<protocol::VideoRenderer> CreateVideoRenderer() override;

  static bool RegisterJni(JNIEnv* env);

  void OnSurfaceCreated(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      const base::android::JavaParamRef<jobject>& surface);

  void OnSurfaceChanged(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& caller,
                        int width,
                        int height);

  void OnSurfaceDestroyed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);

  void OnPixelTransformationChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      const base::android::JavaParamRef<jfloatArray>&  matrix
      );

  void OnCursorPixelPositionChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      int x,
      int y);

  void OnCursorVisibilityChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      bool visible);

  void OnCursorInputFeedback(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      int x,
      int y,
      float diameter);

  void SetRenderEventEnabled(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& caller,
                             jboolean enabled);

 private:
  static void NotifyRenderEventOnUiThread(
      base::android::ScopedJavaGlobalRef<jobject> java_client);

  static void ChangeCanvasSizeOnUiThread(
      base::android::ScopedJavaGlobalRef<jobject> java_client,
      int width,
      int height);

  ChromotingJniRuntime* runtime_;

  base::android::ScopedJavaGlobalRef<jobject> java_display_;

  DISALLOW_COPY_AND_ASSIGN(JniGlDisplayHandler);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_GL_DISPLAY_HANDLER_H_
