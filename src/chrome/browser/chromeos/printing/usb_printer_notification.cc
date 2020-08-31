// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/usb_printer_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

const char kNotifierId[] = "printing.usb_printer";

}  // namespace

UsbPrinterNotification::UsbPrinterNotification(
    const Printer& printer,
    const std::string& notification_id,
    Type type,
    Profile* profile)
    : printer_(printer),
      notification_id_(notification_id),
      type_(type),
      profile_(profile) {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.vector_small_image = &kNotificationPrintingIcon;
  rich_notification_data.accent_color = ash::kSystemNotificationColorNormal;

  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id_,
      base::string16(),  // title
      base::string16(),  // body
      gfx::Image(),      // icon
      l10n_util::GetStringUTF16(IDS_PRINT_JOB_NOTIFICATION_DISPLAY_SOURCE),
      GURL(),  // origin_url
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId),
      rich_notification_data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_factory_.GetWeakPtr()));

  UpdateContents();

  ShowNotification();
}

UsbPrinterNotification::~UsbPrinterNotification() = default;

void UsbPrinterNotification::CloseNotification() {
  NotificationDisplayService* display_service =
      NotificationDisplayService::GetForProfile(profile_);
  display_service->Close(NotificationHandler::Type::TRANSIENT,
                         notification_id_);
}

void UsbPrinterNotification::Close(bool by_user) {
  visible_ = false;
}

void UsbPrinterNotification::Click(
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  if (!button_index) {
    // Body of notification clicked.
    visible_ = false;
    if (type_ == Type::kConfigurationRequired) {
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile_, chromeos::settings::mojom::kPrintingDetailsSubpagePath);
    }
    return;
  }

  NOTREACHED();
}

void UsbPrinterNotification::ShowNotification() {
  NotificationDisplayService* display_service =
      NotificationDisplayService::GetForProfile(profile_);
  display_service->Display(NotificationHandler::Type::TRANSIENT, *notification_,
                           /*metadata=*/nullptr);
  visible_ = true;
}

void UsbPrinterNotification::UpdateContents() {
  switch (type_) {
    case Type::kEphemeral:
    case Type::kSaved:
      notification_->set_title(l10n_util::GetStringUTF16(
          IDS_USB_PRINTER_NOTIFICATION_CONNECTED_TITLE));
      notification_->set_message(l10n_util::GetStringFUTF16(
          IDS_USB_PRINTER_NOTIFICATION_CONNECTED_MESSAGE,
          base::UTF8ToUTF16(printer_.display_name())));
      return;
    case Type::kConfigurationRequired:
      notification_->set_title(l10n_util::GetStringUTF16(
          IDS_USB_PRINTER_NOTIFICATION_CONFIGURATION_REQUIRED_TITLE));
      notification_->set_message(l10n_util::GetStringFUTF16(
          IDS_USB_PRINTER_NOTIFICATION_CONFIGURATION_REQUIRED_MESSAGE,
          base::UTF8ToUTF16(printer_.display_name())));
      return;
  }
  NOTREACHED();
}

}  // namespace chromeos
