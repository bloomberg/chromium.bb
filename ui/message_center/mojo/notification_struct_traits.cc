// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/mojo/notification_struct_traits.h"

#include "mojo/common/string16_struct_traits.h"
#include "ui/gfx/image/mojo/image_skia_struct_traits.h"
#include "url/mojo/url_gurl_struct_traits.h"

namespace mojo {

using message_center::mojom::NotificationDataView;
using message_center::mojom::RichNotificationDataDataView;
using message_center::Notification;
using message_center::RichNotificationData;
using NotificationStructTraits =
    StructTraits<NotificationDataView, Notification>;
using RichNotificationDataStructTraits =
    StructTraits<RichNotificationDataDataView, RichNotificationData>;

// static
int RichNotificationDataStructTraits::progress(
    const message_center::RichNotificationData& r) {
  return r.progress;
}

// static
const base::string16& RichNotificationDataStructTraits::progress_status(
    const message_center::RichNotificationData& r) {
  return r.progress_status;
}

// static
bool RichNotificationDataStructTraits::
    should_make_spoken_feedback_for_popup_updates(
        const message_center::RichNotificationData& r) {
  return r.should_make_spoken_feedback_for_popup_updates;
}

// static
bool RichNotificationDataStructTraits::clickable(
    const message_center::RichNotificationData& r) {
  return r.clickable;
}

// static
bool RichNotificationDataStructTraits::pinned(
    const message_center::RichNotificationData& r) {
  return r.pinned;
}

// static
const base::string16& RichNotificationDataStructTraits::accessible_name(
    const message_center::RichNotificationData& r) {
  return r.accessible_name;
}

// static
SkColor RichNotificationDataStructTraits::accent_color(
    const message_center::RichNotificationData& r) {
  return r.accent_color;
}

// static
bool RichNotificationDataStructTraits::Read(RichNotificationDataDataView data,
                                            RichNotificationData* out) {
  out->progress = data.progress();
  out->should_make_spoken_feedback_for_popup_updates =
      data.should_make_spoken_feedback_for_popup_updates();
  out->clickable = data.clickable();
  out->pinned = data.pinned();
  out->accent_color = data.accent_color();
  return data.ReadProgressStatus(&out->progress_status) &&
         data.ReadAccessibleName(&out->accessible_name);
}

// static
message_center::NotificationType NotificationStructTraits::type(
    const Notification& n) {
  return n.type();
}

// static
const std::string& NotificationStructTraits::id(const Notification& n) {
  return n.id();
}

// static
const base::string16& NotificationStructTraits::title(const Notification& n) {
  return n.title();
}

// static
const base::string16& NotificationStructTraits::message(const Notification& n) {
  return n.message();
}

// static
gfx::ImageSkia NotificationStructTraits::icon(const Notification& n) {
  return n.icon().AsImageSkia();
}

// static
const base::string16& NotificationStructTraits::display_source(
    const Notification& n) {
  return n.display_source();
}

// static
const GURL& NotificationStructTraits::origin_url(const Notification& n) {
  return n.origin_url();
}

// static
const RichNotificationData& NotificationStructTraits::optional_fields(
    const Notification& n) {
  return n.rich_notification_data();
}

// static
bool NotificationStructTraits::Read(NotificationDataView data,
                                    Notification* out) {
  gfx::ImageSkia icon;
  if (!data.ReadIcon(&icon))
    return false;
  out->set_icon(gfx::Image(icon));
  return EnumTraits<message_center::mojom::NotificationType,
                    message_center::NotificationType>::FromMojom(data.type(),
                                                                 &out->type_) &&
         data.ReadId(&out->id_) && data.ReadTitle(&out->title_) &&
         data.ReadMessage(&out->message_) &&
         data.ReadDisplaySource(&out->display_source_) &&
         data.ReadOriginUrl(&out->origin_url_) &&
         data.ReadOptionalFields(&out->optional_fields_);
}

}  // namespace mojo
