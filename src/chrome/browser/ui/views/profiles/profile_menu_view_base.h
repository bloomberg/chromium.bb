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

class Browser;

// TODO(https://crbug.com/934689): Separation of providing content for different
// menus and the UI effort to view it between this class and
// |ProfileChooserView| is in progress.

// This class provides the UI for different menus that are created by user
// clicking the avatar button.
class ProfileMenuViewBase : public content::WebContentsDelegate,
                            public views::BubbleDialogDelegateView,
                            public views::StyledLabelListener {
 public:
  // Shows the bubble if one is not already showing.  This allows us to easily
  // make a button toggle the bubble on and off when clicked: we unconditionally
  // call this function when the button is clicked and if the bubble isn't
  // showing it will appear while if it is showing, nothing will happen here and
  // the existing bubble will auto-close due to focus loss.
  // There are 2 ways to position the Bubble, if |anchor_button| is set, then
  // |parent_window| and |anchor_rect| are ignored. Otherwise, |parent_window|
  // and |anchor_rect| have to be set.
  static void ShowBubble(
      profiles::BubbleViewMode view_mode,
      const signin::ManageAccountsParams& manage_accounts_params,
      signin_metrics::AccessPoint access_point,
      views::Button* anchor_button,
      gfx::NativeView parent_window,
      const gfx::Rect& anchor_rect,
      Browser* browser,
      bool is_source_keyboard);

  static bool IsShowing();
  static void Hide();

  static ProfileMenuViewBase* GetBubbleForTesting();

  typedef std::vector<std::unique_ptr<views::View>> MenuItems;

 protected:
  ProfileMenuViewBase(views::Button* anchor_button,
                      const gfx::Rect& anchor_rect,
                      gfx::NativeView parent_window,
                      Browser* browser);
  ~ProfileMenuViewBase() override;

  void Reset();
  // Adds a set of menu items, either as a |new_group| (using a separator) or
  // appended to the last added items. Takes ownership of the items and passes
  // them to the underlying view when menu is built using
  // |RepopulateViewFromMenuItems|.
  void AddMenuItems(MenuItems& menu_items, bool new_group);
  void RepopulateViewFromMenuItems();

  Browser* browser() const { return browser_; }

  // Return maximal height for the view after which it becomes scrollable.
  // TODO(crbug.com/870303): remove when a general solution is available.
  int GetMaxHeight() const;

  views::Button* anchor_button() const { return anchor_button_; }

  bool ShouldProvideInitiallyFocusedView() const;

  // TODO(https://crbug.com/934689): Remove menu_width function and make
  // decisions inside this class.
  int menu_width() { return menu_width_; }

  gfx::ImageSkia CreateVectorIcon(const gfx::VectorIcon& icon);

  // TODO(https://crbug.com/934689): Remove function and make decisions inside
  // this class.
  int GetDefaultIconSize();

 private:
  friend class ProfileChooserViewExtensionsTest;

  // Requests focus for a button when opened by keyboard.
  virtual void FocusButtonOnKeyboardOpen() {}

  // views::BubbleDialogDelegateView:
  void WindowClosing() override;
  void OnNativeThemeChanged(const ui::NativeTheme* native_theme) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  int GetDialogButtons() const override;
  ax::mojom::Role GetAccessibleWindowRole() override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

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
