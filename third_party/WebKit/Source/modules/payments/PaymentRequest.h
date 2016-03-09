// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentRequest_h
#define PaymentRequest_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/events/EventTarget.h"
#include "modules/payments/PaymentDetails.h"
#include "modules/payments/PaymentOptions.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class ScriptState;
class ShippingAddress;

class PaymentRequest final : public RefCountedGarbageCollectedEventTargetWithInlineData<PaymentRequest> {
    REFCOUNTED_GARBAGE_COLLECTED_EVENT_TARGET(PaymentRequest);
    DEFINE_WRAPPERTYPEINFO();
    WTF_MAKE_NONCOPYABLE(PaymentRequest);

public:
    static PaymentRequest* create(ScriptState*, const Vector<String>& supportedMethods, const PaymentDetails&, ExceptionState&);
    static PaymentRequest* create(ScriptState*, const Vector<String>& supportedMethods, const PaymentDetails&, const PaymentOptions&, ExceptionState&);
    static PaymentRequest* create(ScriptState*, const Vector<String>& supportedMethods, const PaymentDetails&, const PaymentOptions&, const ScriptValue& data, ExceptionState&);

    virtual ~PaymentRequest();

    ScriptPromise show(ScriptState*);
    void abort();

    ShippingAddress* getShippingAddress() const { return m_shippingAddress.get(); }
    const String& shippingOption() const { return m_shippingOption; }

    DEFINE_ATTRIBUTE_EVENT_LISTENER(shippingaddresschange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(shippingoptionchange);

    // EventTargetWithInlineData:
    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override;

    DECLARE_TRACE();

private:
    PaymentRequest(ScriptState*, const Vector<String>& supportedMethods, const PaymentDetails&, const PaymentOptions&, const ScriptValue& data, ExceptionState&);

    RefPtr<ScriptState> m_scriptState;
    Vector<String> m_supportedMethods;
    PaymentDetails m_details;
    PaymentOptions m_options;
    String m_stringifiedData;
    Member<ShippingAddress> m_shippingAddress;
    String m_shippingOption;
};

} // namespace blink

#endif // PaymentRequest_h
