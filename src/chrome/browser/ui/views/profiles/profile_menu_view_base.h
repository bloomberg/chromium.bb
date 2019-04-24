// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_BASE_H_

#include <stddef.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/close_bubble_on_tab_activation_helper.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/styled_label_listener.h"

namespace views {
class GridLayout;
}

class Browser;

// TODO(https://crbug.com/934689): Separation of providing content for different
// menus and the UI effort to view it between this class and
// |ProfileChooserView| is in progress.

// This class provides the UI for different menus that are created by user
// clicking the avatar button.
class ProfileMenuViewBase : public content::WebContentsDelegate,
                            public views::BubbleDialogDelegateView,
                            public views::StyledLabelListener {
 protected:
  ProfileMenuViewBase(views::Button* anchor_button,
                      const gfx::Rect& anchor_rect,
                      gfx::NativeView parent_window,
                      Browser* browser);
  ~ProfileMenuViewBase() override;

  void ShowMenu();
  void SetContentsView(views::View* view, int width_override);

  Browser* browser() const { return browser_; }

  // TODO(https://crbug.com/934689):
  // DEPRECATED: New user menu components should use views::BoxLayout instead.
  // Creates a GridLayout with a single column. This ensures that all the child
  // views added get auto-expanded to fill the full width of the bubble.
  views::GridLayout* CreateSingleColumnLayout(views::View* view, int width);

  // Return maximal height for the view after which it becomes scrollable.
  // TODO(crbug.com/870303): remove when a general solution is available.
  int GetMaxHeight() const;

  views::Button* anchor_button() const { return anchor_button_; }

  // TODO(https://crbug.com/934689): Remove menu_width functions and make
  // decision inside this class.
  int menu_width() { return menu_width_; }
  void set_menu_width(int menu_width) { menu_width_ = menu_width; }

 private:
  friend class ProfileChooserViewExtensionsTest;

  // views::BubbleDialogDelegateView:
  void OnNativeThemeChanged(const ui::NativeTheme* native_theme) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  int GetDialogButtons() const override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  Browser* const browser_;

  int menu_width_;

  views::Button* const anchor_button_;

  CloseBubbleOnTabActivationHelper close_bubble_helper_;

  DISALLOW_COPY_AND_ASSIGN(ProfileMenuViewBase);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_BASE_H_
