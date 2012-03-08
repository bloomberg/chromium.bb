// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_DELEGATE_H_
#define UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_DELEGATE_H_
#pragma once

namespace gfx {
class Point;
}

namespace views {

class View;

////////////////////////////////////////////////////////////////////////////////
//
// MenuButtonDelegate
//
// An interface that allows a component to tell a View about a menu that it
// has constructed that the view can show (e.g. for MenuButton views, or as a
// context menu.)
//
////////////////////////////////////////////////////////////////////////////////
class MenuButtonDelegate {
 public:
  // Creates and shows a menu at the specified position. |source| is the view
  // the MenuButtonDelegate was set on.
  virtual void RunMenu(View* source, const gfx::Point& point) = 0;

 protected:
  virtual ~MenuButtonDelegate() {}
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_DELEGATE_H_
