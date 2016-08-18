// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_GL_DISPLAY_HANDLER_H_
#define REMOTING_CLIENT_JNI_JNI_GL_DISPLAY_HANDLER_H_

#include <EGL/egl.h>
#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/gl_renderer.h"
#include "remoting/client/gl_renderer_delegate.h"
#include "remoting/client/jni/display_updater_factory.h"
#include "remoting/protocol/cursor_shape_stub.h"

namespace remoting {

class ChromotingJniRuntime;
class DualBufferFrameConsumer;
class EglThreadContext;
class QueuedTaskPoster;

// Handles OpenGL display operations. Draws desktop and cursor on the OpenGL
// surface.
// JNI functions should all be called on the UI thread. The display handler
// itself should be deleted on the display thread.
// Please see GlDisplay.java for documentations.
class JniGlDisplayHandler : public DisplayUpdaterFactory,
                            public protocol::CursorShapeStub,
                            public GlRendererDelegate {
 public:
  JniGlDisplayHandler(ChromotingJniRuntime* runtime);
  ~JniGlDisplayHandler() override;

  // Sets the DesktopViewFactory for the Java client.
  void InitializeClient(
      const base::android::JavaRef<jobject>& java_client);

  // DisplayUpdaterFactory interface.
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
      float x,
      float y);

  void OnCursorVisibilityChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      bool visible);

  void OnCursorInputFeedback(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      float x,
      float y,
      float diameter);

 private:
  // GlRendererDelegate interface.
  bool CanRenderFrame() override;
  void OnFrameRendered() override;
  void OnSizeChanged(int width, int height) override;

  // CursorShapeStub interface.
  void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape) override;

  static void NotifyRenderDoneOnUiThread(
      base::android::ScopedJavaGlobalRef<jobject> java_display);

  void SurfaceCreatedOnDisplayThread(
      base::android::ScopedJavaGlobalRef<jobject> surface);

  void SurfaceDestroyedOnDisplayThread();

  static void ChangeCanvasSizeOnUiThread(
      base::android::ScopedJavaGlobalRef<jobject> java_display,
      int width,
      int height);

  ChromotingJniRuntime* runtime_;

  base::android::ScopedJavaGlobalRef<jobject> java_display_;

  std::unique_ptr<EglThreadContext> egl_context_;

  base::WeakPtr<DualBufferFrameConsumer> frame_consumer_;

  // |renderer_| must be deleted earlier than |egl_context_|.
  GlRenderer renderer_;

  std::unique_ptr<QueuedTaskPoster> ui_task_poster_;

  // Used on display thread.
  base::WeakPtr<JniGlDisplayHandler> weak_ptr_;
  base::WeakPtrFactory<JniGlDisplayHandler> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(JniGlDisplayHandler);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_GL_DISPLAY_HANDLER_H_
