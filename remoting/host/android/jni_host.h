// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ANDROID_JNI_HOST_H_
#define REMOTING_HOST_ANDROID_JNI_HOST_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"

namespace remoting {

class JniHost {
 public:
  JniHost();
  ~JniHost();

  // Register C++ methods exposed to Java using JNI.
  static bool RegisterJni(JNIEnv* env);

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& caller);

 private:
  DISALLOW_COPY_AND_ASSIGN(JniHost);
};

}  // namespace remoting

#endif  // REMOTING_HOST_ANDROID_JNI_HOST_H_
