// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InjectedScriptNative_h
#define InjectedScriptNative_h

#include "bindings/core/v8/V8PersistentValueMap.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"
#include <v8.h>

namespace blink {

class InjectedScriptNative final : public RefCounted<InjectedScriptNative> {
public:
    explicit InjectedScriptNative(v8::Isolate*);
    ~InjectedScriptNative();

    void setOnInjectedScriptHost(v8::Handle<v8::Object>);
    static InjectedScriptNative* fromInjectedScriptHost(v8::Handle<v8::Object>);

    int bind(v8::Local<v8::Value>);
    void unbind(int id);
    v8::Local<v8::Value> objectForId(int id);

private:

    int m_lastBoundObjectId;
    v8::Isolate* m_isolate;
    V8PersistentValueMap<int, v8::Value, false> m_idToWrappedObject;
};

} // namespace blink

#endif
