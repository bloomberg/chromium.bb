// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_runner.h"

namespace views {

// Manages the menu. To destroy a Holder invoke Release(). Release() deletes
// immediately if the menu isn't showing. If the menu is showing Release()
// cancels the menu and when the nested RunMenuAt() call returns, deletes itself
// and the menu.
class MenuRunner::Holder {
 public:
  explicit Holder(MenuItemView* menu);

  // See description above class for details.
  void Release();

  // Runs the menu.
  void RunMenuAt(Widget* parent,
                 MenuButton* button,
                 const gfx::Rect& bounds,
                 MenuItemView::AnchorPosition anchor,
                 bool has_mnemonics);

 private:
  ~Holder();

  scoped_ptr<MenuItemView> menu_;

  // Are we in run waiting for it to return?
  bool running_;

  // Set if |running_| and Release() has been invoked.
  bool delete_after_run_;

  DISALLOW_COPY_AND_ASSIGN(Holder);
};

MenuRunner::Holder::Holder(MenuItemView* menu)
    : menu_(menu),
      running_(false),
      delete_after_run_(false) {
}

void MenuRunner::Holder::Release() {
  if (running_) {
    // The menu is running a nested message loop, we can't delete it now
    // otherwise the stack would be in a really bad state (many frames would
    // have deleted objects on them). Instead cancel the menu, when it returns
    // Holder will delete itself.
    delete_after_run_ = true;
    menu_->Cancel();
  } else {
    delete this;
  }
}

void MenuRunner::Holder::RunMenuAt(Widget* parent,
                                   MenuButton* button,
                                   const gfx::Rect& bounds,
                                   MenuItemView::AnchorPosition anchor,
                                   bool has_mnemonics) {
  if (running_) {
    // Ignore requests to show the menu while it's already showing. MenuItemView
    // doesn't handle this very well (meaning it crashes).
    return;
  }
  running_ = true;
  menu_->RunMenuAt(parent, button, bounds, anchor, has_mnemonics);
  if (delete_after_run_) {
    delete this;
    return;
  }
  running_ = false;
}

MenuRunner::Holder::~Holder() {
}

MenuRunner::MenuRunner(MenuItemView* menu) : holder_(new Holder(menu)) {
}

MenuRunner::~MenuRunner() {
  holder_->Release();
}

void MenuRunner::RunMenuAt(Widget* parent,
                           MenuButton* button,
                           const gfx::Rect& bounds,
                           MenuItemView::AnchorPosition anchor,
                           bool has_mnemonics) {
  holder_->RunMenuAt(parent, button, bounds, anchor, has_mnemonics);
}

}  // namespace views
