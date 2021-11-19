// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_new_window_client.h"

#include <string>
#include <utility>
#include <vector>

#include "apps/launcher.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/keyboard_shortcut_viewer.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/arc_web_contents_data.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/ash/arc/intent_helper/custom_tab_session_impl.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/url_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/web_applications/calculator_app/calculator_app_utils.h"
#include "chrome/browser/ash/web_applications/camera_app/chrome_camera_app_ui_delegate.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/shelf/app_window_base.h"
#include "chrome/browser/ui/ash/shelf/app_window_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_util.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/arc/arc_util.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/custom_tab.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "components/url_formatter/url_fixer.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "third_party/blink/public/mojom/navigation/was_activated_option.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/url_constants.h"

using arc::mojom::ChromePage;

namespace {

constexpr std::pair<arc::mojom::ChromePage, const char*> kOSSettingsMapping[] =
    {{ChromePage::ACCOUNTS,
      chromeos::settings::mojom::kManageOtherPeopleSubpagePath},
     {ChromePage::BLUETOOTH,
      chromeos::settings::mojom::kBluetoothDevicesSubpagePath},
     {ChromePage::BLUETOOTHDEVICES,
      chromeos::settings::mojom::kBluetoothDevicesSubpagePath},
     {ChromePage::CHANGEPICTURE,
      chromeos::settings::mojom::kChangePictureSubpagePath},
     {ChromePage::CUPSPRINTERS,
      chromeos::settings::mojom::kPrintingDetailsSubpagePath},
     {ChromePage::DATETIME, chromeos::settings::mojom::kDateAndTimeSectionPath},
     {ChromePage::DISPLAY, chromeos::settings::mojom::kDisplaySubpagePath},
     {ChromePage::HELP, chromeos::settings::mojom::kAboutChromeOsSectionPath},
     {ChromePage::KEYBOARDOVERLAY,
      chromeos::settings::mojom::kKeyboardSubpagePath},
     {ChromePage::OSLANGUAGESINPUT,
      chromeos::settings::mojom::kInputSubpagePath},
     {ChromePage::OSLANGUAGESLANGUAGES,
      chromeos::settings::mojom::kLanguagesSubpagePath},
     {ChromePage::LOCKSCREEN,
      chromeos::settings::mojom::kSecurityAndSignInSubpagePath},
     {ChromePage::MAIN, ""},
     {ChromePage::MANAGEACCESSIBILITY,
      chromeos::settings::mojom::kManageAccessibilitySubpagePath},
     {ChromePage::MANAGEACCESSIBILITYTTS,
      chromeos::settings::mojom::kTextToSpeechSubpagePath},
     {ChromePage::MULTIDEVICE,
      chromeos::settings::mojom::kMultiDeviceSectionPath},
     {ChromePage::NETWORKSTYPEVPN,
      chromeos::settings::mojom::kVpnDetailsSubpagePath},
     {ChromePage::POINTEROVERLAY,
      chromeos::settings::mojom::kPointersSubpagePath},
     {ChromePage::POWER, chromeos::settings::mojom::kPowerSubpagePath},
     {ChromePage::SMARTPRIVACY,
      chromeos::settings::mojom::kSmartPrivacySubpagePath},
     {ChromePage::STORAGE, chromeos::settings::mojom::kStorageSubpagePath},
     {ChromePage::WIFI, chromeos::settings::mojom::kWifiNetworksSubpagePath}};

constexpr std::pair<arc::mojom::ChromePage, const char*>
    kBrowserSettingsMapping[] = {
        {ChromePage::APPEARANCE, chrome::kAppearanceSubPage},
        {ChromePage::AUTOFILL, chrome::kAutofillSubPage},
        {ChromePage::CLEARBROWSERDATA, chrome::kClearBrowserDataSubPage},
        {ChromePage::CLOUDPRINTERS, chrome::kCloudPrintersSubPage},
        {ChromePage::DOWNLOADS, chrome::kDownloadsSubPage},
        {ChromePage::ONSTARTUP, chrome::kOnStartupSubPage},
        {ChromePage::PASSWORDS, chrome::kPasswordManagerSubPage},
        {ChromePage::PRIVACY, chrome::kPrivacySubPage},
        {ChromePage::RESET, chrome::kResetSubPage},
        {ChromePage::SEARCH, chrome::kSearchSubPage},
        {ChromePage::SYNCSETUP, chrome::kSyncSetupSubPage},
        {ChromePage::LANGUAGES, chrome::kLanguagesSubPage},
};

constexpr std::pair<arc::mojom::ChromePage, const char*> kAboutPagesMapping[] =
    {{ChromePage::ABOUTBLANK, url::kAboutBlankURL},
     {ChromePage::ABOUTDOWNLOADS, "chrome://downloads/"},
     {ChromePage::ABOUTHISTORY, "chrome://history/"}};

void RestoreTabUsingProfile(Profile* profile) {
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile);
  service->RestoreMostRecentEntry(nullptr);
}

bool IsIncognitoAllowed() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return profile && !profile->IsGuestSession() &&
         IncognitoModePrefs::GetAvailability(profile->GetPrefs()) !=
             IncognitoModePrefs::Availability::kDisabled;
}

// Converts the given ARC URL to an external file URL to read it via ARC content
// file system when necessary. Otherwise, returns the given URL unchanged.
GURL ConvertArcUrlToExternalFileUrlIfNeeded(const GURL& url) {
  if (url.SchemeIs(url::kFileScheme) || url.SchemeIs(url::kContentScheme)) {
    // Chrome cannot open this URL. Read the contents via ARC content file
    // system with an external file URL.
    return arc::ArcUrlToExternalFileUrl(url);
  }
  return url;
}

// Returns URL path and query without the "/" prefix. For example, for the URL
// "chrome://settings/networks/?type=WiFi" returns "networks/?type=WiFi".
std::string GetPathAndQuery(const GURL& url) {
  std::string result = url.path();
  if (!result.empty() && result[0] == '/')
    result.erase(0, 1);
  if (url.has_query()) {
    result += '?';
    result += url.query();
  }
  return result;
}

// Remove directory components from |file_name|. E.g.,
// StripPathComponents("../directory/file.jpg") returns "file.jpg".
std::string StripPathComponents(const std::string& file_name) {
  return base::FilePath(file_name).BaseName().AsUTF8Unsafe();
}

apps::mojom::IntentPtr ConvertLaunchIntent(
    const arc::mojom::LaunchIntentPtr& launch_intent) {
  apps::mojom::IntentPtr intent = apps::mojom::Intent::New();

  const char* action =
      apps_util::ConvertArcToAppServiceIntentAction(launch_intent->action);
  if (action)
    intent->action = action;

  intent->url = launch_intent->data;
  intent->mime_type = launch_intent->type;
  intent->share_title = launch_intent->extra_subject;
  intent->share_text = launch_intent->extra_text;

  if (launch_intent->files.has_value() && launch_intent->files->size() > 0) {
    std::vector<std::string> mime_types;
    intent->files = std::vector<apps::mojom::IntentFilePtr>();
    for (const auto& file_info : *launch_intent->files) {
      auto file = apps::mojom::IntentFile::New();

      file->url = arc::ArcUrlToExternalFileUrl(file_info->content_uri);
      file->mime_type = file_info->type;
      file->file_name = StripPathComponents(file_info->name);
      file->file_size = file_info->size;
      intent->files->push_back(std::move(file));
      mime_types.push_back(file_info->type);
    }

    // Override the given MIME type based on the files that we're sharing.
    intent->mime_type = apps_util::CalculateCommonMimeType(mime_types);
  }

  return intent;
}

chrome::FeedbackSource MapToChromeSource(
    ash::NewWindowDelegate::FeedbackSource source) {
  switch (source) {
    case ash::NewWindowDelegate::FeedbackSource::kFeedbackSourceAsh:
      return chrome::FeedbackSource::kFeedbackSourceAsh;
    case ash::NewWindowDelegate::FeedbackSource::kFeedbackSourceAssistant:
      return chrome::FeedbackSource::kFeedbackSourceAssistant;
    case ash::NewWindowDelegate::FeedbackSource::kFeedbackSourceQuickAnswers:
      return chrome::FeedbackSource::kFeedbackSourceQuickAnswers;
  }
}

// When the Files SWA is enabled: Open Files SWA.
// Returns true if it opens the SWA.
// `target_directory` is optional, if provided it opens the Files SWA in the
// given directory, instead of the default directory.
bool OpenFilesSwa(Profile* const profile,
                  base::FilePath target_directory = {}) {
  if (!ash::features::IsFileManagerSwaEnabled()) {
    return false;
  }

  GURL directory_url;
  if (!target_directory.empty() &&
      !file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, target_directory, file_manager::util::GetFileManagerURL(),
          &directory_url)) {
    LOG(WARNING) << "Failed to convert the path to FileSystemURL: "
                 << target_directory << " using the default directory";
  }

  std::u16string title;
  GURL files_swa_url =
      ::file_manager::util::GetFileManagerMainPageUrlWithParams(
          ui::SelectFileDialog::SELECT_NONE, title,
          /*current_directory_url=*/directory_url,
          /*selection_url=*/{},
          /*target_name=*/{},
          /*file_types=*/nullptr,
          /*file_type_index=*/0,
          /*search_query=*/{},
          /*show_android_picker_apps=*/false);

  web_app::SystemAppLaunchParams params;
  params.url = files_swa_url;
  web_app::LaunchSystemWebAppAsync(
      profile, web_app::SystemAppType::FILE_MANAGER, params);
  return true;
}

}  // namespace

ChromeNewWindowClient::ChromeNewWindowClient()
    : os_settings_pages_(std::cbegin(kOSSettingsMapping),
                         std::cend(kOSSettingsMapping)),
      browser_settings_pages_(std::cbegin(kBrowserSettingsMapping),
                              std::cend(kBrowserSettingsMapping)),
      about_pages_(std::cbegin(kAboutPagesMapping),
                   std::cend(kAboutPagesMapping)) {
  arc::ArcIntentHelperBridge::SetOpenUrlDelegate(this);
  arc::ArcIntentHelperBridge::SetControlCameraAppDelegate(this);
}

ChromeNewWindowClient::~ChromeNewWindowClient() {
  arc::ArcIntentHelperBridge::SetControlCameraAppDelegate(nullptr);
  arc::ArcIntentHelperBridge::SetOpenUrlDelegate(nullptr);
}

// static
ChromeNewWindowClient* ChromeNewWindowClient::Get() {
  return static_cast<ChromeNewWindowClient*>(
      ash::NewWindowDelegate::GetInstance());
}

// TabRestoreHelper is used to restore a tab. In particular when the user
// attempts to a restore a tab if the TabRestoreService hasn't finished loading
// this waits for it. Once the TabRestoreService finishes loading the tab is
// restored.
class ChromeNewWindowClient::TabRestoreHelper
    : public sessions::TabRestoreServiceObserver {
 public:
  TabRestoreHelper(ChromeNewWindowClient* delegate,
                   Profile* profile,
                   sessions::TabRestoreService* service)
      : delegate_(delegate), profile_(profile), tab_restore_service_(service) {
    tab_restore_service_->AddObserver(this);
  }

  TabRestoreHelper(const TabRestoreHelper&) = delete;
  TabRestoreHelper& operator=(const TabRestoreHelper&) = delete;

  ~TabRestoreHelper() override { tab_restore_service_->RemoveObserver(this); }

  sessions::TabRestoreService* tab_restore_service() {
    return tab_restore_service_;
  }

  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override {
  }

  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override {
    // This destroys us.
    delegate_->tab_restore_helper_.reset();
  }

  void TabRestoreServiceLoaded(sessions::TabRestoreService* service) override {
    RestoreTabUsingProfile(profile_);
    // This destroys us.
    delegate_->tab_restore_helper_.reset();
  }

 private:
  ChromeNewWindowClient* delegate_;
  Profile* profile_;
  sessions::TabRestoreService* tab_restore_service_;
};

void ChromeNewWindowClient::NewTab() {
  Browser* browser = chrome::FindBrowserWithActiveWindow();
  if (browser && browser->is_type_normal()) {
    chrome::NewTab(browser);
    return;
  }

  // Display a browser, setting the focus to the location bar after it is shown.
  {
    chrome::ScopedTabbedBrowserDisplayer displayer(
        ProfileManager::GetActiveUserProfile());
    browser = displayer.browser();
    chrome::NewTab(browser);
  }

  browser->SetFocusToLocationBar();
}

void ChromeNewWindowClient::NewWindow(bool is_incognito,
                                      bool should_trigger_session_restore) {
  if (is_incognito && !IsIncognitoAllowed())
    return;

  Browser* browser = chrome::FindBrowserWithActiveWindow();
  Profile* profile = (browser && browser->profile())
                         ? browser->profile()->GetOriginalProfile()
                         : ProfileManager::GetActiveUserProfile();
  chrome::NewEmptyWindow(
      is_incognito ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
                   : profile,
      should_trigger_session_restore);
}

void ChromeNewWindowClient::NewWindowForDetachingTab(
    aura::Window* source_window,
    const ui::OSExchangeData& drop_data,
    NewWindowForDetachingTabCallback closure) {
  DCHECK(ash::features::IsWebUITabStripTabDragIntegrationEnabled());

  BrowserView* source_view = BrowserView::GetBrowserViewForNativeWindow(
      source_window->GetToplevelWindow());
  if (!source_view) {
    std::move(closure).Run(/*new_window=*/nullptr);
    return;
  }

  Browser::CreateParams params = source_view->browser()->create_params();
  params.user_gesture = true;
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;
  Browser* browser = Browser::Create(params);
  if (!browser) {
    std::move(closure).Run(/*new_window=*/nullptr);
    return;
  }

  if (!tab_strip_ui::DropTabsInNewBrowser(browser, drop_data)) {
    browser->window()->Close();
    std::move(closure).Run(/*new_window=*/nullptr);
    return;
  }

  // TODO(https://crbug.com/1069869): evaluate whether the above
  // failures can happen in valid states, and if so whether we need to
  // reflect failure in UX.

  // TODO(crbug.com/1225667): Loosen restriction for SplitViewController to be
  // able to snap a window without calling Show(). It will simplify the logic
  // without having to set and clear ash::kIsDraggingTabsKey by calling Show()
  // after snapping the window to the right place.

  // We need to mark the newly created window with |ash::kIsDraggingTabsKey|
  // and clear it afterwards in order to prevent
  // SplitViewController::AutoSnapController from snapping it on Show().
  aura::Window* window = browser->window()->GetNativeWindow();
  window->SetProperty(ash::kIsDraggingTabsKey, true);
  browser->window()->Show();
  window->ClearProperty(ash::kIsDraggingTabsKey);
  std::move(closure).Run(window);
}

void ChromeNewWindowClient::OpenUrl(const GURL& url,
                                    bool from_user_interaction) {
  OpenUrlImpl(url, from_user_interaction);
}

void ChromeNewWindowClient::OpenCalculator() {
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(proxy);
  proxy->Launch(ash::calculator_app::GetInstalledCalculatorAppId(profile),
                ui::EF_NONE, apps::mojom::LaunchSource::kFromKeyboard);
}

void ChromeNewWindowClient::OpenFileManager() {
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  if (OpenFilesSwa(profile)) {
    return;
  }

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(proxy);

  auto launch_files_app = [proxy](const apps::AppUpdate& update) {
    if (update.Readiness() != apps::mojom::Readiness::kReady) {
      LOG(WARNING)
          << "Couldn't launch Files app because it isn't ready, readiness: "
          << update.Readiness();
      return;
    }

    proxy->Launch(
        update.AppId(),
        apps::GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerNone,
                            WindowOpenDisposition::NEW_FOREGROUND_TAB,
                            /*preferred_containner=*/true),
        apps::mojom::LaunchSource::kFromKeyboard);
  };

  bool result = proxy->AppRegistryCache().ForOneApp(
      file_manager::kFileManagerAppId, std::move(launch_files_app));
  DCHECK(result);
}

void ChromeNewWindowClient::OpenDownloadsFolder() {
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  base::FilePath target_directory =
      file_manager::util::GetDownloadsFolderForProfile(profile);
  if (OpenFilesSwa(profile, target_directory)) {
    return;
  }

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  auto downloads_path =
      file_manager::util::GetDownloadsFolderForProfile(profile);
  DCHECK(proxy);

  auto launch_files_app = [proxy,
                           downloads_path](const apps::AppUpdate& update) {
    if (update.Readiness() != apps::mojom::Readiness::kReady) {
      LOG(WARNING)
          << "Couldn't launch Files app because it isn't ready, readiness: "
          << update.Readiness();
      return;
    }

    apps::mojom::FilePathsPtr launch_files = apps::mojom::FilePaths::New();
    launch_files->file_paths.push_back(downloads_path);

    proxy->LaunchAppWithFiles(
        update.AppId(),
        apps::GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerNone,
                            WindowOpenDisposition::NEW_FOREGROUND_TAB,
                            /*preferred_containner=*/true),
        apps::mojom::LaunchSource::kFromKeyboard, std::move(launch_files));
  };

  bool result = proxy->AppRegistryCache().ForOneApp(
      file_manager::kFileManagerAppId, launch_files_app);
  DCHECK(result);
}

void ChromeNewWindowClient::OpenCrosh() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (base::FeatureList::IsEnabled(chromeos::features::kCroshSWA)) {
    web_app::LaunchSystemWebAppAsync(profile, web_app::SystemAppType::CROSH);
  } else {
    chrome::ScopedTabbedBrowserDisplayer displayer(profile);
    Browser* browser = displayer.browser();
    content::WebContents* page = browser->OpenURL(content::OpenURLParams(
        GURL(chrome::kChromeUIUntrustedCroshURL), content::Referrer(),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui::PAGE_TRANSITION_GENERATED, false));
    browser->window()->Show();
    browser->window()->Activate();
    page->Focus();
  }
}

void ChromeNewWindowClient::OpenGetHelp() {
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  chrome::ShowHelpForProfile(profile, chrome::HELP_SOURCE_KEYBOARD);
}

void ChromeNewWindowClient::RestoreTab() {
  if (tab_restore_helper_.get()) {
    DCHECK(!tab_restore_helper_->tab_restore_service()->IsLoaded());
    return;
  }

  Browser* browser = chrome::FindBrowserWithActiveWindow();
  Profile* profile = browser ? browser->profile() : nullptr;
  if (!profile)
    profile = ProfileManager::GetActiveUserProfile();
  if (profile->IsOffTheRecord())
    return;
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile);
  if (!service)
    return;

  if (service->IsLoaded()) {
    RestoreTabUsingProfile(profile);
  } else {
    tab_restore_helper_ =
        std::make_unique<TabRestoreHelper>(this, profile, service);
    service->LoadTabsFromLastSession();
  }
}

void ChromeNewWindowClient::ShowKeyboardShortcutViewer() {
  ash::ToggleKeyboardShortcutViewer();
}

void ChromeNewWindowClient::ShowTaskManager() {
  chrome::OpenTaskManager(nullptr);
}

void ChromeNewWindowClient::OpenDiagnostics() {
  if (base::FeatureList::IsEnabled(chromeos::features::kDiagnosticsApp))
    chrome::ShowDiagnosticsApp(ProfileManager::GetActiveUserProfile());
}

void ChromeNewWindowClient::OpenFeedbackPage(
    FeedbackSource source,
    const std::string& description_template) {
  chrome::OpenFeedbackDialog(chrome::FindBrowserWithActiveWindow(),
                             MapToChromeSource(source), description_template);
}

void ChromeNewWindowClient::OpenUrlFromArc(const GURL& url) {
  if (!url.is_valid())
    return;

  GURL url_to_open = ConvertArcUrlToExternalFileUrlIfNeeded(url);
  content::WebContents* tab =
      OpenUrlImpl(url_to_open, false /* from_user_interaction */);
  if (!tab)
    return;

  // Add a flag to remember this tab originated in the ARC context.
  tab->SetUserData(&arc::ArcWebContentsData::kArcTransitionFlag,
                   std::make_unique<arc::ArcWebContentsData>());

  apps::IntentHandlingMetrics::RecordOpenBrowserMetrics(
      apps::IntentHandlingMetrics::AppType::kArc);
}

void ChromeNewWindowClient::OpenWebAppFromArc(const GURL& url) {
  DCHECK(url.is_valid() && url.SchemeIs(url::kHttpsScheme));

  // Fetch the profile associated with ARC. This method should only be called
  // for a |url| which was installed via ARC, and so we want the web app that is
  // opened through here to be installed in the profile associated with ARC.
  // |user| may be null if sign-in hasn't happened yet
  const auto* user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user)
    return;

  // |profile| may be null if sign-in has happened but the profile isn't loaded
  // yet.
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return;

  absl::optional<web_app::AppId> app_id =
      web_app::FindInstalledAppWithUrlInScope(profile, url,
                                              /*window_only=*/true);

  if (!app_id) {
    OpenUrlFromArc(url);
    return;
  }

  int event_flags = apps::GetEventFlags(
      apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW, /*prefer_container=*/false);
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  proxy->AppRegistryCache().ForOneApp(
      *app_id, [&event_flags](const apps::AppUpdate& update) {
        if (update.WindowMode() == apps::mojom::WindowMode::kBrowser) {
          event_flags = apps::GetEventFlags(
              apps::mojom::LaunchContainer::kLaunchContainerTab,
              WindowOpenDisposition::NEW_FOREGROUND_TAB,
              /*prefer_container=*/false);
        }
      });

  proxy->LaunchAppWithUrl(*app_id, event_flags, url,
                          apps::mojom::LaunchSource::kFromArc);

  ash::ApkWebAppService* apk_web_app_service =
      ash::ApkWebAppService::Get(profile);
  if (!apk_web_app_service ||
      !apk_web_app_service->IsWebAppInstalledFromArc(app_id.value())) {
    return;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile);
  if (!prefs)
    return;

  absl::optional<std::string> package_name =
      apk_web_app_service->GetPackageNameForWebApp(app_id.value());
  if (!package_name.has_value())
    return;

  for (const auto& app_id : prefs->GetAppsForPackage(package_name.value())) {
    proxy->StopApp(app_id);
  }
}

void ChromeNewWindowClient::OpenArcCustomTab(
    const GURL& url,
    int32_t task_id,
    arc::mojom::IntentHelperHost::OnOpenCustomTabCallback callback) {
  GURL url_to_open = ConvertArcUrlToExternalFileUrlIfNeeded(url);
  Profile* profile = ProfileManager::GetActiveUserProfile();

  aura::Window* arc_window = arc::GetArcWindow(task_id);
  if (!arc_window) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  auto custom_tab = std::make_unique<arc::CustomTab>(arc_window);
  auto web_contents = arc::CreateArcCustomTabWebContents(profile, url);

  // |custom_tab_browser| will be destroyed when its tab strip becomes empty,
  // either due to the user opening the custom tab page in a tabbed browser or
  // because of the CustomTabSessionImpl object getting destroyed.
  Browser::CreateParams params(Browser::TYPE_CUSTOM_TAB, profile,
                               /* user_gesture= */ true);
  params.omit_from_session_restore = true;
  auto* custom_tab_browser = Browser::Create(params);

  custom_tab_browser->tab_strip_model()->AppendWebContents(
      std::move(web_contents), /* foreground= */ true);

  // TODO(crbug.com/955171): Remove this temporary conversion to InterfacePtr
  // once OnOpenCustomTab from //components/arc/mojom/intent_helper.mojom could
  // take pending_remote directly. Refer to crrev.com/c/1868870.
  auto custom_tab_remote(
      CustomTabSessionImpl::Create(std::move(custom_tab), custom_tab_browser));
  std::move(callback).Run(std::move(custom_tab_remote));
}

content::WebContents* ChromeNewWindowClient::OpenUrlImpl(
    const GURL& url,
    bool from_user_interaction) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if ((url.SchemeIs(url::kAboutScheme) ||
       url.SchemeIs(content::kChromeUIScheme))) {
    // Show browser settings (e.g. chrome://settings). This may open in a window
    // or a tab depending on feature SplitSettings.
    if (url.host() == chrome::kChromeUISettingsHost) {
      std::string sub_page = GetPathAndQuery(url);
      chrome::ShowSettingsSubPageForProfile(profile, sub_page);
      return nullptr;
    }
    // OS settings are shown in a window.
    if (url.host() == chrome::kChromeUIOSSettingsHost) {
      std::string sub_page = GetPathAndQuery(url);
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile,
                                                                   sub_page);
      return nullptr;
    }
  }

  NavigateParams navigate_params(
      profile, url,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_API));

  if (from_user_interaction)
    navigate_params.was_activated = blink::mojom::WasActivatedOption::kYes;

  Navigate(&navigate_params);

  if (navigate_params.browser) {
    // The browser window might be on another user's desktop, and hence not
    // visible. Ensure the browser becomes visible on this user's desktop.
    multi_user_util::MoveWindowToCurrentDesktop(
        navigate_params.browser->window()->GetNativeWindow());
  }
  return navigate_params.navigated_or_inserted_contents;
}

void ChromeNewWindowClient::OpenChromePageFromArc(ChromePage page) {
  auto it = os_settings_pages_.find(page);
  if (it != os_settings_pages_.end()) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile,
                                                                 it->second);
    return;
  }

  it = browser_settings_pages_.find(page);
  if (it != browser_settings_pages_.end()) {
    OpenUrlFromArc(GURL(chrome::kChromeUISettingsURL).Resolve(it->second));
    return;
  }

  it = about_pages_.find(page);
  if (it != about_pages_.end()) {
    OpenUrlFromArc(GURL(it->second));
    return;
  }

  NOTREACHED();
}

void ChromeNewWindowClient::OpenAppWithIntent(
    const GURL& start_url,
    arc::mojom::LaunchIntentPtr arc_intent) {
  DCHECK(start_url.is_valid());
  DCHECK(start_url.SchemeIs(url::kHttpsScheme));

  // Fetch the profile associated with ARC. This method should only be called
  // for a |url| which was installed via ARC, and so we want the web app that is
  // opened through here to be installed in the profile associated with ARC.
  const auto* user = user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(user);

  // |profile| may be null if sign-in has happened but the profile isn't loaded
  // yet.
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return;

  web_app::AppId app_id =
      web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);

  bool app_installed = false;
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&app_installed](const apps::AppUpdate& update) {
        app_installed = apps_util::IsInstalled(update.Readiness());
      });

  if (!app_installed) {
    if (arc_intent->data)
      OpenUrlFromArc(*arc_intent->data);
    return;
  }

  apps::mojom::IntentPtr intent = ConvertLaunchIntent(arc_intent);

  auto launch_container = apps::mojom::LaunchContainer::kLaunchContainerWindow;
  auto disposition = WindowOpenDisposition::NEW_WINDOW;
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&launch_container, &disposition](const apps::AppUpdate& update) {
        if (update.WindowMode() == apps::mojom::WindowMode::kBrowser) {
          launch_container = apps::mojom::LaunchContainer::kLaunchContainerTab;
          disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
        }
      });

  int event_flags = apps::GetEventFlags(launch_container, disposition,
                                        /*prefer_container=*/false);

  proxy->LaunchAppWithIntent(app_id, event_flags, std::move(intent),
                             apps::mojom::LaunchSource::kFromArc);
}

void ChromeNewWindowClient::LaunchCameraApp(const std::string& queries,
                                            int32_t task_id) {
  DCHECK(IsCameraAppEnabled());
  ChromeCameraAppUIDelegate::CameraAppDialog::ShowIntent(
      queries, arc::GetArcWindow(task_id));
  apps::RecordAppLaunch(extension_misc::kCameraAppId,
                        apps::mojom::LaunchSource::kFromArc);
}

void ChromeNewWindowClient::CloseCameraApp() {
  const ash::ShelfID shelf_id(extension_misc::kCameraAppId);
  AppWindowShelfItemController* const app_controller =
      ChromeShelfController::instance()
          ->shelf_model()
          ->GetAppWindowShelfItemController(shelf_id);
  if (!app_controller)
    return;

  DCHECK_LE(app_controller->window_count(), 1lu);
  if (app_controller->window_count() > 0)
    app_controller->windows().front()->Close();
}

bool ChromeNewWindowClient::IsCameraAppEnabled() {
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  auto* provider = web_app::WebAppProvider::GetForSystemWebApps(profile);
  return provider && provider->system_web_app_manager().IsAppEnabled(
                         web_app::SystemAppType::CAMERA);
}
