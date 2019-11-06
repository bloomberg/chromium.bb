// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/machine_level_user_cloud_policy_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/cloud/machine_level_user_cloud_policy_helper.h"
#include "chrome/browser/policy/machine_level_user_cloud_policy_register_watcher.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_metrics.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_WIN)
#include "chrome/install_static/install_util.h"
#endif

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/common/chrome_switches.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/app_controller_mac.h"
#endif

namespace policy {

namespace {

void RecordEnrollmentResult(
    MachineLevelUserCloudPolicyEnrollmentResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "Enterprise.MachineLevelUserCloudPolicyEnrollment.Result", result);
}

// Read the kCloudPolicyOverridesPlatformPolicy from platform provider directly
// because the local_state is not ready when the
// MachineLevelUserCloudPolicyManager is created.
bool DoesCloudPolicyHasPriority(
    ConfigurationPolicyProvider* platform_provider) {
  if (!platform_provider)
    return false;
  const auto* entry =
      platform_provider->policies()
          .Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
          .Get(key::kCloudPolicyOverridesPlatformPolicy);
  if (!entry || entry->scope == POLICY_SCOPE_USER ||
      entry->level == POLICY_LEVEL_RECOMMENDED)
    return false;

  return entry->value->is_bool() && entry->value->GetBool();
}

}  // namespace

const base::FilePath::CharType
    MachineLevelUserCloudPolicyController::kPolicyDir[] =
        FILE_PATH_LITERAL("Policy");

bool MachineLevelUserCloudPolicyController::
    IsMachineLevelUserCloudPolicyEnabled() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableMachineLevelUserCloudPolicy);
#endif
}

MachineLevelUserCloudPolicyController::MachineLevelUserCloudPolicyController() {
}
MachineLevelUserCloudPolicyController::
    ~MachineLevelUserCloudPolicyController() {}

// static
std::unique_ptr<MachineLevelUserCloudPolicyManager>
MachineLevelUserCloudPolicyController::CreatePolicyManager(
    ConfigurationPolicyProvider* platform_provider) {
  if (!IsMachineLevelUserCloudPolicyEnabled())
    return nullptr;

  std::string enrollment_token =
      BrowserDMTokenStorage::Get()->RetrieveEnrollmentToken();
  std::string dm_token = BrowserDMTokenStorage::Get()->RetrieveDMToken();
  std::string client_id = BrowserDMTokenStorage::Get()->RetrieveClientId();

  VLOG(1) << "DM token = " << (dm_token.empty() ? "none" : "from persistence");
  VLOG(1) << "Enrollment token = " << enrollment_token;
  VLOG(1) << "Client ID = " << client_id;

  if (enrollment_token.empty() && dm_token.empty())
    return nullptr;

  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return nullptr;

  DVLOG(1) << "Creating machine level cloud policy manager";

  bool cloud_policy_has_priority =
      DoesCloudPolicyHasPriority(platform_provider);
  if (cloud_policy_has_priority) {
    DVLOG(1) << "Cloud policies are now overriding platform policies with "
                "machine scope.";
  }

  base::FilePath policy_dir =
      user_data_dir.Append(MachineLevelUserCloudPolicyController::kPolicyDir);
  std::unique_ptr<MachineLevelUserCloudPolicyStore> policy_store =
      MachineLevelUserCloudPolicyStore::Create(
          dm_token, client_id, policy_dir, cloud_policy_has_priority,
          base::CreateSequencedTaskRunner(
              {base::ThreadPool(), base::MayBlock(),
               base::TaskPriority::BEST_EFFORT,
               // Block shutdown to make sure the policy cache update is always
               // finished.
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
  return std::make_unique<MachineLevelUserCloudPolicyManager>(
      std::move(policy_store), nullptr, policy_dir,
      base::ThreadTaskRunnerHandle::Get(),
      base::BindRepeating(&content::GetNetworkConnectionTracker));
}

void MachineLevelUserCloudPolicyController::Init(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {

  if (!IsMachineLevelUserCloudPolicyEnabled())
    return;

  MachineLevelUserCloudPolicyManager* policy_manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
  DeviceManagementService* device_management_service =
      g_browser_process->browser_policy_connector()
          ->device_management_service();

  if (!policy_manager)
    return;
  // If there exists an enrollment token, then there are two states:
  //   1/ There also exists a DM token.  This machine is already registered, so
  //      the next step is to fetch policies.
  //   2/ There is no DM token.  In this case the machine is not already
  //      registered and needs to request a DM token.
  std::string enrollment_token;
  std::string client_id;
  std::string dm_token = BrowserDMTokenStorage::Get()->RetrieveDMToken();

  if (!dm_token.empty()) {
    policy_fetcher_ = std::make_unique<MachineLevelUserCloudPolicyFetcher>(
        policy_manager, local_state, device_management_service,
        url_loader_factory);
    return;
  }

  if (!GetEnrollmentTokenAndClientId(&enrollment_token, &client_id))
    return;

  DCHECK(!enrollment_token.empty());
  DCHECK(!client_id.empty());

  policy_registrar_ = std::make_unique<MachineLevelUserCloudPolicyRegistrar>(
      device_management_service, url_loader_factory);
  policy_fetcher_ = std::make_unique<MachineLevelUserCloudPolicyFetcher>(
      policy_manager, local_state, device_management_service,
      url_loader_factory);

  if (dm_token.empty()) {
    policy_register_watcher_ =
        std::make_unique<MachineLevelUserCloudPolicyRegisterWatcher>(this);

    enrollment_start_time_ = base::Time::Now();

    // Not registered already, so do it now.
    policy_registrar_->RegisterForPolicyWithEnrollmentToken(
        enrollment_token, client_id,
        base::Bind(&MachineLevelUserCloudPolicyController::
                       RegisterForPolicyWithEnrollmentTokenCallback,
                   base::Unretained(this)));
#if defined(OS_WIN)
    // This metric is only published on Windows to indicate how many user level
    // installs try to enroll, as these can't store the DM token
    // in the registry at the end of enrollment. Mac and Linux do not need
    // this metric for now as they might use a different token storage mechanism
    // in the future.
    UMA_HISTOGRAM_BOOLEAN(
        "Enterprise.MachineLevelUserCloudPolicyEnrollment.InstallLevel_Win",
        install_static::IsSystemInstall());
#endif
  }
}

bool MachineLevelUserCloudPolicyController::
    WaitUntilPolicyEnrollmentFinished() {
  if (policy_register_watcher_) {
    switch (
        policy_register_watcher_->WaitUntilCloudPolicyEnrollmentFinished()) {
      case RegisterResult::kNoEnrollmentNeeded:
      case RegisterResult::kEnrollmentSuccessBeforeDialogDisplayed:
      case RegisterResult::kEnrollmentFailedSilentlyBeforeDialogDisplayed:
        return true;
      case RegisterResult::kEnrollmentSuccess:
      case RegisterResult::kEnrollmentFailedSilently:
#if defined(OS_MACOSX)
        app_controller_mac::EnterpriseStartupDialogClosed();
#endif
        return true;
      case RegisterResult::kRestartDueToFailure:
        chrome::AttemptRestart();
        return false;
      case RegisterResult::kQuitDueToFailure:
        chrome::AttemptExit();
        return false;
    }
  }
  return true;
}

void MachineLevelUserCloudPolicyController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MachineLevelUserCloudPolicyController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool MachineLevelUserCloudPolicyController::IsEnterpriseStartupDialogShowing() {
  return policy_register_watcher_ &&
         policy_register_watcher_->IsDialogShowing();
}

void MachineLevelUserCloudPolicyController::NotifyPolicyRegisterFinished(
    bool succeeded) {
  for (auto& observer : observers_) {
    observer.OnPolicyRegisterFinished(succeeded);
  }
}

bool MachineLevelUserCloudPolicyController::GetEnrollmentTokenAndClientId(
    std::string* enrollment_token,
    std::string* client_id) {
  *client_id = BrowserDMTokenStorage::Get()->RetrieveClientId();
  if (client_id->empty())
    return false;

  *enrollment_token = BrowserDMTokenStorage::Get()->RetrieveEnrollmentToken();
  return !enrollment_token->empty();
}

void MachineLevelUserCloudPolicyController::
    RegisterForPolicyWithEnrollmentTokenCallback(const std::string& dm_token,
                                                 const std::string& client_id) {
  base::TimeDelta enrollment_time = base::Time::Now() - enrollment_start_time_;

  if (dm_token.empty()) {
    VLOG(1) << "No DM token returned from browser registration.";
    RecordEnrollmentResult(
        MachineLevelUserCloudPolicyEnrollmentResult::kFailedToFetch);
    UMA_HISTOGRAM_TIMES(
        "Enterprise.MachineLevelUserCloudPolicyEnrollment.RequestFailureTime",
        enrollment_time);
    MachineLevelUserCloudPolicyManager* policy_manager =
        g_browser_process->browser_policy_connector()
            ->machine_level_user_cloud_policy_manager();
    if (policy_manager)
      policy_manager->store()->InitWithoutToken();
    NotifyPolicyRegisterFinished(false);
    return;
  }

  VLOG(1) << "DM token retrieved from server.";

  UMA_HISTOGRAM_TIMES(
      "Enterprise.MachineLevelUserCloudPolicyEnrollment.RequestSuccessTime",
      enrollment_time);

  // TODO(alito): Log failures to store the DM token. Should we try again later?
  BrowserDMTokenStorage::Get()->StoreDMToken(
      dm_token, base::BindOnce([](bool success) {
        if (!success) {
          DVLOG(1) << "Failed to store the DM token";
          RecordEnrollmentResult(
              MachineLevelUserCloudPolicyEnrollmentResult::kFailedToStore);
        } else {
          DVLOG(1) << "Successfully stored the DM token";
          RecordEnrollmentResult(
              MachineLevelUserCloudPolicyEnrollmentResult::kSuccess);
        }
      }));

  // Start fetching policies.
  VLOG(1) << "Fetch policy after enrollment.";
  policy_fetcher_->SetupRegistrationAndFetchPolicy(dm_token, client_id);
  NotifyPolicyRegisterFinished(true);
}

}  // namespace policy
