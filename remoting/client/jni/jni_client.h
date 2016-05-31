// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_CLIENT_H_
#define REMOTING_CLIENT_JNI_JNI_CLIENT_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"

namespace remoting {

class JniClient {
 public:
  JniClient();
  ~JniClient();

  // Register C++ methods exposed to Java using JNI.
  static bool RegisterJni(JNIEnv* env);

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& caller);

 private:
  DISALLOW_COPY_AND_ASSIGN(JniClient);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_CLIENT_H_
