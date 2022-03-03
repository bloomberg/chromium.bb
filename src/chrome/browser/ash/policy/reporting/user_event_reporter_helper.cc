// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "components/reporting/client/report_queue_factory.h"

namespace reporting {

UserEventReporterHelper::UserEventReporterHelper(Destination destination)
    : report_queue_(
          ReportQueueFactory::CreateSpeculativeReportQueue(EventType::kDevice,
                                                           destination)) {}

UserEventReporterHelper::UserEventReporterHelper(
    std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> report_queue)
    : report_queue_(std::move(report_queue)) {}

UserEventReporterHelper::~UserEventReporterHelper() = default;

bool UserEventReporterHelper::ShouldReportUser(const std::string& email) const {
  return ash::ChromeUserManager::Get()->ShouldReportUser(email);
}

bool UserEventReporterHelper::ReportingEnabled(
    const std::string& policy_path) const {
  bool enabled = false;
  chromeos::CrosSettings::Get()->GetBoolean(policy_path, &enabled);
  return enabled;
}

void UserEventReporterHelper::ReportEvent(
    const google::protobuf::MessageLite* record,
    Priority priority) {
  if (!report_queue_) {
    DVLOG(1) << "Could not enqueue event: null reporting queue";
    return;
  }

  auto enqueue_cb = base::BindOnce([](Status status) {
    if (!status.ok()) {
      DVLOG(1) << "Could not enqueue event to reporting queue because of: "
               << status;
    }
  });
  report_queue_->Enqueue(record, priority, std::move(enqueue_cb));
}

bool UserEventReporterHelper::IsCurrentUserNew() const {
  return user_manager::UserManager::Get()->IsCurrentUserNew();
}
}  // namespace reporting
