// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8HiddenValue_h
#define V8HiddenValue_h

#include <memory>

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptPromiseProperties.h"
#include "core/CoreExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/PtrUtil.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptState;

class CORE_EXPORT V8HiddenValue {
 public:
  static v8::Local<v8::Value> GetHiddenValue(ScriptState*,
                                             v8::Local<v8::Object>,
                                             v8::Local<v8::String>);
  static bool SetHiddenValue(ScriptState*,
                             v8::Local<v8::Object>,
                             v8::Local<v8::String>,
                             v8::Local<v8::Value>);
  static bool DeleteHiddenValue(ScriptState*,
                                v8::Local<v8::Object>,
                                v8::Local<v8::String>);
};

}  // namespace blink

#endif  // V8HiddenValue_h
