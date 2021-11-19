// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_USER_SERVICE_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_USER_SERVICE_H_

#include <memory>
#include <set>
#include <string>

#include "chrome/browser/ash/child_accounts/time_limits/app_activity_report_interface.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_interface.h"
#include "chrome/browser/ash/child_accounts/website_approval_notifier.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace enterprise_management {
class ChildStatusReportRequest;
}  // namespace enterprise_management

class GURL;
class Profile;

namespace ash {
namespace app_time {
class AppId;
class AppTimeController;
class WebTimeLimitEnforcer;
}  // namespace app_time

// Facade that exposes child user related functionality on Chrome OS.
// TODO(crbug.com/1022231): Migrate ConsumerStatusReportingService,
// EventBasedStatusReporting and ScreenTimeController to ChildUserService.
class ChildUserService : public KeyedService,
                         public app_time::AppTimeLimitInterface,
                         public app_time::AppActivityReportInterface {
 public:
  // Used for tests to get internal implementation details.
  class TestApi {
   public:
    explicit TestApi(ChildUserService* service);
    ~TestApi();

    app_time::WebTimeLimitEnforcer* web_time_enforcer();
    app_time::AppTimeController* app_time_controller();

   private:
    ChildUserService* const service_;
  };

  // These enum values represent the current Family Link user's time limit
  // policy type for the Family Experiences team's metrics. Multiple time
  // limit policy types might be enabled at the same time. These values are
  // logged to UMA. Entries should not be renumbered and numeric values should
  // never be reused. Please keep in sync with "TimeLimitPolicyType" in
  // src/tools/metrics/histograms/enums.xml.
  enum class TimeLimitPolicyType {
    kNoTimeLimit = 0,
    kOverrideTimeLimit = 1,
    kBedTimeLimit = 2,
    kScreenTimeLimit = 3,
    kWebTimeLimit = 4,
    kAppTimeLimit = 5,  // Does not cover blocked apps.

    // Used for UMA. Update kMaxValue to the last value. Add future entries
    // above this comment. Sync with enums.xml.
    kMaxValue = kAppTimeLimit
  };

  // Family Link helper(for child and teens) is an app available to supervised
  // users and the companion app of Family Link app(for parents).
  static const char kFamilyLinkHelperAppPackageName[];
  static const char kFamilyLinkHelperAppPlayStoreURL[];

  static const char* GetTimeLimitPolicyTypesHistogramNameForTest();

  explicit ChildUserService(content::BrowserContext* context);
  ChildUserService(const ChildUserService&) = delete;
  ChildUserService& operator=(const ChildUserService&) = delete;
  ~ChildUserService() override;

  // app_time::AppTimeLimitInterface:
  void PauseWebActivity(const std::string& app_service_id) override;
  void ResumeWebActivity(const std::string& app_service_id) override;
  absl::optional<base::TimeDelta> GetTimeLimitForApp(
      const std::string& app_service_id,
      apps::mojom::AppType app_type) override;

  // app_time::AppActivityReportInterface:
  app_time::AppActivityReportInterface::ReportParams GenerateAppActivityReport(
      enterprise_management::ChildStatusReportRequest* report) override;
  void AppActivityReportSubmitted(
      base::Time report_generation_timestamp) override;

  // Returns whether web time limit was reached for child user.
  // Always returns false if per-app times limits feature is disabled.
  bool WebTimeLimitReached() const;

  // Returns whether given |url| can be used without any time restrictions.
  // Viewing of allowlisted |url| does not count towards usage web time.
  // Always returns false if per-app times limits feature is disabled.
  bool WebTimeLimitAllowlistedURL(const GURL& url) const;

  // Returns whether the application with id |app_id| can be used without any
  // time restrictions.
  bool AppTimeLimitAllowlistedApp(const app_time::AppId& app_id) const;

  // Returns time limit set for using the web on a given day.
  // Should only be called if |features::kPerAppTimeLimits| and
  // |features::kWebTimeLimits| features are enabled.
  base::TimeDelta GetWebTimeLimit() const;

  // Report enabled TimeLimitPolicyType.
  void ReportTimeLimitPolicy() const;

 private:
  // KeyedService:
  void Shutdown() override;

  Profile* const profile_;

  std::unique_ptr<app_time::AppTimeController> app_time_controller_;

  // Preference changes observer.
  PrefChangeRegistrar pref_change_registrar_;

  // Used to display notifications when new websites are approved.
  WebsiteApprovalNotifier website_approval_notifier_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromOS code migration is done.
namespace chromeos {
using ::ash::ChildUserService;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_USER_SERVICE_H_
