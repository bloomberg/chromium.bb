// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/ui_dummy.h"

namespace vr {

extern "C" {
int CreateUi() {
  return 42;
}
}

}  // namespace vr
