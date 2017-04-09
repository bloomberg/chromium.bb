// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleBindingsInitializer_h
#define ModuleBindingsInitializer_h

#include "platform/wtf/Allocator.h"

namespace blink {

class ModuleBindingsInitializer {
  STATIC_ONLY(ModuleBindingsInitializer);

 public:
  static void Init();
};

}  // namespace blink

#endif
