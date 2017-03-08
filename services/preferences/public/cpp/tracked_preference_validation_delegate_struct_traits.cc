// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/tracked_preference_validation_delegate_struct_traits.h"

namespace mojo {

using MojomValueState =
    prefs::mojom::TrackedPreferenceValidationDelegate_ValueState;

MojomValueState
EnumTraits<MojomValueState, ::PrefHashStoreTransaction::ValueState>::ToMojom(
    PrefHashStoreTransaction::ValueState input) {
  switch (input) {
    case PrefHashStoreTransaction::UNCHANGED:
      return MojomValueState::UNCHANGED;
    case PrefHashStoreTransaction::CLEARED:
      return MojomValueState::CLEARED;
    case PrefHashStoreTransaction::SECURE_LEGACY:
      return MojomValueState::SECURE_LEGACY;
    case PrefHashStoreTransaction::CHANGED:
      return MojomValueState::CHANGED;
    case PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE:
      return MojomValueState::UNTRUSTED_UNKNOWN_VALUE;
    case PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE:
      return MojomValueState::TRUSTED_UNKNOWN_VALUE;
    case PrefHashStoreTransaction::TRUSTED_NULL_VALUE:
      return MojomValueState::TRUSTED_NULL_VALUE;
    case PrefHashStoreTransaction::UNSUPPORTED:
      return MojomValueState::UNSUPPORTED;
  }
  NOTREACHED();
  return {};
}

bool EnumTraits<MojomValueState, ::PrefHashStoreTransaction::ValueState>::
    FromMojom(MojomValueState input,
              PrefHashStoreTransaction::ValueState* output) {
  switch (input) {
    case MojomValueState::UNCHANGED:
      *output = PrefHashStoreTransaction::UNCHANGED;
      return true;
    case MojomValueState::CLEARED:
      *output = PrefHashStoreTransaction::CLEARED;
      return true;
    case MojomValueState::SECURE_LEGACY:
      *output = PrefHashStoreTransaction::SECURE_LEGACY;
      return true;
    case MojomValueState::CHANGED:
      *output = PrefHashStoreTransaction::CHANGED;
      return true;
    case MojomValueState::UNTRUSTED_UNKNOWN_VALUE:
      *output = PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE;
      return true;
    case MojomValueState::TRUSTED_UNKNOWN_VALUE:
      *output = PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE;
      return true;
    case MojomValueState::TRUSTED_NULL_VALUE:
      *output = PrefHashStoreTransaction::TRUSTED_NULL_VALUE;
      return true;
    case MojomValueState::UNSUPPORTED:
      *output = PrefHashStoreTransaction::UNSUPPORTED;
      return true;
  }
  return false;
}

}  // namespace mojo
