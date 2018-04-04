// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/views_text_services_context_menu.h"

#import <Cocoa/Cocoa.h>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "ui/base/cocoa/text_services_context_menu.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/decorated_text.h"
#import "ui/gfx/decorated_text_mac.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// This class serves as a bridge to TextServicesContextMenu to add and handle
// text service items in the context menu. The items include Speech, Look Up
// and BiDi.
class ViewsTextServicesContextMenuMac
    : public ViewsTextServicesContextMenu,
      public ui::TextServicesContextMenu::Delegate {
 public:
  ViewsTextServicesContextMenuMac(ui::SimpleMenuModel* menu, Textfield* client)
      : text_services_menu_(this), client_(client) {
    // The index to use when inserting items into the menu.
    int index = 0;

    base::string16 text = GetSelectedText();
    if (!text.empty()) {
      menu->InsertItemAt(
          index++, IDS_CONTENT_CONTEXT_LOOK_UP,
          l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_LOOK_UP, text));
      menu->InsertSeparatorAt(index++, ui::NORMAL_SEPARATOR);
    }
    if (base::FeatureList::IsEnabled(features::kEnableEmojiContextMenu)) {
      menu->InsertItemWithStringIdAt(index++, IDS_CONTENT_CONTEXT_EMOJI,
                                     IDS_CONTENT_CONTEXT_EMOJI);
      menu->InsertSeparatorAt(index++, ui::NORMAL_SEPARATOR);
    }
    text_services_menu_.AppendToContextMenu(menu);
    text_services_menu_.AppendEditableItems(menu);
  }

  ~ViewsTextServicesContextMenuMac() override {}

  // ViewsTextServicesContextMenu:
  bool SupportsCommand(int command_id) const override {
    return command_id == IDS_CONTENT_CONTEXT_EMOJI ||
           command_id == IDS_CONTENT_CONTEXT_LOOK_UP;
  }

  bool IsCommandIdChecked(int command_id) const override {
    DCHECK_EQ(IDS_CONTENT_CONTEXT_LOOK_UP, command_id);
    return false;
  }

  bool IsCommandIdEnabled(int command_id) const override {
    switch (command_id) {
      case IDS_CONTENT_CONTEXT_EMOJI:
        return true;

      case IDS_CONTENT_CONTEXT_LOOK_UP:
        return true;

      default:
        return false;
    }
  }

  void ExecuteCommand(int command_id) override {
    switch (command_id) {
      case IDS_CONTENT_CONTEXT_EMOJI:
        [NSApp orderFrontCharacterPalette:nil];
        break;

      case IDS_CONTENT_CONTEXT_LOOK_UP:
        LookUpInDictionary();
        break;
    }
  }

  // TextServicesContextMenu::Delegate:
  base::string16 GetSelectedText() const override {
    return client_->GetSelectedText();
  }

  bool IsTextDirectionEnabled(
      base::i18n::TextDirection direction) const override {
    return direction != base::i18n::UNKNOWN_DIRECTION;
  }

  bool IsTextDirectionChecked(
      base::i18n::TextDirection direction) const override {
    return direction != base::i18n::UNKNOWN_DIRECTION &&
           client_->GetTextDirection() == direction;
  }

  void UpdateTextDirection(base::i18n::TextDirection direction) override {
    DCHECK_NE(direction, base::i18n::UNKNOWN_DIRECTION);

    base::i18n::TextDirection text_direction =
        direction == base::i18n::LEFT_TO_RIGHT ? base::i18n::LEFT_TO_RIGHT
                                               : base::i18n::RIGHT_TO_LEFT;
    client_->ChangeTextDirectionAndLayoutAlignment(text_direction);
  }

 private:
  // Handler for the "Look Up" menu item.
  void LookUpInDictionary() {
    gfx::Point baseline_point;
    gfx::DecoratedText text;
    if (client_->GetWordLookupDataFromSelection(&text, &baseline_point)) {
      Widget* widget = client_->GetWidget();
      gfx::NativeView view = widget->GetNativeView();
      views::View::ConvertPointToTarget(client_, widget->GetRootView(),
                                        &baseline_point);

      NSPoint lookup_point = NSMakePoint(
          baseline_point.x(), NSHeight([view frame]) - baseline_point.y());
      [view showDefinitionForAttributedString:
                gfx::GetAttributedStringFromDecoratedText(text)
                                      atPoint:lookup_point];
    }
  }

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
  return std::make_unique<ViewsTextServicesContextMenuMac>(menu, client);
}

// static
bool ViewsTextServicesContextMenu::IsTextDirectionCheckedForTesting(
    ViewsTextServicesContextMenu* menu,
    base::i18n::TextDirection direction) {
  return static_cast<views::ViewsTextServicesContextMenuMac*>(menu)
      ->IsTextDirectionChecked(direction);
}
}  // namespace views
