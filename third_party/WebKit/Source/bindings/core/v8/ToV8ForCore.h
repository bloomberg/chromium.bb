// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ToV8ForCore_h
#define ToV8ForCore_h

// ToV8() provides C++ -> V8 conversion. Note that ToV8() can return an empty
// handle. Call sites must check IsEmpty() before using return value.

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/ToV8.h"
#include "core/CoreExport.h"
#include "core/dom/NotShared.h"
#include "v8/include/v8.h"

namespace blink {

class DOMWindow;
class Dictionary;
class EventTarget;
class Node;

inline v8::Local<v8::Value> ToV8(Node* impl,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return ToV8(ScriptWrappable::FromNode(impl), creation_context, isolate);
}

// Special versions for DOMWindow and EventTarget

CORE_EXPORT v8::Local<v8::Value> ToV8(DOMWindow*,
                                      v8::Local<v8::Object> creation_context,
                                      v8::Isolate*);
CORE_EXPORT v8::Local<v8::Value> ToV8(EventTarget*,
                                      v8::Local<v8::Object> creation_context,
                                      v8::Isolate*);

// Dictionary

inline v8::Local<v8::Value> ToV8(const Dictionary& value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  NOTREACHED();
  return v8::Undefined(isolate);
}

template <typename T>
inline v8::Local<v8::Value> ToV8(NotShared<T> value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return ToV8(value.View(), creation_context, isolate);
}

}  // namespace blink

#endif  // ToV8ForCore_h
