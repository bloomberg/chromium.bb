// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_MENU_RUNNER_H_
#define VIEWS_CONTROLS_MENU_MENU_RUNNER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "views/controls/menu/menu_item_view.h"

namespace views {

class MenuButton;
class Widget;

// MenuRunner handles the lifetime of the root MenuItemView. MenuItemView runs a
// nested message loop, which means care must be taken when the MenuItemView
// needs to be deleted. MenuRunner makes sure the menu is deleted after the
// nested message loop completes.
//
// MenuRunner can be deleted at any time and will correctly handle deleting the
// underlying menu.
//
// TODO: this is a work around for 57890. If we fix it this class shouldn't be
// needed.
class VIEWS_API MenuRunner {
 public:
  explicit MenuRunner(MenuItemView* menu);
  ~MenuRunner();

  // Runs the menu.
  void RunMenuAt(Widget* parent,
                 MenuButton* button,
                 const gfx::Rect& bounds,
                 MenuItemView::AnchorPosition anchor,
                 bool has_mnemonics);

 private:
  class Holder;

  Holder* holder_;

  DISALLOW_COPY_AND_ASSIGN(MenuRunner);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_RUNNER_H_
