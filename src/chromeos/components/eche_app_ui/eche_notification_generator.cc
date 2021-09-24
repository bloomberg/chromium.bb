// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/eche_notification_generator.h"

#include "chromeos/components/eche_app_ui/launch_app_helper.h"
#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {
namespace eche_app {

EcheNotificationGenerator::EcheNotificationGenerator(
    LaunchAppHelper* launch_app_helper)
    : launch_app_helper_(launch_app_helper) {}

EcheNotificationGenerator::~EcheNotificationGenerator() = default;

void EcheNotificationGenerator::ShowNotification(
    const std::u16string& title,
    const std::u16string& message,
    chromeos::eche_app::mojom::WebNotificationType type) {
  launch_app_helper_->ShowNotification(
      title, message,
      std::make_unique<LaunchAppHelper::NotificationInfo>(
          LaunchAppHelper::NotificationInfo::Category::kWebUI, type));
}

void EcheNotificationGenerator::Bind(
    mojo::PendingReceiver<mojom::NotificationGenerator> receiver) {
  notification_receiver_.reset();
  notification_receiver_.Bind(std::move(receiver));
}

}  // namespace eche_app
}  // namespace chromeos
