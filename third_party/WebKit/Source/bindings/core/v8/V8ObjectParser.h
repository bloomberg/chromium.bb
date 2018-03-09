// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ObjectParser_h
#define V8ObjectParser_h

#include "core/CoreExport.h"
#include "core/css_property_names.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/AtomicString.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT V8ObjectParser final {
  STATIC_ONLY(V8ObjectParser);

 public:
  // Retrieves the prototype object off a class constructor function. Returns
  // true if the prototype exists, and is an object.
  static bool ParsePrototype(v8::Local<v8::Context>,
                             v8::Local<v8::Function> constructor,
                             v8::Local<v8::Object>* prototype,
                             ExceptionState*);

  // Retrieves the specified function off a class prototype. Returns true if
  // the object exists, and is a function.
  static bool ParseFunction(v8::Local<v8::Context>,
                            v8::Local<v8::Object> prototype,
                            const AtomicString function_name,
                            v8::Local<v8::Function>*,
                            ExceptionState*);

  // Retrieves the specified generator function off a class prototype. Returns
  // true if the object exists, and is a generator function.
  static bool ParseGeneratorFunction(v8::Local<v8::Context>,
                                     v8::Local<v8::Object> prototype,
                                     const AtomicString function_name,
                                     v8::Local<v8::Function>*,
                                     ExceptionState*);

  // Retrieves and parses a CSS property list off the class constructor
  // function. Returns true if the list exists, and successfully converted to a
  // Vector<String> type. It does not fail if the list contains invalid CSS
  // properties, to ensure forward compatibility.
  static bool ParseCSSPropertyList(v8::Local<v8::Context>,
                                   v8::Local<v8::Function> constructor,
                                   const AtomicString list_name,
                                   Vector<CSSPropertyID>* native_properties,
                                   Vector<AtomicString>* custom_properties,
                                   ExceptionState*);
};

}  // namespace blink

#endif  // V8ObjectParser_h
