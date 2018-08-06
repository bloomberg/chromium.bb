// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_HEAP_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_HEAP_CONTROLLER_H_

#include "v8/include/v8.h"

namespace blink {

// Common interface for all V8 heap tracers used in Blink.
class V8HeapController : public v8::EmbedderHeapTracer {
 public:
  ~V8HeapController() override = default;

  virtual void FinalizeAndCleanup() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_HEAP_CONTROLLER_H_
