// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_reporting_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/ash/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/util/status.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "url/gurl.h"

namespace policy {
// TODO(1187477, marcgrimme): revisit if this should be refactored.
DlpPolicyEvent_Mode DlpRulesManagerLevel2DlpEventMode(
    DlpRulesManager::Level level) {
  switch (level) {
    case DlpRulesManager::Level::kBlock:
      return DlpPolicyEvent_Mode_BLOCK;
    case DlpRulesManager::Level::kWarn:
      return DlpPolicyEvent_Mode_BLOCK;
    case DlpRulesManager::Level::kReport:
      return DlpPolicyEvent_Mode_REPORT;
    case DlpRulesManager::Level::kNotSet:
    case DlpRulesManager::Level::kAllow:
      return DlpPolicyEvent_Mode_UNDEFINED_MODE;
  }
}

// TODO(1187477, marcgrimme): revisit if this should be refactored.
DlpPolicyEvent_Restriction DlpRulesManagerRestriction2DlpEventRestriction(
    DlpRulesManager::Restriction restriction) {
  switch (restriction) {
    case DlpRulesManager::Restriction::kPrinting:
      return DlpPolicyEvent_Restriction_PRINTING;
    case DlpRulesManager::Restriction::kScreenshot:
      return DlpPolicyEvent_Restriction_SCREENSHOT;
    case DlpRulesManager::Restriction::kScreenShare:
      return DlpPolicyEvent_Restriction_SCREENCAST;
    case DlpRulesManager::Restriction::kPrivacyScreen:
      return DlpPolicyEvent_Restriction_EPRIVACY;
    case DlpRulesManager::Restriction::kClipboard:
      return DlpPolicyEvent_Restriction_CLIPBOARD;
    case DlpRulesManager::Restriction::kFiles:
    case DlpRulesManager::Restriction::kUnknownRestriction:
      return DlpPolicyEvent_Restriction_UNDEFINED_RESTRICTION;
  }
}

// TODO(1187477, marcgrimme): revisit if this should be refactored.
DlpRulesManager::Restriction DlpEventRestriction2DlpRulesManagerRestriction(
    DlpPolicyEvent_Restriction restriction) {
  switch (restriction) {
    case DlpPolicyEvent_Restriction_PRINTING:
      return DlpRulesManager::Restriction::kPrinting;
    case DlpPolicyEvent_Restriction_SCREENSHOT:
      return DlpRulesManager::Restriction::kScreenshot;
    case DlpPolicyEvent_Restriction_SCREENCAST:
      return DlpRulesManager::Restriction::kScreenShare;
    case DlpPolicyEvent_Restriction_EPRIVACY:
      return DlpRulesManager::Restriction::kPrivacyScreen;
    case DlpPolicyEvent_Restriction_CLIPBOARD:
      return DlpRulesManager::Restriction::kClipboard;
    case DlpPolicyEvent_Restriction_UNDEFINED_RESTRICTION:
      return DlpRulesManager::Restriction::kUnknownRestriction;
  }
}

DlpPolicyEvent_UserType GetCurrentUserType() {
  // Could be not initialized in tests.
  if (!user_manager::UserManager::IsInitialized() ||
      !user_manager::UserManager::Get()->GetPrimaryUser()) {
    return DlpPolicyEvent_UserType_UNDEFINED_USER_TYPE;
  }
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(user);
  switch (user->GetType()) {
    case user_manager::USER_TYPE_REGULAR:
      return DlpPolicyEvent_UserType_REGULAR;
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
      return DlpPolicyEvent_UserType_MANAGED_GUEST;
    case user_manager::USER_TYPE_KIOSK_APP:
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      return DlpPolicyEvent_UserType_KIOSK;
    default:
      NOTREACHED();
      return DlpPolicyEvent_UserType_UNDEFINED_USER_TYPE;
  }
}

DlpPolicyEvent CreateDlpPolicyEvent(const std::string& src_pattern,
                                    DlpRulesManager::Restriction restriction,
                                    DlpRulesManager::Level level) {
  DlpPolicyEvent event;

  DlpPolicyEventSource* event_source = new DlpPolicyEventSource;
  event_source->set_url(src_pattern);
  event.set_allocated_source(event_source);

  event.set_restriction(
      DlpRulesManagerRestriction2DlpEventRestriction(restriction));
  event.set_mode(DlpRulesManagerLevel2DlpEventMode(level));
  event.set_timestamp_micro(base::Time::Now().ToTimeT());
  event.set_user_type(GetCurrentUserType());

  return event;
}

DlpPolicyEvent CreateDlpPolicyEvent(const std::string& src_pattern,
                                    const std::string& dst_pattern,
                                    DlpRulesManager::Restriction restriction,
                                    DlpRulesManager::Level level) {
  auto event = CreateDlpPolicyEvent(src_pattern, restriction, level);

  DlpPolicyEventDestination* event_destination = new DlpPolicyEventDestination;
  event_destination->set_url(dst_pattern);
  event.set_allocated_destination(event_destination);

  return event;
}

DlpPolicyEvent CreateDlpPolicyEvent(const std::string& src_pattern,
                                    DlpRulesManager::Component dst_component,
                                    DlpRulesManager::Restriction restriction,
                                    DlpRulesManager::Level level) {
  auto event = CreateDlpPolicyEvent(src_pattern, restriction, level);

  DlpPolicyEventDestination* event_destination = new DlpPolicyEventDestination;
  switch (dst_component) {
    case (DlpRulesManager::Component::kArc):
      event_destination->set_component(DlpPolicyEventDestination_Component_ARC);
      break;
    case (DlpRulesManager::Component::kCrostini):
      event_destination->set_component(
          DlpPolicyEventDestination_Component_CROSTINI);
      break;
    case (DlpRulesManager::Component::kPluginVm):
      event_destination->set_component(
          DlpPolicyEventDestination_Component_PLUGIN_VM);
      break;
    case (DlpRulesManager::Component::kUnknownComponent):
      event_destination->set_component(
          DlpPolicyEventDestination_Component_UNDEFINED_COMPONENT);
      break;
  }
  event.set_allocated_destination(event_destination);

  return event;
}

DlpReportingManager::DlpReportingManager() : report_queue_{nullptr} {}
DlpReportingManager::~DlpReportingManager() = default;

DlpReportingManager::ReportQueueSetterCallback
DlpReportingManager::GetReportQueueSetter() {
  return base::BindOnce(&DlpReportingManager::SetReportQueue,
                        weak_factory_.GetWeakPtr());
}

void DlpReportingManager::SetReportQueue(
    std::unique_ptr<reporting::ReportQueue> report_queue) {
  report_queue_ = std::move(report_queue);
}

void DlpReportingManager::ReportEvent(const std::string& src_pattern,
                                      DlpRulesManager::Restriction restriction,
                                      DlpRulesManager::Level level) {
  auto event = CreateDlpPolicyEvent(src_pattern, restriction, level);
  ReportEvent(std::move(event));
}

void DlpReportingManager::ReportEvent(const std::string& src_pattern,
                                      const std::string& dst_pattern,
                                      DlpRulesManager::Restriction restriction,
                                      DlpRulesManager::Level level) {
  auto event =
      CreateDlpPolicyEvent(src_pattern, dst_pattern, restriction, level);
  ReportEvent(std::move(event));
}

void DlpReportingManager::ReportEvent(
    const std::string& src_pattern,
    const DlpRulesManager::Component dst_component,
    DlpRulesManager::Restriction restriction,
    DlpRulesManager::Level level) {
  auto event =
      CreateDlpPolicyEvent(src_pattern, dst_component, restriction, level);
  ReportEvent(std::move(event));
}

void DlpReportingManager::OnEventEnqueued(reporting::Status status) {
  if (!status.ok()) {
    VLOG(1) << "Could not enqueue event to DLP reporting queue because of "
            << status;
  }
  events_reported_++;
  base::UmaHistogramEnumeration(
      GetDlpHistogramPrefix() + dlp::kReportedEventStatus, status.code(),
      reporting::error::Code::MAX_VALUE);
}

void DlpReportingManager::ReportEvent(DlpPolicyEvent event) {
  // TODO(1187506, marcgrimme) Refactor to handle gracefully with user
  // interaction when queue is not ready.
  if (!report_queue_.get()) {
    DLOG(WARNING) << "Report queue could not be initialized. DLP reporting "
                     "functionality will be disabled.";
    return;
  }
  reporting::ReportQueue::EnqueueCallback callback = base::BindOnce(
      &DlpReportingManager::OnEventEnqueued, base::Unretained(this));
  report_queue_->Enqueue(&event, reporting::Priority::SLOW_BATCH,
                         std::move(callback));

  switch (event.mode()) {
    case DlpPolicyEvent_Mode_BLOCK:
      base::UmaHistogramEnumeration(
          GetDlpHistogramPrefix() + dlp::kReportedBlockLevelRestriction,
          DlpEventRestriction2DlpRulesManagerRestriction(event.restriction()));
      break;
    case DlpPolicyEvent_Mode_REPORT:
      base::UmaHistogramEnumeration(
          GetDlpHistogramPrefix() + dlp::kReportedReportLevelRestriction,
          DlpEventRestriction2DlpRulesManagerRestriction(event.restriction()));
      break;
    case DlpPolicyEvent_Mode_UNDEFINED_MODE:
      NOTREACHED();
      break;
  }
}

}  // namespace policy
