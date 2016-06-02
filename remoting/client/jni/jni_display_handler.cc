// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_display_handler.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "jni/Display_jni.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "remoting/client/jni/jni_client.h"

using base::android::JavaParamRef;

namespace {

const int kBytesPerPixel = 4;
}

namespace remoting {

JniDisplayHandler::JniDisplayHandler(
    scoped_refptr<base::SingleThreadTaskRunner> display_runner,
    base::android::ScopedJavaGlobalRef<jobject> java_display)
    : display_runner_(display_runner),
      java_display_(java_display),
      weak_factory_(this) {}

JniDisplayHandler::~JniDisplayHandler() {
  DCHECK(display_runner_->BelongsToCurrentThread());
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
  DCHECK(display_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Display_setVideoFrame(env, java_display_.obj(), bitmap.obj());
}

void JniDisplayHandler::UpdateCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  DCHECK(display_runner_->BelongsToCurrentThread());

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

void JniDisplayHandler::RedrawCanvas() {
  DCHECK(display_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Display_redrawGraphicsInternal(env, java_display_.obj());
}

base::WeakPtr<JniDisplayHandler> JniDisplayHandler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// static
bool JniDisplayHandler::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void JniDisplayHandler::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  if (display_runner_->BelongsToCurrentThread()) {
    delete this;
  } else {
    display_runner_->DeleteSoon(FROM_HERE, this);
  }
}

void JniDisplayHandler::ScheduleRedraw(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  display_runner_->PostTask(
      FROM_HERE, base::Bind(&JniDisplayHandler::RedrawCanvas, GetWeakPtr()));
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& caller) {
  return reinterpret_cast<intptr_t>(new JniDisplayHandler(
      ChromotingJniRuntime::GetInstance()->display_task_runner(),
      base::android::ScopedJavaGlobalRef<jobject>(env, caller)));
}

}  // namespace remoting
