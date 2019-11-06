// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ADDRESS_NORMALIZER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ADDRESS_NORMALIZER_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/autofill/core/browser/address_normalizer_impl.h"

namespace autofill {

// Singleton that owns a single AddressNormalizerImpl instance.
class AddressNormalizerFactory {
 public:
  static AddressNormalizer* GetInstance();

 private:
  friend class base::NoDestructor<AddressNormalizerFactory>;

  AddressNormalizerFactory();
  ~AddressNormalizerFactory();

  // The only instance that exists.
  AddressNormalizerImpl address_normalizer_;

  DISALLOW_COPY_AND_ASSIGN(AddressNormalizerFactory);
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ADDRESS_NORMALIZER_FACTORY_H_
