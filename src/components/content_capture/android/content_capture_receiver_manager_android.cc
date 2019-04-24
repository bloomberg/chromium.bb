// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/android/content_capture_receiver_manager_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "content/public/browser/web_contents.h"
#include "jni/ContentCaptureData_jni.h"
#include "jni/ContentCaptureReceiverManager_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaLongArray;

void JNI_ContentCaptureReceiverManager_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  DCHECK(!content_capture::ContentCaptureReceiverManager::FromWebContents(
      web_contents));
  content_capture::ContentCaptureReceiverManagerAndroid::Create(web_contents,
                                                                jcaller);
}

namespace content_capture {

namespace {

ScopedJavaLocalRef<jobject> ToJavaObjectOfContentCaptureData(
    JNIEnv* env,
    const ContentCaptureData& data,
    const JavaRef<jobject>& parent) {
  ScopedJavaLocalRef<jstring> jvalue =
      ConvertUTF16ToJavaString(env, data.value);
  ScopedJavaLocalRef<jobject> jdata =
      Java_ContentCaptureData_createContentCaptureData(
          env, parent, data.id, jvalue, data.bounds.x(), data.bounds.y(),
          data.bounds.width(), data.bounds.height());
  if (jdata.is_null())
    return jdata;
  for (auto child : data.children) {
    ToJavaObjectOfContentCaptureData(env, child, jdata);
  }
  return jdata;
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfContentCaptureData(
    JNIEnv* env,
    const ContentCaptureSession& session) {
  ScopedJavaLocalRef<jclass> object_clazz =
      base::android::GetClass(env, "java/lang/Object");
  jobjectArray joa =
      env->NewObjectArray(session.size(), object_clazz.obj(), NULL);
  jni_generator::CheckException(env);

  for (size_t i = 0; i < session.size(); ++i) {
    ScopedJavaLocalRef<jobject> item =
        ToJavaObjectOfContentCaptureData(env, session[i], JavaRef<jobject>());
    env->SetObjectArrayElement(joa, i, item.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

}  // namespace

ContentCaptureReceiverManagerAndroid::ContentCaptureReceiverManagerAndroid(
    content::WebContents* web_contents,
    const JavaRef<jobject>& jcaller)
    : ContentCaptureReceiverManager(web_contents), java_ref_(jcaller) {}

ContentCaptureReceiverManagerAndroid::~ContentCaptureReceiverManagerAndroid() =
    default;

void ContentCaptureReceiverManagerAndroid::Create(
    content::WebContents* web_contents,
    const JavaRef<jobject>& jcaller) {
  if (FromWebContents(web_contents))
    return;
  new ContentCaptureReceiverManagerAndroid(web_contents, jcaller);
}

void ContentCaptureReceiverManagerAndroid::DidCaptureContent(
    const ContentCaptureSession& parent_session,
    const ContentCaptureData& data) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());

  ScopedJavaLocalRef<jobject> jdata =
      ToJavaObjectOfContentCaptureData(env, data, JavaRef<jobject>());
  if (jdata.is_null())
    return;
  Java_ContentCaptureReceiverManager_didCaptureContent(
      env, java_ref_, ToJavaArrayOfContentCaptureData(env, parent_session),
      jdata);
}

void ContentCaptureReceiverManagerAndroid::DidRemoveContent(
    const ContentCaptureSession& session,
    const std::vector<int64_t>& data) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());
  Java_ContentCaptureReceiverManager_didRemoveContent(
      env, java_ref_, ToJavaArrayOfContentCaptureData(env, session),
      ToJavaLongArray(env, data));
}

void ContentCaptureReceiverManagerAndroid::DidRemoveSession(
    const ContentCaptureSession& session) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());
  Java_ContentCaptureReceiverManager_didRemoveSession(
      env, java_ref_, ToJavaArrayOfContentCaptureData(env, session));
}

}  // namespace content_capture
