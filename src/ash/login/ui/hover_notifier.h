// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_HOVER_NOTIFIER_H_
#define ASH_LOGIN_UI_HOVER_NOTIFIER_H_

#include <string>

#include "base/callback.h"
#include "ui/events/event_handler.h"

namespace views {
class View;
}

namespace ash {

// Runs a callback whenever a view has gained or lost mouse hover.
// TODO(jdufault): see if we can replace this class with views::MouseWatcher.
class HoverNotifier : public ui::EventHandler {
 public:
  using OnHover = base::RepeatingCallback<void(bool has_hover)>;

  HoverNotifier(views::View* target_view, const OnHover& on_hover);
  ~HoverNotifier() override;

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override;

 private:
  bool had_hover_ = false;
  views::View* target_view_ = nullptr;
  OnHover on_hover_;

  DISALLOW_COPY_AND_ASSIGN(HoverNotifier);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_HOVER_NOTIFIER_H_
