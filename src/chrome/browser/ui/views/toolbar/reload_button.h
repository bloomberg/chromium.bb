// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_BUTTON_H_

#include "base/timer/timer.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/button.h"

class CommandUpdater;

////////////////////////////////////////////////////////////////////////////////
//
// ReloadButton
//
// The reload button in the toolbar, which changes to a stop button when a page
// load is in progress. The change from stop back to reload may be delayed if
// the user is hovering the button, to prevent mis-clicks.
//
////////////////////////////////////////////////////////////////////////////////

class ReloadButton : public ToolbarButton,
                     public views::ButtonListener,
                     public ui::SimpleMenuModel::Delegate {
 public:
  enum class IconStyle { kBrowser, kMinimalUi };
  enum class Mode { kReload = 0, kStop };

  // The button's class name.
  static const char kViewClassName[];

  explicit ReloadButton(CommandUpdater* command_updater, IconStyle icon_style);
  ReloadButton(const ReloadButton&) = delete;
  ReloadButton& operator=(const ReloadButton&) = delete;
  ~ReloadButton() override;

  // Ask for a specified button state.  If |force| is true this will be applied
  // immediately.
  void ChangeMode(Mode mode, bool force);

  // Enable reload drop-down menu.
  void set_menu_enabled(bool enable) { menu_enabled_ = enable; }

  void SetColors(SkColor normal_color, SkColor disabled_color);

  // views::View:
  void OnThemeChanged() override;

  // ToolbarButton:
  void OnMouseExited(const ui::MouseEvent& event) override;
  base::string16 GetTooltipText(const gfx::Point& p) const override;
  const char* GetClassName() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool ShouldShowMenu() override;
  void ShowDropDownMenu(ui::MenuSourceType source_type) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* /* button */,
                     const ui::Event& event) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  friend class ReloadButtonTest;

  std::unique_ptr<ui::SimpleMenuModel> CreateMenuModel();

  void ExecuteBrowserCommand(int command, int event_flags);

  // Updates the icon images.
  void UpdateIcon();

  void OnDoubleClickTimer();
  void OnStopToReloadTimer();
  void OnLongLoadTimer();

  base::OneShotTimer double_click_timer_;

  // Timer to delay switching between reload and stop states.
  base::OneShotTimer mode_switch_timer_;

  // This may be NULL when testing.
  CommandUpdater* command_updater_;

  const IconStyle icon_style_;

  // The mode we should be in assuming no timers are running.
  Mode intended_mode_ = Mode::kReload;

  // The currently-visible mode - this may differ from the intended mode.
  Mode visible_mode_ = Mode::kReload;

  // The delay times for the timers.  These are members so that tests can modify
  // them.
  base::TimeDelta double_click_timer_delay_;
  base::TimeDelta mode_switch_timer_delay_;

  // The colors used for the icon if explicitly set by SetColors().
  base::Optional<SkColor> normal_color_;
  base::Optional<SkColor> disabled_color_;

  // Indicates if reload menu is enabled.
  bool menu_enabled_ = false;

  // TESTING ONLY
  // True if we should pretend the button is hovered.
  bool testing_mouse_hovered_ = false;
  // Increments when we would tell the browser to "reload", so
  // test code can tell whether we did so (as there may be no |browser_|).
  int testing_reload_count_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_BUTTON_H_
