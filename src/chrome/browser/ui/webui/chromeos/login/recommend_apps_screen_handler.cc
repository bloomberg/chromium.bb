// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/recommend_apps_screen_handler.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"

namespace {

enum class RecommendAppsScreenState {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. This should be kept in sync with
  // RecommendAppsScreenState in enums.xml.
  SHOW = 0,
  NO_SHOW = 1,
  ERROR = 2,

  kMaxValue = ERROR
};

void RecordUmaScreenState(RecommendAppsScreenState state) {
  base::UmaHistogramEnumeration("OOBE.RecommendApps.Screen.State", state);
}

}  // namespace

namespace chromeos {

RecommendAppsScreenHandler::RecommendAppsScreenHandler()
    : BaseScreenHandler(kScreenId) {}

RecommendAppsScreenHandler::~RecommendAppsScreenHandler() = default;

void RecommendAppsScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  // TODO(crbug.com/1261902): Clean-up old strings once feature is launched.
  builder->Add("recommendAppsLoading", IDS_LOGIN_RECOMMEND_APPS_SCREEN_LOADING);
  if (!features::IsOobeNewRecommendAppsEnabled()) {
    builder->Add("recommendAppsScreenTitle",
                 IDS_LOGIN_RECOMMEND_APPS_OLD_SCREEN_TITLE);
    builder->Add("recommendAppsScreenDescription",
                 IDS_LOGIN_RECOMMEND_APPS_OLD_SCREEN_DESCRIPTION);
    builder->Add("recommendAppsSkip", IDS_LOGIN_RECOMMEND_APPS_DO_IT_LATER);
    builder->Add("recommendAppsInstall", IDS_LOGIN_RECOMMEND_APPS_DONE);
    builder->Add("recommendAppsSelectAll",
                 IDS_LOGIN_RECOMMEND_APPS_OLD_SELECT_ALL);
    return;
  }
  builder->AddF("recommendAppsScreenTitle",
                IDS_LOGIN_RECOMMEND_APPS_SCREEN_TITLE,
                ui::GetChromeOSDeviceName());
  builder->Add("recommendAppsScreenDescription",
               IDS_LOGIN_RECOMMEND_APPS_SCREEN_DESCRIPTION);
  builder->Add("recommendAppsSkip", IDS_LOGIN_RECOMMEND_APPS_SKIP);
  builder->Add("recommendAppsInstall", IDS_LOGIN_RECOMMEND_APPS_INSTALL);
  builder->Add("recommendAppsSelectAll", IDS_LOGIN_RECOMMEND_APPS_SELECT_ALL);
  builder->Add("recommendAppsInAppPurchases",
               IDS_LOGIN_RECOMMEND_APPS_SCREEN_IN_APP_PURCHASES);
  builder->Add("recommendAppsWasInstalled",
               IDS_LOGIN_RECOMMEND_APPS_SCREEN_WAS_INSTALLED);
  builder->Add("recommendAppsContainsAds",
               IDS_LOGIN_RECOMMEND_APPS_SCREEN_CONTAINS_ADS);
  builder->Add("recommendAppsDescriptionExpand",
               IDS_LOGIN_RECOMMEND_APPS_SCREEN_DESCRIPTION_EXPAND_BUTTON);
}

void RecommendAppsScreenHandler::GetAdditionalParameters(
    base::Value::Dict* dict) {
  dict->Set("isOobeNewRecommendAppsEnabled",
            features::IsOobeNewRecommendAppsEnabled());
  BaseScreenHandler::GetAdditionalParameters(dict);
}

void RecommendAppsScreenHandler::Show() {
  ShowInWebUI();
}

void RecommendAppsScreenHandler::Hide() {}

void RecommendAppsScreenHandler::OnLoadSuccess(base::Value app_list) {
  LoadAppListInUI(std::move(app_list));
}

void RecommendAppsScreenHandler::OnParseResponseError() {
  RecordUmaScreenState(RecommendAppsScreenState::NO_SHOW);
}

void RecommendAppsScreenHandler::LoadAppListInUI(base::Value app_list) {
  RecordUmaScreenState(RecommendAppsScreenState::SHOW);
  // TODO(crbug.com/1261902): Clean-up old implementation once feature is
  // launched.
  if (!features::IsOobeNewRecommendAppsEnabled() ||
      !base::FeatureList::IsEnabled(::features::kAppDiscoveryForOobe)) {
    const ui::ResourceBundle& resource_bundle =
        ui::ResourceBundle::GetSharedInstance();
    std::string app_list_webview = resource_bundle.LoadDataResourceString(
        IDR_ARC_SUPPORT_RECOMMEND_APP_OLD_LIST_VIEW_HTML);
    CallExternalAPI("setWebview", app_list_webview);
  }
  CallExternalAPI("loadAppList", std::move(app_list));
}

}  // namespace chromeos
