// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8BindingForModules_h
#define V8BindingForModules_h

#include "bindings/core/v8/V8BindingForCore.h"
#include "modules/ModulesExport.h"
#include "modules/webdatabase/sqlite/SQLValue.h"

namespace blink {

class IDBKey;
class IDBKeyPath;
class IDBKeyRange;
class IDBValue;
class SerializedScriptValue;
class WebBlobInfo;

// Exposed for unit testing:
MODULES_EXPORT v8::Local<v8::Value> DeserializeIDBValue(
    v8::Isolate*,
    v8::Local<v8::Object> creation_context,
    const IDBValue*);
MODULES_EXPORT bool InjectV8KeyIntoV8Value(v8::Isolate*,
                                           v8::Local<v8::Value> key,
                                           v8::Local<v8::Value>,
                                           const IDBKeyPath&);

// For use by Source/modules/indexeddb (and unit testing):
MODULES_EXPORT bool CanInjectIDBKeyIntoScriptValue(v8::Isolate*,
                                                   const ScriptValue&,
                                                   const IDBKeyPath&);
ScriptValue DeserializeScriptValue(ScriptState*,
                                   SerializedScriptValue*,
                                   const Vector<WebBlobInfo>*,
                                   bool read_wasm_from_stream);

#if DCHECK_IS_ON()
void AssertPrimaryKeyValidOrInjectable(ScriptState*, const IDBValue*);
#endif

template <>
struct NativeValueTraits<SQLValue> {
  static SQLValue NativeValue(v8::Isolate*,
                              v8::Local<v8::Value>,
                              ExceptionState&);
};

template <>
struct NativeValueTraits<std::unique_ptr<IDBKey>> {
  static std::unique_ptr<IDBKey> NativeValue(v8::Isolate*,
                                             v8::Local<v8::Value>,
                                             ExceptionState&);
  MODULES_EXPORT static std::unique_ptr<IDBKey> NativeValue(
      v8::Isolate*,
      v8::Local<v8::Value>,
      ExceptionState&,
      const IDBKeyPath&);
};

template <>
struct NativeValueTraits<IDBKeyRange*> {
  static IDBKeyRange* NativeValue(v8::Isolate*,
                                  v8::Local<v8::Value>,
                                  ExceptionState&);
};

}  // namespace blink

#endif  // V8BindingForModules_h
