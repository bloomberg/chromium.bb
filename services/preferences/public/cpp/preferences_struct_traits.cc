// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/preferences_struct_traits.h"

namespace mojo {

using PrefStoreType = prefs::mojom::PrefStoreType;

PrefStoreType EnumTraits<PrefStoreType, PrefValueStore::PrefStoreType>::ToMojom(
    PrefValueStore::PrefStoreType input) {
  switch (input) {
    case PrefValueStore::INVALID_STORE:
      break;
    case PrefValueStore::MANAGED_STORE:
      return PrefStoreType::MANAGED;
    case PrefValueStore::SUPERVISED_USER_STORE:
      return PrefStoreType::SUPERVISED_USER;
    case PrefValueStore::EXTENSION_STORE:
      return PrefStoreType::EXTENSION;
    case PrefValueStore::COMMAND_LINE_STORE:
      return PrefStoreType::COMMAND_LINE;
    case PrefValueStore::USER_STORE:
      return PrefStoreType::USER;
    case PrefValueStore::RECOMMENDED_STORE:
      return PrefStoreType::RECOMMENDED;
    case PrefValueStore::DEFAULT_STORE:
      return PrefStoreType::DEFAULT;
  }
  NOTREACHED();
  return {};
}

bool EnumTraits<PrefStoreType, PrefValueStore::PrefStoreType>::FromMojom(
    PrefStoreType input,
    PrefValueStore::PrefStoreType* output) {
  switch (input) {
    case PrefStoreType::MANAGED:
      *output = PrefValueStore::MANAGED_STORE;
      return true;
    case PrefStoreType::SUPERVISED_USER:
      *output = PrefValueStore::SUPERVISED_USER_STORE;
      return true;
    case PrefStoreType::EXTENSION:
      *output = PrefValueStore::EXTENSION_STORE;
      return true;
    case PrefStoreType::COMMAND_LINE:
      *output = PrefValueStore::COMMAND_LINE_STORE;
      return true;
    case PrefStoreType::USER:
      *output = PrefValueStore::USER_STORE;
      return true;
    case PrefStoreType::RECOMMENDED:
      *output = PrefValueStore::RECOMMENDED_STORE;
      return true;
    case PrefStoreType::DEFAULT:
      *output = PrefValueStore::DEFAULT_STORE;
      return true;
  }
  return false;
}

}  // namespace mojo
