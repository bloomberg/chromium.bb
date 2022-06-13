// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/arc_app_install_event_logger.h"

#include <stdint.h>

#include <algorithm>
#include <iterator>

#include "ash/components/arc/arc_prefs.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/policy/reporting/install_event_logger_base.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr int kNonComplianceReasonAppNotInstalled = 5;

std::set<std::string> GetRequestedPackagesFromPolicy(
    const policy::PolicyMap& policy) {
  const base::Value* const arc_enabled = policy.GetValue(key::kArcEnabled);
  if (!arc_enabled || !arc_enabled->is_bool() || !arc_enabled->GetBool()) {
    return {};
  }

  const base::Value* const arc_policy = policy.GetValue(key::kArcPolicy);
  if (!arc_policy || !arc_policy->is_string()) {
    return {};
  }

  return arc::policy_util::GetRequestedPackagesFromArcPolicy(
      arc_policy->GetString());
}

}  // namespace

ArcAppInstallEventLogger::ArcAppInstallEventLogger(Delegate* delegate,
                                                   Profile* profile)
    : InstallEventLoggerBase(profile), delegate_(delegate) {
  if (!arc::IsArcAllowedForProfile(profile_)) {
    AddForSetOfApps(GetPackagesFromPref(arc::prefs::kArcPushInstallAppsPending),
                    CreateEvent(em::AppInstallReportLogEvent::CANCELED));
    Clear(profile_);
    return;
  }

  policy::PolicyService* const policy_service =
      profile_->GetProfilePolicyConnector()->policy_service();
  EvaluatePolicy(policy_service->GetPolicies(policy::PolicyNamespace(
                     policy::POLICY_DOMAIN_CHROME, std::string())),
                 true /* initial */);

  observing_ = true;
  arc::ArcPolicyBridge* bridge =
      arc::ArcPolicyBridge::GetForBrowserContext(profile_);
  bridge->AddObserver(this);
  policy_service->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
}

ArcAppInstallEventLogger::~ArcAppInstallEventLogger() {
  if (log_collector_) {
    log_collector_->OnLogout();
  }
  if (observing_) {
    arc::ArcPolicyBridge::GetForBrowserContext(profile_)->RemoveObserver(this);
    profile_->GetProfilePolicyConnector()->policy_service()->RemoveObserver(
        policy::POLICY_DOMAIN_CHROME, this);
  }
}

// static
void ArcAppInstallEventLogger::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(arc::prefs::kArcPushInstallAppsRequested);
  registry->RegisterListPref(arc::prefs::kArcPushInstallAppsPending);
}

// static
void ArcAppInstallEventLogger::Clear(Profile* profile) {
  profile->GetPrefs()->ClearPref(arc::prefs::kArcPushInstallAppsRequested);
  profile->GetPrefs()->ClearPref(arc::prefs::kArcPushInstallAppsPending);
}

void ArcAppInstallEventLogger::AddForAllPackages(
    std::unique_ptr<em::AppInstallReportLogEvent> event) {
  EnsureTimestampSet(event.get());
  AddForSetOfApps(GetPackagesFromPref(arc::prefs::kArcPushInstallAppsPending),
                  std::move(event));
}

void ArcAppInstallEventLogger::Add(
    const std::string& package,
    bool gather_disk_space_info,
    std::unique_ptr<em::AppInstallReportLogEvent> event) {
  AddEvent(package, gather_disk_space_info, event);
}

void ArcAppInstallEventLogger::OnPolicyUpdated(
    const policy::PolicyNamespace& ns,
    const policy::PolicyMap& previous,
    const policy::PolicyMap& current) {
  EvaluatePolicy(current, false /* initial */);
}

void ArcAppInstallEventLogger::OnPolicySent(const std::string& policy) {
  requested_in_arc_ =
      arc::policy_util::GetRequestedPackagesFromArcPolicy(policy);
}

void ArcAppInstallEventLogger::OnComplianceReportReceived(
    const base::Value* compliance_report) {
  const base::Value* const details = compliance_report->FindKeyOfType(
      "nonComplianceDetails", base::Value::Type::LIST);
  if (!details) {
    return;
  }

  const std::set<std::string> previous_pending =
      GetPackagesFromPref(arc::prefs::kArcPushInstallAppsPending);

  std::set<std::string> pending_in_arc;
  for (const auto& detail : details->GetList()) {
    const base::Value* const reason =
        detail.FindKeyOfType("nonComplianceReason", base::Value::Type::INTEGER);
    if (!reason || reason->GetInt() != kNonComplianceReasonAppNotInstalled) {
      continue;
    }
    const base::Value* const app_name =
        detail.FindKeyOfType("packageName", base::Value::Type::STRING);
    if (!app_name || app_name->GetString().empty()) {
      continue;
    }
    pending_in_arc.insert(app_name->GetString());
  }
  const std::set<std::string> current_pending = GetDifference(
      previous_pending, GetDifference(requested_in_arc_, pending_in_arc));
  const std::set<std::string> removed =
      GetDifference(previous_pending, current_pending);
  AddForSetOfAppsWithDiskSpaceInfo(
      removed, CreateEvent(em::AppInstallReportLogEvent::SUCCESS));

  if (removed.empty()) {
    return;
  }

  SetPref(arc::prefs::kArcPushInstallAppsPending, current_pending);

  if (!current_pending.empty()) {
    UpdateCollector(current_pending);
  } else {
    StopCollector();
  }
}

std::set<std::string> ArcAppInstallEventLogger::GetPackagesFromPref(
    const std::string& pref_name) const {
  std::set<std::string> packages;
  for (const auto& package :
       profile_->GetPrefs()->GetList(pref_name)->GetList()) {
    if (!package.is_string()) {
      continue;
    }
    packages.insert(package.GetString());
  }
  return packages;
}

void ArcAppInstallEventLogger::SetPref(const std::string& pref_name,
                                       const std::set<std::string>& packages) {
  base::Value value(base::Value::Type::LIST);
  for (const std::string& package : packages) {
    value.Append(package);
  }
  profile_->GetPrefs()->Set(pref_name, value);
}

void ArcAppInstallEventLogger::UpdateCollector(
    const std::set<std::string>& pending) {
  if (!log_collector_) {
    log_collector_ = std::make_unique<ArcAppInstallEventLogCollector>(
        this, profile_, pending);
  } else {
    log_collector_->OnPendingPackagesChanged(pending);
  }
}

void ArcAppInstallEventLogger::StopCollector() {
  log_collector_.reset();
}

void ArcAppInstallEventLogger::EvaluatePolicy(const policy::PolicyMap& policy,
                                              bool initial) {
  const std::set<std::string> previous_requested =
      GetPackagesFromPref(arc::prefs::kArcPushInstallAppsRequested);
  const std::set<std::string> previous_pending =
      GetPackagesFromPref(arc::prefs::kArcPushInstallAppsPending);

  const std::set<std::string> current_requested =
      GetRequestedPackagesFromPolicy(policy);

  const std::set<std::string> added =
      GetDifference(current_requested, previous_requested);
  const std::set<std::string> removed =
      GetDifference(previous_pending, current_requested);
  AddForSetOfAppsWithDiskSpaceInfo(
      added, CreateEvent(em::AppInstallReportLogEvent::SERVER_REQUEST));
  AddForSetOfApps(removed, CreateEvent(em::AppInstallReportLogEvent::CANCELED));

  const std::set<std::string> current_pending = GetDifference(
      current_requested, GetDifference(previous_requested, previous_pending));
  SetPref(arc::prefs::kArcPushInstallAppsRequested, current_requested);
  SetPref(arc::prefs::kArcPushInstallAppsPending, current_pending);

  if (!current_pending.empty()) {
    UpdateCollector(current_pending);
    if (initial) {
      log_collector_->OnLogin();
    }
  } else {
    StopCollector();
  }
}

void ArcAppInstallEventLogger::AddForSetOfApps(
    const std::set<std::string>& packages,
    std::unique_ptr<em::AppInstallReportLogEvent> event) {
  delegate_->GetAndroidId(
      base::BindOnce(&ArcAppInstallEventLogger::OnGetAndroidId,
                     weak_factory_.GetWeakPtr(), packages, std::move(event)));
}

void ArcAppInstallEventLogger::OnGetAndroidId(
    const std::set<std::string>& packages,
    std::unique_ptr<em::AppInstallReportLogEvent> event,
    bool ok,
    int64_t android_id) {
  if (ok) {
    event->set_android_id(android_id);
  }
  delegate_->Add(packages, *event);
}

}  // namespace policy
