// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_H_

#include <string>
#include <vector>

#include "base/string16.h"
#include "base/time.h"
#include "base/values.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notification_types.h"

namespace message_center {

struct MESSAGE_CENTER_EXPORT NotificationItem {
  string16 title;
  string16 message;

  NotificationItem(const string16& title, const string16& message);
};

struct MESSAGE_CENTER_EXPORT ButtonInfo {
  string16 title;
  gfx::ImageSkia icon;

  ButtonInfo(const string16& title);
};

class MESSAGE_CENTER_EXPORT Notification {
 public:
  Notification(NotificationType type,
               const std::string& id,
               const string16& title,
               const string16& message,
               const string16& display_source,
               const std::string& extension_id,
               const DictionaryValue* optional_fields);  // May be NULL.
  virtual ~Notification();

  NotificationType type() const { return type_; }
  const std::string& id() const { return id_; }
  const string16& title() const { return title_; }
  const string16& message() const { return message_; }
  const string16& display_source() const { return display_source_; }
  const std::string& extension_id() const { return extension_id_; }

  // Begin unpacked values from optional_fields.
  int priority() const { return priority_; }
  base::Time timestamp() const { return timestamp_; }
  const string16& expanded_message() const { return expanded_message_; }
  const std::vector<NotificationItem>& items() const { return items_; }
  // End unpacked values.

  // Images fetched asynchronously.
  const gfx::ImageSkia& primary_icon() const { return primary_icon_; }
  void set_primary_icon(const gfx::ImageSkia& icon) { primary_icon_ = icon; }

  const gfx::ImageSkia& image() const { return image_; }
  void set_image(const gfx::ImageSkia& image) { image_ = image; }

  // Buttons, with icons fetched asynchronously.
  const std::vector<ButtonInfo>& buttons() const { return buttons_; }
  bool SetButtonIcon(size_t index, const gfx::ImageSkia& icon);

  // Status in MessageCenter.
  bool is_read() const { return is_read_; }
  void set_is_read(bool is_read) { is_read_ = is_read; }

  bool shown_as_popup() const { return shown_as_popup_; }
  void set_shown_as_popup(bool shown_as_popup) {
    shown_as_popup_ = shown_as_popup;
  }

  // Used to keep the order of notifications with the same timestamp.
  // The notification with lesser serial_number is considered 'older'.
  unsigned serial_number() { return serial_number_; }

 private:
  // Unpacks the provided |optional_fields| and applies the values to override
  // the notification's data members.
  void ApplyOptionalFields(const DictionaryValue* optional_fields);

  NotificationType type_;
  std::string id_;
  string16 title_;
  string16 message_;
  string16 display_source_;
  std::string extension_id_;
  int priority_;
  base::Time timestamp_;
  unsigned serial_number_;
  string16 expanded_message_;
  std::vector<NotificationItem> items_;
  gfx::ImageSkia primary_icon_;
  gfx::ImageSkia image_;
  std::vector<ButtonInfo> buttons_;
  bool is_read_;  // True if this has been seen in the message center.
  bool shown_as_popup_;  // True if this has been shown as a popup notification.

  DISALLOW_COPY_AND_ASSIGN(Notification);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_NOTIFICATION_H_
