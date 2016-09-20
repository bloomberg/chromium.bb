// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CallbackFunctionTest_h
#define CallbackFunctionTest_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class ScriptState;
class V8TestCallback;

class CallbackFunctionTest final : public GarbageCollected<CallbackFunctionTest>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    DECLARE_TRACE();

    static CallbackFunctionTest* create() { return new CallbackFunctionTest(); }

    String testCallback(ScriptState*, V8TestCallback*, const String&, const String&, ExceptionState&);
};

} // namespace blink

#endif // CallbackFunctionTest_h
