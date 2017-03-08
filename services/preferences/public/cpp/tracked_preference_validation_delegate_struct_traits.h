// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_TRACKED_PREFERENCE_VALIDATION_DELEGATE_STRUCT_TRAITS_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_TRACKED_PREFERENCE_VALIDATION_DELEGATE_STRUCT_TRAITS_H_

#include "components/user_prefs/tracked/pref_hash_store_transaction.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "services/preferences/public/interfaces/tracked_preference_validation_delegate.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<
    ::prefs::mojom::TrackedPreferenceValidationDelegate_ValueState,
    ::PrefHashStoreTransaction::ValueState> {
  static prefs::mojom::TrackedPreferenceValidationDelegate_ValueState ToMojom(
      PrefHashStoreTransaction::ValueState input);

  static bool FromMojom(
      prefs::mojom::TrackedPreferenceValidationDelegate_ValueState input,
      PrefHashStoreTransaction::ValueState* output);
};

}  // namespace mojo

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_TRACKED_PREFERENCE_VALIDATION_DELEGATE_STRUCT_TRAITS_H_
