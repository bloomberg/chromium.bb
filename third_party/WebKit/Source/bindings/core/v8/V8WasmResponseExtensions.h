// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8WasmResponseExtensions_h
#define V8WasmResponseExtensions_h

#include "core/CoreExport.h"
#include "platform/wtf/Allocator.h"
#include "v8/include/v8.h"

namespace blink {

// Injects Web Platform - specific overloads for WebAssembly APIs.
// See https://github.com/WebAssembly/design/blob/master/Web.md
class CORE_EXPORT WasmResponseExtensions {
  STATIC_ONLY(WasmResponseExtensions);

 public:
  static void Initialize(v8::Isolate*);
};

}  // namespace blink

#endif  // V8WasmResponseextensions_h
