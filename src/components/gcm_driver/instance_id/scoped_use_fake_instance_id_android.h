// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_SCOPED_USE_FAKE_INSTANCE_ID_ANDROID_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_SCOPED_USE_FAKE_INSTANCE_ID_ANDROID_H_

#include <jni.h>

#include "base/macros.h"

namespace instance_id {

// Tests depending on InstanceID must use this, to avoid hitting the
// network/disk. Also clears cached InstanceIDs when constructed/destructed.
class ScopedUseFakeInstanceIDAndroid {
 public:
  ScopedUseFakeInstanceIDAndroid();
  ~ScopedUseFakeInstanceIDAndroid();

 private:
  bool previous_value_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUseFakeInstanceIDAndroid);
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_SCOPED_USE_FAKE_INSTANCE_ID_ANDROID_H_
