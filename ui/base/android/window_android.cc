// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/android/window_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "jni/WindowAndroid_jni.h"
#include "ui/base/android/window_android_compositor.h"
#include "ui/base/android/window_android_observer.h"

namespace ui {

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

WindowAndroid::WindowAndroid(JNIEnv* env, jobject obj)
  : weak_java_window_(env, obj),
    compositor_(NULL) {
}

void WindowAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

ScopedJavaLocalRef<jobject> WindowAndroid::GetJavaObject() {
  return weak_java_window_.get(AttachCurrentThread());
}

bool WindowAndroid::RegisterWindowAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

WindowAndroid::~WindowAndroid() {
  DCHECK(!compositor_);
  FOR_EACH_OBSERVER(
      WindowAndroidObserver, observer_list_, OnWillDestroyWindow());
}

bool WindowAndroid::GrabSnapshot(
    int content_x, int content_y, int width, int height,
    std::vector<unsigned char>* png_representation) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> result =
      Java_WindowAndroid_grabSnapshot(env, GetJavaObject().obj(),
                                      content_x + content_offset_.x(),
                                      content_y + content_offset_.y(),
                                      width, height);
  if (result.is_null())
    return false;
  base::android::JavaByteArrayToByteVector(
      env, result.obj(), png_representation);
  return true;
}

void WindowAndroid::OnCompositingDidCommit() {
  FOR_EACH_OBSERVER(WindowAndroidObserver,
                    observer_list_,
                    OnCompositingDidCommit());
}

void WindowAndroid::AddObserver(WindowAndroidObserver* observer) {
  if (!observer_list_.HasObserver(observer))
    observer_list_.AddObserver(observer);
}

void WindowAndroid::RemoveObserver(WindowAndroidObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void WindowAndroid::AttachCompositor(WindowAndroidCompositor* compositor) {
  if (compositor_ && compositor != compositor_)
    DetachCompositor();

  compositor_ = compositor;
  FOR_EACH_OBSERVER(WindowAndroidObserver,
                    observer_list_,
                    OnAttachCompositor());
}

void WindowAndroid::DetachCompositor() {
  compositor_ = NULL;
  FOR_EACH_OBSERVER(WindowAndroidObserver,
                    observer_list_,
                    OnDetachCompositor());
  observer_list_.Clear();
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong Init(JNIEnv* env, jobject obj) {
  WindowAndroid* window = new WindowAndroid(env, obj);
  return reinterpret_cast<intptr_t>(window);
}

}  // namespace ui
