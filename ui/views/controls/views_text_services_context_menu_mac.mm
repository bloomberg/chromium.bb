// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/views_text_services_context_menu.h"

#include "base/memory/ptr_util.h"
#include "ui/base/cocoa/text_services_context_menu.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/textfield/textfield.h"

namespace views {

namespace {

// This class serves as a bridge to TextServicesContextMenu to add and handle
// text service items in the context menu. The items include Speech, Look Up
// and BiDi.
// TODO (spqchan): Add Look Up and BiDi.
class ViewsTextServicesContextMenuMac
    : public ViewsTextServicesContextMenu,
      public ui::TextServicesContextMenu::Delegate {
 public:
  ViewsTextServicesContextMenuMac(ui::SimpleMenuModel* menu, Textfield* client)
      : text_services_menu_(this), client_(client) {
    text_services_menu_.AppendToContextMenu(menu);
  }

  ~ViewsTextServicesContextMenuMac() override {}

  // TextServicesContextMenu::Delegate:
  base::string16 GetSelectedText() const override {
    return client_->GetSelectedText();
  }

 private:
  // Appends and handles the text service menu.
  ui::TextServicesContextMenu text_services_menu_;

  // The view associated with the menu. Weak. Owns |this|.
  Textfield* client_;

  DISALLOW_COPY_AND_ASSIGN(ViewsTextServicesContextMenuMac);
};

}  // namespace

// static
std::unique_ptr<ViewsTextServicesContextMenu>
ViewsTextServicesContextMenu::Create(ui::SimpleMenuModel* menu,
                                     Textfield* client) {
  return base::MakeUnique<ViewsTextServicesContextMenuMac>(menu, client);
}

}  // namespace views