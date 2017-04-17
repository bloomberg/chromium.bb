// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorConcurrentHardware_h
#define NavigatorConcurrentHardware_h

#include "core/CoreExport.h"

namespace blink {

class CORE_EXPORT NavigatorConcurrentHardware {
 public:
  unsigned hardwareConcurrency() const;
};

}  // namespace blink

#endif  // NavigatorConcurrentHardware_h
