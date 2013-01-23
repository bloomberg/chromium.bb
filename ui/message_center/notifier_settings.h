// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFIER_SETTINGS_H_
#define UI_MESSAGE_CENTER_NOTIFIER_SETTINGS_H_

#include <map>
#include <string>
#include <vector>

#include "base/string16.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget_delegate.h"

namespace message_center {

// A class to show the list of notifier extensions / URL patterns and allow
// users to customize the settings.
class MESSAGE_CENTER_EXPORT NotifierSettingsView
    : public views::WidgetDelegateView,
      public views::ButtonListener {
 public:
  struct MESSAGE_CENTER_EXPORT Notifier {
    enum NotifierType {
      APPLICATION,
      URL_PATTERN,
    };

    Notifier(const std::string& id, NotifierType type, const string16& name);

    // The identifier of the notifier. The extension id or URL pattern itself.
    std::string id;

    // The human-readable name of the notifier such like the extension name.
    // It can be empty.
    string16 name;

    // True if the source is allowed to send notifications. True is default.
    bool enabled;

    // The type of notifier: Chrome app or URL pattern.
    NotifierType type;

    // The icon image of the notifier. The extension icon or favicon.
    gfx::ImageSkia icon;
  };

  class MESSAGE_CENTER_EXPORT Delegate {
   public:
    // Collects the current notifier list and fills to |notifiers|.
    virtual void GetNotifierList(std::vector<Notifier>* notifiers) = 0;

    // Called when the |enabled| for the |id| has been changed by user
    // operation.
    virtual void SetNotifierEnabled(const std::string& id, bool enabled) = 0;

    // Called when the settings window is closed.
    virtual void OnNotifierSettingsClosing(NotifierSettingsView* view) = 0;
  };

  // Create a new widget of the notifier settings and returns it. Note that
  // the widget and the view is self-owned. It'll be deleted when it's closed
  // or the chrome's shutdown.
  static NotifierSettingsView* Create(Delegate* delegate);

  void UpdateIconImage(const std::string& id, const gfx::ImageSkia& icon);

 private:
  class NotifierButton;

  NotifierSettingsView(Delegate* delegate);
  virtual ~NotifierSettingsView();

  // views::WidgetDelegate overrides:
  virtual bool CanResize() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  Delegate* delegate_;
  std::set<NotifierButton*> buttons_;

  DISALLOW_COPY_AND_ASSIGN(NotifierSettingsView);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_NOTIFIER_SETTINGS_H_
