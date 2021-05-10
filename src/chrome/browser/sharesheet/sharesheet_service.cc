// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/sharesheet/share_action.h"
#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/types/display_constants.h"
#include "ui/views/view.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace sharesheet {

namespace {

base::string16& GetSelectedApp() {
  static base::NoDestructor<base::string16> selected_app;

  return *selected_app;
}

}  // namespace

SharesheetService::SharesheetService(Profile* profile)
    : profile_(profile),
      sharesheet_action_cache_(
          std::make_unique<SharesheetActionCache>(profile_)),
      app_service_proxy_(
          apps::AppServiceProxyFactory::GetForProfile(profile_)) {}

SharesheetService::~SharesheetService() = default;

void SharesheetService::ShowBubble(content::WebContents* web_contents,
                                   apps::mojom::IntentPtr intent,
                                   SharesheetMetrics::LaunchSource source,
                                   CloseCallback close_callback) {
  ShowBubble(web_contents, std::move(intent),
             /*contains_hosted_document=*/false, source,
             std::move(close_callback));
}

void SharesheetService::ShowBubble(content::WebContents* web_contents,
                                   apps::mojom::IntentPtr intent,
                                   bool contains_hosted_document,
                                   SharesheetMetrics::LaunchSource source,
                                   CloseCallback close_callback) {
  DCHECK(intent->action == apps_util::kIntentActionSend ||
         intent->action == apps_util::kIntentActionSendMultiple);
  SharesheetMetrics::RecordSharesheetLaunchSource(source);
  auto* sharesheet_service_delegate =
      GetOrCreateDelegate(web_contents->GetTopLevelNativeWindow());
  ShowBubbleWithDelegate(sharesheet_service_delegate, std::move(intent),
                         contains_hosted_document, std::move(close_callback));
}

// Cleanup delegate when bubble closes.
void SharesheetService::OnBubbleClosed(gfx::NativeWindow native_window,
                                       const base::string16& active_action) {
  auto iter = active_delegates_.begin();
  while (iter != active_delegates_.end()) {
    if ((*iter)->GetNativeWindow() == native_window) {
      if (!active_action.empty()) {
        ShareAction* share_action =
            sharesheet_action_cache_->GetActionFromName(active_action);
        if (share_action != nullptr)
          share_action->OnClosing(iter->get());
      }
      active_delegates_.erase(iter);
      break;
    }
    ++iter;
  }
}

void SharesheetService::OnTargetSelected(gfx::NativeWindow native_window,
                                         const base::string16& target_name,
                                         const TargetType type,
                                         apps::mojom::IntentPtr intent,
                                         views::View* share_action_view) {
  SharesheetServiceDelegate* delegate = GetDelegate(native_window);
  if (delegate == nullptr)
    return;

  RecordUserActionMetrics(target_name);
  if (type == TargetType::kAction) {
    ShareAction* share_action =
        sharesheet_action_cache_->GetActionFromName(target_name);
    if (share_action == nullptr)
      return;
    delegate->OnActionLaunched();
    share_action->LaunchAction(delegate, share_action_view, std::move(intent));
  } else if (type == TargetType::kArcApp || type == TargetType::kWebApp) {
    LaunchApp(target_name, std::move(intent));
    delegate->CloseSharesheet();
  }
}

SharesheetServiceDelegate* SharesheetService::GetOrCreateDelegate(
    gfx::NativeWindow native_window) {
  auto* delegate = GetDelegate(native_window);
  if (delegate == nullptr) {
    auto new_delegate =
        std::make_unique<SharesheetServiceDelegate>(native_window, this);
    delegate = new_delegate.get();
    active_delegates_.push_back(std::move(new_delegate));
  }
  return delegate;
}

SharesheetServiceDelegate* SharesheetService::GetDelegate(
    gfx::NativeWindow native_window) {
  auto iter = active_delegates_.begin();
  while (iter != active_delegates_.end()) {
    if ((*iter)->GetNativeWindow() == native_window) {
      return iter->get();
    }
    ++iter;
  }
  return nullptr;
}

bool SharesheetService::HasShareTargets(const apps::mojom::IntentPtr& intent,
                                        bool contains_hosted_document) {
  std::vector<apps::IntentLaunchInfo> intent_launch_info =
      app_service_proxy_->GetAppsForIntent(intent);

  return sharesheet_action_cache_->HasVisibleActions(
             intent, contains_hosted_document) ||
         (!contains_hosted_document && !intent_launch_info.empty());
}

Profile* SharesheetService::GetProfile() {
  return profile_;
}

const gfx::VectorIcon* SharesheetService::GetVectorIcon(
    const base::string16& display_name) {
  return sharesheet_action_cache_->GetVectorIconFromName(display_name);
}

// static
void SharesheetService::SetSelectedAppForTesting(
    const base::string16& target_name) {
  GetSelectedApp() = target_name;
}

void SharesheetService::LoadAppIcons(
    std::vector<apps::IntentLaunchInfo> intent_launch_info,
    std::vector<TargetInfo> targets,
    size_t index,
    SharesheetServiceIconLoaderCallback callback) {
  if (index >= intent_launch_info.size()) {
    std::move(callback).Run(std::move(targets));
    return;
  }

  // Making a copy because we move |intent_launch_info| out below.
  auto app_id = intent_launch_info[index].app_id;
  auto app_type = app_service_proxy_->AppRegistryCache().GetAppType(app_id);
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  constexpr bool allow_placeholder_icon = false;
  app_service_proxy_->LoadIcon(
      app_type, app_id, icon_type, kIconSize, allow_placeholder_icon,
      base::BindOnce(&SharesheetService::OnIconLoaded,
                     weak_factory_.GetWeakPtr(), std::move(intent_launch_info),
                     std::move(targets), index, std::move(callback)));
}

void SharesheetService::LaunchApp(const base::string16& target_name,
                                  apps::mojom::IntentPtr intent) {
  auto launch_source = apps::mojom::LaunchSource::kFromSharesheet;
  app_service_proxy_->LaunchAppWithIntent(
      base::UTF16ToUTF8(target_name),
      apps::GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                          WindowOpenDisposition::NEW_WINDOW,
                          /*prefer_container=*/true),
      std::move(intent), launch_source,
      apps::MakeWindowInfo(display::kDefaultDisplayId));
}

void SharesheetService::OnIconLoaded(
    std::vector<apps::IntentLaunchInfo> intent_launch_info,
    std::vector<TargetInfo> targets,
    size_t index,
    SharesheetServiceIconLoaderCallback callback,
    apps::mojom::IconValuePtr icon_value) {
  const auto& launch_entry = intent_launch_info[index];
  const auto& app_type =
      app_service_proxy_->AppRegistryCache().GetAppType(launch_entry.app_id);
  auto target_type = TargetType::kUnknown;
  if (app_type == apps::mojom::AppType::kArc) {
    target_type = TargetType::kArcApp;
  } else if (app_type == apps::mojom::AppType::kWeb) {
    target_type = TargetType::kWebApp;
  }

  app_service_proxy_->AppRegistryCache().ForOneApp(
      launch_entry.app_id, [&launch_entry, &targets, &icon_value,
                            &target_type](const apps::AppUpdate& update) {
        targets.emplace_back(target_type, icon_value->uncompressed,
                             base::UTF8ToUTF16(launch_entry.app_id),
                             base::UTF8ToUTF16(update.Name()),
                             base::UTF8ToUTF16(launch_entry.activity_label),
                             launch_entry.activity_name);
      });

  LoadAppIcons(std::move(intent_launch_info), std::move(targets), index + 1,
               std::move(callback));
}

void SharesheetService::OnAppIconsLoaded(SharesheetServiceDelegate* delegate,
                                         apps::mojom::IntentPtr intent,
                                         CloseCallback close_callback,
                                         std::vector<TargetInfo> targets) {
  RecordTargetCountMetrics(targets);

  // If SetSelectedAppForTesting() has been called, immediately launch the app.
  const base::string16 selected_app = GetSelectedApp();
  if (!selected_app.empty()) {
    SharesheetResult result = SharesheetResult::kCancel;
    auto iter = std::find_if(targets.begin(), targets.end(),
                             [selected_app](const auto& target) {
                               return (target.type == TargetType::kArcApp ||
                                       target.type == TargetType::kWebApp) &&
                                      target.launch_name == selected_app;
                             });
    if (iter != targets.end()) {
      LaunchApp(selected_app, std::move(intent));
      result = SharesheetResult::kSuccess;
    }

    std::move(close_callback).Run(result);
    delegate->OnBubbleClosed(/*active_action=*/base::string16());
    return;
  }

  delegate->ShowBubble(std::move(targets), std::move(intent),
                       std::move(close_callback));
}

void SharesheetService::ShowBubbleWithDelegate(
    SharesheetServiceDelegate* delegate,
    apps::mojom::IntentPtr intent,
    bool contains_hosted_document,
    CloseCallback close_callback) {
  std::vector<TargetInfo> targets;
  auto& actions = sharesheet_action_cache_->GetShareActions();
  auto iter = actions.begin();
  while (iter != actions.end()) {
    if ((*iter)->ShouldShowAction(intent, contains_hosted_document)) {
      targets.emplace_back(TargetType::kAction, base::nullopt,
                           (*iter)->GetActionName(), (*iter)->GetActionName(),
                           base::nullopt, base::nullopt);
    }
    ++iter;
  }

  std::vector<apps::IntentLaunchInfo> intent_launch_info =
      contains_hosted_document ? std::vector<apps::IntentLaunchInfo>()
                               : app_service_proxy_->GetAppsForIntent(intent);
  SharesheetMetrics::RecordSharesheetAppCount(intent_launch_info.size());
  LoadAppIcons(std::move(intent_launch_info), std::move(targets), 0,
               base::BindOnce(&SharesheetService::OnAppIconsLoaded,
                              weak_factory_.GetWeakPtr(), delegate,
                              std::move(intent), std::move(close_callback)));
}

void SharesheetService::RecordUserActionMetrics(
    const base::string16& target_name) {
  if (target_name == l10n_util::GetStringUTF16(IDS_NEARBY_SHARE_FEATURE_NAME)) {
    SharesheetMetrics::RecordSharesheetActionMetrics(
        SharesheetMetrics::UserAction::kNearbyAction);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (target_name ==
             l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SHARE_BUTTON_LABEL)) {
    SharesheetMetrics::RecordSharesheetActionMetrics(
        SharesheetMetrics::UserAction::kDriveAction);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  } else {
    // Should be an app if we reached here.
    auto app_type = app_service_proxy_->AppRegistryCache().GetAppType(
        base::UTF16ToUTF8(target_name));
    switch (app_type) {
      case apps::mojom::AppType::kArc:
        SharesheetMetrics::RecordSharesheetActionMetrics(
            SharesheetMetrics::UserAction::kArc);
        break;
      case apps::mojom::AppType::kWeb:
        SharesheetMetrics::RecordSharesheetActionMetrics(
            SharesheetMetrics::UserAction::kWeb);
        break;
      case apps::mojom::AppType::kBuiltIn:
      case apps::mojom::AppType::kCrostini:
      case apps::mojom::AppType::kExtension:
      case apps::mojom::AppType::kMacOs:
      case apps::mojom::AppType::kPluginVm:
      case apps::mojom::AppType::kLacros:
      case apps::mojom::AppType::kRemote:
      case apps::mojom::AppType::kBorealis:
      case apps::mojom::AppType::kUnknown:
        NOTREACHED();
    }
  }
}

void SharesheetService::RecordTargetCountMetrics(
    const std::vector<TargetInfo>& targets) {
  int arc_app_count = 0;
  int web_app_count = 0;
  for (const auto& target : targets) {
    switch (target.type) {
      case TargetType::kArcApp:
        ++arc_app_count;
        break;
      case TargetType::kWebApp:
        ++web_app_count;
        break;
      case TargetType::kAction:
        RecordShareActionMetrics(target.launch_name);
        break;
      case TargetType::kUnknown:
        NOTREACHED();
    }
  }
  SharesheetMetrics::RecordSharesheetArcAppCount(arc_app_count);
  SharesheetMetrics::RecordSharesheetWebAppCount(web_app_count);
}

void SharesheetService::RecordShareActionMetrics(
    const base::string16& target_name) {
  if (target_name == l10n_util::GetStringUTF16(IDS_NEARBY_SHARE_FEATURE_NAME)) {
    SharesheetMetrics::RecordSharesheetShareAction(
        SharesheetMetrics::UserAction::kNearbyAction);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (target_name ==
             l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SHARE_BUTTON_LABEL)) {
    SharesheetMetrics::RecordSharesheetShareAction(
        SharesheetMetrics::UserAction::kDriveAction);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  } else {
    NOTREACHED();
  }
}

}  // namespace sharesheet
