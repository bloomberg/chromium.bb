// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentAddress_h
#define PaymentAddress_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/payments/payment_request.mojom-blink.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class MODULES_EXPORT PaymentAddress final : public GarbageCollectedFinalized<PaymentAddress>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
    WTF_MAKE_NONCOPYABLE(PaymentAddress);

public:
    explicit PaymentAddress(mojom::blink::PaymentAddressPtr);
    virtual ~PaymentAddress();

    const String& regionCode() const { return m_regionCode; }
    const Vector<String>& addressLine() const { return m_addressLine; }
    const String& administrativeArea() const { return m_administrativeArea; }
    const String& locality() const { return m_locality; }
    const String& dependentLocality() const { return m_dependentLocality; }
    const String& postalCode() const { return m_postalCode; }
    const String& sortingCode() const { return m_sortingCode; }
    const String& languageCode() const { return m_languageCode; }
    const String& organization() const { return m_organization; }
    const String& recipient() const { return m_recipient; }

    DEFINE_INLINE_TRACE() {}

private:
    String m_regionCode;
    Vector<String> m_addressLine;
    String m_administrativeArea;
    String m_locality;
    String m_dependentLocality;
    String m_postalCode;
    String m_sortingCode;
    String m_languageCode;
    String m_organization;
    String m_recipient;
};

} // namespace blink

#endif // PaymentAddress_h
