// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_BASE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/close_bubble_on_tab_activation_helper.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"

class Browser;

namespace views {
class Button;
class Label;
}  // namespace views

struct AccountInfo;
class DiceSigninButtonView;

// This class provides the UI for different menus that are created by user
// clicking the avatar button.
class ProfileMenuViewBase : public content::WebContentsDelegate,
                            public views::BubbleDialogDelegateView,
                            public views::ButtonListener,
                            public views::StyledLabelListener {
 public:
  // MenuItems struct keeps the menu items and meta data for a group of items in
  // a menu. It takes the ownership of views and passes it to the menu when menu
  // is constructed.
  struct MenuItems {
    MenuItems();
    MenuItems(MenuItems&&);
    ~MenuItems();

    enum ItemType {
      kNone,
      kTitleCard,
      kLabel,
      kButton,
      kStyledButton,
      kGeneral
    };

    std::vector<std::unique_ptr<views::View>> items;

    ItemType first_item_type;
    ItemType last_item_type;
    bool different_item_types;

    DISALLOW_COPY_AND_ASSIGN(MenuItems);
  };

  enum GroupMarginSize { kNone, kTiny, kSmall, kLarge };

  // Shows the bubble if one is not already showing.  This allows us to easily
  // make a button toggle the bubble on and off when clicked: we unconditionally
  // call this function when the button is clicked and if the bubble isn't
  // showing it will appear while if it is showing, nothing will happen here and
  // the existing bubble will auto-close due to focus loss.
  static void ShowBubble(
      profiles::BubbleViewMode view_mode,
      const signin::ManageAccountsParams& manage_accounts_params,
      signin_metrics::AccessPoint access_point,
      views::Button* anchor_button,
      Browser* browser,
      bool is_source_keyboard);

  static bool IsShowing();
  static void Hide();

  static ProfileMenuViewBase* GetBubbleForTesting();

 protected:
  ProfileMenuViewBase(views::Button* anchor_button,
                      Browser* browser);
  ~ProfileMenuViewBase() override;

  void Reset();

  // Initializes a new group of menu items. A separator is added before them if
  // |add_separator| is true.
  void AddMenuGroup(bool add_separator = true);

  // The following functions add different menu items to the latest menu group.
  // They pass the ownership of the generated item to |menu_item_groups_| and
  // return a raw pointer to the object. The ownership is transferred to the
  // menu when view is repopulated from menu items.
  // Please use |AddViewItem| only if none of the previous ones match.
  views::Button* CreateAndAddTitleCard(std::unique_ptr<views::View> icon_view,
                                       const base::string16& title,
                                       const base::string16& subtitle,
                                       bool enabled = true);
  views::Button* CreateAndAddButton(const gfx::ImageSkia& icon,
                                    const base::string16& title);
  views::Button* CreateAndAddBlueButton(const base::string16& text,
                                        bool md_style);
#if !defined(OS_CHROMEOS)
  DiceSigninButtonView* CreateAndAddDiceSigninButton(
      AccountInfo* account_info = nullptr,
      gfx::Image* account_icon = nullptr);
#endif
  views::Label* CreateAndAddLabel(
      const base::string16& text,
      int text_context = views::style::CONTEXT_LABEL);
  void AddViewItem(std::unique_ptr<views::View> view);

  void RepopulateViewFromMenuItems();

  Browser* browser() const { return browser_; }

  // Return maximal height for the view after which it becomes scrollable.
  // TODO(crbug.com/870303): remove when a general solution is available.
  int GetMaxHeight() const;

  views::Button* anchor_button() const { return anchor_button_; }

  bool ShouldProvideInitiallyFocusedView() const;

  gfx::ImageSkia CreateVectorIcon(const gfx::VectorIcon& icon);

  int GetDefaultIconSize();

 private:
  friend class ProfileChooserViewExtensionsTest;

  // Requests focus for a button when opened by keyboard.
  virtual void FocusButtonOnKeyboardOpen() {}

  // views::BubbleDialogDelegateView:
  void WindowClosing() override;
  void OnThemeChanged() override;
  int GetDialogButtons() const override;
  ax::mojom::Role GetAccessibleWindowRole() override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // Returns the size of different margin types.
  int GetMarginSize(GroupMarginSize margin_size) const;

  void AddMenuItemInternal(std::unique_ptr<views::View> view,
                           MenuItems::ItemType item_type);

  Browser* const browser_;

  int menu_width_;

  // ProfileMenuViewBase takes ownership of all menu_items and passes it to the
  // underlying view when it is created.
  std::vector<MenuItems> menu_item_groups_;

  views::Button* const anchor_button_;

  CloseBubbleOnTabActivationHelper close_bubble_helper_;

  DISALLOW_COPY_AND_ASSIGN(ProfileMenuViewBase);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_BASE_H_
