// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_REMOTING_JNI_REGISTRAR_H_
#define REMOTING_CLIENT_JNI_REMOTING_JNI_REGISTRAR_H_

#include <jni.h>

namespace remoting {

// Register all JNI bindings necessary for remoting.
bool RegisterJni(JNIEnv* env);

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_REMOTING_JNI_REGISTRAR_H_
