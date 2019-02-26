// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_
#define COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/arc/common/metrics.mojom.h"
#include "components/arc/common/process.mojom.h"
#include "components/arc/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "ui/wm/public/activation_change_observer.h"

class BrowserContextKeyedServiceFactory;
class PrefService;

namespace aura {
class Window;
}  // namespace aura

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Collects information from other ArcServices and send UMA metrics.
class ArcMetricsService : public KeyedService,
                          public wm::ActivationChangeObserver,
                          public session_manager::SessionManagerObserver,
                          public chromeos::PowerManagerClient::Observer,
                          public mojom::MetricsHost {
 public:
  // These values are persisted to logs, and should therefore never be
  // renumbered nor reused. They are public for testing only.
  enum class NativeBridgeType {
    // Native bridge value has not been received from the container yet.
    UNKNOWN = 0,
    // Native bridge is not used.
    NONE = 1,
    // Using houdini translator.
    HOUDINI = 2,
    // Using ndk-translation translator.
    NDK_TRANSLATION = 3,
    kMaxValue = NDK_TRANSLATION,
  };

  // Delegate for handling window focus observation that is used to track ARC
  // app usage metrics.
  class ArcWindowDelegate {
   public:
    virtual ~ArcWindowDelegate() = default;
    // Returns whether |window| is an ARC window.
    virtual bool IsArcAppWindow(const aura::Window* window) const = 0;
    virtual void RegisterActivationChangeObserver() = 0;
    virtual void UnregisterActivationChangeObserver() = 0;
  };

  // Sets the fake ArcWindowDelegate for testing.
  void SetArcWindowDelegateForTesting(
      std::unique_ptr<ArcWindowDelegate> delegate);

  // Sets Clock for testing.
  void SetClockForTesting(base::Clock* clock);

  // Sets TickClock for testing.
  void SetTickClockForTesting(base::TickClock* tick_clock);

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcMetricsService* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcMetricsService* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  // Returns factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  ArcMetricsService(content::BrowserContext* context,
                    ArcBridgeService* bridge_service);
  ~ArcMetricsService() override;

  // Implementations for ConnectionObserver<mojom::ProcessInstance>.
  void OnProcessConnectionReady();
  void OnProcessConnectionClosed();

  // MetricsHost overrides.
  void ReportBootProgress(std::vector<mojom::BootProgressEventPtr> events,
                          mojom::BootType boot_type) override;
  void ReportNativeBridge(mojom::NativeBridgeType native_bridge_type) override;

  // Records native bridge UMA according to value received from the
  // container or as UNKNOWN if the value has not been recieved yet.
  void RecordNativeBridgeUMA();

  // wm::ActivationChangeObserver overrides.
  // Records to UMA when a user has interacted with an ARC app window.
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // session_manager::SessionManagerObserver overrides.
  void OnSessionStateChanged() override;

  // chromeos::PowerManagerClient::Observer overrides.
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& proto) override;

  // ArcAppListPrefs::Observer callbacks which are called through
  // ArcMetricsServiceProxy.
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent);
  void OnTaskDestroyed(int32_t task_id);

  NativeBridgeType native_bridge_type_for_testing() const {
    return native_bridge_type_;
  }

 private:
  // Adapter to be able to also observe ProcessInstance events.
  class ProcessObserver : public ConnectionObserver<mojom::ProcessInstance> {
   public:
    explicit ProcessObserver(ArcMetricsService* arc_metrics_service);
    ~ProcessObserver() override;

   private:
    // ConnectionObserver<mojom::ProcessInstance> overrides.
    void OnConnectionReady() override;
    void OnConnectionClosed() override;

    ArcMetricsService* arc_metrics_service_;

    DISALLOW_COPY_AND_ASSIGN(ProcessObserver);
  };

  void RequestProcessList();
  void ParseProcessList(std::vector<mojom::RunningAppProcessInfoPtr> processes);

  // DBus callbacks.
  void OnArcStartTimeRetrieved(std::vector<mojom::BootProgressEventPtr> events,
                               mojom::BootType boot_type,
                               base::Optional<base::TimeTicks> arc_start_time);

  // Restores accumulated ARC++ engagement time in previous sessions from
  // profile preferences.
  void RestoreEngagementTimeFromPrefs();

  // Called periodically to save accumulated results to profile preferences.
  void SaveEngagementTimeToPrefs();

  // Called whenever engagement state is changed. Time spent in last state is
  // accumulated to corresponding metrics.
  void UpdateEngagementTime();

  // Records accumulated engagement time metrics to UMA if necessary (i.e. day
  // has changed).
  void RecordEngagementTimeToUmaIfNeeded();

  // Resets accumulated engagement times to zero, and updates both OS version
  // and day ID.
  void ResetEngagementTimePrefs();

  bool ShouldAccumulateEngagementTotalTime() const;
  bool ShouldAccumulateEngagementForegroundTime() const;
  bool ShouldAccumulateEngagementBackgroundTime() const;
  bool ShouldRecordEngagementTimeToUma() const;

  THREAD_CHECKER(thread_checker_);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
  std::unique_ptr<ArcWindowDelegate> arc_window_delegate_;

  ProcessObserver process_observer_;
  base::RepeatingTimer request_process_list_timer_;

  NativeBridgeType native_bridge_type_;

  PrefService* const pref_service_;
  const base::Clock* clock_;
  const base::TickClock* tick_clock_;
  base::RepeatingTimer update_engagement_time_timer_;
  base::RepeatingTimer save_engagement_time_to_prefs_timer_;
  base::TimeTicks last_update_ticks_;

  // States for determining which engagement metrics should we accumulate to.
  bool was_session_active_ = false;
  bool was_screen_dimmed_ = false;
  bool was_arc_window_active_ = false;
  std::vector<int32_t> task_ids_;

  // Accumulated results and associated state which are saved to profile
  // preferences at fixed interval.
  int day_id_ = 0;
  base::TimeDelta engagement_time_total_;
  base::TimeDelta engagement_time_foreground_;
  base::TimeDelta engagement_time_background_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<ArcMetricsService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcMetricsService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_
