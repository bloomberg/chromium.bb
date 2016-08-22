// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_gl_display_handler.h"

#include <android/native_window_jni.h>
#include <array>

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "jni/GlDisplay_jni.h"
#include "remoting/client/cursor_shape_stub_proxy.h"
#include "remoting/client/dual_buffer_frame_consumer.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "remoting/client/jni/egl_thread_context.h"
#include "remoting/client/queued_task_poster.h"
#include "remoting/client/software_video_renderer.h"
#include "remoting/protocol/frame_consumer.h"

namespace remoting {

JniGlDisplayHandler::JniGlDisplayHandler(ChromotingJniRuntime* runtime)
    : runtime_(runtime), weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
  java_display_.Reset(Java_GlDisplay_createJavaDisplayObject(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));
  renderer_.SetDelegate(weak_ptr_);
  ui_task_poster_.reset(new QueuedTaskPoster(runtime->display_task_runner()));
}

JniGlDisplayHandler::~JniGlDisplayHandler() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  Java_GlDisplay_invalidate(base::android::AttachCurrentThread(),
                            java_display_);
  runtime_->ui_task_runner()->DeleteSoon(FROM_HERE, ui_task_poster_.release());
}

void JniGlDisplayHandler::InitializeClient(
    const base::android::JavaRef<jobject>& java_client) {
  return Java_GlDisplay_initializeClient(base::android::AttachCurrentThread(),
                                         java_display_, java_client);
}

std::unique_ptr<protocol::CursorShapeStub>
JniGlDisplayHandler::CreateCursorShapeStub() {
  return base::MakeUnique<CursorShapeStubProxy>(
      weak_ptr_, runtime_->display_task_runner());
}

std::unique_ptr<protocol::VideoRenderer>
JniGlDisplayHandler::CreateVideoRenderer() {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  DCHECK(!frame_consumer_);
  std::unique_ptr<DualBufferFrameConsumer> consumer =
      base::MakeUnique<DualBufferFrameConsumer>(
          base::Bind(&GlRenderer::OnFrameReceived, renderer_.GetWeakPtr()),
          runtime_->display_task_runner(),
          protocol::FrameConsumer::PixelFormat::FORMAT_RGBA);
  frame_consumer_ = consumer->GetWeakPtr();
  return base::MakeUnique<SoftwareVideoRenderer>(std::move(consumer));
}

// static
bool JniGlDisplayHandler::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void JniGlDisplayHandler::OnSurfaceCreated(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const base::android::JavaParamRef<jobject>& surface) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  runtime_->display_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&JniGlDisplayHandler::SurfaceCreatedOnDisplayThread, weak_ptr_,
                 base::android::ScopedJavaGlobalRef<jobject>(env, surface)));
}

void JniGlDisplayHandler::OnSurfaceChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    int width,
    int height) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  runtime_->display_task_runner()->PostTask(
      FROM_HERE, base::Bind(&GlRenderer::OnSurfaceChanged,
                            renderer_.GetWeakPtr(), width, height));
}

void JniGlDisplayHandler::OnSurfaceDestroyed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  runtime_->display_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&JniGlDisplayHandler::SurfaceDestroyedOnDisplayThread,
                 weak_ptr_));
}

void JniGlDisplayHandler::OnPixelTransformationChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const base::android::JavaParamRef<jfloatArray>& jmatrix) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  DCHECK(env->GetArrayLength(jmatrix.obj()) == 9);
  std::array<float, 9> matrix;
  env->GetFloatArrayRegion(jmatrix.obj(), 0, 9, matrix.data());
  ui_task_poster_->AddTask(base::Bind(&GlRenderer::OnPixelTransformationChanged,
                                      renderer_.GetWeakPtr(), matrix));
}

void JniGlDisplayHandler::OnCursorPixelPositionChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    float x,
    float y) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  ui_task_poster_->AddTask(base::Bind(&GlRenderer::OnCursorMoved,
                                      renderer_.GetWeakPtr(), x, y));
}

void JniGlDisplayHandler::OnCursorVisibilityChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    bool visible) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  ui_task_poster_->AddTask(base::Bind(&GlRenderer::OnCursorVisibilityChanged,
                                      renderer_.GetWeakPtr(), visible));
}

void JniGlDisplayHandler::OnCursorInputFeedback(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    float x,
    float y,
    float diameter) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  ui_task_poster_->AddTask(base::Bind(&GlRenderer::OnCursorInputFeedback,
                                      renderer_.GetWeakPtr(), x, y, diameter));
}

bool JniGlDisplayHandler::CanRenderFrame() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  return egl_context_ && egl_context_->IsWindowBound();
}

void JniGlDisplayHandler::OnFrameRendered() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  egl_context_->SwapBuffers();
  runtime_->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&JniGlDisplayHandler::NotifyRenderDoneOnUiThread,
                            java_display_));
}

void JniGlDisplayHandler::OnSizeChanged(int width, int height) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  runtime_->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&JniGlDisplayHandler::ChangeCanvasSizeOnUiThread,
                            java_display_, width, height));
}

void JniGlDisplayHandler::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  renderer_.OnCursorShapeChanged(cursor_shape);
}

// static
void JniGlDisplayHandler::NotifyRenderDoneOnUiThread(
    base::android::ScopedJavaGlobalRef<jobject> java_display) {
  Java_GlDisplay_canvasRendered(base::android::AttachCurrentThread(),
                                java_display);
}

void JniGlDisplayHandler::SurfaceCreatedOnDisplayThread(
    base::android::ScopedJavaGlobalRef<jobject> surface) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  renderer_.RequestCanvasSize();
  ANativeWindow* window = ANativeWindow_fromSurface(
      base::android::AttachCurrentThread(), surface.obj());
  egl_context_.reset(new EglThreadContext());
  egl_context_->BindToWindow(window);
  ANativeWindow_release(window);
  renderer_.OnSurfaceCreated(static_cast<int>(egl_context_->client_version()));
  runtime_->network_task_runner()->PostTask(
      FROM_HERE, base::Bind(&DualBufferFrameConsumer::RequestFullDesktopFrame,
                            frame_consumer_));
}

void JniGlDisplayHandler::SurfaceDestroyedOnDisplayThread() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  renderer_.OnSurfaceDestroyed();
  egl_context_.reset();
}

// static
void JniGlDisplayHandler::ChangeCanvasSizeOnUiThread(
    base::android::ScopedJavaGlobalRef<jobject> java_display,
    int width,
    int height) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GlDisplay_changeCanvasSize(env, java_display, width, height);
}

}  // namespace remoting
