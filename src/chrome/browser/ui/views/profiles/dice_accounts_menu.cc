// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_accounts_menu.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

namespace {

constexpr int kProfilesAvatarIconSize = 16;

// Used to identify the "Use another account" button.
constexpr int kUseAnotherAccountCmdId = std::numeric_limits<int>::max();

// Used to identify the "Sign out" button.
constexpr int kSignOutCmdId = std::numeric_limits<int>::max() - 1;

// Anchor inset used to position the accounts menu.
constexpr int kAnchorInset = 8;

constexpr int kVerticalItemMargins = 12;
constexpr int kHorizontalIconSpacing = 16;

gfx::ImageSkia SizeAndCircleIcon(const gfx::Image& icon) {
  gfx::Image circled_icon = profiles::GetSizedAvatarIcon(
      icon, true, kProfilesAvatarIconSize, kProfilesAvatarIconSize,
      profiles::SHAPE_CIRCLE);
  return *circled_icon.ToImageSkia();
}

}  // namespace

DiceAccountsMenu::DiceAccountsMenu(const std::vector<AccountInfo>& accounts,
                                   const std::vector<gfx::Image>& icons,
                                   Callback account_selected_callback)
    : accounts_(accounts),
      icons_(icons),
      account_selected_callback_(std::move(account_selected_callback)) {
  DCHECK_EQ(accounts.size(), icons.size());
}

void DiceAccountsMenu::Show(views::View* anchor_view,
                            views::MenuButton* menu_button) {
  DCHECK(!runner_);
  runner_ = std::make_unique<views::MenuRunner>(BuildMenu(),
                                                views::MenuRunner::COMBOBOX);
  // Calculate custom anchor bounds to position the menu.
  // The menu is aligned along the right edge (left edge in RTL mode) of the
  // anchor, slightly shifted inside by |kAnchorInset| and overlapping
  // |anchor_view| on the bottom by |kAnchorInset|. |anchor_bounds| is collapsed
  // so the menu only takes the width it needs.
  gfx::Rect anchor_bounds = anchor_view->GetBoundsInScreen();
  anchor_bounds.Inset(kAnchorInset, kAnchorInset);

  if (base::i18n::IsRTL())
    anchor_bounds.Inset(0, 0, anchor_bounds.width(), 0);
  else
    anchor_bounds.Inset(anchor_bounds.width(), 0, 0, 0);

  runner_->RunMenuAt(anchor_view->GetWidget(), menu_button, anchor_bounds,
                     views::MENU_ANCHOR_TOPRIGHT, ui::MENU_SOURCE_MOUSE);
}

DiceAccountsMenu::~DiceAccountsMenu() {}

void DiceAccountsMenu::SetSignOutButtonCallback(
    base::OnceClosure signout_callback) {
  signout_callback_ = std::move(signout_callback);
}

views::MenuItemView* DiceAccountsMenu::BuildMenu() {
  views::MenuItemView* menu = new views::MenuItemView(this);
  gfx::Image default_icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          profiles::GetPlaceholderAvatarIconResourceID());
#if !defined(OS_MACOSX)
  // Add spacing at top.
  menu->AppendMenuItemImpl(
      0, base::string16(), base::string16(), base::string16(), nullptr,
      gfx::ImageSkia(), views::MenuItemView::SEPARATOR, ui::SPACING_SEPARATOR);
#endif
  // Add a menu item for each account.
  for (size_t idx = 0; idx < accounts_.size(); idx++) {
    views::MenuItemView* item = menu->AppendMenuItemWithIcon(
        idx, base::UTF8ToUTF16(accounts_[idx].email),
        SizeAndCircleIcon(icons_[idx].IsEmpty() ? default_icon : icons_[idx]));
    item->SetMargins(kVerticalItemMargins, kVerticalItemMargins);
  }
  // Add the "Use another account" button.
  views::MenuItemView* item = menu->AppendMenuItemWithIcon(
      kUseAnotherAccountCmdId,
      l10n_util::GetStringUTF16(IDS_PROFILES_DICE_USE_ANOTHER_ACCOUNT_BUTTON),
      SizeAndCircleIcon(default_icon));
  item->SetMargins(kVerticalItemMargins, kVerticalItemMargins);
  if (!signout_callback_.is_null()) {
    // Add the "Sign out" button.
    item = menu->AppendMenuItemWithIcon(
        kSignOutCmdId, l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_SIGN_OUT),
        gfx::CreateVectorIcon(kSignOutIcon, kProfilesAvatarIconSize,
                              gfx::kGoogleGrey600));
    item->SetMargins(kVerticalItemMargins, kVerticalItemMargins);
  }
#if !defined(OS_MACOSX)
  // Add spacing at bottom.
  menu->AppendMenuItemImpl(
      0, base::string16(), base::string16(), base::string16(), nullptr,
      gfx::ImageSkia(), views::MenuItemView::SEPARATOR, ui::SPACING_SEPARATOR);
#endif

  menu->set_has_icons(true);
  return menu;
}

void DiceAccountsMenu::ExecuteCommand(int id) {
  DCHECK((0 <= id && static_cast<size_t>(id) < accounts_.size()) ||
         id == kUseAnotherAccountCmdId || id == kSignOutCmdId);
  if (id == kSignOutCmdId) {
    std::move(signout_callback_).Run();
    return;
  }
  base::Optional<AccountInfo> account;
  if (id != kUseAnotherAccountCmdId)
    account = accounts_[id];
  std::move(account_selected_callback_).Run(account);
}

void DiceAccountsMenu::GetHorizontalIconMargins(int command_id,
                                                int icon_size,
                                                int* left_margin,
                                                int* right_margin) const {
  const views::MenuConfig& config = views::MenuConfig::instance();
  *left_margin = kHorizontalIconSpacing - config.item_horizontal_padding;
  *right_margin = kHorizontalIconSpacing - config.item_horizontal_padding;
}

void DiceAccountsMenu::GetLabelStyle(int id, LabelStyle* style) const {
  const views::LayoutProvider* provider = views::LayoutProvider::Get();
  // Match the font of the HoverButtons in the avatar bubble.
  style->font_list = provider->GetTypographyProvider().GetFont(
      views::style::CONTEXT_BUTTON, STYLE_SECONDARY);
}
