// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_
#define COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/arc/mojom/metrics.mojom.h"
#include "components/arc/mojom/process.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/connection_observer.h"
#include "components/guest_os/guest_os_engagement_metrics.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/events/ozone/gamepad/gamepad_observer.h"
#include "ui/wm/public/activation_change_observer.h"

class BrowserContextKeyedServiceFactory;

namespace aura {
class Window;
}  // namespace aura

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

namespace mojom {
class AppInstance;
class IntentHelperInstance;
}  // namespace mojom

// Collects information from other ArcServices and send UMA metrics.
class ArcMetricsService : public KeyedService,
                          public wm::ActivationChangeObserver,
                          public mojom::MetricsHost,
                          public ui::GamepadObserver {
 public:
  using HistogramNamer =
      base::RepeatingCallback<std::string(const std::string& base_name)>;

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

  // Sets the histogram namer. Required to not have a dependency on browser
  // codebase.
  void SetHistogramNamer(HistogramNamer histogram_namer);

  // Implementations for ConnectionObserver<mojom::ProcessInstance>.
  void OnProcessConnectionReady();
  void OnProcessConnectionClosed();

  // MetricsHost overrides.
  void ReportBootProgress(std::vector<mojom::BootProgressEventPtr> events,
                          mojom::BootType boot_type) override;
  void ReportNativeBridge(mojom::NativeBridgeType native_bridge_type) override;
  void ReportCompanionLibApiUsage(mojom::CompanionLibApiId api_id) override;

  // wm::ActivationChangeObserver overrides.
  // Records to UMA when a user has interacted with an ARC app window.
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // ui::GamepadObserver overrides.
  void OnGamepadEvent(const ui::GamepadEvent& event) override;

  // ArcAppListPrefs::Observer callbacks which are called through
  // ArcMetricsServiceProxy.
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent);
  void OnTaskDestroyed(int32_t task_id);

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

  class ArcBridgeServiceObserver : public arc::ArcBridgeService::Observer {
   public:
    ArcBridgeServiceObserver();
    ~ArcBridgeServiceObserver() override;

    // Whether the arc bridge is in the process of closing.
    bool arc_bridge_closing_ = false;

   private:
    // arc::ArcBridgeService::Observer overrides.
    void BeforeArcBridgeClosed() override;
    void AfterArcBridgeClosed() override;
    DISALLOW_COPY_AND_ASSIGN(ArcBridgeServiceObserver);
  };

  class IntentHelperObserver
      : public ConnectionObserver<mojom::IntentHelperInstance> {
   public:
    IntentHelperObserver(ArcMetricsService* arc_metrics_service,
                         ArcBridgeServiceObserver* arc_bridge_service_observer);
    ~IntentHelperObserver() override;

   private:
    // arc::internal::ConnectionObserver<mojom::IntentHelperInstance>
    // overrides.
    void OnConnectionClosed() override;

    ArcMetricsService* arc_metrics_service_;
    ArcBridgeServiceObserver* arc_bridge_service_observer_;

    DISALLOW_COPY_AND_ASSIGN(IntentHelperObserver);
  };

  class AppLauncherObserver : public ConnectionObserver<mojom::AppInstance> {
   public:
    AppLauncherObserver(ArcMetricsService* arc_metrics_service,
                        ArcBridgeServiceObserver* arc_bridge_service_observer);
    ~AppLauncherObserver() override;

   private:
    // arc::internal::ConnectionObserver<mojom::IntentHelperInstance>
    // overrides.
    void OnConnectionClosed() override;

    ArcMetricsService* arc_metrics_service_;
    ArcBridgeServiceObserver* arc_bridge_service_observer_;

    DISALLOW_COPY_AND_ASSIGN(AppLauncherObserver);
  };

  void RequestProcessList();
  void ParseProcessList(std::vector<mojom::RunningAppProcessInfoPtr> processes);

  // DBus callbacks.
  void OnArcStartTimeRetrieved(std::vector<mojom::BootProgressEventPtr> events,
                               mojom::BootType boot_type,
                               base::Optional<base::TimeTicks> arc_start_time);

  THREAD_CHECKER(thread_checker_);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  // Helper class for tracking engagement metrics.
  guest_os::GuestOsEngagementMetrics guest_os_engagement_metrics_;

  // A function that appends a suffix to the base of a histogram name based on
  // the current user profile.
  HistogramNamer histogram_namer_;

  ProcessObserver process_observer_;
  base::RepeatingTimer request_process_list_timer_;

  ArcBridgeServiceObserver arc_bridge_service_observer_;
  IntentHelperObserver intent_helper_observer_;
  AppLauncherObserver app_launcher_observer_;

  bool was_arc_window_active_ = false;
  std::vector<int32_t> task_ids_;

  bool gamepad_interaction_recorded_ = false;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<ArcMetricsService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcMetricsService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_
