// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_registry_serializer.h"

#include "components/prefs/pref_registry.h"

namespace prefs {

mojom::PrefRegistryPtr SerializePrefRegistry(PrefRegistry& pref_registry) {
  auto registry = mojom::PrefRegistry::New();
  for (auto& pref : pref_registry) {
    registry->registrations.push_back(pref.first);
  }
  return registry;
}

}  // namespace prefs
