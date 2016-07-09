// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_gl_display_handler.h"

#include "base/logging.h"
#include "jni/GlDisplay_jni.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "remoting/protocol/video_renderer.h"

namespace remoting {

JniGlDisplayHandler::JniGlDisplayHandler(ChromotingJniRuntime* runtime) :
  runtime_(runtime) {
  java_display_.Reset(Java_GlDisplay_createJavaDisplayObject(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));
}

JniGlDisplayHandler::~JniGlDisplayHandler() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  Java_GlDisplay_invalidate(base::android::AttachCurrentThread(),
                            java_display_.obj());
}

base::android::ScopedJavaLocalRef<jobject>
JniGlDisplayHandler::CreateDesktopViewFactory() {
  return Java_GlDisplay_createDesktopViewFactory(
      base::android::AttachCurrentThread(), java_display_.obj());
}

std::unique_ptr<protocol::CursorShapeStub>
JniGlDisplayHandler::CreateCursorShapeStub() {
  NOTIMPLEMENTED();
  return std::unique_ptr<protocol::CursorShapeStub>();
}

std::unique_ptr<protocol::VideoRenderer>
JniGlDisplayHandler::CreateVideoRenderer() {
  NOTIMPLEMENTED();
  return std::unique_ptr<protocol::VideoRenderer>();
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
  NOTIMPLEMENTED();
}

void JniGlDisplayHandler::OnSurfaceChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    int width,
    int height) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void JniGlDisplayHandler::OnSurfaceDestroyed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void JniGlDisplayHandler::OnPixelTransformationChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const base::android::JavaParamRef<jfloatArray>& jmatrix) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void JniGlDisplayHandler::OnCursorPixelPositionChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    int x,
    int y) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void JniGlDisplayHandler::OnCursorVisibilityChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    bool visible) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void JniGlDisplayHandler::OnCursorInputFeedback(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    int x,
    int y,
    float diameter) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void JniGlDisplayHandler::SetRenderEventEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    jboolean enabled) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

// static
void JniGlDisplayHandler::NotifyRenderEventOnUiThread(
    base::android::ScopedJavaGlobalRef<jobject> java_client) {
  Java_GlDisplay_canvasRendered(base::android::AttachCurrentThread(),
                                java_client.obj());
}

// static
void JniGlDisplayHandler::ChangeCanvasSizeOnUiThread(
    base::android::ScopedJavaGlobalRef<jobject> java_client,
    int width,
    int height) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GlDisplay_changeCanvasSize(env, java_client.obj(), width, height);
}

}  // namespace remoting
