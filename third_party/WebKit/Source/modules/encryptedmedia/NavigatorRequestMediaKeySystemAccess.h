// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorRequestMediaKeySystemAccess_h
#define NavigatorRequestMediaKeySystemAccess_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/modules/v8/V8MediaKeySystemConfiguration.h"
#include "core/frame/Navigator.h"
#include "modules/encryptedmedia/MediaKeySystemConfiguration.h"
#include "platform/Supplementable.h"
#include "wtf/Vector.h"

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
        const Vector<MediaKeySystemConfiguration>& supportedConfigurations);

    ScriptPromise requestMediaKeySystemAccess(
        ScriptState*,
        const String& keySystem,
        const Vector<MediaKeySystemConfiguration>& supportedConfigurations);

    virtual void trace(Visitor*) override;

private:
    NavigatorRequestMediaKeySystemAccess();
    static const char* supplementName();
};

} // namespace blink

#endif // NavigatorRequestMediaKeySystemAccess_h
