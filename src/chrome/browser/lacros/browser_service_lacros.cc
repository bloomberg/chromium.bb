// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/browser_service_lacros.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/statistics_recorder.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#include "chrome/browser/lacros/feedback_util.h"
#include "chrome/browser/lacros/system_logs/lacros_system_log_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
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

// Finds any (Lacros) Browser which has a tab matching a given URL
// without ref (e.g. chrome://flags == chrome://flags/#).
// If such a tab exists, it gets activated, and true gets returned.
bool ActivateTabMatchingURLWithoutRef(Profile* profile, const GURL& url) {
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    if (browser->profile() == profile) {
      TabStripModel* tab_strip = browser->tab_strip_model();
      for (int i = 0; i < tab_strip->count(); ++i) {
        if (tab_strip->ContainsIndex(i) && !tab_strip->IsTabBlocked(i)) {
          content::WebContents* content = tab_strip->GetWebContentsAt(i);
          if (content->GetVisibleURL().EqualsIgnoringRef(url)) {
            browser->window()->Activate();
            tab_strip->ActivateTabAt(i);
            return true;
          }
        }
      }
    }
  }
  return false;
}

}  // namespace

BrowserServiceLacros::BrowserServiceLacros() {
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
                                     NewWindowCallback callback) {
  // TODO(crbug.com/1102815): Find what profile should be used.
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  DCHECK(profile) << "No last used profile is found.";
  chrome::NewEmptyWindow(
      incognito ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
                : profile);
  std::move(callback).Run();
}

void BrowserServiceLacros::NewFullscreenWindow(
    const GURL& url,
    NewFullscreenWindowCallback callback) {
  // Get the current user profile. Report an error to ash if it doesn't exist.
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
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

  // TODO(crbug/1247638): we'd better figure out a better solution to move this
  // special logic for web Kiosk out of this method.
  if (chromeos::LacrosService::Get()->init_params()->session_type ==
      crosapi::mojom::SessionType::kWebKioskSession) {
    KioskSessionServiceLacros::Get()->InitWebKioskSession(browser);
  }

  // Report a success result to ash. Please note that showing Lacros window is
  // asynchronous. Ash-chrome should use the `exo::WMHelper` class rather than
  // this callback method call to track window creation status.
  std::move(callback).Run(crosapi::mojom::CreationResult::kSuccess);
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
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  if (!profile) {
    LOG(ERROR) << "No last used profile is found.";
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

void BrowserServiceLacros::NewTab(NewTabCallback callback) {
  // TODO(crbug.com/1102815): Find what profile should be used.
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  DCHECK(profile) << "No last used profile is found.";
  Browser* browser = chrome::FindBrowserWithProfile(profile);
  DCHECK(browser) << "No browser is found.";
  chrome::NewTab(browser);
  std::move(callback).Run();
}

void BrowserServiceLacros::OpenUrl(const GURL& url, OpenUrlCallback callback) {
  // TODO(crbug.com/1102815): Find what profile should be used.
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  DCHECK(profile) << "No last used profile is found.";

  // Try to re-activate an existing tab for a few specified URLs.
  if (url.SchemeIs(content::kChromeUIScheme) &&
      (url.host() == chrome::kChromeUIFlagsHost ||
       url.host() == chrome::kChromeUIVersionHost ||
       url.host() == chrome::kChromeUIComponentsHost)) {
    if (ActivateTabMatchingURLWithoutRef(profile, url)) {
      std::move(callback).Run();
      return;
    }
  }

  Browser* browser = chrome::FindBrowserWithProfile(profile);
  DCHECK(browser) << "No browser is found.";
  NavigateParams navigate_params(
      browser, url,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_API));
  navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&navigate_params);
  std::move(callback).Run();
}

void BrowserServiceLacros::RestoreTab(RestoreTabCallback callback) {
  // TODO(crbug.com/1102815): Find what profile should be used.
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  DCHECK(profile) << "No last used profile is found.";
  Browser* browser = chrome::FindBrowserWithProfile(profile);
  DCHECK(browser) << "No browser is found.";
  chrome::RestoreTab(browser);
  std::move(callback).Run();
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

void BrowserServiceLacros::OnBrowserAdded(Browser* broser) {
  // Note: this happens only when ash-chrome is too old.
  // Please see the comment in the ctor for the detail.
  BrowserList::RemoveObserver(this);
  keep_alive_.reset();
}
