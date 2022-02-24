// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/browser_service_lacros.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/statistics_recorder.h"
#include "base/ranges/algorithm.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#include "chrome/browser/lacros/feedback_util.h"
#include "chrome/browser/lacros/system_logs/lacros_system_log_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/views/tabs/tab_scrubber_chromeos.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/feedback/feedback_common.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_util.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"
#include "url/gurl.h"

namespace {

constexpr char kHistogramsFilename[] = "lacros_histograms.txt";

std::string GetCompressedHistograms() {
  std::string histograms =
      base::StatisticsRecorder::ToJSON(base::JSON_VERBOSITY_LEVEL_FULL);
  std::string compressed_histograms;
  if (feedback_util::ZipString(base::FilePath(kHistogramsFilename),
                               std::move(histograms), &compressed_histograms)) {
    return compressed_histograms;
  } else {
    LOG(ERROR) << "Failed to compress lacros histograms.";
    return std::string();
  }
}

}  // namespace

// A struct to keep the pending OpenUrl task.
struct BrowserServiceLacros::PendingOpenUrl {
  Profile* profile;
  GURL url;
  crosapi::mojom::OpenUrlParamsPtr params;
  OpenUrlCallback callback;
};

BrowserServiceLacros::BrowserServiceLacros() {
  session_restored_subscription_ =
      SessionRestore::RegisterOnSessionRestoredCallback(
          base::BindRepeating(&BrowserServiceLacros::OnSessionRestored,
                              weak_ptr_factory_.GetWeakPtr()));

  auto* lacros_service = chromeos::LacrosService::Get();
  const auto* init_params = lacros_service->init_params();

  if (init_params->initial_keep_alive ==
      crosapi::mojom::BrowserInitParams::InitialKeepAlive::kUnknown) {
    // ash-chrome is too old, so for backward compatibility fallback to the old
    // way, which is "if launched with kDoNotOpenWindow, run the lacro process
    // on background, and reset the state when a Browser instance is created."
    // Thus, if a user creates a browser window then close it, Lacros is
    // terminated, but ash-chrome has responsibility to re-launch it soon.
    if (init_params->initial_browser_action ==
        crosapi::mojom::InitialBrowserAction::kDoNotOpenWindow) {
      keep_alive_ = std::make_unique<ScopedKeepAlive>(
          KeepAliveOrigin::BROWSER_PROCESS_LACROS,
          KeepAliveRestartOption::ENABLED);
      BrowserList::AddObserver(this);
    }
  } else {
    if (init_params->initial_keep_alive ==
        crosapi::mojom::BrowserInitParams::InitialKeepAlive::kEnabled) {
      keep_alive_ = std::make_unique<ScopedKeepAlive>(
          KeepAliveOrigin::BROWSER_PROCESS_LACROS,
          KeepAliveRestartOption::ENABLED);
    }
  }

  if (lacros_service->IsAvailable<crosapi::mojom::BrowserServiceHost>()) {
    lacros_service->GetRemote<crosapi::mojom::BrowserServiceHost>()
        ->AddBrowserService(receiver_.BindNewPipeAndPassRemoteWithVersion());
  }
}

BrowserServiceLacros::~BrowserServiceLacros() {
  BrowserList::RemoveObserver(this);
}

void BrowserServiceLacros::REMOVED_0(REMOVED_0Callback callback) {
  NOTIMPLEMENTED();
}

void BrowserServiceLacros::REMOVED_2(crosapi::mojom::BrowserInitParamsPtr) {
  NOTIMPLEMENTED();
}

void BrowserServiceLacros::NewWindow(bool incognito,
                                     bool should_trigger_session_restore,
                                     NewWindowCallback callback) {
  if (ProfilePicker::ShouldShowAtLaunch() && !incognito) {
    // Profile picker does not support passing through the incognito param. It
    // also does not support passing though the `should_trigger_session_restore`
    // param but that's true very common (left clicking the launcher icon) so
    // we can't skip the picker in this case. The default behavior for the first
    // browser window supports session restore, additional windows are opened
    // blank and thus it works reasonably well for BrowserServiceLacros.
    ProfilePicker::Show(
        ProfilePicker::EntryPoint::kNewSessionOnExistingProcess);
    std::move(callback).Run();
    return;
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // TODO(crbug.com/1102815): Find what profile should be used.
  // Open the last used profile, unless it's a special profile like Guest. This
  // deviates from other entry points here as this is the primary entry point to
  // _launch_ Chrome. As there's no way to open other profiles from within
  // Guest, user could get stuck with Guest forever without this treatment.
  base::FilePath profile_path = profile_manager->GetLastUsedProfileDir();
  if (profile_path == ProfileManager::GetGuestProfilePath() ||
      profile_path == ProfileManager::GetSystemProfilePath()) {
    profile_path = ProfileManager::GetPrimaryUserProfilePath();
  }
  profile_manager->LoadProfileByPath(
      profile_path, /*incognito=*/false,
      base::BindOnce(&ProfileManager::MaybeForceOffTheRecordMode)
          .Then(base::BindOnce(&BrowserServiceLacros::NewWindowWithProfile,
                               weak_ptr_factory_.GetWeakPtr(), incognito,
                               should_trigger_session_restore,
                               std::move(callback))));
}

void BrowserServiceLacros::NewFullscreenWindow(
    const GURL& url,
    NewFullscreenWindowCallback callback) {
  ProfileManager::LoadLastUsedProfileAllowedByPolicy(
      base::BindOnce(&BrowserServiceLacros::NewFullscreenWindowWithProfile,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

void BrowserServiceLacros::NewGuestWindow(NewGuestWindowCallback callback) {
  if (profiles::IsGuestModeEnabled())
    profiles::SwitchToGuestProfile();

  std::move(callback).Run();
}

void BrowserServiceLacros::NewWindowForDetachingTab(
    const std::u16string& tab_id,
    const std::u16string& group_id,
    NewWindowForDetachingTabCallback callback) {
  // TODO(https://crbug.com/1102815, https://crbug.com/1259946): Find what
  // profile should be used.
  //
  // Alternative: Pass specific stable identifier over crosapi/wayland
  // dnd so that we can re-extract here.
  // Alternative2: look up for Profile instance that tab/group_id belongs to.
  //
  // Note: with multi-users scenario, an user can detach a tab from a window
  // that belongs to user B, then tries to drop it on a window that belongs
  // to profile A, and this should be rejected.
  ProfileManager::LoadLastUsedProfileAllowedByPolicy(base::BindOnce(
      &BrowserServiceLacros::NewWindowForDetachingTabWithProfile,
      weak_ptr_factory_.GetWeakPtr(), tab_id, group_id, std::move(callback)));
}

void BrowserServiceLacros::NewTab(NewTabCallback callback) {
  // TODO(crbug.com/1102815): Find what profile should be used.
  ProfileManager::LoadLastUsedProfileAllowedByPolicy(
      base::BindOnce(&BrowserServiceLacros::NewTabWithProfile,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BrowserServiceLacros::OpenUrl(const GURL& url,
                                   crosapi::mojom::OpenUrlParamsPtr params,
                                   OpenUrlCallback callback) {
  // TODO(crbug.com/1102815): Find what profile should be used.
  ProfileManager::LoadLastUsedProfileAllowedByPolicy(base::BindOnce(
      &BrowserServiceLacros::OpenUrlWithProfile, weak_ptr_factory_.GetWeakPtr(),
      url, std::move(params), std::move(callback)));
}

void BrowserServiceLacros::RestoreTab(RestoreTabCallback callback) {
  // TODO(crbug.com/1102815): Find what profile should be used.
  ProfileManager::LoadLastUsedProfileAllowedByPolicy(
      base::BindOnce(&BrowserServiceLacros::RestoreTabWithProfile,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BrowserServiceLacros::HandleTabScrubbing(float x_offset) {
  TabScrubberChromeOS::GetInstance()->SynthesizedScrollEvent(x_offset);
}

void BrowserServiceLacros::GetFeedbackData(GetFeedbackDataCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Self-deleting object.
  system_logs::SystemLogsFetcher* fetcher =
      system_logs::BuildLacrosSystemLogsFetcher(/*scrub_data=*/true);
  fetcher->Fetch(base::BindOnce(&BrowserServiceLacros::OnSystemInformationReady,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(callback)));
}

void BrowserServiceLacros::GetHistograms(GetHistogramsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // GetCompressedHistograms calls functions marking as blocking, so it
  // can not be running on UI thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(GetCompressedHistograms),
      base::BindOnce(&BrowserServiceLacros::OnGetCompressedHistograms,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BrowserServiceLacros::GetActiveTabUrl(GetActiveTabUrlCallback callback) {
  Browser* browser = chrome::FindBrowserWithActiveWindow();
  GURL page_url;
  if (browser) {
    page_url = chrome::GetTargetTabUrl(
        browser->session_id(), browser->tab_strip_model()->active_index());
  }
  std::move(callback).Run(page_url);
}

void BrowserServiceLacros::UpdateDeviceAccountPolicy(
    const std::vector<uint8_t>& policy) {
  chromeos::LacrosService::Get()->NotifyPolicyUpdated(policy);
}

void BrowserServiceLacros::UpdateKeepAlive(bool enabled) {
  if (enabled == static_cast<bool>(keep_alive_))
    return;

  if (enabled) {
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::BROWSER_PROCESS_LACROS,
        KeepAliveRestartOption::ENABLED);
  } else {
    keep_alive_.reset();
  }
}

void BrowserServiceLacros::OnSystemInformationReady(
    GetFeedbackDataCallback callback,
    std::unique_ptr<system_logs::SystemLogsResponse> sys_info) {
  base::Value system_log_entries(base::Value::Type::DICTIONARY);
  if (sys_info) {
    std::string user_email = feedback_util::GetSignedInUserEmail();
    const bool google_email = gaia::IsGoogleInternalAccountEmail(user_email);

    for (auto& it : *sys_info) {
      // We only send the list of all the crash report IDs if the user has a
      // @google.com email. We strip this here so that the system information
      // view properly reflects what we will be uploading to the server. It is
      // also stripped later on in the feedback processing for other code paths
      // that don't go through this.
      if (FeedbackCommon::IncludeInSystemLogs(it.first, google_email)) {
        system_log_entries.SetStringKey(std::move(it.first),
                                        std::move(it.second));
      }
    }
  }

  DCHECK(!callback.is_null());
  std::move(callback).Run(std::move(system_log_entries));
}

void BrowserServiceLacros::OnGetCompressedHistograms(
    GetHistogramsCallback callback,
    const std::string& compressed_histograms) {
  DCHECK(!callback.is_null());
  std::move(callback).Run(compressed_histograms);
}

void BrowserServiceLacros::OnSessionRestored(Profile* profile,
                                             int num_tabs_restored) {
  if (pending_open_urls_.empty())
    return;
  // Extract pending OpenUrl tasks for the restored |profile|.
  std::vector<PendingOpenUrl> pendings;
  for (auto& pending : pending_open_urls_) {
    if (pending.profile == profile) {
      pendings.push_back(std::move(pending));
      pending.profile = nullptr;  // Mark as moved.
    }
  }
  // Remove marked entries.
  pending_open_urls_.erase(base::ranges::remove(pending_open_urls_, nullptr,
                                                [](PendingOpenUrl& pending) {
                                                  return pending.profile;
                                                }),
                           pending_open_urls_.end());

  // Then, run for each.
  for (auto& pending : pendings)
    OpenUrlImpl(pending.profile, pending.url, std::move(pending.params),
                std::move(pending.callback));
}

void BrowserServiceLacros::OpenUrlImpl(Profile* profile,
                                       const GURL& url,
                                       crosapi::mojom::OpenUrlParamsPtr params,
                                       OpenUrlCallback callback) {
  NavigateParams navigate_params(
      profile, url,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_API));

  using OpenUrlParams = crosapi::mojom::OpenUrlParams;

  // Set up the window disposition.
  auto mojo_disposition =
      params ? params->disposition
             : OpenUrlParams::WindowOpenDisposition::kLegacyAutoDetection;
  switch (mojo_disposition) {
    // This is to support M99 or earlier ash-chrome behavior.
    // We can drop this when we deprecate to support it.
    case OpenUrlParams::WindowOpenDisposition::kLegacyAutoDetection:
      if (url.SchemeIs(content::kChromeUIScheme) &&
          (url.host() == chrome::kChromeUIFlagsHost ||
           url.host() == chrome::kChromeUIVersionHost ||
           url.host() == chrome::kChromeUIAboutHost ||
           url.host() == chrome::kChromeUIComponentsHost)) {
        // Try to re-activate an existing tab for a few specified URLs.
        navigate_params.disposition = WindowOpenDisposition::SWITCH_TO_TAB;
      } else {
        navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
      }
      break;
    case OpenUrlParams::WindowOpenDisposition::kNewForegroundTab:
      navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
      break;
    case OpenUrlParams::WindowOpenDisposition::kSwitchToTab:
      navigate_params.disposition = WindowOpenDisposition::SWITCH_TO_TAB;
      break;
  }

  // Ensure the browser window is showing when the URL is opened. This avoids
  // the user being unaware a new tab with `url` has been opened (if the window
  // was minimized for example).
  navigate_params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&navigate_params);

  auto* tab = navigate_params.navigated_or_inserted_contents;
  if (tab && params->from == crosapi::mojom::OpenUrlFrom::kArc) {
    // Add a flag to remember this tab originated in the ARC context.
    tab->SetUserData(&arc::ArcWebContentsData::kArcTransitionFlag,
                     std::make_unique<arc::ArcWebContentsData>(tab));
  }

  std::move(callback).Run();
}

void BrowserServiceLacros::NewWindowWithProfile(
    bool incognito,
    bool should_trigger_session_restore,
    NewWindowCallback callback,
    Profile* profile) {
  DCHECK(profile) << "No profile is found.";
  chrome::NewEmptyWindow(
      incognito ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
                : profile,
      should_trigger_session_restore);
  std::move(callback).Run();
}

void BrowserServiceLacros::NewFullscreenWindowWithProfile(
    const GURL& url,
    NewFullscreenWindowCallback callback,
    Profile* profile) {
  if (!profile) {
    std::move(callback).Run(crosapi::mojom::CreationResult::kProfileNotExist);
    return;
  }

  // Launch a fullscreen window with the user profile, and navigate to the
  // target URL.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "app_name", true, gfx::Rect(), profile, false);
  params.initial_show_state = ui::SHOW_STATE_FULLSCREEN;
  Browser* browser = Browser::Create(params);
  NavigateParams nav_params(browser, url,
                            ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
  Navigate(&nav_params);

  // Verify the creation result of browser window.
  if (!browser || !browser->window()) {
    std::move(callback).Run(
        crosapi::mojom::CreationResult::kBrowserWindowUnavailable);
    return;
  }

  browser->window()->Show();

  if (chromeos::LacrosService::Get()->init_params()->session_type ==
      crosapi::mojom::SessionType::kWebKioskSession) {
    KioskSessionServiceLacros::Get()->InitWebKioskSession(browser);
  }

  // Report a success result to ash. Please note that showing Lacros window is
  // asynchronous. Ash-chrome should use the `exo::WMHelper` class rather than
  // this callback method call to track window creation status.
  std::move(callback).Run(crosapi::mojom::CreationResult::kSuccess);
}

void BrowserServiceLacros::NewWindowForDetachingTabWithProfile(
    const std::u16string& tab_id,
    const std::u16string& group_id,
    NewWindowForDetachingTabCallback callback,
    Profile* profile) {
  if (!profile) {
    LOG(ERROR) << "No profile is found.";
    std::move(callback).Run(crosapi::mojom::CreationResult::kUnknown,
                            std::string());
  }

  Browser* browser = chrome::FindBrowserWithProfile(profile);
  if (!browser) {
    LOG(ERROR) << "No browser is found.";
    std::move(callback).Run(crosapi::mojom::CreationResult::kUnknown,
                            std::string());
  }

  Browser::CreateParams params = browser->create_params();
  params.user_gesture = true;
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;
  Browser* new_browser = Browser::Create(params);
  CHECK(new_browser);

  if (!tab_strip_ui::DropTabsInNewBrowser(new_browser, tab_id, group_id)) {
    new_browser->window()->Close();
    // TODO(tonikitoo): Return a more specific error status, in case anything
    // goes wrong.
    std::move(callback).Run(crosapi::mojom::CreationResult::kUnknown,
                            std::string());
    return;
  }

  new_browser->window()->Show();

  auto* native_window = new_browser->window()->GetNativeWindow();
  auto* dwth_linux =
      views::DesktopWindowTreeHostLinux::From(native_window->GetHost());
  auto* platform_window = dwth_linux->platform_window();
  std::move(callback).Run(crosapi::mojom::CreationResult::kSuccess,
                          platform_window->GetWindowUniqueId());
}

void BrowserServiceLacros::NewTabWithProfile(NewTabCallback callback,
                                             Profile* profile) {
  DCHECK(profile) << "No profile is found.";
  Browser* browser = chrome::FindBrowserWithProfile(profile);
  DCHECK(browser) << "No browser is found.";
  chrome::NewTab(browser);
  std::move(callback).Run();
}

void BrowserServiceLacros::OpenUrlWithProfile(
    const GURL& url,
    crosapi::mojom::OpenUrlParamsPtr params,
    OpenUrlCallback callback,
    Profile* profile) {
  DCHECK(profile) << "No profile is found.";

  // If there is on-going session restoring task, wait for its completion.
  if (SessionRestore::IsRestoring(profile)) {
    pending_open_urls_.push_back(
        PendingOpenUrl{profile, url, std::move(params), std::move(callback)});
    return;
  }

  // If there's no available browsers, but there's a session to be restored,
  // trigger it, and wait for its completion.
  SessionService* session_service =
      SessionServiceFactory::GetForProfileForSessionRestore(profile);
  if (!chrome::FindBrowserWithProfile(profile) && session_service &&
      session_service->ShouldRestore(nullptr)) {
    pending_open_urls_.push_back(
        PendingOpenUrl{profile, url, std::move(params), std::move(callback)});
    session_service->RestoreIfNecessary(StartupTabs(),
                                        /* restore apps */ false);
    return;
  }

  // Otherwise, directly try to open the URL.
  OpenUrlImpl(profile, url, std::move(params), std::move(callback));
}

void BrowserServiceLacros::RestoreTabWithProfile(RestoreTabCallback callback,
                                                 Profile* profile) {
  DCHECK(profile) << "No profile is found.";
  Browser* browser = chrome::FindBrowserWithProfile(profile);
  DCHECK(browser) << "No browser is found.";
  chrome::RestoreTab(browser);
  std::move(callback).Run();
}

void BrowserServiceLacros::OnBrowserAdded(Browser* browser) {
  // Note: this happens only when ash-chrome is too old.
  // Please see the comment in the ctor for the detail.
  BrowserList::RemoveObserver(this);
  keep_alive_.reset();
}
