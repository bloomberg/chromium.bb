// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_BACKGROUND_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_BACKGROUND_TASK_H_

#include <memory.h>

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace web_app {

// A struct used to configure a periodic background task for a SWA.
struct SystemAppBackgroundTaskInfo {
  SystemAppBackgroundTaskInfo();
  SystemAppBackgroundTaskInfo(const SystemAppBackgroundTaskInfo& other);
  SystemAppBackgroundTaskInfo(const absl::optional<base::TimeDelta>& period,
                              const GURL& url,
                              bool open_immediately = false);
  ~SystemAppBackgroundTaskInfo();
  // The amount of time between each opening of the background url.
  // The url is opened using the same WebContents, so if the
  // previous task is still running, it will be closed.
  // You should have at least one of period or open_immediately set for the task
  // to do anything.
  absl::optional<base::TimeDelta> period;

  // The url of the background page to open. This should do one specific thing.
  // (Probably opening a shared worker, waiting for a response, and closing)
  GURL url;

  // A flag to indicate that the task should be opened soon upon user
  // login, after the SWAs are done installing as opposed to waiting for the
  // first period to be reached. "Soon" means about 2 minutes, to give the
  // login time processing a chance to settle down.
  bool open_immediately;
};

// Used to manage a running periodic background task for a SWA.
class SystemAppBackgroundTask {
 public:
  enum TimerState {
    INITIAL_WAIT = 0,
    WAIT_PERIOD = 1,
    WAIT_IDLE = 2,
    INACTIVE = 3
  };

  // Wait for 2 minutes before starting background tasks. User login is busy,
  // and this will give a little time to settle down. We could get even more
  // sophisticated, and smear all the different start_immediately tasks across a
  // couple minutes instead of setting their start timers to the same time.
  static const int kInitialWaitForBackgroundTasksSeconds = 120;

  // User idle for 1 minute.
  static const int kIdleThresholdSeconds = 60;

  // Else, poll every 30 seconds
  static const int kIdlePollIntervalSeconds = 30;

  // For up to an hour.
  static const int kIdlePollMaxTimeToWaitSeconds = 3600;

  // The duration we polled for before becoming idle and starting the background
  // task.
  static constexpr char kBackgroundStartDelayHistogramName[] =
      "Webapp.SystemApps.BackgroundTaskStartDelay";

  SystemAppBackgroundTask(Profile* profile,
                          const SystemAppBackgroundTaskInfo& info);
  ~SystemAppBackgroundTask();

  // Start the timer, at the specified period. This will also run immediately if
  // needed
  void StartTask();

  // Bring down the background task if open, and stop the timer.
  void StopTask();

  bool open_immediately_for_testing() const { return open_immediately_; }

  SystemAppType app_type_for_testing() const { return app_type_; }

  GURL url_for_testing() const { return url_; }

  content::WebContents* web_contents_for_testing() const {
    return web_contents_.get();
  }

  absl::optional<base::TimeDelta> period_for_testing() const { return period_; }

  unsigned long opened_count_for_testing() const { return opened_count_; }

  unsigned long timer_activated_count_for_testing() const {
    return timer_activated_count_;
  }

  base::Time polling_since_time_for_testing() const {
    return polling_since_time_;
  }

  WebAppUrlLoader* UrlLoaderForTesting() { return web_app_url_loader_.get(); }

  // Set the url loader for testing. Takes ownership of the argument.
  void SetUrlLoaderForTesting(std::unique_ptr<WebAppUrlLoader> loader) {
    web_app_url_loader_ = std::move(loader);
  }

  TimerState get_state_for_testing() const { return state_; }

  base::OneShotTimer* get_timer_for_testing() { return timer_.get(); }

 private:
  // A delegate to reset the WebContents owned by this background task and free
  // up the resources. Called when the page calls window.close() to exit.
  class CloseDelegate : public content::WebContentsDelegate {
   public:
    explicit CloseDelegate(SystemAppBackgroundTask* task) : task_(task) {}
    void CloseContents(content::WebContents* contents) override;

   private:
    raw_ptr<SystemAppBackgroundTask> task_;
  };
  // A state machine to either poll and fail, stop polling and succeed, or stop
  // polling and fail
  void MaybeOpenPage();

  void NavigateBackgroundPage();
  void OnLoaderReady(WebAppUrlLoader::Result);
  void OnPageReady(WebAppUrlLoader::Result);

  void CloseWebContents(content::WebContents* contents);

  raw_ptr<Profile> profile_;
  SystemAppType app_type_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<WebAppUrlLoader> web_app_url_loader_;
  std::unique_ptr<base::OneShotTimer> timer_;
  TimerState state_;
  GURL url_;
  absl::optional<base::TimeDelta> period_;
  unsigned long opened_count_;
  unsigned long timer_activated_count_;
  bool open_immediately_;
  base::Time polling_since_time_;
  CloseDelegate delegate_;

  base::WeakPtrFactory<SystemAppBackgroundTask> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_BACKGROUND_TASK_H_
