// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/threading/hang_watcher.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_process_singleton.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
#include "chrome/browser/downgrade/downgrade_manager.h"
#endif

class BrowserProcessImpl;
class ChromeBrowserMainExtraParts;
class StartupData;
class PrefService;
class Profile;
class StartupBrowserCreator;
class ShutdownWatcherHelper;
class WebUsbDetector;

namespace base {
class RunLoop;
}

namespace tracing {
class TraceEventSystemStatsMonitor;
}

class ChromeBrowserMainParts : public content::BrowserMainParts {
 public:
  ChromeBrowserMainParts(const ChromeBrowserMainParts&) = delete;
  ChromeBrowserMainParts& operator=(const ChromeBrowserMainParts&) = delete;
  ~ChromeBrowserMainParts() override;

  // Add additional ChromeBrowserMainExtraParts.
  void AddParts(std::unique_ptr<ChromeBrowserMainExtraParts> parts);

#if !defined(OS_ANDROID)
  // Returns the RunLoop that would be run by MainMessageLoopRun. This is used
  // by InProcessBrowserTests to allow them to run until the BrowserProcess is
  // ready for the browser to exit.
  static std::unique_ptr<base::RunLoop> TakeRunLoopForTest();
#endif

 protected:
  ChromeBrowserMainParts(content::MainFunctionParams parameters,
                         StartupData* startup_data);

  // content::BrowserMainParts overrides.
  // These are called in-order by content::BrowserMainLoop.
  // Each stage calls the same stages in any ChromeBrowserMainExtraParts added
  // with AddParts() from ChromeContentBrowserClient::CreateBrowserMainParts.
  int PreEarlyInitialization() override;
  void PostEarlyInitialization() override;
  void ToolkitInitialized() override;
  void PreCreateMainMessageLoop() override;
  void PostCreateMainMessageLoop() override;
  int PreCreateThreads() override;
  void PostCreateThreads() override;
  int PreMainMessageLoopRun() override;
#if !defined(OS_ANDROID)
  bool ShouldInterceptMainMessageLoopRun() override;
#endif
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void OnFirstIdle() override;
  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

  // Additional stages for ChromeBrowserMainExtraParts. These stages are called
  // in order from PreMainMessageLoopRun(). See implementation for details.
  virtual void PreProfileInit();
  virtual void PostProfileInit();
  virtual void PreBrowserStart();
  virtual void PostBrowserStart();

  // Displays a warning message that we can't find any locale data files.
  virtual void ShowMissingLocaleMessageBox() = 0;

  const content::MainFunctionParams& parameters() const {
    return parameters_;
  }
  const base::CommandLine& parsed_command_line() const {
    return parsed_command_line_;
  }
  const base::FilePath& user_data_dir() const {
    return user_data_dir_;
  }

  Profile* profile() { return profile_; }

 private:
  friend class ChromeBrowserMainPartsTestApi;

  // Constructs the metrics service and initializes metrics recording.
  void SetupMetrics();

  // Starts recording of metrics. This can only be called after we have a file
  // thread.
  static void StartMetricsRecording();

  // Record time from process startup to present time in an UMA histogram.
  void RecordBrowserStartupTime();

  // Reads origin trial policy data from local state and configures command line
  // for child processes.
  void SetupOriginTrialsCommandLine(PrefService* local_state);

  // Calling during PreEarlyInitialization() to complete the remaining tasks
  // after the local state is loaded. Return value is an exit status,
  // RESULT_CODE_NORMAL_EXIT indicates success. If the return value is
  // RESULT_CODE_MISSING_DATA, then |failed_to_load_resource_bundle| indicates
  // if the ResourceBundle couldn't be loaded.
  int OnLocalStateLoaded(bool* failed_to_load_resource_bundle);

  // Applies any preferences (to local state) needed for first run. This is
  // always called and early outs if not first-run. Return value is an exit
  // status, RESULT_CODE_NORMAL_EXIT indicates success.
  int ApplyFirstRunPrefs();

  // Methods for Main Message Loop -------------------------------------------

  int PreCreateThreadsImpl();
  int PreMainMessageLoopRunImpl();

  // Members initialized on construction ---------------------------------------

  content::MainFunctionParams parameters_;
  // TODO(sky): remove this. This class (and related calls), may mutate the
  // CommandLine, so it is misleading keeping a const ref here.
  const base::CommandLine& parsed_command_line_;
  int result_code_ = content::RESULT_CODE_NORMAL_EXIT;

#if !defined(OS_ANDROID)
  // Create ShutdownWatcherHelper object for watching jank during shutdown.
  // Please keep |shutdown_watcher| as the first object constructed, and hence
  // it is destroyed last.
  std::unique_ptr<ShutdownWatcherHelper> shutdown_watcher_;

  // HangWatcher based equivalent to |shutdown_watcher_|
  absl::optional<base::WatchHangsInScope> watch_hangs_scope_;

  std::unique_ptr<WebUsbDetector> web_usb_detector_;
#endif  // !defined(OS_ANDROID)

  // Vector of additional ChromeBrowserMainExtraParts.
  // Parts are deleted in the inverse order they are added.
  std::vector<std::unique_ptr<ChromeBrowserMainExtraParts>> chrome_extra_parts_;

  // The system stats monitor used by chrome://tracing. This doesn't do anything
  // until tracing of the |system_stats| category is enabled.
  std::unique_ptr<tracing::TraceEventSystemStatsMonitor>
      trace_event_system_stats_monitor_;

  // Whether PerformPreMainMessageLoopStartup() is called on VariationsService.
  // Initialized to true if |MainFunctionParams::ui_task| is null (meaning not
  // running browser_tests), but may be forced to true for tests.
  bool should_call_pre_main_loop_start_startup_on_variations_service_;

  // Members initialized after / released before main_message_loop_ ------------

  std::unique_ptr<BrowserProcessImpl> browser_process_;

#if !defined(OS_ANDROID)
  // Browser creation happens on the Java side in Android.
  std::unique_ptr<StartupBrowserCreator> browser_creator_;

  // Android doesn't support multiple browser processes, so it doesn't implement
  // ProcessSingleton.
  std::unique_ptr<ChromeProcessSingleton> process_singleton_;

  ProcessSingleton::NotifyResult notify_result_ =
      ProcessSingleton::PROCESS_NONE;

  // Members needed across shutdown methods.
  bool restart_last_session_ = false;
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
  downgrade::DowngradeManager downgrade_manager_;
#endif

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // Android's first run is done in Java instead of native. Chrome OS does not
  // use master preferences.
  std::unique_ptr<first_run::MasterPrefs> master_prefs_;
#endif

  raw_ptr<Profile> profile_ = nullptr;

  base::FilePath user_data_dir_;

  raw_ptr<StartupData> startup_data_;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_H_
