// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDLDictionaryBase_h
#define IDLDictionaryBase_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "v8/include/v8.h"

namespace blink {

// This class provides toV8Impl() virtual function which will be overridden
// by auto-generated IDL dictionary impl classes. toV8Impl() is used
// in ToV8.h to provide a consistent API of ToV8().
class CORE_EXPORT IDLDictionaryBase {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  IDLDictionaryBase() = default;
  virtual ~IDLDictionaryBase() = default;

  virtual v8::Local<v8::Value> ToV8Impl(v8::Local<v8::Object> creation_context,
                                        v8::Isolate*) const;

  virtual void Trace(blink::Visitor*);
};

}  // namespace blink

#endif  // IDLDictionaryBase_h
