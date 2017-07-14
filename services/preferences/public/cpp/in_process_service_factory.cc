// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/in_process_service_factory.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_registry.h"
#include "services/preferences/public/cpp/pref_service_main.h"

namespace prefs {
namespace {

static std::unique_ptr<service_manager::Service> WeakCreatePrefService(
    base::WeakPtr<InProcessPrefServiceFactory> weak_factory) {
  if (!weak_factory)
    return base::MakeUnique<service_manager::Service>();

  return weak_factory->CreatePrefService();
}

}  // namespace

// Registers all provided |PrefStore|s with the pref service. The pref stores
// remaining registered for the life time of |this|.
class InProcessPrefServiceFactory::RegisteringDelegate
    : public PrefValueStore::Delegate {
 public:
  RegisteringDelegate(base::WeakPtr<InProcessPrefServiceFactory> factory)
      : factory_(std::move(factory)) {}

  // PrefValueStore::Delegate:
  void Init(PrefStore* managed_prefs,
            PrefStore* supervised_user_prefs,
            PrefStore* extension_prefs,
            PrefStore* command_line_prefs,
            PrefStore* user_prefs,
            PrefStore* recommended_prefs,
            PrefStore* default_prefs,
            PrefNotifier* pref_notifier) override {
    if (!factory_)
      return;

    factory_->managed_prefs_ = make_scoped_refptr(managed_prefs);
    factory_->supervised_user_prefs_ =
        make_scoped_refptr(supervised_user_prefs);
    factory_->extension_prefs_ = make_scoped_refptr(extension_prefs);
    factory_->command_line_prefs_ = make_scoped_refptr(command_line_prefs);
    factory_->user_prefs_ =
        make_scoped_refptr(static_cast<PersistentPrefStore*>(user_prefs));
    factory_->recommended_prefs_ = make_scoped_refptr(recommended_prefs);
  }

  void InitIncognitoUnderlay(
      PersistentPrefStore* incognito_user_prefs_underlay) override {
    factory_->incognito_user_prefs_underlay_ =
        make_scoped_refptr(incognito_user_prefs_underlay);
  }

  void InitPrefRegistry(PrefRegistry* pref_registry) override {
    factory_->pref_registry_ = make_scoped_refptr(pref_registry);
  }

  void UpdateCommandLinePrefStore(PrefStore* command_line_prefs) override {
    // TODO(tibell): Once we have a way to deregister stores, do so here. At the
    // moment only local state uses this and local state doesn't use the pref
    // service yet.
  }

 private:
  base::WeakPtr<InProcessPrefServiceFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(RegisteringDelegate);
};

InProcessPrefServiceFactory::InProcessPrefServiceFactory()
    : weak_factory_(this) {}

InProcessPrefServiceFactory::~InProcessPrefServiceFactory() {
  if (quit_closure_)
    std::move(quit_closure_).Run();
}

std::unique_ptr<PrefValueStore::Delegate>
InProcessPrefServiceFactory::CreateDelegate() {
  return base::MakeUnique<RegisteringDelegate>(weak_factory_.GetWeakPtr());
}

base::Callback<std::unique_ptr<service_manager::Service>()>
InProcessPrefServiceFactory::CreatePrefServiceFactory() {
  return base::Bind(&WeakCreatePrefService, weak_factory_.GetWeakPtr());
}

std::unique_ptr<service_manager::Service>
InProcessPrefServiceFactory::CreatePrefService() {
  auto result = prefs::CreatePrefService(
      managed_prefs_.get(), supervised_user_prefs_.get(),
      extension_prefs_.get(), command_line_prefs_.get(), user_prefs_.get(),
      incognito_user_prefs_underlay_.get(), recommended_prefs_.get(),
      pref_registry_.get());
  quit_closure_ = std::move(result.second);
  return std::move(result.first);
}

}  // namespace prefs
