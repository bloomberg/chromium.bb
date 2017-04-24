// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ToV8ForModules_h
#define ToV8ForModules_h

#include "bindings/core/v8/V8BindingForCore.h"
#include "modules/ModulesExport.h"
#include "modules/webdatabase/sqlite/SQLValue.h"

namespace blink {

class IDBAny;
class IDBKey;
class IDBKeyPath;

inline v8::Local<v8::Value> ToV8(const SQLValue& sql_value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  switch (sql_value.GetType()) {
    case SQLValue::kNullValue:
      return v8::Null(isolate);
    case SQLValue::kNumberValue:
      return v8::Number::New(isolate, sql_value.Number());
    case SQLValue::kStringValue:
      return V8String(isolate, sql_value.GetString());
  }
  NOTREACHED();
  return v8::Local<v8::Value>();
}

v8::Local<v8::Value> ToV8(const IDBKeyPath&,
                          v8::Local<v8::Object> creation_context,
                          v8::Isolate*);
MODULES_EXPORT v8::Local<v8::Value> ToV8(const IDBKey*,
                                         v8::Local<v8::Object> creation_context,
                                         v8::Isolate*);
v8::Local<v8::Value> ToV8(const IDBAny*,
                          v8::Local<v8::Object> creation_context,
                          v8::Isolate*);

}  // namespace blink

#endif  // ToV8ForModules_h
