// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorRequestMediaKeySystemAccess_h
#define NavigatorRequestMediaKeySystemAccess_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/modules/v8/V8MediaKeySystemOptions.h"
#include "core/frame/Navigator.h"
#include "modules/encryptedmedia/MediaKeySystemOptions.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class NavigatorRequestMediaKeySystemAccess final : public NoBaseWillBeGarbageCollectedFinalized<NavigatorRequestMediaKeySystemAccess>, public WillBeHeapSupplement<Navigator> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NavigatorRequestMediaKeySystemAccess);
public:
    virtual ~NavigatorRequestMediaKeySystemAccess();

    static NavigatorRequestMediaKeySystemAccess& from(Navigator&);

    static ScriptPromise requestMediaKeySystemAccess(
        ScriptState*,
        Navigator&,
        const String& keySystem);
    static ScriptPromise requestMediaKeySystemAccess(
        ScriptState*,
        Navigator&,
        const String& keySystem,
        const Vector<MediaKeySystemOptions>& supportedConfigurations);

    ScriptPromise requestMediaKeySystemAccess(
        ScriptState*,
        const String& keySystem,
        const Vector<MediaKeySystemOptions>& supportedConfigurations);

    virtual void trace(Visitor*) override;

private:
    NavigatorRequestMediaKeySystemAccess();
    static const char* supplementName();
};

// This is needed by the generated code for Navigator to convert a JavaScript
// object into a MediaKeySystemOptions object.
template <>
struct NativeValueTraits<MediaKeySystemOptions> {
    static inline MediaKeySystemOptions nativeValue(const v8::Handle<v8::Value>& value, v8::Isolate* isolate, ExceptionState& exceptionState)
    {
        MediaKeySystemOptions impl;
        V8MediaKeySystemOptions::toImpl(isolate, value, impl, exceptionState);
        return impl;
    }
};

} // namespace blink

#endif // NavigatorRequestMediaKeySystemAccess_h
