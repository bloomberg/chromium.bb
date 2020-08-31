// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_checkup.h"

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_set.h"

using content::BrowserContext;

namespace {

bool ShouldShowExtensionsCheckup(content::BrowserContext* context) {
  // Don't show the promo if the extensions checkup experiment isn't enabled.
  if (!base::FeatureList::IsEnabled(extensions_features::kExtensionsCheckup)) {
    return false;
  }

  // Don't show promo if extensions are not enabled.
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(context)->extension_service();
  if (!extension_service || !extension_service->extensions_enabled())
    return false;

  // Don't show the promo if there are no extensions installed.
  std::unique_ptr<extensions::ExtensionSet> extension_set =
      extensions::ExtensionRegistry::Get(context)
          ->GenerateInstalledExtensionsSet();
  if (extension_set->is_empty())
    return false;

  // Check if users have non policy-installed extensions. If all the extensions
  // are policy-installed (even if they can be disabled), then do not show the
  // extensions checkup experiment.
  for (const auto& extension : *extension_set) {
    if (extension->is_extension() &&
        !extensions::Manifest::IsPolicyLocation(extension->location()) &&
        !extensions::Manifest::IsComponentLocation(extension->location()) &&
        !(extension->creation_flags() &
          extensions::Extension::WAS_INSTALLED_BY_DEFAULT)) {
      return true;
    }
  }
  return false;
}

}  // namespace

namespace extensions {

bool ShouldShowExtensionsCheckupOnStartup(content::BrowserContext* context) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  if (ShouldShowExtensionsCheckup(context) &&
      base::GetFieldTrialParamValueByFeature(
          extensions_features::kExtensionsCheckup,
          extensions_features::kExtensionsCheckupEntryPointParameter) ==
          extensions_features::kStartupEntryPoint &&
      !prefs->HasUserSeenExtensionsCheckupOnStartup()) {
    return true;
  }
  return false;
}

bool ShouldShowExtensionsCheckupPromo(content::BrowserContext* context) {
  return ShouldShowExtensionsCheckup(context) &&
         base::GetFieldTrialParamValueByFeature(
             extensions_features::kExtensionsCheckup,
             extensions_features::kExtensionsCheckupEntryPointParameter) ==
             extensions_features::kNtpPromoEntryPoint;
}

CheckupMessage GetCheckupMessageFocus() {
  return static_cast<CheckupMessage>(base::GetFieldTrialParamByFeatureAsInt(
      extensions_features::kExtensionsCheckup,
      extensions_features::kExtensionsCheckupBannerMessageParameter, 2));
}

}  // namespace extensions
