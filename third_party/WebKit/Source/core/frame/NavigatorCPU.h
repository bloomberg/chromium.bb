// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorCPU_h
#define NavigatorCPU_h

#include "core/CoreExport.h"

namespace blink {

class CORE_EXPORT NavigatorCPU {
 public:
  unsigned hardwareConcurrency() const;
};

}  // namespace blink

#endif  // NavigatorCPU_h
