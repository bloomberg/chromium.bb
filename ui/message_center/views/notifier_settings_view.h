// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFIER_SETTINGS_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFIER_SETTINGS_VIEW_H_

#include <set>
#include <string>

#include "base/string16.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget_delegate.h"

namespace message_center {

class NotifierSettingsViewDelegate;

// A class to show the list of notifier extensions / URL patterns and allow
// users to customize the settings.
class MESSAGE_CENTER_EXPORT NotifierSettingsView
    : public views::WidgetDelegateView,
      public views::ButtonListener {
 public:
  // Create a new widget of the notifier settings and returns it. Note that
  // the widget and the view is self-owned. It'll be deleted when it's closed
  // or the chrome's shutdown.
  static NotifierSettingsView* Create(NotifierSettingsViewDelegate* delegate,
                                      gfx::NativeView context);

  void UpdateIconImage(const std::string& id, const gfx::ImageSkia& icon);
  void UpdateFavicon(const GURL& url, const gfx::ImageSkia& icon);

  void set_delegate(NotifierSettingsViewDelegate* new_delegate) {
    delegate_ = new_delegate;
  }

 private:
  class NotifierButton;

  NotifierSettingsView(NotifierSettingsViewDelegate* delegate);
  virtual ~NotifierSettingsView();

  // views::WidgetDelegate overrides:
  virtual bool CanResize() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  NotifierSettingsViewDelegate* delegate_;
  std::set<NotifierButton*> buttons_;

  DISALLOW_COPY_AND_ASSIGN(NotifierSettingsView);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFIER_SETTINGS_VIEW_H_
