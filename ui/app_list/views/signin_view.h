// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SIGNIN_VIEW_H_
#define UI_APP_LIST_VIEWS_SIGNIN_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/view.h"

namespace gfx {
class Font;
}

namespace app_list {

class SigninDelegate;

// The SigninView is shown in the app list when the user needs to sign in.
class SigninView : public views::View,
                   public views::ButtonListener,
                   public views::LinkListener {
 public:
  SigninView(SigninDelegate* delegate, int width);
  virtual ~SigninView();

 private:
  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener overrides:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  views::Link* learn_more_link_;
  views::Link* settings_link_;
  scoped_ptr<gfx::Font> title_font_;
  scoped_ptr<gfx::Font> text_font_;
  scoped_ptr<gfx::Font> button_font_;

  SigninDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(SigninView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SIGNIN_VIEW_H_
