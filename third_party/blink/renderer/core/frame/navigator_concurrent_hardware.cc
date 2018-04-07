// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_concurrent_hardware.h"

#include "third_party/blink/public/platform/platform.h"

namespace blink {

unsigned NavigatorConcurrentHardware::hardwareConcurrency() const {
  return Platform::Current()->NumberOfProcessors();
}

}  // namespace blink
