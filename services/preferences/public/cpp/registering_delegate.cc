// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/registering_delegate.h"

#include "services/preferences/public/cpp/pref_store_impl.h"

namespace sync_preferences {

RegisteringDelegate::RegisteringDelegate(
    prefs::mojom::PrefStoreRegistryPtr registry)
    : registry_(std::move(registry)) {}

RegisteringDelegate::~RegisteringDelegate() = default;

void RegisteringDelegate::Init(PrefStore* managed_prefs,
                               PrefStore* supervised_user_prefs,
                               PrefStore* extension_prefs,
                               PrefStore* command_line_prefs,
                               PrefStore* user_prefs,
                               PrefStore* recommended_prefs,
                               PrefStore* default_prefs,
                               PrefNotifier* pref_notifier) {
  RegisterPrefStore(managed_prefs, PrefValueStore::MANAGED_STORE);
  RegisterPrefStore(supervised_user_prefs,
                    PrefValueStore::SUPERVISED_USER_STORE);
  RegisterPrefStore(extension_prefs, PrefValueStore::EXTENSION_STORE);
  RegisterPrefStore(command_line_prefs, PrefValueStore::COMMAND_LINE_STORE);
  RegisterPrefStore(recommended_prefs, PrefValueStore::RECOMMENDED_STORE);
}

void RegisteringDelegate::UpdateCommandLinePrefStore(
    PrefStore* command_line_prefs) {
  // TODO(tibell): Once we have a way to deregister stores, do so here. At the
  // moment only local state uses this and local state doesn't use the pref
  // service yet.
}

void RegisteringDelegate::RegisterPrefStore(
    scoped_refptr<PrefStore> backing_pref_store,
    PrefValueStore::PrefStoreType type) {
  if (!backing_pref_store)
    return;

  impls_.push_back(
      prefs::PrefStoreImpl::Create(registry_.get(), backing_pref_store, type));
}

}  // namespace sync_preferences
