// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_VIEWS_TEXT_SERVICES_CONTEXT_MENU_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_VIEWS_TEXT_SERVICES_CONTEXT_MENU_H_

#include <memory>

namespace ui {
class SimpleMenuModel;
}

namespace views {

class Textfield;

// This class is used to add and handle text service items in the text context
// menu.
class ViewsTextServicesContextMenu {
 public:
  virtual ~ViewsTextServicesContextMenu() {}

  // Creates a platform-specific ViewsTextServicesContextMenu object.
  static std::unique_ptr<ViewsTextServicesContextMenu> Create(
      ui::SimpleMenuModel* menu,
      Textfield* textfield);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_VIEWS_TEXT_SERVICES_CONTEXT_MENU_H_
