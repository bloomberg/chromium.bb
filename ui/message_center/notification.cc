// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification.h"

#include <memory>

#include "base/logging.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/strings/grit/ui_strings.h"

namespace message_center {

namespace {

unsigned g_next_serial_number_ = 0;

const gfx::ImageSkia CreateSolidColorImage(int width,
                                           int height,
                                           SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

gfx::Image DeepCopyImage(const gfx::Image& image) {
  if (image.IsEmpty())
    return gfx::Image();
  std::unique_ptr<gfx::ImageSkia> image_skia(image.CopyImageSkia());
  return gfx::Image(*image_skia);
}

}  // namespace

NotificationItem::NotificationItem(const base::string16& title,
                                   const base::string16& message)
    : title(title),
      message(message) {
}

ButtonInfo::ButtonInfo(const base::string16& title)
    : title(title) {
}

ButtonInfo::ButtonInfo(const ButtonInfo& other) = default;

ButtonInfo::~ButtonInfo() = default;

ButtonInfo& ButtonInfo::operator=(const ButtonInfo& other) = default;

RichNotificationData::RichNotificationData() : timestamp(base::Time::Now()) {}

RichNotificationData::RichNotificationData(const RichNotificationData& other)
    : priority(other.priority),
      never_timeout(other.never_timeout),
      timestamp(other.timestamp),
      context_message(other.context_message),
      image(other.image),
      small_image(other.small_image),
      vector_small_image(other.vector_small_image),
      items(other.items),
      progress(other.progress),
      progress_status(other.progress_status),
      buttons(other.buttons),
      should_make_spoken_feedback_for_popup_updates(
          other.should_make_spoken_feedback_for_popup_updates),
      clickable(other.clickable),
#if defined(OS_CHROMEOS)
      pinned(other.pinned),
#endif  // defined(OS_CHROMEOS)
      vibration_pattern(other.vibration_pattern),
      renotify(other.renotify),
      silent(other.silent),
      accessible_name(other.accessible_name),
      accent_color(other.accent_color),
      use_image_as_icon(other.use_image_as_icon),
      settings_button_handler(other.settings_button_handler),
      fullscreen_visibility(other.fullscreen_visibility) {
}

RichNotificationData::~RichNotificationData() = default;

Notification::Notification() = default;

Notification::Notification(NotificationType type,
                           const std::string& id,
                           const base::string16& title,
                           const base::string16& message,
                           const gfx::Image& icon,
                           const base::string16& display_source,
                           const GURL& origin_url,
                           const NotifierId& notifier_id,
                           const RichNotificationData& optional_fields,
                           scoped_refptr<NotificationDelegate> delegate)
    : type_(type),
      id_(id),
      title_(title),
      message_(message),
      icon_(icon),
      display_source_(display_source),
      origin_url_(origin_url),
      notifier_id_(notifier_id),
      serial_number_(g_next_serial_number_++),
      optional_fields_(optional_fields),
      shown_as_popup_(false),
      is_read_(false),
      delegate_(std::move(delegate)) {}

Notification::Notification(const std::string& id, const Notification& other)
    : type_(other.type_),
      id_(id),
      title_(other.title_),
      message_(other.message_),
      icon_(other.icon_),
      display_source_(other.display_source_),
      origin_url_(other.origin_url_),
      notifier_id_(other.notifier_id_),
      serial_number_(other.serial_number_),
      optional_fields_(other.optional_fields_),
      shown_as_popup_(other.shown_as_popup_),
      is_read_(other.is_read_),
      delegate_(other.delegate_) {}

Notification::Notification(const Notification& other)
    : type_(other.type_),
      id_(other.id_),
      title_(other.title_),
      message_(other.message_),
      icon_(other.icon_),
      display_source_(other.display_source_),
      origin_url_(other.origin_url_),
      notifier_id_(other.notifier_id_),
      serial_number_(other.serial_number_),
      optional_fields_(other.optional_fields_),
      shown_as_popup_(other.shown_as_popup_),
      is_read_(other.is_read_),
      delegate_(other.delegate_) {}

Notification& Notification::operator=(const Notification& other) {
  type_ = other.type_;
  id_ = other.id_;
  title_ = other.title_;
  message_ = other.message_;
  icon_ = other.icon_;
  display_source_ = other.display_source_;
  origin_url_ = other.origin_url_;
  notifier_id_ = other.notifier_id_;
  serial_number_ = other.serial_number_;
  optional_fields_ = other.optional_fields_;
  shown_as_popup_ = other.shown_as_popup_;
  is_read_ = other.is_read_;
  delegate_ = other.delegate_;

  return *this;
}

Notification::~Notification() = default;

// static
std::unique_ptr<Notification> Notification::DeepCopy(
    const Notification& notification,
    bool include_body_image,
    bool include_small_image,
    bool include_icon_images) {
  std::unique_ptr<Notification> notification_copy =
      std::make_unique<Notification>(notification);
  notification_copy->set_icon(DeepCopyImage(notification_copy->icon()));
  notification_copy->set_image(include_body_image
                                   ? DeepCopyImage(notification_copy->image())
                                   : gfx::Image());
  notification_copy->set_small_image(
      include_small_image ? notification_copy->small_image() : gfx::Image());
  for (size_t i = 0; i < notification_copy->buttons().size(); i++) {
    notification_copy->SetButtonIcon(
        i, include_icon_images
               ? DeepCopyImage(notification_copy->buttons()[i].icon)
               : gfx::Image());
  }
  return notification_copy;
}

bool Notification::IsRead() const {
  return is_read_ || optional_fields_.priority == MIN_PRIORITY;
}

void Notification::CopyState(Notification* base) {
  shown_as_popup_ = base->shown_as_popup();
  is_read_ = base->is_read_;
  if (!delegate_.get())
    delegate_ = base->delegate();
  optional_fields_.never_timeout = base->never_timeout();
}

void Notification::SetButtonIcon(size_t index, const gfx::Image& icon) {
  if (index >= optional_fields_.buttons.size())
    return;
  optional_fields_.buttons[index].icon = icon;
}

void Notification::SetSystemPriority() {
  optional_fields_.priority = SYSTEM_PRIORITY;
  optional_fields_.never_timeout = true;
}

bool Notification::UseOriginAsContextMessage() const {
  return optional_fields_.context_message.empty() && origin_url_.is_valid() &&
         origin_url_.SchemeIsHTTPOrHTTPS();
}

gfx::Image Notification::GenerateMaskedSmallIcon(int dip_size,
                                                 SkColor color) const {
  if (!vector_small_image().is_empty())
    return gfx::Image(
        gfx::CreateVectorIcon(vector_small_image(), dip_size, color));

  if (small_image().IsEmpty())
    return gfx::Image();

  // If |vector_small_image| is not available, fallback to raster based
  // masking and resizing.
  gfx::ImageSkia image = small_image().AsImageSkia();
  gfx::ImageSkia masked = gfx::ImageSkiaOperations::CreateMaskedImage(
      CreateSolidColorImage(image.width(), image.height(), color), image);
  gfx::ImageSkia resized = gfx::ImageSkiaOperations::CreateResizedImage(
      masked, skia::ImageOperations::ResizeMethod::RESIZE_BEST,
      gfx::Size(dip_size, dip_size));
  return gfx::Image(resized);
}

// static
std::unique_ptr<Notification> Notification::CreateSystemNotification(
    const std::string& notification_id,
    const base::string16& title,
    const base::string16& message,
    const gfx::Image& icon,
    const std::string& system_component_id,
    const base::Closure& click_callback) {
  DCHECK(!click_callback.is_null());
  std::unique_ptr<Notification> notification = CreateSystemNotification(
      NOTIFICATION_TYPE_SIMPLE, notification_id, title, message, icon,
      base::string16() /* display_source */, GURL(),
      NotifierId(NotifierId::SYSTEM_COMPONENT, system_component_id),
      RichNotificationData(),
      new HandleNotificationClickDelegate(click_callback), gfx::kNoneIcon,
      SystemNotificationWarningLevel::CRITICAL_WARNING);
  notification->set_clickable(true);
  notification->SetSystemPriority();
  return notification;
}

// static
std::unique_ptr<Notification> Notification::CreateSystemNotification(
    NotificationType type,
    const std::string& id,
    const base::string16& title,
    const base::string16& message,
    const gfx::Image& icon,
    const base::string16& display_source,
    const GURL& origin_url,
    const NotifierId& notifier_id,
    const RichNotificationData& optional_fields,
    scoped_refptr<NotificationDelegate> delegate,
    const gfx::VectorIcon& small_image,
    SystemNotificationWarningLevel color_type) {
  SkColor color = message_center::kSystemNotificationColorNormal;
  switch (color_type) {
    case SystemNotificationWarningLevel::NORMAL:
      color = message_center::kSystemNotificationColorNormal;
      break;
    case SystemNotificationWarningLevel::WARNING:
      color = message_center::kSystemNotificationColorWarning;
      break;
    case SystemNotificationWarningLevel::CRITICAL_WARNING:
      color = message_center::kSystemNotificationColorCriticalWarning;
      break;
  }
  base::string16 display_source_or_default = display_source;
  if (display_source_or_default.empty()) {
    display_source_or_default = l10n_util::GetStringFUTF16(
        IDS_MESSAGE_CENTER_NOTIFICATION_CHROMEOS_SYSTEM,
        MessageCenter::Get()->GetProductOSName());
  }
  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      type, id, title, message, icon, display_source_or_default, origin_url,
      notifier_id, optional_fields, delegate);
  notification->set_accent_color(color);
  notification->set_small_image(
      small_image.is_empty() ? gfx::Image()
                             : gfx::Image(gfx::CreateVectorIcon(
                                   small_image, kSmallImageSizeMD, color)));
  if (!small_image.is_empty())
    notification->set_vector_small_image(small_image);
  return notification;
}

}  // namespace message_center
