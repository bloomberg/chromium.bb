// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_gl_display_handler.h"

#include "base/logging.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "remoting/client/jni/jni_video_renderer.h"

namespace remoting {

JniGlDisplayHandler::JniGlDisplayHandler() {}

JniGlDisplayHandler::~JniGlDisplayHandler() {}

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
  NOTIMPLEMENTED();
  return false;
}

void JniGlDisplayHandler::OnSurfaceCreated(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  NOTIMPLEMENTED();
}

void JniGlDisplayHandler::OnSurfaceChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    int width,
    int height) {
  NOTIMPLEMENTED();
}

void JniGlDisplayHandler::OnDrawFrame(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  NOTIMPLEMENTED();
}

}  // namespace remoting
