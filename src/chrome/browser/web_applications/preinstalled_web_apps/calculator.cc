// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/calculator.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_definition_utils.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/grit/preinstalled_web_apps_resources.h"

namespace web_app {

ExternalInstallOptions GetConfigForCalculator() {
  ExternalInstallOptions options(
      /*install_url=*/GURL("https://calculator.apps.chrome/install"),
      /*user_display_mode=*/DisplayMode::kStandalone,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.gate_on_feature = kDefaultCalculatorWebApp.name;
  options.uninstall_and_replace.push_back("joodangkbfjnajiiifokapkpmhfnpleo");
  return options;
}

}  // namespace web_app
