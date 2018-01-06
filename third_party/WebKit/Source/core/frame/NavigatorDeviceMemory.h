// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorDeviceMemory_h
#define NavigatorDeviceMemory_h

#include "core/CoreExport.h"

namespace blink {

class CORE_EXPORT NavigatorDeviceMemory {
 public:
  float deviceMemory() const;
};

}  // namespace blink

#endif  // NavigatorDeviceMemory_h
