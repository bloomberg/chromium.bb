// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/android/fake_server_helper_android.h"

#include <jni.h>

#include "base/basictypes.h"
#include "jni/FakeServerHelper_jni.h"
#include "sync/internal_api/public/network_resources.h"
#include "sync/test/fake_server/fake_server.h"
#include "sync/test/fake_server/fake_server_network_resources.h"

FakeServerHelperAndroid::FakeServerHelperAndroid(JNIEnv* env, jobject obj) {
}

FakeServerHelperAndroid::~FakeServerHelperAndroid() { }

static jlong Init(JNIEnv* env, jobject obj) {
  FakeServerHelperAndroid* fake_server_android =
      new FakeServerHelperAndroid(env, obj);
  return reinterpret_cast<intptr_t>(fake_server_android);
}

jlong FakeServerHelperAndroid::CreateFakeServer(JNIEnv* env, jobject obj) {
  fake_server::FakeServer* fake_server = new fake_server::FakeServer();
  return reinterpret_cast<intptr_t>(fake_server);
}

jlong FakeServerHelperAndroid::CreateNetworkResources(JNIEnv* env,
                                                      jobject obj,
                                                      jlong fake_server) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  syncer::NetworkResources* resources =
      new fake_server::FakeServerNetworkResources(fake_server_ptr);
  return reinterpret_cast<intptr_t>(resources);
}

void FakeServerHelperAndroid::DeleteFakeServer(JNIEnv* env,
                                               jobject obj,
                                               jlong fake_server) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  delete fake_server_ptr;
}

// static
bool FakeServerHelperAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
