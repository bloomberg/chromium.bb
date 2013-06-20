// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFIER_SETTINGS_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFIER_SETTINGS_VIEW_H_

#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notifier_settings.h"
#include "ui/message_center/views/message_bubble_base.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

namespace message_center {

// A class to show the list of notifier extensions / URL patterns and allow
// users to customize the settings.
class MESSAGE_CENTER_EXPORT NotifierSettingsView
    : public NotifierSettingsObserver,
      public views::View,
      public views::ButtonListener {
 public:
  explicit NotifierSettingsView(NotifierSettingsProvider* provider);
  virtual ~NotifierSettingsView();

  bool IsScrollable();

  // Overridden from NotifierSettingsDelegate:
  virtual void UpdateIconImage(const std::string& id,
                               const gfx::Image& icon) OVERRIDE;
  virtual void UpdateFavicon(const GURL& url, const gfx::Image& icon) OVERRIDE;

  void set_provider(NotifierSettingsProvider* new_provider) {
    provider_ = new_provider;
  }

 private:
  class NotifierButton;

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetMinimumSize() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual bool OnMouseWheel(const ui::MouseWheelEvent& event) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  views::ImageButton* title_arrow_;
  views::View* title_entry_;
  views::ScrollView* scroller_;
  NotifierSettingsProvider* provider_;
  std::set<NotifierButton*> buttons_;

  DISALLOW_COPY_AND_ASSIGN(NotifierSettingsView);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFIER_SETTINGS_VIEW_H_
