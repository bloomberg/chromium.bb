// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentsValidators_h
#define PaymentsValidators_h

#include "modules/ModulesExport.h"
#include "wtf/Allocator.h"
#include "wtf/text/WTFString.h"

namespace blink {

class MODULES_EXPORT PaymentsValidators final {
    STATIC_ONLY(PaymentsValidators);

public:
    // Returns true if |code| is a valid ISO 4217 currency code.
    static bool isValidCurrencyCodeFormat(const String& code, String* optionalErrorMessage);

    // Returns true if |amount| is a valid currency code as defined in ISO 20022 CurrencyAnd30Amount.
    static bool isValidAmountFormat(const String& amount, String* optionalErrorMessage);

    // Returns true if |code| is a valid ISO 3166 country code.
    static bool isValidRegionCodeFormat(const String& code, String* optionalErrorMessage);

    // Returns true if |code| is a valid ISO 639 language code.
    static bool isValidLanguageCodeFormat(const String& code, String* optionalErrorMessage);

    // Returns true if |code| is a valid ISO 15924 script code.
    static bool isValidScriptCodeFormat(const String& code, String* optionalErrorMessage);
};

} // namespace blink

#endif // PaymentsValidators_h
