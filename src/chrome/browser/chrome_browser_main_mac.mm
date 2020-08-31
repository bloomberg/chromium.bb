// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/bundle_locations.h"
#import "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/branding_buildflags.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_listener.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/mac/install_from_dmg.h"
#import "chrome/browser/mac/keystone_glue.h"
#include "chrome/browser/mac/mac_startup_profiler.h"
#include "chrome/browser/ui/cocoa/main_menu_builder.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "components/crash/core/app/crashpad.h"
#include "components/metrics/metrics_service.h"
#include "components/os_crypt/os_crypt.h"
#include "components/version_info/channel.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_handle.h"
#include "ui/native_theme/native_theme_mac.h"

namespace {

// Writes an undocumented sentinel file that prevents Spotlight from indexing
// below a particular path in order to reap some power savings.
void EnsureMetadataNeverIndexFileOnFileThread(
    const base::FilePath& user_data_dir) {
  const char kMetadataNeverIndexFilename[] = ".metadata_never_index";
  base::FilePath metadata_file_path =
      user_data_dir.Append(kMetadataNeverIndexFilename);
  if (base::PathExists(metadata_file_path))
    return;

  if (base::WriteFile(metadata_file_path, nullptr, 0) == -1)
    DLOG(FATAL) << "Could not write .metadata_never_index file.";
}

void EnsureMetadataNeverIndexFile(const base::FilePath& user_data_dir) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&EnsureMetadataNeverIndexFileOnFileThread, user_data_dir));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

// These values are persisted to logs as OSXOtherChromeInstancesResult.
// Entries should not be renumbered and numeric values should never be reused.
enum class OtherInstancesResult {
  kFailureDontKnowWhenOtherChromeUsed,
  kFailureToReadPlist,
  kNoOtherChrome,
  kOneOtherChromeAndLastUsedWithinWeek,
  kOneOtherChromeAndLastUsedWithinMonth,
  kOneOtherChromeAndLastUsedMoreThanAMonthAgo,
  kMoreThanOneOtherChromeAndLastUsedWithinWeek,
  kMoreThanOneOtherChromeAndLastUsedWithinMonth,
  kMoreThanOneOtherChromeAndLastUsedMoreThanAMonthAgo,
  kMaxValue = kMoreThanOneOtherChromeAndLastUsedMoreThanAMonthAgo,
};

struct WhenLastUsed {
  int within_last_week = 0;
  int within_last_month = 0;
  int before_last_month = 0;
};

OtherInstancesResult OtherInstancesResultForWhenLastUsed(
    const WhenLastUsed& used) {
  if (used.within_last_week + used.within_last_month + used.before_last_month ==
      0) {
    return OtherInstancesResult::kNoOtherChrome;
  }

  if (used.within_last_week + used.within_last_month + used.before_last_month ==
      1) {
    if (used.within_last_week)
      return OtherInstancesResult::kOneOtherChromeAndLastUsedWithinWeek;

    if (used.within_last_month)
      return OtherInstancesResult::kOneOtherChromeAndLastUsedWithinMonth;

    return OtherInstancesResult::kOneOtherChromeAndLastUsedMoreThanAMonthAgo;
  }

  if (used.within_last_week)
    return OtherInstancesResult::kMoreThanOneOtherChromeAndLastUsedWithinWeek;

  if (used.within_last_month)
    return OtherInstancesResult::kMoreThanOneOtherChromeAndLastUsedWithinMonth;

  return OtherInstancesResult::
      kMoreThanOneOtherChromeAndLastUsedMoreThanAMonthAgo;
}

void RecordChromeQueryResults(NSMetadataQuery* query) {
  __block bool other_chrome_last_used_unknown = false;
  __block bool failed_to_read_plist = false;
  __block WhenLastUsed same_channel;
  __block WhenLastUsed different_channel;

  NSURL* this_url = NSBundle.mainBundle.bundleURL;
  std::string this_channel = chrome::GetChannelName();
  NSDate* about_a_week_ago =
      [[NSDate date] dateByAddingTimeInterval:-7 * 24 * 60 * 60];
  NSDate* about_a_month_ago =
      [[NSDate date] dateByAddingTimeInterval:-30 * 24 * 60 * 60];

  [query enumerateResultsUsingBlock:^(id result, NSUInteger idx, BOOL* stop) {
    // Skip this copy of Chrome. Note that NSMetadataItemURLKey is not used as
    // it always returns nil while NSMetadataItemPathKey returns a legit path.
    // Filed as FB7689234.
    NSString* app_path = base::mac::ObjCCast<NSString>(
        [result valueForAttribute:NSMetadataItemPathKey]);
    if (!app_path) {
      // It seems implausible, but there are Macs in the field for which
      // Spotlight will find results for the query of locating Chrome but cannot
      // actually return a path to the result. https://crbug.com/1086555
      failed_to_read_plist = true;
      *stop = YES;
      return;
    }

    NSURL* app_url = [NSURL fileURLWithPath:app_path isDirectory:YES];
    if ([app_url isEqual:this_url])
      return;

    NSURL* plist_url = [[app_url URLByAppendingPathComponent:@"Contents"
                                                 isDirectory:YES]
        URLByAppendingPathComponent:@"Info.plist"
                        isDirectory:NO];
    NSDictionary* plist = [NSDictionary dictionaryWithContentsOfURL:plist_url];
    if (!plist) {
      failed_to_read_plist = true;
      *stop = YES;
      return;
    }

    // Skip any SxS-capable copies of Chrome.
    if (plist[@"CrProductDirName"])
      return;

    WhenLastUsed* when_last_used = &different_channel;
    if (this_channel == base::SysNSStringToUTF8(plist[@"KSChannelID"]))
      when_last_used = &same_channel;

    NSDate* last_used = base::mac::ObjCCast<NSDate>(
        [result valueForAttribute:NSMetadataItemLastUsedDateKey]);
    if (!last_used) {
      other_chrome_last_used_unknown = true;
      *stop = YES;
      return;
    }

    if ([last_used compare:about_a_week_ago] == NSOrderedDescending)
      ++when_last_used->within_last_week;
    else if ([last_used compare:about_a_month_ago] == NSOrderedDescending)
      ++when_last_used->within_last_month;
    else
      ++when_last_used->before_last_month;
  }];

  if (other_chrome_last_used_unknown) {
    base::UmaHistogramEnumeration(
        "OSX.Installation.OtherChromeInstances.SameChannel",
        OtherInstancesResult::kFailureDontKnowWhenOtherChromeUsed);
    base::UmaHistogramEnumeration(
        "OSX.Installation.OtherChromeInstances.DifferentChannel",
        OtherInstancesResult::kFailureDontKnowWhenOtherChromeUsed);
    return;
  }

  if (failed_to_read_plist) {
    base::UmaHistogramEnumeration(
        "OSX.Installation.OtherChromeInstances.SameChannel",
        OtherInstancesResult::kFailureToReadPlist);
    base::UmaHistogramEnumeration(
        "OSX.Installation.OtherChromeInstances.DifferentChannel",
        OtherInstancesResult::kFailureToReadPlist);
    return;
  }

  base::UmaHistogramEnumeration(
      "OSX.Installation.OtherChromeInstances.SameChannel",
      OtherInstancesResultForWhenLastUsed(same_channel));
  base::UmaHistogramEnumeration(
      "OSX.Installation.OtherChromeInstances.DifferentChannel",
      OtherInstancesResultForWhenLastUsed(different_channel));
}

void ExecuteChromeQuery() {
  __block NSMetadataQuery* query = [[NSMetadataQuery alloc] init];

  __block id token = [[NSNotificationCenter defaultCenter]
      addObserverForName:NSMetadataQueryDidFinishGatheringNotification
                  object:query
                   queue:[NSOperationQueue mainQueue]
              usingBlock:^(NSNotification* note) {
                [query stopQuery];
                RecordChromeQueryResults(query);
                [query release];
                [[NSNotificationCenter defaultCenter] removeObserver:token];
              }];

  query.predicate =
      [NSPredicate predicateWithFormat:
                       @"kMDItemContentType == 'com.apple.application-bundle'"
                       @"AND kMDItemCFBundleIdentifier == 'com.google.Chrome'"];

  [query startQuery];
}

// Records statistics about this install of Chromium if it is a Google Chrome
// Beta or Google Chrome Dev instance. This is to allow for decisions to be made
// about the migration of user data directories.
void RecordBetaAndDevStats() {
  version_info::Channel channel = chrome::GetChannel();
  if (channel != version_info::Channel::BETA &&
      channel != version_info::Channel::DEV) {
    return;
  }

  ExecuteChromeQuery();
}

#endif  // GOOGLE_CHROME_BRANDING

}  // namespace

// ChromeBrowserMainPartsMac ---------------------------------------------------

ChromeBrowserMainPartsMac::ChromeBrowserMainPartsMac(
    const content::MainFunctionParams& parameters,
    StartupData* startup_data)
    : ChromeBrowserMainPartsPosix(parameters, startup_data) {}

ChromeBrowserMainPartsMac::~ChromeBrowserMainPartsMac() {
}

int ChromeBrowserMainPartsMac::PreEarlyInitialization() {
  if (base::mac::WasLaunchedAsLoginItemRestoreState()) {
    base::CommandLine* singleton_command_line =
        base::CommandLine::ForCurrentProcess();
    singleton_command_line->AppendSwitch(switches::kRestoreLastSession);
  } else if (base::mac::WasLaunchedAsHiddenLoginItem()) {
    base::CommandLine* singleton_command_line =
        base::CommandLine::ForCurrentProcess();
    singleton_command_line->AppendSwitch(switches::kNoStartupWindow);
  }

  return ChromeBrowserMainPartsPosix::PreEarlyInitialization();
}

void ChromeBrowserMainPartsMac::PreMainMessageLoopStart() {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::PRE_MAIN_MESSAGE_LOOP_START);
  ChromeBrowserMainPartsPosix::PreMainMessageLoopStart();

  // ChromeBrowserMainParts should have loaded the resource bundle by this
  // point (needed to load the nib).
  CHECK(ui::ResourceBundle::HasSharedInstance());

  // This is a no-op if the KeystoneRegistration framework is not present.
  // The framework is only distributed with branded Google Chrome builds.
  [[KeystoneGlue defaultKeystoneGlue] registerWithKeystone];

  // Disk image installation is sort of a first-run task, so it shares the
  // no first run switches.
  //
  // This needs to be done after the resource bundle is initialized (for
  // access to localizations in the UI) and after Keystone is initialized
  // (because the installation may need to promote Keystone) but before the
  // app controller is set up (and thus before MainMenu.nib is loaded, because
  // the app controller assumes that a browser has been set up and will crash
  // upon receipt of certain notifications if no browser exists), before
  // anyone tries doing anything silly like firing off an import job, and
  // before anything creating preferences like Local State in order for the
  // relaunched installed application to still consider itself as first-run.
  if (!first_run::IsFirstRunSuppressed(parsed_command_line())) {
    if (MaybeInstallFromDiskImage()) {
      // The application was installed and the installed copy has been
      // launched.  This process is now obsolete.  Exit.
      exit(0);
    }
  }

  // Create the app delegate. This object is intentionally leaked as a global
  // singleton. It is accessed through -[NSApp delegate].
  AppController* app_controller = [[AppController alloc] init];
  [NSApp setDelegate:app_controller];

  chrome::BuildMainMenu(NSApp, app_controller,
                        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME), false);
  [app_controller mainMenuCreated];

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  // AppKit only restores windows to their original spaces when relaunching
  // apps after a restart, and puts them all on the current space when an app
  // is manually quit and relaunched. If Chrome restarted itself, ask AppKit to
  // treat this launch like a system restart and restore everything.
  if (local_state->GetBoolean(prefs::kWasRestarted)) {
    [NSUserDefaults.standardUserDefaults registerDefaults:@{
      @"NSWindowRestoresWorkspaceAtLaunch" : @YES
    }];
  }
}

void ChromeBrowserMainPartsMac::PostMainMessageLoopStart() {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::POST_MAIN_MESSAGE_LOOP_START);
  ChromeBrowserMainPartsPosix::PostMainMessageLoopStart();

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  RecordBetaAndDevStats();
#endif  // GOOGLE_CHROME_BRANDING
}

void ChromeBrowserMainPartsMac::PreProfileInit() {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::PRE_PROFILE_INIT);
  ChromeBrowserMainPartsPosix::PreProfileInit();

  // This is called here so that the app shim socket is only created after
  // taking the singleton lock.
  g_browser_process->platform_part()->app_shim_listener()->Init();
}

void ChromeBrowserMainPartsMac::PostProfileInit() {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::POST_PROFILE_INIT);
  ChromeBrowserMainPartsPosix::PostProfileInit();

  g_browser_process->metrics_service()->RecordBreakpadRegistration(
      crash_reporter::GetUploadsEnabled());

  if (first_run::IsChromeFirstRun())
    EnsureMetadataNeverIndexFile(user_data_dir());

  // Activation of Keystone is not automatic but done in response to the
  // counting and reporting of profiles.
  KeystoneGlue* glue = [KeystoneGlue defaultKeystoneGlue];
  if (glue && ![glue isRegisteredAndActive]) {
    // If profile loading has failed, we still need to handle other tasks
    // like marking of the product as active.
    [glue setRegistrationActive];
  }
}

void ChromeBrowserMainPartsMac::DidEndMainMessageLoop() {
  AppController* appController =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
  [appController didEndMainMessageLoop];
}
