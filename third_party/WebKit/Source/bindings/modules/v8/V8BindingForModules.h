// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8BindingForModules_h
#define V8BindingForModules_h

#include "bindings/core/v8/V8Binding.h"
#include "modules/webdatabase/sqlite/SQLValue.h"

namespace blink {

class IDBAny;
class IDBKey;
class IDBKeyPath;
class IDBKeyRange;
class SerializedScriptValue;
class SharedBuffer;
class WebBlobInfo;

// Exposed for unit testing:
bool injectV8KeyIntoV8Value(v8::Isolate*, v8::Local<v8::Value> key, v8::Local<v8::Value>, const IDBKeyPath&);

// For use by Source/modules/indexeddb:
IDBKey* createIDBKeyFromScriptValueAndKeyPath(v8::Isolate*, const ScriptValue&, const IDBKeyPath&);
bool canInjectIDBKeyIntoScriptValue(v8::Isolate*, const ScriptValue&, const IDBKeyPath&);
ScriptValue idbAnyToScriptValue(ScriptState*, IDBAny*);
ScriptValue idbKeyToScriptValue(ScriptState*, const IDBKey*);
ScriptValue idbKeyPathToScriptValue(ScriptState*, const IDBKeyPath&);
IDBKey* scriptValueToIDBKey(v8::Isolate*, const ScriptValue&);
IDBKeyRange* scriptValueToIDBKeyRange(v8::Isolate*, const ScriptValue&);
ScriptValue deserializeScriptValue(ScriptState*, SerializedScriptValue*, const Vector<blink::WebBlobInfo>*);

#if ENABLE(ASSERT)
void assertPrimaryKeyValidOrInjectable(ScriptState*, PassRefPtr<SharedBuffer>, const Vector<blink::WebBlobInfo>*, IDBKey*, const IDBKeyPath&);
#endif

template <>
struct NativeValueTraits<SQLValue> {
    static SQLValue nativeValue(const v8::Local<v8::Value>& value, v8::Isolate* isolate, ExceptionState& exceptionState)
    {
        if (value.IsEmpty() || value->IsNull())
            return SQLValue();
        if (value->IsNumber())
            return SQLValue(value->NumberValue());
        V8StringResource<> stringValue(value);
        if (!stringValue.prepare(exceptionState))
            return SQLValue();
        return SQLValue(stringValue);
    }
};

} // namespace blink

#endif // V8BindingForModules_h
