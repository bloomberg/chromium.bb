// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_FAKE_SERVER_ANDROID_FAKE_SERVER_HELPER_ANDROID
#define SYNC_TEST_FAKE_SERVER_ANDROID_FAKE_SERVER_HELPER_ANDROID

#include <jni.h>

#include "base/basictypes.h"
#include "sync/test/fake_server/entity_builder_factory.h"

// Helper for utilizing native FakeServer infrastructure in Android tests.
class FakeServerHelperAndroid {
 public:
  // Registers the native methods.
  static bool Register(JNIEnv* env);

  // Creates a FakeServerHelperAndroid.
  FakeServerHelperAndroid(JNIEnv* env, jobject obj);

  // Factory method for creating a native FakeServer object. The caller assumes
  // ownership.
  jlong CreateFakeServer(JNIEnv* env, jobject obj);

  // Factory method for creating a native NetworkResources object. The caller
  // assumes ownership.
  jlong CreateNetworkResources(JNIEnv* env, jobject obj, jlong fake_server);

  // Deletes the given |fake_server| (a FakeServer pointer created via
  // CreateFakeServer).
  void DeleteFakeServer(JNIEnv* env, jobject obj, jlong fake_server);

  // Returns true if and only if |fake_server| contains |count| entities that
  // match |model_type_string| and |name|.
  jboolean VerifyEntityCountByTypeAndName(JNIEnv* env,
                                          jobject obj,
                                          jlong fake_server,
                                          jlong count,
                                          jstring model_type_string,
                                          jstring name);

  // Injects a typed URL into |fake_server|.
  //
  // TODO(pvalenzuela): Generalize this method to accept a serialized
  // EntitySpecifics so that a separate method is not required for each data
  // type.
  void InjectTypedUrl(JNIEnv* env,
                      jobject obj,
                      jlong fake_server,
                      jstring url);

 private:
  virtual ~FakeServerHelperAndroid();
};

#endif  // SYNC_TEST_FAKE_SERVER_ANDROID_FAKE_SERVER_HELPER_ANDROID
