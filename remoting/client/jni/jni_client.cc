// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_client.h"

#include "jni/Client_jni.h"

namespace remoting {

JniClient::JniClient() {
}

JniClient::~JniClient() {
}

// static
bool JniClient::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void JniClient::Destroy(JNIEnv* env, jobject caller) {
  delete this;
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& caller) {
  return reinterpret_cast<intptr_t>(new JniClient());
}

}  // namespace remoting
