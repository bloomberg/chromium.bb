// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/fling_animator_impl_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"

using namespace base::android;

namespace webkit_glue {

FlingAnimatorImpl::FlingAnimatorImpl()
    : is_active_(false) {
  // hold the global reference of the Java objects.
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  ScopedJavaLocalRef<jclass> cls(GetClass(env, "android/widget/OverScroller"));
  jmethodID constructor = GetMethodID(env, cls, "<init>",
                                      "(Landroid/content/Context;)V");
  ScopedJavaLocalRef<jobject> tmp(env, env->NewObject(cls.obj(), constructor,
                                                      GetApplicationContext()));
  DCHECK(tmp.obj());
  java_scroller_.Reset(tmp);

  fling_method_id_ = GetMethodID(env, cls, "fling", "(IIIIIIII)V");
  abort_method_id_ = GetMethodID(env, cls, "abortAnimation", "()V");
  compute_method_id_ = GetMethodID(env, cls, "computeScrollOffset", "()Z");
  getX_method_id_ = GetMethodID(env, cls, "getCurrX", "()I");
  getY_method_id_ = GetMethodID(env, cls, "getCurrY", "()I");
}

FlingAnimatorImpl::~FlingAnimatorImpl()
{
}

void FlingAnimatorImpl::startFling(const WebKit::WebFloatPoint& velocity,
                                   const WebKit::WebRect& /* range */)
{
  // Ignore "range" as it's always empty -- see http://webkit.org/b/96403
  // Instead, use the largest possible bounds for minX/maxX/minY/maxY. The
  // compositor will ignore any attempt to scroll beyond the end of the page.

  DCHECK(velocity.x || velocity.y);
  if (is_active_)
    cancelFling();

  is_active_ = true;

  JNIEnv* env = AttachCurrentThread();

  env->CallVoidMethod(java_scroller_.obj(), fling_method_id_, 0, 0,
                      static_cast<int>(velocity.x),
                      static_cast<int>(velocity.y),
                      INT_MIN, INT_MAX, INT_MIN, INT_MAX);
  CheckException(env);
}

void FlingAnimatorImpl::cancelFling()
{
  if (!is_active_)
    return;

  is_active_ = false;
  JNIEnv* env = AttachCurrentThread();
  env->CallVoidMethod(java_scroller_.obj(), abort_method_id_);
  CheckException(env);
}

bool FlingAnimatorImpl::updatePosition()
{
  JNIEnv* env = AttachCurrentThread();
  bool result = env->CallBooleanMethod(java_scroller_.obj(),
                                       compute_method_id_);
  CheckException(env);
  return is_active_ = result;
}

WebKit::WebPoint FlingAnimatorImpl::getCurrentPosition()
{
  JNIEnv* env = AttachCurrentThread();
  WebKit::WebPoint position(
      env->CallIntMethod(java_scroller_.obj(), getX_method_id_),
      env->CallIntMethod(java_scroller_.obj(), getY_method_id_));
  CheckException(env);
  return position;
}

} // namespace webkit_glue
