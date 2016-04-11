// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/android/jni_host.h"

#include "jni/Host_jni.h"

namespace remoting {

JniHost::JniHost() {}

JniHost::~JniHost() {}

// static
bool JniHost::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void JniHost::Destroy(JNIEnv* env, const JavaParamRef<jobject>& caller) {
  delete this;
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& caller) {
  return reinterpret_cast<intptr_t>(new JniHost());
}

}  // namespace remoting
