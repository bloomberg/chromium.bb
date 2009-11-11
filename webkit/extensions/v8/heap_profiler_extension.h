// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// HeapProfilerExtension is a V8 extension to expose a JS function for
// dumping native heap profiles.  This should only be used for
// debugging.

#ifndef WEBKIT_EXTENSIONS_V8_HEAP_PROFILER_EXTENSION_H_
#define WEBKIT_EXTENSIONS_V8_HEAP_PROFILER_EXTENSION_H_

#include "v8/include/v8.h"

namespace extensions_v8 {

class HeapProfilerExtension {
 public:
  static v8::Extension* Get();
};

}  // namespace extensions_v8

#endif  // WEBKIT_EXTENSIONS_V8_HEAP_PROFILER_EXTENSION_H_
