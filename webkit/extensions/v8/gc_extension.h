// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_EXTENSIONS_V8_GC_EXTENSION_H_
#define WEBKIT_EXTENSIONS_V8_GC_EXTENSION_H_
#pragma once

#include "webkit/extensions/webkit_extensions_export.h"

namespace v8 {
class Extension;
}

namespace extensions_v8 {

// GCExtension is a v8 extension to expose a method into JS for triggering
// garbage collection. This should only be used for debugging.
class GCExtension {
 public:
  WEBKIT_EXTENSIONS_EXPORT static v8::Extension* Get();
};

}  // namespace extensions_v8

#endif  // WEBKIT_EXTENSIONS_V8_GC_EXTENSION_H_
