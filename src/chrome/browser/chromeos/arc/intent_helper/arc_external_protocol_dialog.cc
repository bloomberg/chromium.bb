// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/intent_helper/arc_external_protocol_dialog.h"

#include <map>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/apps/intent_helper/page_transition_util.h"
#include "chrome/browser/chromeos/apps/intent_helper/chromeos_apps_navigation_throttle.h"
#include "chrome/browser/chromeos/apps/metrics/intent_handling_metrics.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_intent_picker_app_fetcher.h"
#include "chrome/browser/chromeos/external_protocol_dialog.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_metrics.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"

using content::WebContents;

namespace arc {

namespace {

// The proxy activity for launching an ARC IME's settings activity. These names
// have to be in sync with the ones used in ArcInputMethodManagerService.java on
// the container side. Otherwise, the picker dialog might pop up unexpectedly.
constexpr char kPackageForOpeningArcImeSettingsPage[] =
    "org.chromium.arc.applauncher";
constexpr char kActivityForOpeningArcImeSettingsPage[] =
    "org.chromium.arc.applauncher.InputMethodSettingsActivity";

// Size of device icons in DIPs.
constexpr int kDeviceIconSize = 16;

using IntentPickerResponseWithDevices = base::OnceCallback<void(
    std::vector<std::unique_ptr<syncer::DeviceInfo>> devices,
    const std::string& launch_name,
    apps::PickerEntryType entry_type,
    apps::IntentPickerCloseReason close_reason,
    bool should_persist)>;

// Creates an icon for a specific |device_type|.
gfx::Image CreateDeviceIcon(const sync_pb::SyncEnums::DeviceType device_type) {
  SkColor color = ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
      ui::NativeTheme::kColorId_DefaultIconColor);
  const gfx::VectorIcon& icon = device_type == sync_pb::SyncEnums::TYPE_TABLET
                                    ? kTabletIcon
                                    : kHardwareSmartphoneIcon;
  return gfx::Image(gfx::CreateVectorIcon(icon, kDeviceIconSize, color));
}

// Adds |devices| to |picker_entries| and returns the new list. The devices are
// added to the beginning of the list.
std::vector<apps::IntentPickerAppInfo> AddDevices(
    const std::vector<std::unique_ptr<syncer::DeviceInfo>>& devices,
    std::vector<apps::IntentPickerAppInfo> picker_entries) {
  DCHECK(!devices.empty());

  // First add all devices to the list.
  std::vector<apps::IntentPickerAppInfo> all_entries;
  for (const auto& device : devices) {
    all_entries.emplace_back(apps::PickerEntryType::kDevice,
                             CreateDeviceIcon(device->device_type()),
                             device->guid(), device->client_name());
  }

  // Append the previous list by moving its elements.
  for (auto& entry : picker_entries)
    all_entries.emplace_back(std::move(entry));

  return all_entries;
}

// Adds remote devices to |app_info| and shows the intent picker dialog if there
// is at least one app or device to choose from.
bool MaybeAddDevicesAndShowPicker(
    const GURL& url,
    const base::Optional<url::Origin>& initiating_origin,
    WebContents* web_contents,
    std::vector<apps::IntentPickerAppInfo> app_info,
    bool stay_in_chrome,
    bool show_remember_selection,
    IntentPickerResponseWithDevices callback) {
  Browser* browser =
      web_contents ? chrome::FindBrowserWithWebContents(web_contents) : nullptr;
  if (!browser)
    return false;

  bool has_apps = !app_info.empty();
  bool has_devices = false;

  PageActionIconType icon_type = PageActionIconType::kIntentPicker;
  ClickToCallUiController* controller = nullptr;
  std::vector<std::unique_ptr<syncer::DeviceInfo>> devices;

  if (ShouldOfferClickToCallForURL(web_contents->GetBrowserContext(), url)) {
    icon_type = PageActionIconType::kClickToCall;
    controller =
        ClickToCallUiController::GetOrCreateFromWebContents(web_contents);
    devices = controller->GetDevices();
    has_devices = !devices.empty();
    if (has_devices)
      app_info = AddDevices(devices, std::move(app_info));
  }

  if (app_info.empty())
    return false;

  IntentPickerTabHelper::SetShouldShowIcon(
      web_contents, icon_type == PageActionIconType::kIntentPicker);
  browser->window()->ShowIntentPickerBubble(
      std::move(app_info), stay_in_chrome, show_remember_selection, icon_type,
      initiating_origin,
      base::BindOnce(std::move(callback), std::move(devices)));

  if (controller)
    controller->OnDialogShown(has_devices, has_apps);

  return true;
}

// Shows the Chrome OS' original external protocol dialog as a fallback.
void ShowFallbackExternalProtocolDialog(int render_process_host_id,
                                        int routing_id,
                                        const GURL& url) {
  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  new ExternalProtocolDialog(web_contents, url);
}

void CloseTabIfNeeded(int render_process_host_id,
                      int routing_id,
                      bool safe_to_bypass_ui) {
  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  if (!web_contents)
    return;

  if (web_contents->GetController().IsInitialNavigation() ||
      safe_to_bypass_ui) {
    web_contents->Close();
  }
}

// Tells whether or not Chrome is an app candidate for the current navigation.
bool IsChromeAnAppCandidate(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers) {
  for (const auto& handle : handlers) {
    if (ArcIntentHelperBridge::IsIntentHelperPackage(handle->package_name))
      return true;
  }
  return false;
}

// Returns true if |handlers| only contains Chrome as an app candidate for the
// current navigation.
bool IsChromeOnlyAppCandidate(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers) {
  return handlers.size() == 1 && IsChromeAnAppCandidate(handlers);
}

// Returns true if the |handler| is for opening ARC IME settings page.
bool ForOpeningArcImeSettingsPage(const mojom::IntentHandlerInfoPtr& handler) {
  return (handler->package_name == kPackageForOpeningArcImeSettingsPage) &&
         (handler->activity_name == kActivityForOpeningArcImeSettingsPage);
}

// Shows |url| in the current tab.
void OpenUrlInChrome(int render_process_host_id,
                     int routing_id,
                     const GURL& url) {
  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  if (!web_contents)
    return;

  const ui::PageTransition page_transition_type =
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK);
  constexpr bool kIsRendererInitiated = false;
  const content::OpenURLParams params(
      url,
      content::Referrer(web_contents->GetLastCommittedURL(),
                        network::mojom::ReferrerPolicy::kDefault),
      WindowOpenDisposition::CURRENT_TAB, page_transition_type,
      kIsRendererInitiated);
  web_contents->OpenURL(params);
}

mojom::IntentInfoPtr CreateIntentInfo(const GURL& url, bool ui_bypassed) {
  // Create an intent with action VIEW, the |url| we are redirecting the user to
  // and a flag that tells whether or not the user interacted with the picker UI
  arc::mojom::IntentInfoPtr intent = arc::mojom::IntentInfo::New();
  constexpr char kArcIntentActionView[] = "org.chromium.arc.intent.action.VIEW";
  intent->action = kArcIntentActionView;
  intent->data = url.spec();
  intent->ui_bypassed = ui_bypassed;

  return intent;
}

// Sends |url| to ARC.
void HandleUrlInArc(int render_process_host_id,
                    int routing_id,
                    const GurlAndActivityInfo& url_and_activity,
                    bool ui_bypassed) {
  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager)
    return;
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(), HandleIntent);
  if (!instance)
    return;

  // We want to inform ARC about whether or not the user interacted with the
  // picker UI, also since we want to be more explicit about the package and
  // activity we are using, we are relying in HandleIntent() to comunicate back
  // to ARC.
  arc::mojom::ActivityNamePtr activity = arc::mojom::ActivityName::New();
  activity->package_name = url_and_activity.second.package_name;
  activity->activity_name = url_and_activity.second.activity_name;

  instance->HandleIntent(CreateIntentInfo(url_and_activity.first, ui_bypassed),
                         std::move(activity));
  CloseTabIfNeeded(render_process_host_id, routing_id, ui_bypassed);
}

// A helper function called by GetAction().
GetActionResult GetActionInternal(
    const GURL& original_url,
    const mojom::IntentHandlerInfoPtr& handler,
    GurlAndActivityInfo* out_url_and_activity_name) {
  if (handler->fallback_url.has_value()) {
    *out_url_and_activity_name =
        GurlAndActivityInfo(GURL(*handler->fallback_url),
                            ArcIntentHelperBridge::ActivityName(
                                handler->package_name, handler->activity_name));
    if (ArcIntentHelperBridge::IsIntentHelperPackage(handler->package_name)) {
      // Since |package_name| is "Chrome", and |fallback_url| is not null, the
      // URL must be either http or https. Check it just in case, and if not,
      // fallback to HANDLE_URL_IN_ARC;
      if (out_url_and_activity_name->first.SchemeIsHTTPOrHTTPS())
        return GetActionResult::OPEN_URL_IN_CHROME;

      LOG(WARNING) << "Failed to handle " << out_url_and_activity_name->first
                   << " in Chrome. Falling back to ARC...";
    }
    // |fallback_url| which Chrome doesn't support is passed (e.g. market:).
    return GetActionResult::HANDLE_URL_IN_ARC;
  }

  // Unlike |handler->fallback_url|, the |original_url| should always be handled
  // in ARC since it's external to Chrome.
  *out_url_and_activity_name = GurlAndActivityInfo(
      original_url, ArcIntentHelperBridge::ActivityName(
                        handler->package_name, handler->activity_name));
  return GetActionResult::HANDLE_URL_IN_ARC;
}

// Gets an action that should be done when ARC has the |handlers| for the
// |original_url| and the user selects |selected_app_index|. When the user
// hasn't selected any app, |selected_app_index| must be set to
// |handlers.size()|.
//
// When the returned action is either OPEN_URL_IN_CHROME or HANDLE_URL_IN_ARC,
// |out_url_and_activity_name| is filled accordingly.
//
// |in_out_safe_to_bypass_ui| is used to reflect whether or not we should
// display the UI: it initially informs whether or not this navigation was
// initiated within ARC, and then gets double-checked and used to store whether
// or not the user can safely bypass the UI.
GetActionResult GetAction(
    const GURL& original_url,
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers,
    size_t selected_app_index,
    GurlAndActivityInfo* out_url_and_activity_name,
    bool* in_out_safe_to_bypass_ui) {
  DCHECK(out_url_and_activity_name);
  DCHECK(!handlers.empty());

  if (selected_app_index == handlers.size()) {
    // The user hasn't made the selection yet.

    // If |handlers| has only one element and either of the following conditions
    // is met, open the URL in Chrome or ARC without showing the picker UI.
    // 1) its package is "Chrome", open the fallback URL in the current tab
    // without showing the dialog.
    // 2) its package is not "Chrome" but it has been marked as
    // |in_out_safe_to_bypass_ui|, this means that we trust the current tab
    // since its content was originated from ARC.
    // 3) its package and activity are for opening ARC IME settings page. The
    // activity is launched with an explicit user action in chrome://settings.
    if (handlers.size() == 1) {
      const GetActionResult internal_result = GetActionInternal(
          original_url, handlers[0], out_url_and_activity_name);

      if ((internal_result == GetActionResult::HANDLE_URL_IN_ARC &&
           (*in_out_safe_to_bypass_ui ||
            ForOpeningArcImeSettingsPage(handlers[0]))) ||
          internal_result == GetActionResult::OPEN_URL_IN_CHROME) {
        // Make sure the |in_out_safe_to_bypass_ui| flag is actually marked, its
        // maybe not important for OPEN_URL_IN_CHROME but just for consistency.
        *in_out_safe_to_bypass_ui = true;
        return internal_result;
      }
    }

    // Since we have 2+ app candidates we should display the UI, unless there is
    // an already preferred app. |is_preferred| will never be true unless the
    // user explicitly marked it as such.
    *in_out_safe_to_bypass_ui = false;
    for (size_t i = 0; i < handlers.size(); ++i) {
      const mojom::IntentHandlerInfoPtr& handler = handlers[i];
      if (!handler->is_preferred)
        continue;
      // This is another way to bypass the UI, since the user already expressed
      // some sort of preference.
      *in_out_safe_to_bypass_ui = true;
      // A preferred activity is found. Decide how to open it, either in Chrome
      // or ARC.
      return GetActionInternal(original_url, handler,
                               out_url_and_activity_name);
    }
    // Ask the user to pick one.
    return GetActionResult::ASK_USER;
  }

  // The user already made a selection so this should be false.
  *in_out_safe_to_bypass_ui = false;
  return GetActionInternal(original_url, handlers[selected_app_index],
                           out_url_and_activity_name);
}

// Returns true if the |url| is safe to be forwarded to ARC without showing the
// disambig dialog, besides having this flag set we need to check that there is
// only one app candidate, this is enforced via GetAction(). Any navigation
// coming from ARC via ChromeShellDelegate MUST be marked as such.
//
// Mark as not "safe" (aka return false) on the contrary, most likely those
// cases will require the user to pass thru the intent picker UI.
bool GetAndResetSafeToRedirectToArcWithoutUserConfirmationFlag(
    WebContents* web_contents) {
  const char* key =
      arc::ArcWebContentsData::ArcWebContentsData::kArcTransitionFlag;
  arc::ArcWebContentsData* arc_data =
      static_cast<arc::ArcWebContentsData*>(web_contents->GetUserData(key));
  if (!arc_data)
    return false;

  web_contents->RemoveUserData(key);
  return true;
}

void HandleDeviceSelection(
    WebContents* web_contents,
    const std::vector<std::unique_ptr<syncer::DeviceInfo>>& devices,
    const std::string& device_guid,
    const GURL& url) {
  if (!web_contents)
    return;

  const auto it = std::find_if(devices.begin(), devices.end(),
                               [&device_guid](const auto& device) {
                                 return device->guid() == device_guid;
                               });
  DCHECK(it != devices.end());
  auto* device = it->get();

  ClickToCallUiController::GetOrCreateFromWebContents(web_contents)
      ->OnDeviceSelected(GetUnescapedURLContent(url), *device,
                         SharingClickToCallEntryPoint::kLeftClickLink);
}

// Handles |url| if possible. Returns true if it is actually handled.
bool HandleUrl(int render_process_host_id,
               int routing_id,
               const GURL& url,
               const std::vector<mojom::IntentHandlerInfoPtr>& handlers,
               size_t selected_app_index,
               GetActionResult* out_result,
               bool safe_to_bypass_ui) {
  GurlAndActivityInfo url_and_activity_name(
      GURL(), ArcIntentHelperBridge::ActivityName{/*package=*/std::string(),
                                                  /*activity=*/std::string()});

  const GetActionResult result =
      GetAction(url, handlers, selected_app_index, &url_and_activity_name,
                &safe_to_bypass_ui);
  if (out_result)
    *out_result = result;

  switch (result) {
    case GetActionResult::OPEN_URL_IN_CHROME:
      OpenUrlInChrome(render_process_host_id, routing_id,
                      url_and_activity_name.first);
      return true;
    case GetActionResult::HANDLE_URL_IN_ARC:
      HandleUrlInArc(render_process_host_id, routing_id, url_and_activity_name,
                     safe_to_bypass_ui);
      return true;
    case GetActionResult::ASK_USER:
      break;
  }
  return false;
}

// Returns a fallback http(s) in |handlers| which Chrome can handle. Returns
// an empty GURL if none found.
GURL GetUrlToNavigateOnDeactivate(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers) {
  const GURL empty_url;
  for (size_t i = 0; i < handlers.size(); ++i) {
    GurlAndActivityInfo url_and_package(
        GURL(),
        ArcIntentHelperBridge::ActivityName{/*package=*/std::string(),
                                            /*activity=*/std::string()});
    if (GetActionInternal(empty_url, handlers[i], &url_and_package) ==
        GetActionResult::OPEN_URL_IN_CHROME) {
      DCHECK(url_and_package.first.SchemeIsHTTPOrHTTPS());
      return url_and_package.first;
    }
  }
  return empty_url;  // nothing found.
}

// Called when the dialog is just deactivated without pressing one of the
// buttons.
void OnIntentPickerDialogDeactivated(
    int render_process_host_id,
    int routing_id,
    bool safe_to_bypass_ui,
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers) {
  const GURL url_to_open_in_chrome = GetUrlToNavigateOnDeactivate(handlers);
  if (url_to_open_in_chrome.is_empty())
    CloseTabIfNeeded(render_process_host_id, routing_id, safe_to_bypass_ui);
  else
    OpenUrlInChrome(render_process_host_id, routing_id, url_to_open_in_chrome);
}

// Called when the dialog is closed. Note that once we show the UI, we should
// never show the Chrome OS' fallback dialog.
void OnIntentPickerClosed(
    int render_process_host_id,
    int routing_id,
    const GURL& url,
    bool safe_to_bypass_ui,
    std::vector<mojom::IntentHandlerInfoPtr> handlers,
    std::vector<std::unique_ptr<syncer::DeviceInfo>> devices,
    const std::string& selected_app_package,
    apps::PickerEntryType entry_type,
    apps::IntentPickerCloseReason reason,
    bool should_persist) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Even if ArcExternalProtocolDialog shares the same icon on the omnibox as an
  // http(s) request (via AppsNavigationThrottle), the UI here shouldn't stay in
  // the omnibox since the decision should be taken right away in a kind of
  // blocking fashion.
  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);

  if (web_contents)
    IntentPickerTabHelper::SetShouldShowIcon(web_contents, false);

  if (entry_type == apps::PickerEntryType::kDevice) {
    DCHECK_EQ(apps::IntentPickerCloseReason::OPEN_APP, reason);
    DCHECK(!should_persist);
    HandleDeviceSelection(web_contents, devices, selected_app_package, url);
    apps::IntentHandlingMetrics::RecordExternalProtocolMetrics(
        Scheme::TEL, entry_type, /*accepted=*/true, should_persist);
    apps::IntentHandlingMetrics::RecordIntentPickerUserInteractionMetrics(
        selected_app_package, entry_type, reason,
        apps::Source::kExternalProtocol, should_persist);
    return;
  }

  // If the user selected an app to continue the navigation, confirm that the
  // |package_name| matches a valid option and return the index.
  const size_t selected_app_index =
      ArcIntentPickerAppFetcher::GetAppIndex(handlers, selected_app_package);

  // Make sure that the instance at least supports HandleUrl.
  auto* arc_service_manager = ArcServiceManager::Get();
  mojom::IntentHelperInstance* instance = nullptr;
  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(), HandleUrl);
  }

  if (!instance)
    reason = apps::IntentPickerCloseReason::ERROR_AFTER_PICKER;

  if (reason == apps::IntentPickerCloseReason::OPEN_APP ||
      reason == apps::IntentPickerCloseReason::STAY_IN_CHROME) {
    if (selected_app_index == handlers.size()) {
      // Selected app does not exist.
      reason = apps::IntentPickerCloseReason::ERROR_AFTER_PICKER;
    }
  }

  switch (reason) {
    case apps::IntentPickerCloseReason::OPEN_APP:
      // Only ARC apps are offered in the external protocol intent picker, so if
      // the user decided to open in app the type must be ARC.
      DCHECK_EQ(apps::PickerEntryType::kArc, entry_type);
      DCHECK(arc_service_manager);

      if (should_persist) {
        if (ARC_GET_INSTANCE_FOR_METHOD(
                arc_service_manager->arc_bridge_service()->intent_helper(),
                AddPreferredPackage)) {
          instance->AddPreferredPackage(
              handlers[selected_app_index]->package_name);
        }
      }

      // Launch the selected app.
      HandleUrl(render_process_host_id, routing_id, url, handlers,
                selected_app_index, /*out_result=*/nullptr, safe_to_bypass_ui);
      break;
    case apps::IntentPickerCloseReason::PREFERRED_APP_FOUND:
      // We shouldn't be here if a preferred app was found.
      NOTREACHED();
      return;  // no UMA recording.
    case apps::IntentPickerCloseReason::STAY_IN_CHROME:
      LOG(ERROR) << "Chrome is not a valid option for external protocol URLs";
      NOTREACHED();
      return;  // no UMA recording.
    case apps::IntentPickerCloseReason::ERROR_BEFORE_PICKER:
      // This can happen since an error could occur right before invoking
      // Show() on the bubble's UI code.
      FALLTHROUGH;
    case apps::IntentPickerCloseReason::ERROR_AFTER_PICKER:
      LOG(ERROR) << "IntentPickerBubbleView returned CloseReason::ERROR: "
                 << "instance=" << instance
                 << ", selected_app_index=" << selected_app_index
                 << ", handlers.size=" << handlers.size();
      FALLTHROUGH;
    case apps::IntentPickerCloseReason::DIALOG_DEACTIVATED:
      // The user didn't select any ARC activity.
      OnIntentPickerDialogDeactivated(render_process_host_id, routing_id,
                                      safe_to_bypass_ui, handlers);
      break;
  }

  const std::map<base::StringPiece, Scheme> string_to_scheme = {
      {"bitcoin", Scheme::BITCOIN}, {"geo", Scheme::GEO},
      {"im", Scheme::IM},           {"irc", Scheme::IRC},
      {"magnet", Scheme::MAGNET},   {"mailto", Scheme::MAILTO},
      {"mms", Scheme::MMS},         {"sip", Scheme::SIP},
      {"skype", Scheme::SKYPE},     {"sms", Scheme::SMS},
      {"spotify", Scheme::SPOTIFY}, {"ssh", Scheme::SSH},
      {"tel", Scheme::TEL},         {"telnet", Scheme::TELNET},
      {"webcal", Scheme::WEBCAL}};

  bool protocol_accepted =
      (reason == apps::IntentPickerCloseReason::OPEN_APP) ? true : false;

  Scheme url_scheme = Scheme::OTHER;
  base::StringPiece scheme = url.scheme_piece();
  auto scheme_it = string_to_scheme.find(scheme);
  if (scheme_it != string_to_scheme.end())
    url_scheme = scheme_it->second;
  apps::IntentHandlingMetrics::RecordExternalProtocolMetrics(
      url_scheme, entry_type, protocol_accepted, should_persist);

  apps::IntentHandlingMetrics::RecordIntentPickerUserInteractionMetrics(
      selected_app_package, entry_type, reason, apps::Source::kExternalProtocol,
      should_persist);
}

// Called when ARC returned activity icons for the |handlers|.
void OnAppIconsReceived(
    int render_process_host_id,
    int routing_id,
    const GURL& url,
    const base::Optional<url::Origin>& initiating_origin,
    bool safe_to_bypass_ui,
    std::vector<mojom::IntentHandlerInfoPtr> handlers,
    std::unique_ptr<ArcIntentHelperBridge::ActivityToIconsMap> icons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  using AppInfo = apps::IntentPickerAppInfo;
  std::vector<AppInfo> app_info;

  for (const auto& handler : handlers) {
    const ArcIntentHelperBridge::ActivityName activity(handler->package_name,
                                                       handler->activity_name);
    const auto it = icons->find(activity);
    app_info.emplace_back(apps::PickerEntryType::kArc,
                          it != icons->end() ? it->second.icon16 : gfx::Image(),
                          handler->package_name, handler->name);
  }

  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);

  Browser* browser =
      web_contents ? chrome::FindBrowserWithWebContents(web_contents) : nullptr;

  if (!web_contents || !browser)
    return;

  const bool stay_in_chrome = IsChromeAnAppCandidate(handlers);
  MaybeAddDevicesAndShowPicker(
      url, initiating_origin, web_contents, std::move(app_info), stay_in_chrome,
      /*show_remember_selection=*/true,
      base::BindOnce(OnIntentPickerClosed, render_process_host_id, routing_id,
                     url, safe_to_bypass_ui, std::move(handlers)));
}

void ShowExternalProtocolDialogWithoutApps(
    int render_process_host_id,
    int routing_id,
    const GURL& url,
    const base::Optional<url::Origin>& initiating_origin) {
  // Try to show the device picker and fallback to the default dialog otherwise.
  if (MaybeAddDevicesAndShowPicker(
          url, initiating_origin,
          tab_util::GetWebContentsByID(render_process_host_id, routing_id),
          /*app_info=*/{}, /*stay_in_chrome=*/false,
          /*show_remember_selection=*/false,
          base::BindOnce(OnIntentPickerClosed, render_process_host_id,
                         routing_id, url, /*safe_to_bypass_ui=*/false,
                         std::vector<mojom::IntentHandlerInfoPtr>()))) {
    return;
  }

  ShowFallbackExternalProtocolDialog(render_process_host_id, routing_id, url);
}

// Called when ARC returned a handler list for the |url|.
void OnUrlHandlerList(int render_process_host_id,
                      int routing_id,
                      const GURL& url,
                      const base::Optional<url::Origin>& initiating_origin,
                      bool safe_to_bypass_ui,
                      std::vector<mojom::IntentHandlerInfoPtr> handlers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager) {
    // ARC is not running anymore. Show the Chrome OS dialog.
    ShowExternalProtocolDialogWithoutApps(render_process_host_id, routing_id,
                                          url, initiating_origin);
    return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(), HandleUrl);

  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  auto* intent_helper_bridge =
      web_contents ? ArcIntentHelperBridge::GetForBrowserContext(
                         web_contents->GetBrowserContext())
                   : nullptr;

  // We only reach here if Chrome doesn't think it can handle the URL. If ARC is
  // not running anymore, or Chrome is the only candidate returned, show the
  // usual Chrome OS dialog that says we cannot handle the URL.
  if (!instance || !intent_helper_bridge || handlers.empty() ||
      IsChromeOnlyAppCandidate(handlers)) {
    ShowExternalProtocolDialogWithoutApps(render_process_host_id, routing_id,
                                          url, initiating_origin);
    return;
  }

  // Check if the |url| should be handled right away without showing the UI.
  GetActionResult result;
  if (HandleUrl(render_process_host_id, routing_id, url, handlers,
                handlers.size(), &result, safe_to_bypass_ui)) {
    if (result == GetActionResult::HANDLE_URL_IN_ARC) {
      apps::IntentHandlingMetrics::RecordIntentPickerUserInteractionMetrics(
          std::string(), apps::PickerEntryType::kArc,
          apps::IntentPickerCloseReason::PREFERRED_APP_FOUND,
          apps::Source::kExternalProtocol,
          /*should_persist=*/false);
    }
    return;  // the |url| has been handled.
  }

  // Otherwise, retrieve icons of the activities. Since this function is for
  // handling external protocols, Chrome is rarely in the list, but if the |url|
  // is intent: with fallback or geo:, for example, it may be.
  std::vector<ArcIntentHelperBridge::ActivityName> activities;
  for (const auto& handler : handlers) {
    activities.emplace_back(handler->package_name, handler->activity_name);
  }
  intent_helper_bridge->GetActivityIcons(
      activities, base::BindOnce(OnAppIconsReceived, render_process_host_id,
                                 routing_id, url, initiating_origin,
                                 safe_to_bypass_ui, std::move(handlers)));
}

}  // namespace

bool RunArcExternalProtocolDialog(
    const GURL& url,
    const base::Optional<url::Origin>& initiating_origin,
    int render_process_host_id,
    int routing_id,
    ui::PageTransition page_transition,
    bool has_user_gesture) {
  // This function is for external protocols that Chrome cannot handle.
  DCHECK(!url.SchemeIsHTTPOrHTTPS()) << url;

  // For external protocol navigation, always ignore the FROM_API qualifier.
  const ui::PageTransition masked_page_transition = apps::MaskOutPageTransition(
      page_transition, ui::PAGE_TRANSITION_FROM_API);

  if (apps::ShouldIgnoreNavigation(masked_page_transition,
                                   /*allow_form_submit=*/true,
                                   /*allow_client_redirect=*/true)) {
    LOG(WARNING) << "RunArcExternalProtocolDialog: ignoring " << url
                 << " with PageTransition=" << masked_page_transition;
    return false;
  }

  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager) {
    // ARC is either not supported or not yet ready.
    ShowExternalProtocolDialogWithoutApps(render_process_host_id, routing_id,
                                          url, initiating_origin);
    return true;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(),
      RequestUrlHandlerList);
  if (!instance) {
    // ARC is either not supported or not yet ready.
    ShowExternalProtocolDialogWithoutApps(render_process_host_id, routing_id,
                                          url, initiating_origin);
    return true;
  }

  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  if (!web_contents || !web_contents->GetBrowserContext() ||
      web_contents->GetBrowserContext()->IsOffTheRecord()) {
    return false;
  }

  const bool safe_to_bypass_ui =
      GetAndResetSafeToRedirectToArcWithoutUserConfirmationFlag(web_contents);

  // Show ARC version of the dialog, which is IntentPickerBubbleView. To show
  // the bubble view, we need to ask ARC for a handler list first.
  instance->RequestUrlHandlerList(
      url.spec(),
      base::BindOnce(OnUrlHandlerList, render_process_host_id, routing_id, url,
                     initiating_origin, safe_to_bypass_ui));
  return true;
}

GetActionResult GetActionForTesting(
    const GURL& original_url,
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers,
    size_t selected_app_index,
    GurlAndActivityInfo* out_url_and_activity_name,
    bool* safe_to_bypass_ui) {
  return GetAction(original_url, handlers, selected_app_index,
                   out_url_and_activity_name, safe_to_bypass_ui);
}

GURL GetUrlToNavigateOnDeactivateForTesting(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers) {
  return GetUrlToNavigateOnDeactivate(handlers);
}

bool GetAndResetSafeToRedirectToArcWithoutUserConfirmationFlagForTesting(
    WebContents* web_contents) {
  return GetAndResetSafeToRedirectToArcWithoutUserConfirmationFlag(
      web_contents);
}

bool IsChromeAnAppCandidateForTesting(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers) {
  return IsChromeAnAppCandidate(handlers);
}

void OnIntentPickerClosedForTesting(
    int render_process_host_id,
    int routing_id,
    const GURL& url,
    bool safe_to_bypass_ui,
    std::vector<mojom::IntentHandlerInfoPtr> handlers,
    std::vector<std::unique_ptr<syncer::DeviceInfo>> devices,
    const std::string& selected_app_package,
    apps::PickerEntryType entry_type,
    apps::IntentPickerCloseReason reason,
    bool should_persist) {
  OnIntentPickerClosed(render_process_host_id, routing_id, url,
                       safe_to_bypass_ui, std::move(handlers),
                       std::move(devices), selected_app_package, entry_type,
                       reason, should_persist);
}

ProtocolAction GetProtocolAction(Scheme scheme,
                                 apps::PickerEntryType entry_type,
                                 bool accepted,
                                 bool persisted) {
  if (entry_type == apps::PickerEntryType::kDevice) {
    DCHECK_EQ(Scheme::TEL, scheme);
    DCHECK(accepted);
    DCHECK(!persisted);
    return ProtocolAction::TEL_DEVICE_SELECTED;
  }
  switch (scheme) {
    case Scheme::OTHER:
      if (!accepted)
        return ProtocolAction::OTHER_REJECTED;
      if (persisted)
        return ProtocolAction::OTHER_ACCEPTED_PERSISTED;
      return ProtocolAction::OTHER_ACCEPTED_NOT_PERSISTED;
    case Scheme::BITCOIN:
      if (!accepted)
        return ProtocolAction::BITCOIN_REJECTED;
      if (persisted)
        return ProtocolAction::BITCOIN_ACCEPTED_PERSISTED;
      return ProtocolAction::BITCOIN_ACCEPTED_NOT_PERSISTED;
    case Scheme::GEO:
      if (!accepted)
        return ProtocolAction::GEO_REJECTED;
      if (persisted)
        return ProtocolAction::GEO_ACCEPTED_PERSISTED;
      return ProtocolAction::GEO_ACCEPTED_NOT_PERSISTED;
    case Scheme::IM:
      if (!accepted)
        return ProtocolAction::IM_REJECTED;
      if (persisted)
        return ProtocolAction::IM_ACCEPTED_PERSISTED;
      return ProtocolAction::IM_ACCEPTED_NOT_PERSISTED;
    case Scheme::IRC:
      if (!accepted)
        return ProtocolAction::IRC_REJECTED;
      if (persisted)
        return ProtocolAction::IRC_ACCEPTED_PERSISTED;
      return ProtocolAction::IRC_ACCEPTED_NOT_PERSISTED;
    case Scheme::MAGNET:
      if (!accepted)
        return ProtocolAction::MAGNET_REJECTED;
      if (persisted)
        return ProtocolAction::MAGNET_ACCEPTED_PERSISTED;
      return ProtocolAction::MAGNET_ACCEPTED_NOT_PERSISTED;
    case Scheme::MAILTO:
      if (!accepted)
        return ProtocolAction::MAILTO_REJECTED;
      if (persisted)
        return ProtocolAction::MAILTO_ACCEPTED_PERSISTED;
      return ProtocolAction::MAILTO_ACCEPTED_NOT_PERSISTED;
    case Scheme::MMS:
      if (!accepted)
        return ProtocolAction::MMS_REJECTED;
      if (persisted)
        return ProtocolAction::MMS_ACCEPTED_PERSISTED;
      return ProtocolAction::MMS_ACCEPTED_NOT_PERSISTED;
    case Scheme::SIP:
      if (!accepted)
        return ProtocolAction::SIP_REJECTED;
      if (persisted)
        return ProtocolAction::SIP_ACCEPTED_PERSISTED;
      return ProtocolAction::SIP_ACCEPTED_NOT_PERSISTED;
    case Scheme::SKYPE:
      if (!accepted)
        return ProtocolAction::SKYPE_REJECTED;
      if (persisted)
        return ProtocolAction::SKYPE_ACCEPTED_PERSISTED;
      return ProtocolAction::SKYPE_ACCEPTED_NOT_PERSISTED;
    case Scheme::SMS:
      if (!accepted)
        return ProtocolAction::SMS_REJECTED;
      if (persisted)
        return ProtocolAction::SMS_ACCEPTED_PERSISTED;
      return ProtocolAction::SMS_ACCEPTED_NOT_PERSISTED;
    case Scheme::SPOTIFY:
      if (!accepted)
        return ProtocolAction::SPOTIFY_REJECTED;
      if (persisted)
        return ProtocolAction::SPOTIFY_ACCEPTED_PERSISTED;
      return ProtocolAction::SPOTIFY_ACCEPTED_NOT_PERSISTED;
    case Scheme::SSH:
      if (!accepted)
        return ProtocolAction::SSH_REJECTED;
      if (persisted)
        return ProtocolAction::SSH_ACCEPTED_PERSISTED;
      return ProtocolAction::SSH_ACCEPTED_NOT_PERSISTED;
    case Scheme::TEL:
      if (!accepted)
        return ProtocolAction::TEL_REJECTED;
      if (persisted)
        return ProtocolAction::TEL_ACCEPTED_PERSISTED;
      return ProtocolAction::TEL_ACCEPTED_NOT_PERSISTED;
    case Scheme::TELNET:
      if (!accepted)
        return ProtocolAction::TELNET_REJECTED;
      if (persisted)
        return ProtocolAction::TELNET_ACCEPTED_PERSISTED;
      return ProtocolAction::TELNET_ACCEPTED_NOT_PERSISTED;
    case Scheme::WEBCAL:
      if (!accepted)
        return ProtocolAction::WEBCAL_REJECTED;
      if (persisted)
        return ProtocolAction::WEBCAL_ACCEPTED_PERSISTED;
      return ProtocolAction::WEBCAL_ACCEPTED_NOT_PERSISTED;
  }
  NOTREACHED();
  return ProtocolAction::OTHER_REJECTED;
}

}  // namespace arc
