// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentResponse_h
#define PaymentResponse_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class ScriptState;

class PaymentResponse final : public GarbageCollectedFinalized<PaymentResponse>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
    WTF_MAKE_NONCOPYABLE(PaymentResponse);

public:
    PaymentResponse();
    virtual ~PaymentResponse();

    const String& methodName() const { return m_methodName; }
    ScriptValue details(ScriptState*, ExceptionState&) const;

    ScriptPromise complete(ScriptState*, bool success);

    DEFINE_INLINE_TRACE() {}

private:
    String m_methodName;
    String m_stringifiedDetails;
};

} // namespace blink

#endif // PaymentResponse_h
