// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/power_monitor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/daily_event.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/background_mode_manager.h"
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

namespace metrics {

namespace {

// The interval at which the DailyEvent::CheckInterval function should be
// called.
constexpr base::TimeDelta kDailyEventIntervalTimeDelta = base::Minutes(30);

// The intervals at which we report the number of unused tabs. This is used for
// all the tab usage histograms listed below.
//
// The 'Tabs.TabUsageIntervalLength' histogram suffixes entry in histograms.xml
// should be kept in sync with these values.
constexpr base::TimeDelta kTabUsageReportingIntervals[] = {
    base::Seconds(30), base::Minutes(1), base::Minutes(10),
    base::Hours(1),    base::Hours(5),   base::Hours(12)};

#if defined(OS_WIN)
const base::TimeDelta kNativeWindowOcclusionCalculationInterval =
    base::Minutes(10);
#endif

// The interval at which the heartbeat tab metrics should be reported.
const base::TimeDelta kTabsHeartbeatReportingInterval = base::Minutes(5);

// The global TabStatsTracker instance.
TabStatsTracker* g_tab_stats_tracker_instance = nullptr;

// Ensure that an interval is a valid one (i.e. listed in
// |kTabUsageReportingIntervals|).
bool IsValidInterval(base::TimeDelta interval) {
  return base::Contains(kTabUsageReportingIntervals, interval);
}

void UmaHistogramCounts10000WithBatteryStateVariant(const char* histogram_name,
                                                    size_t value) {
  DCHECK(base::PowerMonitor::IsInitialized());

  base::UmaHistogramCounts10000(histogram_name, value);

  const char* suffix =
      base::PowerMonitor::IsOnBatteryPower() ? ".OnBattery" : ".PluggedIn";

  base::UmaHistogramCounts10000(base::StrCat({histogram_name, suffix}), value);
}

}  // namespace

// static
const char TabStatsTracker::kTabStatsDailyEventHistogramName[] =
    "Tabs.TabsStatsDailyEventInterval";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kNumberOfTabsOnResumeHistogramName[] = "Tabs.NumberOfTabsOnResume";
const char
    TabStatsTracker::UmaStatsReportingDelegate::kMaxTabsInADayHistogramName[] =
        "Tabs.MaxTabsInADay";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kMaxTabsPerWindowInADayHistogramName[] = "Tabs.MaxTabsPerWindowInADay";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kMaxWindowsInADayHistogramName[] = "Tabs.MaxWindowsInADay";

// Tab usage histograms.
const char TabStatsTracker::UmaStatsReportingDelegate::
    kUnusedAndClosedInIntervalHistogramNameBase[] =
        "Tabs.UnusedAndClosedInInterval.Count";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kUnusedTabsInIntervalHistogramNameBase[] = "Tabs.UnusedInInterval.Count";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kUsedAndClosedInIntervalHistogramNameBase[] =
        "Tabs.UsedAndClosedInInterval.Count";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kUsedTabsInIntervalHistogramNameBase[] = "Tabs.UsedInInterval.Count";

const char
    TabStatsTracker::UmaStatsReportingDelegate::kTabCountHistogramName[] =
        "Tabs.TabCount";
const char
    TabStatsTracker::UmaStatsReportingDelegate::kWindowCountHistogramName[] =
        "Tabs.WindowCount";
const char
    TabStatsTracker::UmaStatsReportingDelegate::kWindowWidthHistogramName[] =
        "Tabs.WindowWidth";
const char
    TabStatsTracker::UmaStatsReportingDelegate::kCollapsedTabHistogramName[] =
        "TabGroups.CollapsedTabCount";

const TabStatsDataStore::TabsStats& TabStatsTracker::tab_stats() const {
  return tab_stats_data_store_->tab_stats();
}

TabStatsTracker::TabStatsTracker(PrefService* pref_service)
    : reporting_delegate_(std::make_unique<UmaStatsReportingDelegate>()),
      delegate_(std::make_unique<TabStatsTrackerDelegate>()),
      tab_stats_data_store_(std::make_unique<TabStatsDataStore>(pref_service)),
      daily_event_(
          std::make_unique<DailyEvent>(pref_service,
                                       ::prefs::kTabStatsDailySample,
                                       kTabStatsDailyEventHistogramName)) {
  DCHECK(pref_service);

  // Add owned observers to the list manually since they are about to be
  // initialized. Subsequent observers should be added with
  // AddObserverAndSetInitialState().
  tab_stats_observers_.AddObserver(tab_stats_data_store_.get());

  // Get the list of existing windows/tabs. There shouldn't be any if this is
  // initialized at startup but this will ensure that the counts stay accurate
  // if the initialization gets moved to after the creation of the first tab.
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    OnBrowserAdded(browser);
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i)
      OnInitialOrInsertedTab(browser->tab_strip_model()->GetWebContentsAt(i));
    tab_stats_data_store_->UpdateMaxTabsPerWindowIfNeeded(
        static_cast<size_t>(browser->tab_strip_model()->count()));
  }

  browser_list->AddObserver(this);
  base::PowerMonitor::AddPowerSuspendObserver(this);

  // Setup daily reporting of the stats aggregated in |tab_stats_data_store|.
  daily_event_->AddObserver(std::make_unique<TabStatsDailyObserver>(
      reporting_delegate_.get(), tab_stats_data_store_.get()));

  // Call the CheckInterval method to see if the data need to be immediately
  // reported.
  daily_event_->CheckInterval();
  daily_event_timer_.Start(FROM_HERE, kDailyEventIntervalTimeDelta,
                           daily_event_.get(), &DailyEvent::CheckInterval);

  // Initialize the interval maps and timers associated with them.
  // Only |tab_stats_data_store_| (and not other observers) is registered for
  // callbacks since only it computes intervals.
  for (base::TimeDelta interval : kTabUsageReportingIntervals) {
    TabStatsDataStore::TabsStateDuringIntervalMap* interval_map =
        tab_stats_data_store_->AddInterval();
    // Setup the timer associated with this interval.
    std::unique_ptr<base::RepeatingTimer> timer =
        std::make_unique<base::RepeatingTimer>();
    timer->Start(
        FROM_HERE, interval,
        base::BindRepeating(&TabStatsTracker::OnInterval,
                            base::Unretained(this), interval, interval_map));
    usage_interval_timers_.push_back(std::move(timer));
  }

// The native window occlusion calculation is specific to Windows.
#if defined(OS_WIN)
  native_window_occlusion_timer_.Start(
      FROM_HERE, kNativeWindowOcclusionCalculationInterval,
      base::BindRepeating(
          &TabStatsTracker::CalculateAndRecordNativeWindowVisibilities,
          base::Unretained(this)));
#endif

  heartbeat_timer_.Start(FROM_HERE, kTabsHeartbeatReportingInterval,
                         base::BindRepeating(&TabStatsTracker::OnHeartbeatEvent,
                                             base::Unretained(this)));
}

TabStatsTracker::~TabStatsTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BrowserList::GetInstance()->RemoveObserver(this);
  base::PowerMonitor::RemovePowerSuspendObserver(this);
}

// static
void TabStatsTracker::SetInstance(std::unique_ptr<TabStatsTracker> instance) {
  DCHECK_EQ(nullptr, g_tab_stats_tracker_instance);
  g_tab_stats_tracker_instance = instance.release();
}

TabStatsTracker* TabStatsTracker::GetInstance() {
  return g_tab_stats_tracker_instance;
}

void TabStatsTracker::AddObserverAndSetInitialState(
    TabStatsObserver* observer) {
  tab_stats_observers_.AddObserver(observer);

  // Initialization of |this| is complete at this point and all existing
  // Browsers are already observed. TabStatsObserver functions are called
  // directly only for |observer| which is new and needs to be caught up to the
  // current state.
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    observer->OnWindowAdded();
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      auto* wc = browser->tab_strip_model()->GetWebContentsAt(i);
      observer->OnTabAdded(wc);
      if (wc->GetCurrentlyPlayingVideoCount())
        observer->OnVideoStartedPlaying(wc);
      if (wc->IsCurrentlyAudible())
        observer->OnTabIsAudibleChanged(wc);
      if (wc->IsFullscreen() && wc->HasActiveEffectivelyFullscreenVideo())
        observer->OnMediaEffectivelyFullscreenChanged(wc, true);
    }
  }
}

void TabStatsTracker::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(::prefs::kTabStatsTotalTabCountMax, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsMaxTabsPerWindow, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsWindowCountMax, 0);
  DailyEvent::RegisterPref(registry, ::prefs::kTabStatsDailySample);
}

void TabStatsTracker::SetDelegateForTesting(
    std::unique_ptr<TabStatsTrackerDelegate> new_delegate) {
  delegate_ = std::move(new_delegate);
}

void TabStatsTracker::TabStatsDailyObserver::OnDailyEvent(
    DailyEvent::IntervalType type) {
  reporting_delegate_->ReportDailyMetrics(data_store_->tab_stats());
  data_store_->ResetMaximumsToCurrentState();
}

class TabStatsTracker::WebContentsUsageObserver
    : public content::WebContentsObserver {
 public:
  WebContentsUsageObserver(content::WebContents* web_contents,
                           TabStatsTracker* tab_stats_tracker)
      : content::WebContentsObserver(web_contents),
        tab_stats_tracker_(tab_stats_tracker),
        ukm_source_id_(ukm::GetSourceIdForWebContentsDocument(web_contents)) {}

  WebContentsUsageObserver(const WebContentsUsageObserver&) = delete;
  WebContentsUsageObserver& operator=(const WebContentsUsageObserver&) = delete;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    // Treat browser-initiated navigations as user interactions.
    if (!navigation_handle->IsRendererInitiated()) {
      for (TabStatsObserver& tab_stats_observer :
           tab_stats_tracker_->tab_stats_observers_) {
        tab_stats_observer.OnTabInteraction(web_contents());
      }
    }
  }

  // TODO(crbug.com/1245014): Change this to PrimaryPageChanged and use
  // RFH::GetUkmPageSourceId instead of navigation_handle->GetNavigationId() for
  // the Ukm source id.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->HasCommitted() ||
        !navigation_handle->IsInPrimaryMainFrame() ||
        navigation_handle->IsSameDocument()) {
      return;
    }
    // Update navigation time for UKM reporting.
    navigation_time_ = navigation_handle->NavigationStart();
    ukm_source_id_ = ukm::ConvertToSourceId(
        navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);

    // Update observers.
    for (TabStatsObserver& tab_stats_observer :
         tab_stats_tracker_->tab_stats_observers_) {
      tab_stats_observer.OnPrimaryMainFrameNavigationCommitted(web_contents());
    }
  }

  void DidGetUserInteraction(const blink::WebInputEvent& event) override {
    for (TabStatsObserver& tab_stats_observer :
         tab_stats_tracker_->tab_stats_observers_) {
      tab_stats_observer.OnTabInteraction(web_contents());
    }
  }

  void OnVisibilityChanged(content::Visibility visibility) override {
    for (TabStatsObserver& tab_stats_observer :
         tab_stats_tracker_->tab_stats_observers_) {
      tab_stats_observer.OnTabVisibilityChanged(web_contents());
    }
  }

  void WebContentsDestroyed() override {
    if (ukm_source_id_) {
      ukm::builders::TabManager_TabLifetime(ukm_source_id_)
          .SetTimeSinceNavigation(
              (base::TimeTicks::Now() - navigation_time_).InMilliseconds())
          .Record(ukm::UkmRecorder::Get());
    }

    tab_stats_tracker_->OnWebContentsDestroyed(web_contents());
    // The call above will free |this| and so nothing should be done on this
    // object starting from here.
  }

  void OnAudioStateChanged(bool audible) override {
    for (TabStatsObserver& tab_stats_observer :
         tab_stats_tracker_->tab_stats_observers_) {
      tab_stats_observer.OnTabIsAudibleChanged(web_contents());
    }
  }

  void MediaEffectivelyFullscreenChanged(bool is_fullscreen) override {
    for (TabStatsObserver& tab_stats_observer :
         tab_stats_tracker_->tab_stats_observers_) {
      tab_stats_observer.OnMediaEffectivelyFullscreenChanged(web_contents(),
                                                             is_fullscreen);
    }
  }

  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& media_type,
      const content::MediaPlayerId& id) override {
    if (!media_type.has_video)
      return;
    video_playing_count_++;
    if (video_playing_count_ == 1) {
      for (TabStatsObserver& tab_stats_observer :
           tab_stats_tracker_->tab_stats_observers_) {
        tab_stats_observer.OnVideoStartedPlaying(web_contents());
      }
    }
  }

  void MediaStoppedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& media_type,
      const content::MediaPlayerId& id,
      content::WebContentsObserver::MediaStoppedReason reason) override {
    if (!media_type.has_video)
      return;
    video_playing_count_--;
    if (video_playing_count_ == 0) {
      for (TabStatsObserver& tab_stats_observer :
           tab_stats_tracker_->tab_stats_observers_) {
        tab_stats_observer.OnVideoStoppedPlaying(web_contents());
      }
    }
  }

 private:
  raw_ptr<TabStatsTracker> tab_stats_tracker_;
  // The last navigation time associated with this tab.
  base::TimeTicks navigation_time_ = base::TimeTicks::Now();
  // Updated when a navigation is finished.
  ukm::SourceId ukm_source_id_ = 0;
  // The number of video currently playing in this tab.
  size_t video_playing_count_ = 0;
};

void TabStatsTracker::OnBrowserAdded(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (TabStatsObserver& tab_stats_observer : tab_stats_observers_) {
    tab_stats_observer.OnWindowAdded();
  }
  browser->tab_strip_model()->AddObserver(this);
}

void TabStatsTracker::OnBrowserRemoved(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (TabStatsObserver& tab_stats_observer : tab_stats_observers_) {
    tab_stats_observer.OnWindowRemoved();
  }
  browser->tab_strip_model()->RemoveObserver(this);
}

void TabStatsTracker::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents)
      OnInitialOrInsertedTab(contents.contents);

    tab_stats_data_store_->UpdateMaxTabsPerWindowIfNeeded(
        static_cast<size_t>(tab_strip_model->count()));

    return;
  }

  if (change.type() == TabStripModelChange::kReplaced) {
    auto* replace = change.GetReplace();
    for (TabStatsObserver& tab_stats_observer : tab_stats_observers_) {
      tab_stats_observer.OnTabReplaced(replace->old_contents,
                                       replace->new_contents);
    }
    web_contents_usage_observers_.insert(std::make_pair(
        replace->new_contents, std::make_unique<WebContentsUsageObserver>(
                                   replace->new_contents, this)));
    web_contents_usage_observers_.erase(replace->old_contents);
  }
}

void TabStatsTracker::OnResume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reporting_delegate_->ReportTabCountOnResume(
      tab_stats_data_store_->tab_stats().total_tab_count);
}

void TabStatsTracker::OnInterval(
    base::TimeDelta interval,
    TabStatsDataStore::TabsStateDuringIntervalMap* interval_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(interval_map);
  reporting_delegate_->ReportUsageDuringInterval(*interval_map, interval);

  // Only |tab_stats_data_store_| (and not other obsevers) resets since only it
  // computes intervals.
  tab_stats_data_store_->ResetIntervalData(interval_map);
}

void TabStatsTracker::OnInitialOrInsertedTab(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If we already have a WebContentsObserver for this tab then it means that
  // it's already tracked and it's being dragged into a new window, there's
  // nothing to do here.
  if (!base::Contains(web_contents_usage_observers_, web_contents)) {
    for (TabStatsObserver& tab_stats_observer : tab_stats_observers_) {
      tab_stats_observer.OnTabAdded(web_contents);
    }
    web_contents_usage_observers_.insert(std::make_pair(
        web_contents,
        std::make_unique<WebContentsUsageObserver>(web_contents, this)));
  }
}

void TabStatsTracker::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(web_contents_usage_observers_, web_contents));
  web_contents_usage_observers_.erase(
      web_contents_usage_observers_.find(web_contents));
  for (TabStatsObserver& tab_stats_observer : tab_stats_observers_) {
    tab_stats_observer.OnTabRemoved(web_contents);
  }
}

void TabStatsTracker::OnHeartbeatEvent() {
  reporting_delegate_->ReportHeartbeatMetrics(
      tab_stats_data_store_->tab_stats());
}

void TabStatsTracker::UmaStatsReportingDelegate::ReportTabCountOnResume(
    size_t tab_count) {
  // Don't report the number of tabs on resume if Chrome is running in
  // background with no visible window.
  if (IsChromeBackgroundedWithoutWindows())
    return;
  UmaHistogramCounts10000WithBatteryStateVariant(
      kNumberOfTabsOnResumeHistogramName, tab_count);
}

void TabStatsTracker::UmaStatsReportingDelegate::ReportDailyMetrics(
    const TabStatsDataStore::TabsStats& tab_stats) {
  // Don't report the counts if they're equal to 0, this means that Chrome has
  // only been running in the background since the last time the metrics have
  // been reported.
  if (tab_stats.total_tab_count_max == 0)
    return;
  UmaHistogramCounts10000WithBatteryStateVariant(kMaxTabsInADayHistogramName,
                                                 tab_stats.total_tab_count_max);
  UmaHistogramCounts10000WithBatteryStateVariant(
      kMaxTabsPerWindowInADayHistogramName, tab_stats.max_tab_per_window);
  UmaHistogramCounts10000WithBatteryStateVariant(kMaxWindowsInADayHistogramName,
                                                 tab_stats.window_count_max);
}

void TabStatsTracker::UmaStatsReportingDelegate::ReportHeartbeatMetrics(
    const TabStatsDataStore::TabsStats& tab_stats) {
  // Don't report anything if Chrome is running in background with no visible
  // window.
  if (IsChromeBackgroundedWithoutWindows())
    return;

  UmaHistogramCounts10000WithBatteryStateVariant(kTabCountHistogramName,
                                                 tab_stats.total_tab_count);
  UmaHistogramCounts10000WithBatteryStateVariant(kWindowCountHistogramName,
                                                 tab_stats.window_count);
  int collapsed_tab_count = 0;

  // Record the width of all open browser windows with tabs.
  for (Browser* browser : *BrowserList::GetInstance()) {
    TabGroupModel* const tab_group_model =
        browser->tab_strip_model()->group_model();
    const std::vector<tab_groups::TabGroupId>& groups =
        tab_group_model->ListTabGroups();
    for (const tab_groups::TabGroupId& group_id : groups) {
      const TabGroup* const tab_group = tab_group_model->GetTabGroup(group_id);
      if (tab_group->visual_data()->is_collapsed())
        collapsed_tab_count += tab_group->ListTabs().length();
    }

    if (browser->type() != Browser::TYPE_NORMAL)
      continue;

    const BrowserWindow* window = browser->window();

    // Only consider visible windows.
    if (!window->IsVisible() || window->IsMinimized())
      continue;

    // Get the window's size (in DIPs).
    const gfx::Size window_size = browser->window()->GetBounds().size();

    // If the size is for some reason 0 in either dimension, skip it.
    if (window_size.IsEmpty())
      continue;

    // A 4K screen is 4096 pixels wide. Doubling this and rounding up to
    // 10000 should give a reasonable upper bound on DIPs. For the
    // minimum width, pick an arbitrary value of 100. Most screens are
    // unlikely to be this small, and likewise a browser window's min
    // width is around this size.
    UMA_HISTOGRAM_CUSTOM_COUNTS(kWindowWidthHistogramName, window_size.width(),
                                100, 10000, 50);
  }

  base::UmaHistogramCustomCounts(kCollapsedTabHistogramName,
                                 collapsed_tab_count, 1, 200, 50);
}

void TabStatsTracker::UmaStatsReportingDelegate::ReportUsageDuringInterval(
    const TabStatsDataStore::TabsStateDuringIntervalMap& interval_map,
    base::TimeDelta interval) {
  // Counts the number of used/unused tabs during this interval, a tabs counts
  // as unused if it hasn't been interacted with or visible during the duration
  // of the interval.
  size_t used_tabs = 0;
  size_t used_and_closed_tabs = 0;
  size_t unused_tabs = 0;
  size_t unused_and_closed_tabs = 0;
  for (const auto& iter : interval_map) {
    // There's currently no distinction between a visible/audible tab and one
    // that has been interacted with in these metrics.
    // TODO(sebmarchand): Add a metric that track the number of tab that have
    // been visible/audible but not interacted with during an interval,
    // https://crbug.com/800828.
    if (iter.second.interacted_during_interval ||
        iter.second.visible_or_audible_during_interval) {
      if (iter.second.exists_currently)
        ++used_tabs;
      else
        ++used_and_closed_tabs;
    } else {
      if (iter.second.exists_currently)
        ++unused_tabs;
      else
        ++unused_and_closed_tabs;
    }
  }

  std::string used_and_closed_histogram_name = GetIntervalHistogramName(
      UmaStatsReportingDelegate::kUsedAndClosedInIntervalHistogramNameBase,
      interval);
  std::string used_histogram_name = GetIntervalHistogramName(
      UmaStatsReportingDelegate::kUsedTabsInIntervalHistogramNameBase,
      interval);
  std::string unused_and_closed_histogram_name = GetIntervalHistogramName(
      UmaStatsReportingDelegate::kUnusedAndClosedInIntervalHistogramNameBase,
      interval);
  std::string unused_histogram_name = GetIntervalHistogramName(
      UmaStatsReportingDelegate::kUnusedTabsInIntervalHistogramNameBase,
      interval);

  base::UmaHistogramCounts10000(used_and_closed_histogram_name,
                                used_and_closed_tabs);
  base::UmaHistogramCounts10000(used_histogram_name, used_tabs);
  base::UmaHistogramCounts10000(unused_and_closed_histogram_name,
                                unused_and_closed_tabs);
  base::UmaHistogramCounts10000(unused_histogram_name, unused_tabs);
}

// static
std::string
TabStatsTracker::UmaStatsReportingDelegate::GetIntervalHistogramName(
    const char* base_name,
    base::TimeDelta interval) {
  DCHECK(IsValidInterval(interval));
  return base::StringPrintf("%s_%zu", base_name,
                            static_cast<size_t>(interval.InSeconds()));
}

bool TabStatsTracker::UmaStatsReportingDelegate::
    IsChromeBackgroundedWithoutWindows() {
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  if (g_browser_process && g_browser_process->background_mode_manager()
                               ->IsBackgroundWithoutWindows()) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)
  return false;
}

}  // namespace metrics
