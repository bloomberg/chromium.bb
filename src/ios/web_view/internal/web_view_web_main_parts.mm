// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_web_main_parts.h"

#include "base/base_paths.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/variations/variations_ids_provider.h"
#include "ios/web/public/webui/web_ui_ios_controller_factory.h"
#include "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/cwv_flags_internal.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#include "ios/web_view/internal/translate/web_view_translate_service.h"
#include "ios/web_view/internal/webui/web_view_web_ui_ios_controller_factory.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewWebMainParts::WebViewWebMainParts()
    : field_trial_list_(/*entropy_provider=*/nullptr) {}

WebViewWebMainParts::~WebViewWebMainParts() = default;

void WebViewWebMainParts::PreCreateMainMessageLoop() {
  l10n_util::OverrideLocaleWithCocoaLocale();
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      std::string(), nullptr, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  LoadNonScalableResources();
  LoadScalableResources();
}

void WebViewWebMainParts::PreCreateThreads() {
  // Initialize local state.
  PrefService* local_state = ApplicationContext::GetInstance()->GetLocalState();
  DCHECK(local_state);

  // Flags are converted here to ensure it is set before being read by others.
  [[CWVFlags sharedInstance] convertFlagsToCommandLineSwitches];

  ApplicationContext::GetInstance()->PreCreateThreads();

  variations::VariationsIdsProvider::Create(
      variations::VariationsIdsProvider::Mode::kUseSignedInState);

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  std::string enable_features = base::JoinString(
      {
          autofill::features::kAutofillUpstream.name,
          password_manager::features::kEnablePasswordsAccountStorage.name,
          switches::kSyncTrustedVaultPassphraseiOSRPC.name,
          switches::kSyncTrustedVaultPassphraseRecovery.name,
      },
      ",");
  std::string disabled_features = base::JoinString(
      {
      },
      ",");
  feature_list->InitializeFromCommandLine(
      /*enable_features=*/enable_features,
      /*disable_features=*/disabled_features);
  base::FeatureList::SetInstance(std::move(feature_list));
}

void WebViewWebMainParts::PreMainMessageLoopRun() {
  WebViewTranslateService::GetInstance()->Initialize();

  web::WebUIIOSControllerFactory::RegisterFactory(
      WebViewWebUIIOSControllerFactory::GetInstance());
}

void WebViewWebMainParts::PostMainMessageLoopRun() {
  // CWVWebViewConfiguration must destroy its WebViewBrowserStates before the
  // threads are stopped by ApplicationContext.
  [CWVWebViewConfiguration shutDown];

  // Translate must be shutdown AFTER CWVWebViewConfiguration since translate
  // may receive final callbacks during webstate shutdowns.
  WebViewTranslateService::GetInstance()->Shutdown();

  ApplicationContext::GetInstance()->SaveState();
}

void WebViewWebMainParts::PostDestroyThreads() {
  ApplicationContext::GetInstance()->PostDestroyThreads();
}

void WebViewWebMainParts::LoadNonScalableResources() {
  base::FilePath pak_file;
  base::PathService::Get(base::DIR_MODULE, &pak_file);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("web_view_resources.pak"));
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  resource_bundle.AddDataPackFromPath(pak_file, ui::kScaleFactorNone);
}

void WebViewWebMainParts::LoadScalableResources() {
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  if (ui::ResourceBundle::IsScaleFactorSupported(ui::k100Percent)) {
    base::FilePath pak_file_100;
    base::PathService::Get(base::DIR_MODULE, &pak_file_100);
    pak_file_100 =
        pak_file_100.Append(FILE_PATH_LITERAL("web_view_100_percent.pak"));
    resource_bundle.AddDataPackFromPath(pak_file_100, ui::k100Percent);
  }

  if (ui::ResourceBundle::IsScaleFactorSupported(ui::k200Percent)) {
    base::FilePath pak_file_200;
    base::PathService::Get(base::DIR_MODULE, &pak_file_200);
    pak_file_200 =
        pak_file_200.Append(FILE_PATH_LITERAL("web_view_200_percent.pak"));
    resource_bundle.AddDataPackFromPath(pak_file_200, ui::k200Percent);
  }

  if (ui::ResourceBundle::IsScaleFactorSupported(ui::k300Percent)) {
    base::FilePath pak_file_300;
    base::PathService::Get(base::DIR_MODULE, &pak_file_300);
    pak_file_300 =
        pak_file_300.Append(FILE_PATH_LITERAL("web_view_300_percent.pak"));
    resource_bundle.AddDataPackFromPath(pak_file_300, ui::k300Percent);
  }
}

}  // namespace ios_web_view
