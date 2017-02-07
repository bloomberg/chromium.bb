// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_VIEWS_TEXT_SERVICES_CONTEXT_MENU_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_VIEWS_TEXT_SERVICES_CONTEXT_MENU_H_

#include <memory>

#include "base/i18n/rtl.h"

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

  // Creates a ViewsTextServicesContextMenu object.
  static std::unique_ptr<ViewsTextServicesContextMenu> Create(
      ui::SimpleMenuModel* menu,
      Textfield* textfield);

  // Returns true if the given |command_id| is handled by the menu.
  virtual bool HandlesCommandId(int command_id) const = 0;

  // Methods associated with SimpleMenuModel::Delegate.
  virtual bool IsCommandIdChecked(int command_id) const = 0;
  virtual bool IsCommandIdEnabled(int command_id) const = 0;
  virtual void ExecuteCommand(int command_id, int event_flags) = 0;

  // Exposed for testing. Returns true if the text direction BiDi submenu
  // item should be checked.
  virtual bool IsTextDirectionCheckedForTesting(
      base::i18n::TextDirection direction) const = 0;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_VIEWS_TEXT_SERVICES_CONTEXT_MENU_H_
