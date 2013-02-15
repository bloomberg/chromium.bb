// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFIER_SETTINGS_VIEW_DELEGATE_H_
#define UI_MESSAGE_CENTER_NOTIFIER_SETTINGS_VIEW_DELEGATE_H_

#include <string>

#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center_export.h"

namespace message_center {

// The struct to hold the information of notifiers. The information will be
// used by NotifierSettingsView.
struct MESSAGE_CENTER_EXPORT Notifier {
  enum NotifierType {
    APPLICATION,
    WEB_PAGE,
  };

  // Constructor for APPLICATION type.
  Notifier(const std::string& id, const string16& name, bool enabled);

  // Constructor for WEB_PAGE type.
  Notifier(const GURL& url, const string16& name, bool enabled);

  ~Notifier();

  // The identifier of the app notifier. Empty if it's URL_PATTERN.
  std::string id;

  // The URL pattern of the notifer.
  GURL url;

  // The human-readable name of the notifier such like the extension name.
  // It can be empty.
  string16 name;

  // True if the source is allowed to send notifications. True is default.
  bool enabled;

  // The type of notifier: Chrome app or URL pattern.
  NotifierType type;

  // The icon image of the notifier. The extension icon or favicon.
  gfx::ImageSkia icon;

 private:
  DISALLOW_COPY_AND_ASSIGN(Notifier);
};

// The delegate class for NotifierSettingsView.
class MESSAGE_CENTER_EXPORT NotifierSettingsViewDelegate {
 public:
  // Collects the current notifier list and fills to |notifiers|. Caller takes
  // the ownership of the elements of |notifiers|.
  virtual void GetNotifierList(std::vector<Notifier*>* notifiers) = 0;

  // Called when the |enabled| for the |notifier| has been changed by user
  // operation.
  virtual void SetNotifierEnabled(const Notifier& notifier, bool enabled) = 0;

  // Called when the settings window is closed.
  virtual void OnNotifierSettingsClosing() = 0;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_NOTIFIER_SETTINGS_VIEW_DELEGATE_H_
