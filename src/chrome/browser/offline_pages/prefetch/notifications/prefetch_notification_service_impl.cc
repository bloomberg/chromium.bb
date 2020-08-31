// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_service_impl.h"

#include "base/bind.h"

#include "chrome/browser/notifications/scheduler/public/client_overview.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/notifications/scheduler/public/throttle_config.h"
#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_service_bridge.h"

namespace offline_pages {
namespace prefetch {
namespace {
void BuildNotificationData(const base::string16& title,
                           const base::string16& body,
                           notifications::NotificationData* out) {
  DCHECK(out);
  out->title = title;
  out->message = body;
}

notifications::ScheduleParams BuildScheduleParams() {
  notifications::ScheduleParams schedule_params;
  // Explicit dismissing, clicking unhelpful button, and not interacting for a
  // while(1 day at first stage, 7 days at second stage) are considered as
  // negative feedback.
  // TODO(hesen): Support timeout config for implicit dismiss(ignore) in
  // framework.
  schedule_params.impression_mapping.emplace(
      notifications::UserFeedback::kDismiss,
      notifications::ImpressionResult::kNegative);
  schedule_params.deliver_time_start = base::make_optional(base::Time::Now());
  schedule_params.deliver_time_end =
      base::make_optional(base::Time::Now() + base::TimeDelta::FromMinutes(1));
  return schedule_params;
}

}  // namespace

PrefetchNotificationServiceImpl::PrefetchNotificationServiceImpl(
    notifications::NotificationScheduleService* schedule_service,
    std::unique_ptr<PrefetchNotificationServiceBridge> bridge)
    : schedule_service_(schedule_service), bridge_(std::move(bridge)) {
  DCHECK(schedule_service_);
}

PrefetchNotificationServiceImpl::~PrefetchNotificationServiceImpl() = default;

void PrefetchNotificationServiceImpl::Schedule(const base::string16& title,
                                               const base::string16& body) {
  schedule_service_->GetClientOverview(
      notifications::SchedulerClientType::kPrefetch,
      base::BindOnce(&PrefetchNotificationServiceImpl::ScheduleInternal,
                     weak_ptr_factory_.GetWeakPtr(), title, body));
}

void PrefetchNotificationServiceImpl::ScheduleInternal(
    const base::string16& title,
    const base::string16& body,
    notifications::ClientOverview overview) {
  // Do nothing if under throttle.
  if (overview.impression_detail.current_max_daily_show == 0)
    return;

  notifications::NotificationData data;
  BuildNotificationData(title, body, &data);
  auto params = std::make_unique<notifications::NotificationParams>(
      notifications::SchedulerClientType::kPrefetch, std::move(data),
      BuildScheduleParams());
  params->enable_ihnr_buttons = true;
  schedule_service_->Schedule(std::move(params));
}

void PrefetchNotificationServiceImpl::OnClick() {
  bridge_->LaunchDownloadHome();
}

void PrefetchNotificationServiceImpl::GetThrottleConfig(
    ThrottleConfigCallback callback) {
  schedule_service_->GetClientOverview(
      notifications::SchedulerClientType::kPrefetch,
      base::BindOnce(&PrefetchNotificationServiceImpl::OnClientOverviewQueried,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PrefetchNotificationServiceImpl::OnClientOverviewQueried(
    ThrottleConfigCallback callback,
    notifications::ClientOverview client_overview) {
  auto res = std::make_unique<notifications::ThrottleConfig>();
  // At first stage 3 consecutive dismiss(default in config) will lead a 7 days
  // suppression. After that we show max 1 notification per week.
  res->suppression_duration = base::TimeDelta::FromDays(7);
  if (client_overview.impression_detail.num_negative_events > 0) {
    res->negative_action_count_threshold = 1;
  }
  std::move(callback).Run(std::move(res));
}

}  // namespace prefetch
}  // namespace offline_pages
