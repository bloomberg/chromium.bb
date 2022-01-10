// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/bubble_menu_item_factory.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kSecondaryIconSizeDp = 16;
// Set secondary item insets to get to square buttons.
constexpr gfx::Insets kSecondaryButtonInsets = gfx::Insets(
    (ExtensionsMenuItemView::kMenuItemHeightDp - kSecondaryIconSizeDp) / 2);
constexpr int EXTENSION_CONTEXT_MENU = 13;
constexpr int EXTENSION_PINNING = 14;

void SetButtonIconWithColor(HoverButton* button,
                            const gfx::VectorIcon& icon,
                            SkColor icon_color) {
  button->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(icon, kSecondaryIconSizeDp, icon_color));
  button->SetImage(views::Button::STATE_DISABLED,
                   gfx::CreateVectorIcon(
                       icon, kSecondaryIconSizeDp,
                       SkColorSetA(icon_color, gfx::kDisabledControlAlpha)));
}

}  // namespace

// static
constexpr gfx::Size ExtensionsMenuItemView::kIconSize;

ExtensionsMenuItemView::ExtensionsMenuItemView(
    MenuItemType item_type,
    Browser* browser,
    std::unique_ptr<ToolbarActionViewController> controller,
    bool allow_pinning)
    : item_type_(item_type),
      profile_(browser->profile()),
      primary_action_button_(
          new ExtensionsMenuButton(browser, controller.get(), allow_pinning)),
      controller_(std::move(controller)),
      model_(ToolbarActionsModel::Get(profile_)) {
  // Set so the extension button receives enter/exit on children to retain hover
  // status when hovering child views.
  SetNotifyEnterExitOnChild(true);

  views::FlexLayout* layout_manager_ =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager_->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetIgnoreDefaultMainAxisMargins(true);

  AddChildView(primary_action_button_.get());
  primary_action_button_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  if (item_type_ == MenuItemType::kExtensions) {
    AddPinButton();
    AddContextMenuButton();
  }
  // TODO(crbug.com/1263310): Add a dropdown view for MenuItemType::kSiteAccess.
}

ExtensionsMenuItemView::~ExtensionsMenuItemView() = default;

void ExtensionsMenuItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  if (item_type_ == MenuItemType::kExtensions) {
    const SkColor icon_color =
        GetAdjustedIconColor(GetColorProvider()->GetColor(ui::kColorMenuIcon));

    if (pin_button_)
      views::InkDrop::Get(pin_button_)->SetBaseColor(icon_color);

    SetButtonIconWithColor(context_menu_button_, kBrowserToolsIcon, icon_color);

    UpdatePinButton();
  }
}

void ExtensionsMenuItemView::UpdatePinButton() {
  DCHECK_EQ(item_type_, MenuItemType::kExtensions);
  if (!pin_button_)
    return;

  bool is_force_pinned =
      model_ && model_->IsActionForcePinned(controller_->GetId());
  int pin_button_string_id = 0;
  if (is_force_pinned)
    pin_button_string_id = IDS_EXTENSIONS_PINNED_BY_ADMIN;
  else if (IsPinned())
    pin_button_string_id = IDS_EXTENSIONS_UNPIN_FROM_TOOLBAR;
  else
    pin_button_string_id = IDS_EXTENSIONS_PIN_TO_TOOLBAR;
  pin_button_->SetTooltipText(l10n_util::GetStringUTF16(pin_button_string_id));
  // Extension pinning is not available in Incognito as it leaves a trace of
  // user activity.
  pin_button_->SetEnabled(!is_force_pinned && !profile_->IsOffTheRecord());

  if (!GetWidget())
    return;
  SkColor unpinned_icon_color =
      GetAdjustedIconColor(GetColorProvider()->GetColor(ui::kColorMenuIcon));
  SkColor icon_color = IsPinned()
                           ? GetAdjustedIconColor(GetColorProvider()->GetColor(
                                 ui::kColorButtonBackgroundProminent))
                           : unpinned_icon_color;
  SetButtonIconWithColor(pin_button_,
                         IsPinned() ? views::kUnpinIcon : views::kPinIcon,
                         icon_color);
}

bool ExtensionsMenuItemView::IsContextMenuRunningForTesting() const {
  DCHECK_EQ(item_type_, MenuItemType::kExtensions);
  return context_menu_controller_->IsMenuRunning();
}

bool ExtensionsMenuItemView::IsPinned() const {
  DCHECK_EQ(item_type_, MenuItemType::kExtensions);
  // |model_| can be null in unit tests.
  return model_ && model_->IsActionPinned(controller_->GetId());
}

void ExtensionsMenuItemView::ContextMenuPressed() {
  DCHECK_EQ(item_type_, MenuItemType::kExtensions);
  base::RecordAction(base::UserMetricsAction(
      "Extensions.Toolbar.MoreActionsButtonPressedFromMenu"));
  // TODO(crbug.com/998298): Cleanup the menu source type.
  context_menu_controller_->ShowContextMenuForViewImpl(
      context_menu_button_, context_menu_button_->GetMenuPosition(),
      ui::MenuSourceType::MENU_SOURCE_MOUSE);
}

void ExtensionsMenuItemView::PinButtonPressed() {
  DCHECK_EQ(item_type_, MenuItemType::kExtensions);

  base::RecordAction(
      base::UserMetricsAction("Extensions.Toolbar.PinButtonPressed"));
  model_->SetActionVisibility(controller_->GetId(), !IsPinned());
  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      IsPinned() ? IDS_EXTENSION_PINNED : IDS_EXTENSION_UNPINNED));
}

ExtensionsMenuButton*
ExtensionsMenuItemView::primary_action_button_for_testing() {
  return primary_action_button_;
}

void ExtensionsMenuItemView::AddPinButton() {
  DCHECK_EQ(item_type_, MenuItemType::kExtensions);

  if (primary_action_button_->CanShowIconInToolbar()) {
    auto pin_button = std::make_unique<HoverButton>(
        base::BindRepeating(&ExtensionsMenuItemView::PinButtonPressed,
                            base::Unretained(this)),
        std::u16string());
    pin_button->SetID(EXTENSION_PINNING);
    pin_button->SetBorder(views::CreateEmptyBorder(kSecondaryButtonInsets));

    pin_button_ = pin_button.get();
    AddChildView(std::move(pin_button));
  }
  UpdatePinButton();
}

void ExtensionsMenuItemView::AddContextMenuButton() {
  DCHECK_EQ(item_type_, MenuItemType::kExtensions);

  context_menu_controller_ =
      std::make_unique<ExtensionContextMenuController>(controller_.get());

  auto context_menu_button = std::make_unique<HoverButton>(
      views::Button::PressedCallback(), std::u16string());
  context_menu_button->SetID(EXTENSION_CONTEXT_MENU);
  context_menu_button->SetBorder(
      views::CreateEmptyBorder(kSecondaryButtonInsets));
  context_menu_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_MENU_CONTEXT_MENU_TOOLTIP));
  context_menu_button->SetButtonController(
      std::make_unique<views::MenuButtonController>(
          context_menu_button.get(),
          base::BindRepeating(&ExtensionsMenuItemView::ContextMenuPressed,
                              base::Unretained(this)),
          std::make_unique<views::Button::DefaultButtonControllerDelegate>(
              context_menu_button.get())));
  context_menu_button_ = AddChildView(std::move(context_menu_button));
}

SkColor ExtensionsMenuItemView::GetAdjustedIconColor(SkColor icon_color) const {
  const SkColor background_color =
      GetColorProvider()->GetColor(ui::kColorBubbleBackground);
  if (background_color != SK_ColorTRANSPARENT) {
    return color_utils::BlendForMinContrast(icon_color, background_color).color;
  }
  return icon_color;
}

BEGIN_METADATA(ExtensionsMenuItemView, views::View)
END_METADATA
