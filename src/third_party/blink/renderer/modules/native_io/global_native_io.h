// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_GLOBAL_NATIVE_IO_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_GLOBAL_NATIVE_IO_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LocalDOMWindow;
class NativeIOManager;
class WorkerGlobalScope;

// The "nativeIO" attribute on the Window global and Worker global scope.
class GlobalNativeIO {
  STATIC_ONLY(GlobalNativeIO);

 public:
  static NativeIOManager* nativeIO(LocalDOMWindow&);
  static NativeIOManager* nativeIO(WorkerGlobalScope&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_GLOBAL_NATIVE_IO_H_
