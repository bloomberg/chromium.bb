// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentAddress.h"

namespace blink {

PaymentAddress::PaymentAddress(mojom::blink::PaymentAddressPtr address)
    : m_regionCode(address->region_code)
    , m_addressLine(address->address_line.PassStorage())
    , m_administrativeArea(address->administrative_area)
    , m_locality(address->locality)
    , m_dependentLocality(address->dependent_locality)
    , m_postalCode(address->postal_code)
    , m_sortingCode(address->sorting_code)
    , m_languageCode(address->language_code)
    , m_organization(address->organization)
    , m_recipient(address->recipient)
{
    if (!m_languageCode.isEmpty() && !address->script_code.isEmpty()) {
        m_languageCode.append("-");
        m_languageCode.append(address->script_code);
    }
}

PaymentAddress::~PaymentAddress() {}

} // namespace blink
