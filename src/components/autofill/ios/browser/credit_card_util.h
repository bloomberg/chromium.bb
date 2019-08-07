// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_CREDIT_CARD_UTIL_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_CREDIT_CARD_UTIL_H_

#import <Foundation/Foundation.h>

#include <string>

namespace autofill {

class CreditCard;

// Returns |credit_card| name in |locale| as an autoreleased NSString.
NSString* GetCreditCardName(const CreditCard& credit_card,
                            const std::string& locale);

// Returns |credit_card| obfuscated number as an autoreleased NSString.
NSString* GetCreditCardObfuscatedNumber(const CreditCard& credit_card);

// Returns |credit_card| expiration date as an autoreleased NSDateComponents.
// Only |year| and |month| fields of the NSDateComponents are valid.
NSDateComponents* GetCreditCardExpirationDate(const CreditCard& credit_card);

// Returns whether |credit_card| is a local card.
BOOL IsCreditCardLocal(const CreditCard& credit_card);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_CREDIT_CARD_UTIL_H_
