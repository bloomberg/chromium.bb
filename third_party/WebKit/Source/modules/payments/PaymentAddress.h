// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentAddress_h
#define PaymentAddress_h

#include "bindings/core/v8/ScriptValue.h"
#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/payments/payment_request.mojom-blink.h"

namespace blink {

class MODULES_EXPORT PaymentAddress final
    : public GarbageCollectedFinalized<PaymentAddress>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(PaymentAddress);

 public:
  explicit PaymentAddress(payments::mojom::blink::PaymentAddressPtr);
  virtual ~PaymentAddress();

  ScriptValue toJSONForBinding(ScriptState*) const;

  const String& country() const { return country_; }
  const Vector<String>& addressLine() const { return address_line_; }
  const String& region() const { return region_; }
  const String& city() const { return city_; }
  const String& dependentLocality() const { return dependent_locality_; }
  const String& postalCode() const { return postal_code_; }
  const String& sortingCode() const { return sorting_code_; }
  const String& languageCode() const { return language_code_; }
  const String& organization() const { return organization_; }
  const String& recipient() const { return recipient_; }
  const String& phone() const { return phone_; }

  void Trace(blink::Visitor* visitor) {}

 private:
  String country_;
  Vector<String> address_line_;
  String region_;
  String city_;
  String dependent_locality_;
  String postal_code_;
  String sorting_code_;
  String language_code_;
  String organization_;
  String recipient_;
  String phone_;
};

}  // namespace blink

#endif  // PaymentAddress_h
