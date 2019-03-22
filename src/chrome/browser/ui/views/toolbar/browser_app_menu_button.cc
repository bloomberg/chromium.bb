// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"

#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/metrics.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/ash/chrome_keyboard_controller_client.h"
#endif  // defined(OS_CHROMEOS)

// static
bool BrowserAppMenuButton::g_open_app_immediately_for_testing = false;

BrowserAppMenuButton::BrowserAppMenuButton(ToolbarView* toolbar_view)
    : AppMenuButton(toolbar_view), toolbar_view_(toolbar_view) {
  SetInkDropMode(InkDropMode::ON);
  SetFocusPainter(nullptr);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  set_ink_drop_visible_opacity(kToolbarInkDropVisibleOpacity);

  md_observer_.Add(ui::MaterialDesignController::GetInstance());
  UpdateBorder();
}

BrowserAppMenuButton::~BrowserAppMenuButton() {}

void BrowserAppMenuButton::SetTypeAndSeverity(
    AppMenuIconController::TypeAndSeverity type_and_severity) {
  type_and_severity_ = type_and_severity;

  SetTooltipText(
      type_and_severity_.severity == AppMenuIconController::Severity::NONE
          ? l10n_util::GetStringUTF16(IDS_APPMENU_TOOLTIP)
          : l10n_util::GetStringUTF16(IDS_APPMENU_TOOLTIP_UPDATE_AVAILABLE));
  UpdateIcon();
}

void BrowserAppMenuButton::SetIsProminent(bool is_prominent) {
  if (is_prominent) {
    SetBackground(views::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_ProminentButtonColor)));
  } else {
    SetBackground(nullptr);
  }
  SchedulePaint();
}

void BrowserAppMenuButton::ShowMenu(bool for_drop) {
  if (IsMenuShowing())
    return;

#if defined(OS_CHROMEOS)
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (keyboard_client->is_keyboard_visible())
    keyboard_client->HideKeyboard(ash::mojom::HideReason::kSystem);
#endif

  Browser* browser = toolbar_view_->browser();

  InitMenu(
      std::make_unique<AppMenuModel>(toolbar_view_, browser,
                                     toolbar_view_->app_menu_icon_controller()),
      browser, for_drop ? AppMenu::FOR_DROP : AppMenu::NO_FLAGS);

  base::TimeTicks menu_open_time = base::TimeTicks::Now();
  menu()->RunMenu(this);

  if (!for_drop) {
    // Record the time-to-action for the menu. We don't record in the case of a
    // drag-and-drop command because menus opened for drag-and-drop don't block
    // the message loop.
    UMA_HISTOGRAM_TIMES("Toolbar.AppMenuTimeToAction",
                        base::TimeTicks::Now() - menu_open_time);
  }
}

void BrowserAppMenuButton::OnThemeChanged() {
  UpdateIcon();
}

void BrowserAppMenuButton::UpdateIcon() {
  SkColor severity_color = gfx::kPlaceholderColor;

  const ui::NativeTheme* native_theme = GetNativeTheme();
  switch (type_and_severity_.severity) {
    case AppMenuIconController::Severity::NONE:
      severity_color = GetThemeProvider()->GetColor(
          ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
      break;
    case AppMenuIconController::Severity::LOW:
      severity_color = native_theme->GetSystemColor(
          ui::NativeTheme::kColorId_AlertSeverityLow);
      break;
    case AppMenuIconController::Severity::MEDIUM:
      severity_color = native_theme->GetSystemColor(
          ui::NativeTheme::kColorId_AlertSeverityMedium);
      break;
    case AppMenuIconController::Severity::HIGH:
      severity_color = native_theme->GetSystemColor(
          ui::NativeTheme::kColorId_AlertSeverityHigh);
      break;
  }

  const bool touch_ui = ui::MaterialDesignController::touch_ui();
  const gfx::VectorIcon* icon_id = nullptr;
  switch (type_and_severity_.type) {
    case AppMenuIconController::IconType::NONE:
      icon_id = touch_ui ? &kBrowserToolsTouchIcon : &kBrowserToolsIcon;
      DCHECK_EQ(AppMenuIconController::Severity::NONE,
                type_and_severity_.severity);
      break;
    case AppMenuIconController::IconType::UPGRADE_NOTIFICATION:
      icon_id =
          touch_ui ? &kBrowserToolsUpdateTouchIcon : &kBrowserToolsUpdateIcon;
      break;
    case AppMenuIconController::IconType::GLOBAL_ERROR:
      icon_id =
          touch_ui ? &kBrowserToolsErrorTouchIcon : &kBrowserToolsErrorIcon;
      break;
  }

  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(*icon_id, severity_color));
}

void BrowserAppMenuButton::SetTrailingMargin(int margin) {
  if (margin == margin_trailing_)
    return;
  margin_trailing_ = margin;
  UpdateBorder();
  InvalidateLayout();
}

void BrowserAppMenuButton::OnTouchUiChanged() {
  UpdateIcon();
  UpdateBorder();
  PreferredSizeChanged();
}

const char* BrowserAppMenuButton::GetClassName() const {
  return "BrowserAppMenuButton";
}

void BrowserAppMenuButton::UpdateBorder() {
  SetBorder(views::CreateEmptyBorder(GetLayoutInsets(TOOLBAR_BUTTON) +
                                     gfx::Insets(0, 0, 0, margin_trailing_)));
}

void BrowserAppMenuButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // TODO(pbos): Consolidate with ToolbarButton::OnBoundsChanged.
  SetToolbarButtonHighlightPath(this, gfx::Insets(0, 0, 0, margin_trailing_));

  AppMenuButton::OnBoundsChanged(previous_bounds);
}

gfx::Rect BrowserAppMenuButton::GetAnchorBoundsInScreen() const {
  gfx::Rect bounds = GetBoundsInScreen();
  gfx::Insets insets =
      GetToolbarInkDropInsets(this, gfx::Insets(0, 0, 0, margin_trailing_));
  // If the button is extended, don't inset the trailing edge. The anchored menu
  // should extend to the screen edge as well so the menu is easier to hit
  // (Fitts's law).
  // TODO(pbos): Make sure the button is aware of that it is being extended or
  // not (margin_trailing_ cannot be used as it can be 0 in fullscreen on
  // Touch). When this is implemented, use 0 as a replacement for
  // margin_trailing_ in fullscreen only. Always keep the rest.
  insets.Set(insets.top(), 0, insets.bottom(), 0);
  bounds.Inset(insets);
  return bounds;
}

bool BrowserAppMenuButton::GetDropFormats(
    int* formats,
    std::set<ui::Clipboard::FormatType>* format_types) {
  return BrowserActionDragData::GetDropFormats(format_types);
}

bool BrowserAppMenuButton::AreDropTypesRequired() {
  return BrowserActionDragData::AreDropTypesRequired();
}

bool BrowserAppMenuButton::CanDrop(const ui::OSExchangeData& data) {
  return BrowserActionDragData::CanDrop(data,
                                        toolbar_view_->browser()->profile());
}

void BrowserAppMenuButton::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(!weak_factory_.HasWeakPtrs());
  if (!g_open_app_immediately_for_testing) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BrowserAppMenuButton::ShowMenu,
                       weak_factory_.GetWeakPtr(), true),
        base::TimeDelta::FromMilliseconds(views::GetMenuShowDelay()));
  } else {
    ShowMenu(true);
  }
}

int BrowserAppMenuButton::OnDragUpdated(const ui::DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_MOVE;
}

void BrowserAppMenuButton::OnDragExited() {
  weak_factory_.InvalidateWeakPtrs();
}

int BrowserAppMenuButton::OnPerformDrop(const ui::DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_MOVE;
}

std::unique_ptr<views::InkDrop> BrowserAppMenuButton::CreateInkDrop() {
  return CreateToolbarInkDrop(this);
}

std::unique_ptr<views::InkDropHighlight>
BrowserAppMenuButton::CreateInkDropHighlight() const {
  return CreateToolbarInkDropHighlight(this);
}
