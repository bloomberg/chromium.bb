// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_display_handler.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "jni/Display_jni.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "remoting/client/jni/jni_client.h"
#include "remoting/client/jni/jni_video_renderer.h"

using base::android::JavaParamRef;

namespace {

const int kBytesPerPixel = 4;

}

namespace remoting {

// CursorShapeStub with a lifetime separated from JniDisplayHandler.
class DisplayCursorShapeStub : public protocol::CursorShapeStub {
 public:
  DisplayCursorShapeStub(
      base::WeakPtr<JniDisplayHandler> display,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape) override;
 private:
  base::WeakPtr<JniDisplayHandler> display_handler_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

DisplayCursorShapeStub::DisplayCursorShapeStub(
    base::WeakPtr<JniDisplayHandler> display,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) :
        display_handler_(display),
        task_runner_(task_runner) {
}

void DisplayCursorShapeStub::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&JniDisplayHandler::UpdateCursorShape,
                                    display_handler_,
                                    cursor_shape));
}

// JniDisplayHandler definitions.
JniDisplayHandler::JniDisplayHandler(ChromotingJniRuntime* runtime)
    : runtime_(runtime),
      weak_factory_(this) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_display_.Reset(Java_Display_createJavaDisplayObject(
      env, reinterpret_cast<intptr_t>(this)));
}

JniDisplayHandler::~JniDisplayHandler() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  Java_Display_invalidate(base::android::AttachCurrentThread(),
                          java_display_.obj());
}

base::android::ScopedJavaLocalRef<jobject> JniDisplayHandler::GetJavaDisplay() {
  return base::android::ScopedJavaLocalRef<jobject>(java_display_);
}

void JniDisplayHandler::UpdateCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());

  // const_cast<> is safe as long as the Java updateCursorShape() method copies
  // the data out of the buffer without mutating it, and doesn't keep any
  // reference to the buffer afterwards. Unfortunately, there seems to be no way
  // to create a read-only ByteBuffer from a pointer-to-const.
  char* data = string_as_array(const_cast<std::string*>(&cursor_shape.data()));
  int cursor_total_bytes =
      cursor_shape.width() * cursor_shape.height() * kBytesPerPixel;

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> buffer(
      env, env->NewDirectByteBuffer(data, cursor_total_bytes));
  Java_Display_updateCursorShape(
      env, java_display_.obj(), cursor_shape.width(), cursor_shape.height(),
      cursor_shape.hotspot_x(), cursor_shape.hotspot_y(), buffer.obj());
}

std::unique_ptr<protocol::CursorShapeStub>
JniDisplayHandler::CreateCursorShapeStub() {
  return base::WrapUnique(
      new DisplayCursorShapeStub(weak_factory_.GetWeakPtr(),
                                 runtime_->display_task_runner()));
}

std::unique_ptr<protocol::VideoRenderer>
JniDisplayHandler::CreateVideoRenderer() {
  return base::WrapUnique(
      new JniVideoRenderer(runtime_, weak_factory_.GetWeakPtr()));
}

// static
base::android::ScopedJavaLocalRef<jobject> JniDisplayHandler::NewBitmap(
    int width,
    int height) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_Display_newBitmap(env, width, height);
}

void JniDisplayHandler::UpdateFrameBitmap(
    base::android::ScopedJavaGlobalRef<jobject> bitmap) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Display_setVideoFrame(env, java_display_.obj(), bitmap.obj());
}

void JniDisplayHandler::RedrawCanvas() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Display_redrawGraphicsInternal(env, java_display_.obj());
}

// static
bool JniDisplayHandler::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void JniDisplayHandler::ScheduleRedraw(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  runtime_->display_task_runner()->PostTask(
      FROM_HERE, base::Bind(&JniDisplayHandler::RedrawCanvas,
                            weak_factory_.GetWeakPtr()));
}

}  // namespace remoting
