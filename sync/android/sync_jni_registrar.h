// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_JNI_REGISTRAR_H_
#define SYNC_JNI_REGISTRAR_H_

#include <jni.h>

#include "sync/base/sync_export.h"

namespace syncer {

SYNC_EXPORT bool RegisterSyncJni(JNIEnv* env);

}

#endif  // SYNC_JNI_REGISTRAR_H_
