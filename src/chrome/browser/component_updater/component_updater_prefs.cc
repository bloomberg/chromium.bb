// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_updater_prefs.h"

#include "chrome/browser/component_updater/autofill_states_component_installer.h"
#include "chrome/browser/component_updater/chrome_component_updater_configurator.h"
#include "chrome/browser/component_updater/recovery_component_installer.h"
#include "chrome/common/buildflags.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/component_updater/supervised_user_whitelist_installer.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/component_updater/soda_component_installer.h"
#endif

namespace component_updater {

void RegisterPrefs(PrefRegistrySimple* registry) {
  RegisterPrefsForChromeComponentUpdaterConfigurator(registry);
  RegisterPrefsForRecoveryComponent(registry);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  SupervisedUserWhitelistInstaller::RegisterPrefs(registry);
#endif

#if !defined(OS_ANDROID)
  RegisterPrefsForSodaComponent(registry);
#endif

  AutofillStatesComponentInstallerPolicy::RegisterPrefs(registry);
}

}  // namespace component_updater
