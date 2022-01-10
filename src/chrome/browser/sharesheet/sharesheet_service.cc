// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/sharesheet/share_action/share_action.h"
#include "chrome/browser/sharesheet/sharesheet_service_delegator.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/types/display_constants.h"
#include "ui/views/view.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace sharesheet {

namespace {

std::u16string& GetSelectedApp() {
  static base::NoDestructor<std::u16string> selected_app;

  return *selected_app;
}

}  // namespace

SharesheetService::SharesheetService(Profile* profile)
    : profile_(profile),
      share_action_cache_(std::make_unique<ShareActionCache>(profile_)),
      app_service_proxy_(
          apps::AppServiceProxyFactory::GetForProfile(profile_)) {}

SharesheetService::~SharesheetService() = default;

void SharesheetService::ShowBubble(content::WebContents* web_contents,
                                   apps::mojom::IntentPtr intent,
                                   SharesheetMetrics::LaunchSource source,
                                   DeliveredCallback delivered_callback,
                                   CloseCallback close_callback) {
  ShowBubble(web_contents, std::move(intent),
             /*contains_hosted_document=*/false, source,
             std::move(delivered_callback), std::move(close_callback));
}

void SharesheetService::ShowBubble(content::WebContents* web_contents,
                                   apps::mojom::IntentPtr intent,
                                   bool contains_hosted_document,
                                   SharesheetMetrics::LaunchSource source,
                                   DeliveredCallback delivered_callback,
                                   CloseCallback close_callback) {
  DCHECK(apps_util::IsShareIntent(intent));
  SharesheetMetrics::RecordSharesheetLaunchSource(source);
  PrepareToShowBubble(web_contents->GetWeakPtr(), std::move(intent),
                      contains_hosted_document, std::move(delivered_callback),
                      std::move(close_callback));
}

SharesheetController* SharesheetService::GetSharesheetController(
    gfx::NativeWindow native_window) {
  SharesheetServiceDelegator* delegator = GetDelegator(native_window);
  if (!delegator)
    return nullptr;
  return delegator->GetSharesheetController();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void SharesheetService::ShowNearbyShareBubbleForArc(
    gfx::NativeWindow native_window,
    apps::mojom::IntentPtr intent,
    SharesheetMetrics::LaunchSource source,
    DeliveredCallback delivered_callback,
    CloseCallback close_callback,
    ActionCleanupCallback action_cleanup_callback) {
  DCHECK(apps_util::IsShareIntent(intent));

  ShareAction* share_action = share_action_cache_->GetActionFromName(
      l10n_util::GetStringUTF16(IDS_NEARBY_SHARE_FEATURE_NAME));
  if (!share_action || !share_action->ShouldShowAction(
                           intent, false /*contains_google_document=*/)) {
    std::move(delivered_callback).Run(SharesheetResult::kCancel);
    return;
  }
  share_action->SetActionCleanupCallbackForArc(
      std::move(action_cleanup_callback));
  SharesheetMetrics::RecordSharesheetLaunchSource(source);

  auto* sharesheet_service_delegator = GetOrCreateDelegator(native_window);
  sharesheet_service_delegator->ShowNearbyShareBubbleForArc(
      std::move(intent), std::move(delivered_callback),
      std::move(close_callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Cleanup delegator when bubble closes.
void SharesheetService::OnBubbleClosed(gfx::NativeWindow native_window,
                                       const std::u16string& active_action) {
  auto iter = active_delegators_.begin();
  while (iter != active_delegators_.end()) {
    if ((*iter)->GetNativeWindow() == native_window) {
      if (!active_action.empty()) {
        ShareAction* share_action =
            share_action_cache_->GetActionFromName(active_action);
        if (share_action != nullptr)
          share_action->OnClosing(iter->get()->GetSharesheetController());
      }
      active_delegators_.erase(iter);
      break;
    }
    ++iter;
  }
}

void SharesheetService::OnTargetSelected(gfx::NativeWindow native_window,
                                         const std::u16string& target_name,
                                         const TargetType type,
                                         apps::mojom::IntentPtr intent,
                                         views::View* share_action_view) {
  SharesheetServiceDelegator* delegator = GetDelegator(native_window);
  if (!delegator)
    return;

  RecordUserActionMetrics(target_name);
  if (type == TargetType::kAction) {
    ShareAction* share_action =
        share_action_cache_->GetActionFromName(target_name);
    if (share_action == nullptr)
      return;
    bool has_action_view = share_action->HasActionView();
    delegator->OnActionLaunched(has_action_view);
    share_action->LaunchAction(delegator->GetSharesheetController(),
                               (has_action_view ? share_action_view : nullptr),
                               std::move(intent));
  } else if (type == TargetType::kArcApp || type == TargetType::kWebApp) {
    DCHECK(intent);
    LaunchApp(target_name, std::move(intent));
    delegator->CloseBubble(SharesheetResult::kSuccess);
  }
}

bool SharesheetService::OnAcceleratorPressed(
    const ui::Accelerator& accelerator,
    const std::u16string& active_action) {
  if (active_action.empty())
    return false;
  ShareAction* share_action =
      share_action_cache_->GetActionFromName(active_action);
  DCHECK(share_action);
  return share_action == nullptr
             ? false
             : share_action->OnAcceleratorPressed(accelerator);
}

bool SharesheetService::HasShareTargets(const apps::mojom::IntentPtr& intent,
                                        bool contains_hosted_document) {
  std::vector<apps::IntentLaunchInfo> intent_launch_info =
      app_service_proxy_->GetAppsForIntent(intent);

  return share_action_cache_->HasVisibleActions(intent,
                                                contains_hosted_document) ||
         (!contains_hosted_document && !intent_launch_info.empty());
}

Profile* SharesheetService::GetProfile() {
  return profile_;
}

const gfx::VectorIcon* SharesheetService::GetVectorIcon(
    const std::u16string& display_name) {
  return share_action_cache_->GetVectorIconFromName(display_name);
}

void SharesheetService::ShowBubbleForTesting(
    gfx::NativeWindow native_window,
    apps::mojom::IntentPtr intent,
    bool contains_hosted_document,
    SharesheetMetrics::LaunchSource source,
    DeliveredCallback delivered_callback,
    CloseCallback close_callback) {
  SharesheetMetrics::RecordSharesheetLaunchSource(source);
  auto targets = GetActionsForIntent(intent, contains_hosted_document);
  OnReadyToShowBubble(native_window, std::move(intent),
                      std::move(delivered_callback), std::move(close_callback),
                      std::move(targets));
}

SharesheetUiDelegate* SharesheetService::GetUiDelegateForTesting(
    gfx::NativeWindow native_window) {
  auto* delegator = GetDelegator(native_window);
  return delegator->GetUiDelegateForTesting();  // IN-TEST
}

// static
void SharesheetService::SetSelectedAppForTesting(
    const std::u16string& target_name) {
  GetSelectedApp() = target_name;
}

void SharesheetService::PrepareToShowBubble(
    base::WeakPtr<content::WebContents> web_contents,
    apps::mojom::IntentPtr intent,
    bool contains_hosted_document,
    DeliveredCallback delivered_callback,
    CloseCallback close_callback) {
  auto targets = GetActionsForIntent(intent, contains_hosted_document);

  std::vector<apps::IntentLaunchInfo> intent_launch_info =
      contains_hosted_document ? std::vector<apps::IntentLaunchInfo>()
                               : app_service_proxy_->GetAppsForIntent(intent);
  SharesheetMetrics::RecordSharesheetAppCount(intent_launch_info.size());
  LoadAppIcons(std::move(intent_launch_info), std::move(targets), 0,
               base::BindOnce(&SharesheetService::OnAppIconsLoaded,
                              weak_factory_.GetWeakPtr(), web_contents,
                              std::move(intent), std::move(delivered_callback),
                              std::move(close_callback)));
}

std::vector<TargetInfo> SharesheetService::GetActionsForIntent(
    const apps::mojom::IntentPtr& intent,
    bool contains_hosted_document) {
  std::vector<TargetInfo> targets;
  auto& actions = share_action_cache_->GetShareActions();
  auto iter = actions.begin();
  while (iter != actions.end()) {
    if ((*iter)->ShouldShowAction(intent, contains_hosted_document)) {
      targets.emplace_back(TargetType::kAction, absl::nullopt,
                           (*iter)->GetActionName(), (*iter)->GetActionName(),
                           absl::nullopt, absl::nullopt);
    }
    ++iter;
  }
  return targets;
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
  constexpr bool allow_placeholder_icon = false;
  if (base::FeatureList::IsEnabled(features::kAppServiceLoadIconWithoutMojom)) {
    app_service_proxy_->LoadIcon(
        apps::ConvertMojomAppTypToAppType(app_type), app_id,
        apps::IconType::kStandard, kIconSize, allow_placeholder_icon,
        base::BindOnce(&SharesheetService::OnIconLoaded,
                       weak_factory_.GetWeakPtr(),
                       std::move(intent_launch_info), std::move(targets), index,
                       std::move(callback)));
  } else {
    app_service_proxy_->LoadIcon(
        app_type, app_id, apps::mojom::IconType::kStandard, kIconSize,
        allow_placeholder_icon,
        apps::MojomIconValueToIconValueCallback(base::BindOnce(
            &SharesheetService::OnIconLoaded, weak_factory_.GetWeakPtr(),
            std::move(intent_launch_info), std::move(targets), index,
            std::move(callback))));
  }
}

void SharesheetService::OnIconLoaded(
    std::vector<apps::IntentLaunchInfo> intent_launch_info,
    std::vector<TargetInfo> targets,
    size_t index,
    SharesheetServiceIconLoaderCallback callback,
    apps::IconValuePtr icon_value) {
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
        gfx::ImageSkia image_skia =
            (icon_value && icon_value->icon_type == apps::IconType::kStandard)
                ? icon_value->uncompressed
                : gfx::ImageSkia();
        targets.emplace_back(target_type, image_skia,
                             base::UTF8ToUTF16(launch_entry.app_id),
                             base::UTF8ToUTF16(update.Name()),
                             base::UTF8ToUTF16(launch_entry.activity_label),
                             launch_entry.activity_name);
      });

  LoadAppIcons(std::move(intent_launch_info), std::move(targets), index + 1,
               std::move(callback));
}

void SharesheetService::OnAppIconsLoaded(
    base::WeakPtr<content::WebContents> web_contents,
    apps::mojom::IntentPtr intent,
    DeliveredCallback delivered_callback,
    CloseCallback close_callback,
    std::vector<TargetInfo> targets) {
  if (!web_contents) {
    std::move(delivered_callback).Run(SharesheetResult::kErrorWindowClosed);
    return;
  }
  OnReadyToShowBubble(web_contents->GetTopLevelNativeWindow(),
                      std::move(intent), std::move(delivered_callback),
                      std::move(close_callback), std::move(targets));
}

void SharesheetService::OnReadyToShowBubble(
    gfx::NativeWindow native_window,
    apps::mojom::IntentPtr intent,
    DeliveredCallback delivered_callback,
    CloseCallback close_callback,
    std::vector<TargetInfo> targets) {
  auto* delegator = GetOrCreateDelegator(native_window);

  RecordTargetCountMetrics(targets);
  RecordShareDataMetrics(intent);

  // If SetSelectedAppForTesting() has been called, immediately launch the app.
  const std::u16string selected_app = GetSelectedApp();
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

    std::move(delivered_callback).Run(result);
    delegator->OnBubbleClosed(/*active_action=*/std::u16string());
    return;
  }

  delegator->ShowBubble(std::move(targets), std::move(intent),
                        std::move(delivered_callback),
                        std::move(close_callback));
}

void SharesheetService::LaunchApp(const std::u16string& target_name,
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

SharesheetServiceDelegator* SharesheetService::GetOrCreateDelegator(
    gfx::NativeWindow native_window) {
  auto* delegator = GetDelegator(native_window);
  if (delegator == nullptr) {
    auto new_delegator =
        std::make_unique<SharesheetServiceDelegator>(native_window, this);
    delegator = new_delegator.get();
    active_delegators_.push_back(std::move(new_delegator));
  }
  return delegator;
}

SharesheetServiceDelegator* SharesheetService::GetDelegator(
    gfx::NativeWindow native_window) {
  auto iter = active_delegators_.begin();
  while (iter != active_delegators_.end()) {
    if ((*iter)->GetNativeWindow() == native_window) {
      return iter->get();
    }
    ++iter;
  }
  return nullptr;
}

void SharesheetService::RecordUserActionMetrics(
    const std::u16string& target_name) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (target_name == l10n_util::GetStringUTF16(IDS_NEARBY_SHARE_FEATURE_NAME)) {
    SharesheetMetrics::RecordSharesheetActionMetrics(
        SharesheetMetrics::UserAction::kNearbyAction);
  } else if (target_name ==
             l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SHARE_BUTTON_LABEL)) {
    SharesheetMetrics::RecordSharesheetActionMetrics(
        SharesheetMetrics::UserAction::kDriveAction);
  } else if (target_name ==
             l10n_util::GetStringUTF16(
                 IDS_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_LABEL)) {
    SharesheetMetrics::RecordSharesheetActionMetrics(
        SharesheetMetrics::UserAction::kCopyAction);
  } else {
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    // Should be an app if we reached here.
    auto app_type = app_service_proxy_->AppRegistryCache().GetAppType(
        base::UTF16ToUTF8(target_name));
    switch (app_type) {
      case apps::mojom::AppType::kArc:
        SharesheetMetrics::RecordSharesheetActionMetrics(
            SharesheetMetrics::UserAction::kArc);
        break;
      case apps::mojom::AppType::kWeb:
      // TODO(crbug.com/1186533): Add a separate metrics for System Web Apps if
      // needed.
      case apps::mojom::AppType::kSystemWeb:
        SharesheetMetrics::RecordSharesheetActionMetrics(
            SharesheetMetrics::UserAction::kWeb);
        break;
      case apps::mojom::AppType::kBuiltIn:
      case apps::mojom::AppType::kCrostini:
      case apps::mojom::AppType::kChromeApp:
      case apps::mojom::AppType::kMacOs:
      case apps::mojom::AppType::kPluginVm:
      case apps::mojom::AppType::kStandaloneBrowser:
      case apps::mojom::AppType::kRemote:
      case apps::mojom::AppType::kBorealis:
      case apps::mojom::AppType::kStandaloneBrowserChromeApp:
      case apps::mojom::AppType::kExtension:
      case apps::mojom::AppType::kUnknown:
        NOTREACHED();
    }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
    const std::u16string& target_name) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (target_name == l10n_util::GetStringUTF16(IDS_NEARBY_SHARE_FEATURE_NAME)) {
    SharesheetMetrics::RecordSharesheetShareAction(
        SharesheetMetrics::UserAction::kNearbyAction);
  } else if (target_name ==
             l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SHARE_BUTTON_LABEL)) {
    SharesheetMetrics::RecordSharesheetShareAction(
        SharesheetMetrics::UserAction::kDriveAction);
  } else if (target_name ==
             l10n_util::GetStringUTF16(
                 IDS_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_LABEL)) {
    SharesheetMetrics::RecordSharesheetShareAction(
        SharesheetMetrics::UserAction::kCopyAction);
  } else {
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    NOTREACHED();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void SharesheetService::RecordShareDataMetrics(
    const apps::mojom::IntentPtr& intent) {
  // Record whether or not we're sharing a drive folder.

  // If |intent| has a |drive_share_url| but does not contain |share_text|,
  // it is a Drive Folder.
  const bool is_drive_folder = intent->drive_share_url.has_value() &&
                               intent->drive_share_url.value().is_valid() &&
                               intent->share_text.value_or("").empty();
  SharesheetMetrics::RecordSharesheetIsDriveFolder(is_drive_folder);

  // Record file count.
  const size_t file_count =
      intent->files.has_value() ? intent->files->size() : 0;
  SharesheetMetrics::RecordSharesheetFilesSharedCount(file_count);
}

}  // namespace sharesheet
