// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator.h"

#include <stddef.h>

#include <limits>
#include <set>
#include <string>

#include "apps/switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string16.h"
#include "base/strings/string_tokenizer.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/platform_apps/app_load_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/startup_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/util.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/switches.h"
#include "net/base/port_util.h"
#include "printing/buildflags/buildflags.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/app_mode/app_launch_utils.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "components/user_manager/user_manager.h"
#else
#include "chrome/browser/ui/user_manager.h"
#endif

#if defined(TOOLKIT_VIEWS) && defined(OS_LINUX)
#include "ui/events/devices/x11/touch_factory_x11.h"  // nogncheck
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut_mac.h"
#endif

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/metrics/jumplist_metrics_win.h"
#include "chrome/browser/notifications/notification_platform_bridge_win.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_dialog_cloud.h"
#endif

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;

namespace {

// Keeps track on which profiles have been launched.
class ProfileLaunchObserver : public content::NotificationObserver {
 public:
  ProfileLaunchObserver()
      : profile_to_activate_(NULL),
        activated_profile_(false) {
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
                   content::NotificationService::AllSources());
  }
  ~ProfileLaunchObserver() override {}

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    switch (type) {
      case chrome::NOTIFICATION_PROFILE_DESTROYED: {
        Profile* profile = content::Source<Profile>(source).ptr();
        launched_profiles_.erase(profile);
        opened_profiles_.erase(profile);
        if (profile == profile_to_activate_)
          profile_to_activate_ = NULL;
        // If this profile was the last launched one without an opened window,
        // then we may be ready to activate |profile_to_activate_|.
        MaybeActivateProfile();
        break;
      }
      case chrome::NOTIFICATION_BROWSER_OPENED: {
        Browser* browser = content::Source<Browser>(source).ptr();
        DCHECK(browser);
        opened_profiles_.insert(browser->profile());
        MaybeActivateProfile();
        break;
      }
      default:
        NOTREACHED();
    }
  }

  bool HasBeenLaunched(const Profile* profile) const {
    return launched_profiles_.find(profile) != launched_profiles_.end();
  }

  void AddLaunched(Profile* profile) {
    launched_profiles_.insert(profile);
    if (chrome::FindBrowserWithProfile(profile)) {
      // A browser may get opened before we get initialized (e.g., in tests),
      // so we never see the NOTIFICATION_BROWSER_OPENED for it.
      opened_profiles_.insert(profile);
    }
  }

  void Clear() {
    launched_profiles_.clear();
    opened_profiles_.clear();
  }

  bool activated_profile() { return activated_profile_; }

  void set_profile_to_activate(Profile* profile) {
    profile_to_activate_ = profile;
    MaybeActivateProfile();
  }

 private:
  void MaybeActivateProfile() {
    if (!profile_to_activate_)
      return;
    // Check that browsers have been opened for all the launched profiles.
    // Note that browsers opened for profiles that were not added as launched
    // profiles are simply ignored.
    std::set<const Profile*>::const_iterator i = launched_profiles_.begin();
    for (; i != launched_profiles_.end(); ++i) {
      if (opened_profiles_.find(*i) == opened_profiles_.end())
        return;
    }
    // Asynchronous post to give a chance to the last window to completely
    // open and activate before trying to activate |profile_to_activate_|.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&ProfileLaunchObserver::ActivateProfile,
                       base::Unretained(this)));
    // Avoid posting more than once before ActivateProfile gets called.
    registrar_.Remove(this, chrome::NOTIFICATION_BROWSER_OPENED,
                      content::NotificationService::AllSources());
    registrar_.Remove(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                      content::NotificationService::AllSources());
  }

  void ActivateProfile() {
    // We need to test again, in case the profile got deleted in the mean time.
    if (profile_to_activate_) {
      Browser* browser = chrome::FindBrowserWithProfile(profile_to_activate_);
      // |profile| may never get launched, e.g., if it only had
      // incognito Windows and one of them was used to exit Chrome.
      // So it won't have a browser in that case.
      if (browser)
        browser->window()->Activate();
      // No need try to activate this profile again.
      profile_to_activate_ = NULL;
    }
    // Assign true here, even if no browser was actually activated, so that
    // the test can stop waiting, and fail gracefully when needed.
    activated_profile_ = true;
  }

  // These are the profiles that get launched by
  // StartupBrowserCreator::LaunchBrowser.
  std::set<const Profile*> launched_profiles_;
  // These are the profiles for which at least one browser window has been
  // opened. This is needed to know when it is safe to activate
  // |profile_to_activate_|, otherwise, new browser windows being opened will
  // be activated on top of it.
  std::set<const Profile*> opened_profiles_;
  content::NotificationRegistrar registrar_;
  // This is NULL until the profile to activate has been chosen. This value,
  // should only be set once all profiles have been launched, otherwise,
  // activation may not happen after the launch of newer profiles.
  Profile* profile_to_activate_;
  // Set once we attempted to activate a profile. We only get one shot at this.
  bool activated_profile_;

  DISALLOW_COPY_AND_ASSIGN(ProfileLaunchObserver);
};

base::LazyInstance<ProfileLaunchObserver>::DestructorAtExit
    profile_launch_observer = LAZY_INSTANCE_INITIALIZER;

// Dumps the current set of the browser process's histograms to |output_file|.
// The file is overwritten if it exists. This function should only be called in
// the blocking pool.
void DumpBrowserHistograms(const base::FilePath& output_file) {
  std::string output_string(
      base::StatisticsRecorder::ToJSON(base::JSON_VERBOSITY_LEVEL_FULL));

  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  base::WriteFile(output_file, output_string.data(),
                  static_cast<int>(output_string.size()));
}

// Returns whether |profile| can be opened during Chrome startup without
// explicit user action.
bool CanOpenProfileOnStartup(Profile* profile) {
#if defined(OS_CHROMEOS)
  // On ChromeOS, the user has already chosen and logged into the profile before
  // Chrome starts up.
  return true;
#else
  // Profiles that require signin are not available.
  ProfileAttributesEntry* entry = nullptr;
  if (g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath(), &entry) &&
      entry->IsSigninRequired()) {
    return false;
  }

  // Guest or system profiles are not available unless a separate process
  // already has a window open for the profile.
  return (!profile->IsGuestSession() && !profile->IsSystemProfile()) ||
         (chrome::GetBrowserCount(profile->GetOffTheRecordProfile()) > 0);
#endif
}

void ShowUserManagerOnStartup(const base::CommandLine& command_line) {
#if !defined(OS_CHROMEOS)
  UserManager::Show(base::FilePath(),
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
#endif  // !defined(OS_CHROMEOS)
}

}  // namespace

StartupBrowserCreator::StartupBrowserCreator()
    : is_default_browser_dialog_suppressed_(false),
      show_main_browser_window_(true) {}

StartupBrowserCreator::~StartupBrowserCreator() {}

// static
bool StartupBrowserCreator::was_restarted_read_ = false;

// static
bool StartupBrowserCreator::in_synchronous_profile_launch_ = false;

void StartupBrowserCreator::AddFirstRunTab(const GURL& url) {
  first_run_tabs_.push_back(url);
}

bool StartupBrowserCreator::Start(const base::CommandLine& cmd_line,
                                  const base::FilePath& cur_dir,
                                  Profile* last_used_profile,
                                  const Profiles& last_opened_profiles) {
  TRACE_EVENT0("startup", "StartupBrowserCreator::Start");
  SCOPED_UMA_HISTOGRAM_TIMER("Startup.StartupBrowserCreator_Start");
  return ProcessCmdLineImpl(cmd_line, cur_dir, true, last_used_profile,
                            last_opened_profiles);
}

// static
bool StartupBrowserCreator::InSynchronousProfileLaunch() {
  return in_synchronous_profile_launch_;
}

bool StartupBrowserCreator::LaunchBrowser(
    const base::CommandLine& command_line,
    Profile* profile,
    const base::FilePath& cur_dir,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run) {
  DCHECK(profile);
  in_synchronous_profile_launch_ =
      process_startup == chrome::startup::IS_PROCESS_STARTUP;

  // ChromeOS does a direct browser launch from UserSessionManager, so this is
  // the earliest place we can enable the log.
  if (command_line.HasSwitch(switches::kDnsLogDetails))
    chrome_browser_net::EnablePredictorDetailedLog(true);

  // Continue with the incognito profile from here on if Incognito mode
  // is forced.
  if (IncognitoModePrefs::ShouldLaunchIncognito(command_line,
                                                profile->GetPrefs())) {
    profile = profile->GetOffTheRecordProfile();
  } else if (command_line.HasSwitch(switches::kIncognito)) {
    LOG(WARNING) << "Incognito mode disabled by policy, launching a normal "
                 << "browser session.";
  }

  // Note: This check should have been done in ProcessCmdLineImpl()
  // before calling this function. However chromeos/login/login_utils.cc
  // calls this function directly (see comments there) so it has to be checked
  // again.
  const bool silent_launch = command_line.HasSwitch(switches::kSilentLaunch);

  if (!silent_launch) {
    StartupBrowserCreatorImpl lwp(cur_dir, command_line, this, is_first_run);
    const std::vector<GURL> urls_to_launch =
        GetURLsFromCommandLine(command_line, cur_dir, profile);
    const bool launched =
        lwp.Launch(profile, urls_to_launch, in_synchronous_profile_launch_);
    in_synchronous_profile_launch_ = false;
    if (!launched) {
      LOG(ERROR) << "launch error";
      return false;
    }
  } else {
    in_synchronous_profile_launch_ = false;
  }

  profile_launch_observer.Get().AddLaunched(profile);

  return true;
}

// static
bool StartupBrowserCreator::WasRestarted() {
  // Stores the value of the preference kWasRestarted had when it was read.
  static bool was_restarted = false;

  if (!was_restarted_read_) {
    PrefService* pref_service = g_browser_process->local_state();
    was_restarted = pref_service->GetBoolean(prefs::kWasRestarted);
    pref_service->SetBoolean(prefs::kWasRestarted, false);
    was_restarted_read_ = true;
  }
  return was_restarted;
}

// static
SessionStartupPref StartupBrowserCreator::GetSessionStartupPref(
    const base::CommandLine& command_line,
    Profile* profile) {
  DCHECK(profile);
  PrefService* prefs = profile->GetPrefs();
  SessionStartupPref pref = SessionStartupPref::GetStartupPref(prefs);

  // IsChromeFirstRun() looks for a sentinel file to determine whether the user
  // is starting Chrome for the first time. On Chrome OS, the sentinel is stored
  // in a location shared by all users and the check is meaningless. Query the
  // UserManager instead to determine whether the user is new.
#if defined(OS_CHROMEOS)
  const bool is_first_run =
      user_manager::UserManager::Get()->IsCurrentUserNew();
  // On ChromeOS restarts force the user to login again. The expectation is that
  // after a login the user gets clean state. For this reason we ignore
  // StartupBrowserCreator::WasRestarted(). However
  // StartupBrowserCreator::WasRestarted has to be called in order to correctly
  // update pref values.
  const bool did_restart = false;
  StartupBrowserCreator::WasRestarted();
#else
  const bool is_first_run = first_run::IsChromeFirstRun();
  const bool did_restart = StartupBrowserCreator::WasRestarted();
#endif

  // The pref has an OS-dependent default value. For the first run only, this
  // default is overridden with SessionStartupPref::DEFAULT so that first run
  // behavior (sync promo, welcome page) is consistently invoked.
  // This applies only if the pref is still at its default and has not been
  // set by the user, managed prefs or policy.
  if (is_first_run && SessionStartupPref::TypeIsDefault(prefs))
    pref.type = SessionStartupPref::DEFAULT;

  // The switches::kRestoreLastSession command line switch is used to restore
  // sessions after a browser self restart (e.g. after a Chrome upgrade).
  // However, new profiles can be created from a browser process that has this
  // switch so do not set the session pref to SessionStartupPref::LAST for
  // those as there is nothing to restore.
  if ((command_line.HasSwitch(switches::kRestoreLastSession) || did_restart) &&
      !profile->IsNewProfile()) {
    pref.type = SessionStartupPref::LAST;
  }

  // A browser starting for a profile being unlocked should always restore.
  if (!profile->IsGuestSession()) {
    ProfileAttributesEntry* entry = nullptr;
    bool has_entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile->GetPath(), &entry);

    if (has_entry && entry->IsSigninRequired())
      pref.type = SessionStartupPref::LAST;
  }

  if (pref.type == SessionStartupPref::LAST &&
      IncognitoModePrefs::ShouldLaunchIncognito(command_line, prefs)) {
    // We don't store session information when incognito. If the user has
    // chosen to restore last session and launched incognito, fallback to
    // default launch behavior.
    pref.type = SessionStartupPref::DEFAULT;
  }

  return pref;
}

// static
void StartupBrowserCreator::ClearLaunchedProfilesForTesting() {
  profile_launch_observer.Get().Clear();
}

// static
void StartupBrowserCreator::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
#if defined(OS_WIN)
  registry->RegisterStringPref(prefs::kLastWelcomedOSVersion, std::string());
  registry->RegisterBooleanPref(prefs::kWelcomePageOnOSUpgradeEnabled, true);
  registry->RegisterBooleanPref(prefs::kHasSeenWin10PromoPage, false);
#endif
#if !defined(OS_CHROMEOS)
  registry->RegisterBooleanPref(prefs::kPromotionalTabsEnabled, true);
#endif
  registry->RegisterBooleanPref(prefs::kSuppressUnsupportedOSWarning, false);
  registry->RegisterBooleanPref(prefs::kWasRestarted, false);
}

// static
void StartupBrowserCreator::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // Default to true so that existing users are not shown the Welcome page.
  // ProfileManager handles setting this to false for new profiles upon
  // creation.
  registry->RegisterBooleanPref(prefs::kHasSeenWelcomePage, true);
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  // TODO(scottchen): To make this testable early by trybots, instead of hiding
  // behind GOOGLE_CHROME_BUILD, use a function that returns true for official
  // builds and conditionally returns true based on a command line switch to
  // be set by tests.
  registry->RegisterBooleanPref(prefs::kHasSeenGoogleAppsPromoPage, true);
  registry->RegisterBooleanPref(prefs::kHasSeenEmailPromoPage, true);
  // This  will be set to true for newly created profiles, and is used to
  // indicate which users went through FRE after NUX is enabled.
  registry->RegisterBooleanPref(prefs::kOnboardDuringNUX, false);
#endif  // defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
}

// static
std::vector<GURL> StartupBrowserCreator::GetURLsFromCommandLine(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile) {
  DCHECK(profile);

  std::vector<GURL> urls;

  const base::CommandLine::StringVector& params = command_line.GetArgs();
  for (size_t i = 0; i < params.size(); ++i) {
    base::FilePath param = base::FilePath(params[i]);
    // Handle Vista way of searching - "? <search-term>"
    if ((param.value().size() > 2) && (param.value()[0] == '?') &&
        (param.value()[1] == ' ')) {
      GURL url(GetDefaultSearchURLForSearchTerms(
          TemplateURLServiceFactory::GetForProfile(profile),
          param.LossyDisplayName().substr(2)));
      if (url.is_valid()) {
        urls.push_back(url);
        continue;
      }
    }

    // Otherwise, fall through to treating it as a URL.

    // This will create a file URL or a regular URL.
    // This call can (in rare circumstances) block the UI thread.
    // Allow it until this bug is fixed.
    //  http://code.google.com/p/chromium/issues/detail?id=60641
    GURL url = GURL(param.MaybeAsASCII());

    // http://crbug.com/371030: Only use URLFixerUpper if we don't have a valid
    // URL, otherwise we will look in the current directory for a file named
    // 'about' if the browser was started with a about:foo argument.
    // http://crbug.com/424991: Always use URLFixerUpper on file:// URLs,
    // otherwise we wouldn't correctly handle '#' in a file name.
    if (!url.is_valid() || url.SchemeIsFile()) {
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      url = url_formatter::FixupRelativeFile(cur_dir, param);
    }
    // Exclude dangerous schemes.
    if (!url.is_valid())
      continue;

    const GURL settings_url = GURL(chrome::kChromeUISettingsURL);
    bool url_points_to_an_approved_settings_page = false;
#if defined(OS_CHROMEOS)
    // In ChromeOS, allow any settings page to be specified on the command line.
    url_points_to_an_approved_settings_page =
        url.GetOrigin() == settings_url.GetOrigin();
#else
    // Exposed for external cleaners to offer a settings reset to the
    // user. The allowed URLs must match exactly.
    const GURL reset_settings_url =
        settings_url.Resolve(chrome::kResetProfileSettingsSubPage);
    url_points_to_an_approved_settings_page = url == reset_settings_url;
#if defined(OS_WIN)
    // On Windows, also allow a hash for the Chrome Cleanup Tool.
    const GURL reset_settings_url_with_cct_hash = reset_settings_url.Resolve(
        std::string("#") +
        settings::ResetSettingsHandler::kCctResetSettingsHash);
    url_points_to_an_approved_settings_page =
        url_points_to_an_approved_settings_page ||
        url == reset_settings_url_with_cct_hash;
#endif  // defined(OS_WIN)
#endif  // defined(OS_CHROMEOS)

    ChildProcessSecurityPolicy* policy =
        ChildProcessSecurityPolicy::GetInstance();
    if (policy->IsWebSafeScheme(url.scheme()) ||
        url.SchemeIs(url::kFileScheme) ||
        url_points_to_an_approved_settings_page ||
        (url.spec().compare(url::kAboutBlankURL) == 0)) {
      urls.push_back(url);
    }
  }
  return urls;
}

bool StartupBrowserCreator::ProcessCmdLineImpl(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    bool process_startup,
    Profile* last_used_profile,
    const Profiles& last_opened_profiles) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT0("startup", "StartupBrowserCreator::ProcessCmdLineImpl");

  DCHECK(last_used_profile);
  if (process_startup &&
      command_line.HasSwitch(switches::kDisablePromptOnRepost)) {
    content::NavigationController::DisablePromptOnRepost();
  }

  bool silent_launch = false;
  bool can_use_last_profile =
      (CanOpenProfileOnStartup(last_used_profile) &&
       !IncognitoModePrefs::ShouldLaunchIncognito(
           command_line, last_used_profile->GetPrefs()));

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // If we are just displaying a print dialog we shouldn't open browser
  // windows.
  if (command_line.HasSwitch(switches::kCloudPrintFile) &&
      can_use_last_profile &&
      print_dialog_cloud::CreatePrintDialogFromCommandLine(last_used_profile,
                                                           command_line)) {
    silent_launch = true;
  }
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

  if (command_line.HasSwitch(switches::kExplicitlyAllowedPorts)) {
    std::string allowed_ports =
        command_line.GetSwitchValueASCII(switches::kExplicitlyAllowedPorts);
    net::SetExplicitlyAllowedPorts(allowed_ports);
  }

  if (command_line.HasSwitch(switches::kValidateCrx)) {
    if (!process_startup) {
      LOG(ERROR) << "chrome is already running; you must close all running "
                 << "instances before running with the --"
                 << switches::kValidateCrx << " flag";
      return false;
    }
    extensions::StartupHelper helper;
    std::string message;
    std::string error;
    if (helper.ValidateCrx(command_line, &error))
      message = std::string("ValidateCrx Success");
    else
      message = std::string("ValidateCrx Failure: ") + error;
    printf("%s\n", message.c_str());
    return false;
  }

#if defined(OS_CHROMEOS)

  // The browser will be launched after the user logs in.
  if (command_line.HasSwitch(chromeos::switches::kLoginManager))
    silent_launch = true;

  if (chrome::IsRunningInAppMode() &&
      command_line.HasSwitch(switches::kAppId)) {
    chromeos::LaunchAppOrDie(
        last_used_profile,
        command_line.GetSwitchValueASCII(switches::kAppId));

    // Skip browser launch since app mode launches its app window.
    silent_launch = true;
  }

  // If we are a demo app session and we crashed, there is no safe recovery
  // possible. We should instead cleanly exit and go back to the OOBE screen,
  // where we will launch again after the timeout has expired.
  if (chromeos::DemoAppLauncher::IsDemoAppSession(
          cryptohome::Identification::FromString(
              command_line.GetSwitchValueASCII(chromeos::switches::kLoginUser))
              .GetAccountId())) {
    chrome::AttemptUserExit();
    return false;
  }
#endif  // OS_CHROMEOS

#if defined(TOOLKIT_VIEWS) && defined(USE_X11)
  ui::TouchFactory::SetTouchDeviceListFromCommandLine();
#endif

#if defined(OS_MACOSX)
  if (web_app::MaybeRebuildShortcut(command_line))
    return true;
#endif

  if (!process_startup &&
      command_line.HasSwitch(switches::kDumpBrowserHistograms)) {
    // Only handle --dump-browser-histograms from a rendezvous. In this case, do
    // not open a new browser window even if no output file was given.
    base::FilePath output_file(
        command_line.GetSwitchValuePath(switches::kDumpBrowserHistograms));
    if (!output_file.empty()) {
      base::PostTaskWithTraits(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::BindOnce(&DumpBrowserHistograms, output_file));
    }
    silent_launch = true;
  }

  // If --no-startup-window is specified and Chrome is already running then do
  // not open a new window.
  if (!process_startup && command_line.HasSwitch(switches::kNoStartupWindow))
    silent_launch = true;

  // If we don't want to launch a new browser window or tab we are done here.
  if (silent_launch) {
    if (process_startup)
      startup_metric_utils::SetNonBrowserUIDisplayed();
    return true;
  }

  if (command_line.HasSwitch(extensions::switches::kLoadApps) &&
      can_use_last_profile) {
    if (!ProcessLoadApps(command_line, cur_dir, last_used_profile))
      return false;

    // Return early here to avoid opening a browser window.
    // The exception is when there are no browser windows, since we don't want
    // chrome to shut down.
    // TODO(jackhou): Do this properly once keep-alive is handled by the
    // background page of apps. Tracked at http://crbug.com/175381
    if (chrome::GetBrowserCount(last_used_profile) != 0)
      return true;
  }

  // Check for --load-and-launch-app.
  if (command_line.HasSwitch(apps::kLoadAndLaunchApp) && can_use_last_profile) {
    base::CommandLine::StringType path =
        command_line.GetSwitchValueNative(apps::kLoadAndLaunchApp);

    if (!apps::AppLoadService::Get(last_used_profile)->LoadAndLaunch(
            base::FilePath(path), command_line, cur_dir)) {
      return false;
    }

    // Return early here since we don't want to open a browser window.
    // The exception is when there are no browser windows, since we don't want
    // chrome to shut down.
    // TODO(jackhou): Do this properly once keep-alive is handled by the
    // background page of apps. Tracked at http://crbug.com/175381
    if (chrome::GetBrowserCount(last_used_profile) != 0)
      return true;
  }

#if defined(OS_WIN)
  // Log whether this process was a result of an action in the Windows Jumplist.
  if (command_line.HasSwitch(switches::kWinJumplistAction)) {
    jumplist::LogJumplistActionFromSwitchValue(
        command_line.GetSwitchValueASCII(switches::kWinJumplistAction));
    // Use a non-NULL pointer to indicate JumpList has been used. We re-use
    // chrome::kJumpListIconDirname as the key to the data.
    last_used_profile->SetUserData(
        chrome::kJumpListIconDirname,
        base::WrapUnique(new base::SupportsUserData::Data()));
  }
#endif  // defined(OS_WIN)

  return LaunchBrowserForLastProfiles(command_line, cur_dir, process_startup,
                                      last_used_profile, last_opened_profiles);
}

bool StartupBrowserCreator::LaunchBrowserForLastProfiles(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    bool process_startup,
    Profile* last_used_profile,
    const Profiles& last_opened_profiles) {
  chrome::startup::IsProcessStartup is_process_startup = process_startup ?
      chrome::startup::IS_PROCESS_STARTUP :
      chrome::startup::IS_NOT_PROCESS_STARTUP;
  chrome::startup::IsFirstRun is_first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;

  // On Windows, when chrome is launched by notification activation where the
  // kNotificationLaunchId switch is used, always use |last_used_profile| which
  // contains the profile id extracted from the notification launch id.
  bool was_windows_notification_launch = false;
#if defined(OS_WIN)
  was_windows_notification_launch =
      command_line.HasSwitch(switches::kNotificationLaunchId);
#endif  // defined(OS_WIN)

  // |last_opened_profiles| will be empty in the following circumstances:
  // - This is the first launch. |last_used_profile| is the initial profile.
  // - The user exited the browser by closing all windows for all profiles.
  //   |last_used_profile| is the profile which owned the last open window.
  // - Only incognito windows were open when the browser exited.
  //   |last_used_profile| is the last used incognito profile. Restoring it will
  //   create a browser window for the corresponding original profile.
  // - All of the last opened profiles fail to initialize.
  if (last_opened_profiles.empty() || was_windows_notification_launch) {
    if (CanOpenProfileOnStartup(last_used_profile)) {
      Profile* profile_to_open =
          last_used_profile->IsGuestSession()
              ? last_used_profile->GetOffTheRecordProfile()
              : last_used_profile;

      return LaunchBrowser(command_line, profile_to_open, cur_dir,
                           is_process_startup, is_first_run);
    }

    // Show UserManager if |last_used_profile| can't be auto opened.
    ShowUserManagerOnStartup(command_line);
    return true;
  }
  return ProcessLastOpenedProfiles(command_line, cur_dir, is_process_startup,
                                   is_first_run, last_used_profile,
                                   last_opened_profiles);
}

bool StartupBrowserCreator::ProcessLastOpenedProfiles(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    chrome::startup::IsProcessStartup is_process_startup,
    chrome::startup::IsFirstRun is_first_run,
    Profile* last_used_profile,
    const Profiles& last_opened_profiles) {
  base::CommandLine command_line_without_urls(command_line.GetProgram());
  for (auto& switch_pair : command_line.GetSwitches()) {
    command_line_without_urls.AppendSwitchNative(switch_pair.first,
                                                 switch_pair.second);
  }

  // TODO(scottmg): DEBUG_ variables added for https://crbug.com/614753.
  size_t DEBUG_num_profiles_on_entry = last_opened_profiles.size();
  base::debug::Alias(&DEBUG_num_profiles_on_entry);
  int DEBUG_loop_counter = 0;
  base::debug::Alias(&DEBUG_loop_counter);

  base::debug::Alias(&last_opened_profiles);
  const Profile* DEBUG_profile_0 = nullptr;
  const Profile* DEBUG_profile_1 = nullptr;
  if (last_opened_profiles.size() > 0)
    DEBUG_profile_0 = last_opened_profiles[0];
  if (last_opened_profiles.size() > 1)
    DEBUG_profile_1 = last_opened_profiles[1];
  base::debug::Alias(&DEBUG_profile_0);
  base::debug::Alias(&DEBUG_profile_1);

  size_t DEBUG_num_profiles_at_loop_start = std::numeric_limits<size_t>::max();
  base::debug::Alias(&DEBUG_num_profiles_at_loop_start);

  Profiles::const_iterator DEBUG_it_begin = last_opened_profiles.begin();
  base::debug::Alias(&DEBUG_it_begin);
  Profiles::const_iterator DEBUG_it_end = last_opened_profiles.end();
  base::debug::Alias(&DEBUG_it_end);

  // Launch the profiles in the order they became active.
  for (Profiles::const_iterator it = last_opened_profiles.begin();
       it != last_opened_profiles.end(); ++it, ++DEBUG_loop_counter) {
    DEBUG_num_profiles_at_loop_start = last_opened_profiles.size();
    DCHECK(!(*it)->IsGuestSession());

#if !defined(OS_CHROMEOS)
    // Skip any locked profile.
    if (!CanOpenProfileOnStartup(*it))
      continue;

    // Guest profiles should not be reopened on startup. This can happen if
    // the last used profile was a Guest, but other profiles were also open
    // when Chrome was closed. In this case, pick a different open profile
    // to be the active one, since the Guest profile is never added to the
    // list of open profiles.
    if (last_used_profile->IsGuestSession())
      last_used_profile = *it;
#endif

    // Don't launch additional profiles which would only open a new tab
    // page. When restarting after an update, all profiles will reopen last
    // open pages.
    SessionStartupPref startup_pref = GetSessionStartupPref(command_line, *it);
    if (*it != last_used_profile &&
        startup_pref.type == SessionStartupPref::DEFAULT &&
        !HasPendingUncleanExit(*it)) {
      continue;
    }
    if (!LaunchBrowser((*it == last_used_profile) ? command_line
                                                  : command_line_without_urls,
                       *it, cur_dir, is_process_startup, is_first_run)) {
      return false;
    }
    // We've launched at least one browser.
    is_process_startup = chrome::startup::IS_NOT_PROCESS_STARTUP;
  }

// Set the |last_used_profile| to activate if a browser is launched for at
// least one profile. Otherwise, show UserManager.
// Note that this must be done after all profiles have
// been launched so the observer knows about all profiles to wait before
// activation this one.
#if !defined(OS_CHROMEOS)
  if (is_process_startup == chrome::startup::IS_PROCESS_STARTUP)
    ShowUserManagerOnStartup(command_line);
  else
#endif
    profile_launch_observer.Get().set_profile_to_activate(last_used_profile);
  return true;
}

// static
bool StartupBrowserCreator::ProcessLoadApps(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile) {
  base::CommandLine::StringType path_list =
      command_line.GetSwitchValueNative(extensions::switches::kLoadApps);

  base::StringTokenizerT<base::CommandLine::StringType,
                         base::CommandLine::StringType::const_iterator>
      tokenizer(path_list, FILE_PATH_LITERAL(","));

  if (!tokenizer.GetNext())
    return false;

  base::FilePath app_absolute_dir =
      base::MakeAbsoluteFilePath(base::FilePath(tokenizer.token()));
  if (!apps::AppLoadService::Get(profile)->LoadAndLaunch(
          app_absolute_dir, command_line, cur_dir)) {
    return false;
  }

  while (tokenizer.GetNext()) {
    app_absolute_dir =
        base::MakeAbsoluteFilePath(base::FilePath(tokenizer.token()));

    if (!apps::AppLoadService::Get(profile)->Load(app_absolute_dir)) {
      return false;
    }
  }

  return true;
}

// static
void StartupBrowserCreator::ProcessCommandLineOnProfileCreated(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    Profile::CreateStatus status) {
  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;
  StartupBrowserCreator startup_browser_creator;
  startup_browser_creator.ProcessCmdLineImpl(command_line, cur_dir, false,
                                             profile, Profiles());
}

// static
void StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    const base::FilePath& profile_path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetProfileByPath(profile_path);

  // The profile isn't loaded yet and so needs to be loaded asynchronously.
  if (!profile) {
    profile_manager->CreateProfileAsync(
        profile_path,
        base::Bind(&ProcessCommandLineOnProfileCreated, command_line, cur_dir),
        base::string16(), std::string());
    return;
  }
  StartupBrowserCreator startup_browser_creator;
  startup_browser_creator.ProcessCmdLineImpl(command_line, cur_dir, false,
                                             profile, Profiles());
}

// static
void StartupBrowserCreator::OpenStartupPages(Browser* browser,
                                             bool process_startup) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun is_first_run =
      first_run::IsChromeFirstRun() ? chrome::startup::IS_FIRST_RUN
                                    : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl startup_browser_creator_impl(
      base::FilePath(), command_line, is_first_run);
  SessionStartupPref session_startup_pref =
      StartupBrowserCreator::GetSessionStartupPref(command_line,
                                                   browser->profile());
  startup_browser_creator_impl.OpenURLsInBrowser(browser, process_startup,
                                                 session_startup_pref.urls);
}

// static
bool StartupBrowserCreator::ActivatedProfile() {
  return profile_launch_observer.Get().activated_profile();
}

bool HasPendingUncleanExit(Profile* profile) {
  return profile->GetLastSessionExitType() == Profile::EXIT_CRASHED &&
      !profile_launch_observer.Get().HasBeenLaunched(profile);
}

base::FilePath GetStartupProfilePath(const base::FilePath& user_data_dir,
                                     const base::CommandLine& command_line) {
// If the browser is launched due to activation on Windows native notification,
// the profile id encoded in the notification launch id should be chosen over
// all others.
#if defined(OS_WIN)
  if (command_line.HasSwitch(switches::kNotificationLaunchId)) {
    std::string profile_id =
        NotificationPlatformBridgeWin::GetProfileIdFromLaunchId(
            command_line.GetSwitchValueNative(switches::kNotificationLaunchId));
    if (!profile_id.empty()) {
      return user_data_dir.Append(
          base::FilePath(base::UTF8ToUTF16(profile_id)));
    }
  }
#endif  // defined(OS_WIN)

  if (command_line.HasSwitch(switches::kProfileDirectory)) {
    return user_data_dir.Append(
        command_line.GetSwitchValuePath(switches::kProfileDirectory));
  }

  return g_browser_process->profile_manager()->GetLastUsedProfileDir(
      user_data_dir);
}

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
Profile* GetStartupProfile(const base::FilePath& user_data_dir,
                           const base::CommandLine& command_line) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath profile_path =
      GetStartupProfilePath(user_data_dir, command_line);
  Profile* profile = profile_manager->GetProfile(profile_path);

  // If there is no entry in profile attributes storage, the profile is deleted,
  // and we should show the user manager. Also, when using
  // --new-profile-management, if the profile is locked we should show the user
  // manager as well. When neither of these is true, we can safely start up with
  // |profile|.
  auto* storage = &profile_manager->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry;
  bool has_entry = storage->GetProfileAttributesWithPath(profile_path, &entry);
  if (has_entry && (!entry->IsSigninRequired() || !profile)) {
    return profile;
  }

  // We want to show the user manager. To indicate this, return the guest
  // profile. However, we can only do this if the system profile (where the user
  // manager lives) also exists (or is creatable).
  return profile_manager->GetProfile(ProfileManager::GetSystemProfilePath()) ?
         profile_manager->GetProfile(ProfileManager::GetGuestProfilePath()) :
         nullptr;
}

Profile* GetFallbackStartupProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // The only known reason for profiles to fail initialization is being unable
  // to create the profile directory, and this has already happened in
  // GetStartupProfilePath() before calling this function. In this case,
  // creation of new profiles is expected to fail. So only existing profiles are
  // attempted for fallback.

  // If the last used profile could not be initialized, see if any of other last
  // opened profiles can be initialized successfully.
  auto* storage = &profile_manager->GetProfileAttributesStorage();
  for (Profile* profile : ProfileManager::GetLastOpenedProfiles()) {
    // Return any profile that is not locked.
    ProfileAttributesEntry* entry;
    bool has_entry = storage->GetProfileAttributesWithPath(profile->GetPath(),
                                                           &entry);
    if (!has_entry || !entry->IsSigninRequired())
      return profile;
  }

  // Couldn't initialize any last opened profiles. Try to show the user manager,
  // which requires successful initialization of the guest and system profiles.
  Profile* guest_profile =
      profile_manager->GetProfile(ProfileManager::GetGuestProfilePath());
  Profile* system_profile =
      profile_manager->GetProfile(ProfileManager::GetSystemProfilePath());
  if (guest_profile && system_profile)
    return guest_profile;

  // Couldn't show the user manager either. Try to open any profile that is not
  // locked.
  for (ProfileAttributesEntry* entry : storage->GetAllProfilesAttributes()) {
    if (!entry->IsSigninRequired()) {
      Profile* profile = profile_manager->GetProfile(entry->GetPath());
      if (profile)
        return profile;
    }
  }

  return nullptr;
}
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
