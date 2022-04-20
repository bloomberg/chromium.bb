// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/extensions/extension_site_access_combobox_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/bubble_menu_item_factory.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
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
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kSecondaryIconSizeDp = 16;
// Set secondary item insets to get to square buttons.
constexpr gfx::Insets kSecondaryButtonInsets = gfx::Insets(
    (InstalledExtensionMenuItemView::kMenuItemHeightDp - kSecondaryIconSizeDp) /
    2);
constexpr int EXTENSION_CONTEXT_MENU = 13;
constexpr int EXTENSION_PINNING = 14;

void SetButtonIconWithColor(HoverButton* button,
                            const gfx::VectorIcon& icon,
                            SkColor icon_color,
                            SkColor disabled_icon_color) {
  button->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(icon, kSecondaryIconSizeDp, icon_color));
  button->SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(icon, kSecondaryIconSizeDp, disabled_icon_color));
}

}  // namespace

// static
constexpr gfx::Size InstalledExtensionMenuItemView::kIconSize;

SiteAccessMenuItemView::SiteAccessMenuItemView(
    Browser* browser,
    std::unique_ptr<ToolbarActionViewController> controller,
    bool allow_pinning)
    : browser_(browser), controller_(std::move(controller)) {
  // Items with kSiteAccess type should only be created if their
  // associated extension has or requests access to the current page.
  DCHECK(controller_->GetSiteInteraction(
             browser->tab_strip_model()->GetActiveWebContents()) !=
         extensions::SitePermissionsHelper::SiteInteraction::kNone);

  // Create the combobox model that will be used by the builder.
  auto* extension = extensions::ExtensionRegistry::Get(browser_->profile())
                        ->enabled_extensions()
                        .GetByID(controller_->GetId());
  auto combobox_model =
      std::make_unique<ExtensionSiteAccessComboboxModel>(browser_, extension);
  site_access_combobox_model_ = combobox_model.get();

  views::Builder<SiteAccessMenuItemView>(this)
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetIgnoreDefaultMainAxisMargins(true)
      // Set so the extension button receives enter/exit on children to retain
      // hover status when hovering child views.
      .SetNotifyEnterExitOnChild(true)
      .AddChildren(
          views::Builder<ExtensionsMenuButton>(
              std::make_unique<ExtensionsMenuButton>(
                  browser_, controller_.get(), allow_pinning))
              .CopyAddressTo(&primary_action_button_)
              .SetProperty(views::kFlexBehaviorKey,
                           views::FlexSpecification(
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)),
          views::Builder<views::Combobox>(
              std::make_unique<views::Combobox>(std::move(combobox_model)))
              .CopyAddressTo(&site_access_combobox_)
              .SetAccessibleName(l10n_util::GetStringUTF16(
                  IDS_ACCNAME_EXTENSIONS_MENU_SITE_ACCESS_COMBOBOX))
              .SetCallback(base::BindRepeating(
                  &SiteAccessMenuItemView::OnComboboxSelectionChanged,
                  base::Unretained(this))))
      .BuildChildren();
}

SiteAccessMenuItemView::~SiteAccessMenuItemView() = default;

void SiteAccessMenuItemView::Update() {
  view_controller()->UpdateState();

  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;
  int index = site_access_combobox_model_->GetCurrentSiteAccessIndex();
  site_access_combobox_->SetSelectedIndex(index);
}

void SiteAccessMenuItemView::OnComboboxSelectionChanged() {
  int selected_index = site_access_combobox_->GetSelectedIndex();
  site_access_combobox_model_->HandleSelection(selected_index);
}

InstalledExtensionMenuItemView::InstalledExtensionMenuItemView(
    Browser* browser,
    std::unique_ptr<ToolbarActionViewController> controller,
    bool allow_pinning)
    : browser_(browser),
      controller_(std::move(controller)),
      model_(ToolbarActionsModel::Get(browser_->profile())) {
  auto builder =
      views::Builder<InstalledExtensionMenuItemView>(this)
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetIgnoreDefaultMainAxisMargins(true)
          // Set so the extension button receives enter/exit on children to
          // retain hover status when hovering child views.
          .SetNotifyEnterExitOnChild(true)
          .AddChildren(
              views::Builder<ExtensionsMenuButton>(
                  std::make_unique<ExtensionsMenuButton>(
                      browser_, controller_.get(), allow_pinning))
                  .CopyAddressTo(&primary_action_button_)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded)),
              views::Builder<HoverButton>(
                  std::make_unique<HoverButton>(
                      views::Button::PressedCallback(), std::u16string()))
                  .CopyAddressTo(&context_menu_button_)
                  .SetID(EXTENSION_CONTEXT_MENU)
                  .SetBorder(views::CreateEmptyBorder(kSecondaryButtonInsets))
                  .SetTooltipText(l10n_util::GetStringUTF16(
                      IDS_EXTENSIONS_MENU_CONTEXT_MENU_TOOLTIP)));

  if (primary_action_button_->CanShowIconInToolbar()) {
    builder.AddChildAt(
        views::Builder<HoverButton>(
            std::make_unique<HoverButton>(
                base::BindRepeating(
                    &InstalledExtensionMenuItemView::OnPinButtonPressed,
                    base::Unretained(this)),
                std::u16string()))
            .CopyAddressTo(&pin_button_)
            .SetID(EXTENSION_PINNING)
            .SetBorder(views::CreateEmptyBorder(kSecondaryButtonInsets)),
        1);
  }

  std::move(builder).BuildChildren();

  // Add a controller to the context menu
  context_menu_controller_ = std::make_unique<ExtensionContextMenuController>(
      controller_.get(),
      extensions::ExtensionContextMenuModel::ContextMenuSource::kMenuItem);

  context_menu_button_->SetButtonController(
      std::make_unique<views::MenuButtonController>(
          context_menu_button_.get(),
          base::BindRepeating(
              &InstalledExtensionMenuItemView::OnContextMenuPressed,
              base::Unretained(this)),
          std::make_unique<views::Button::DefaultButtonControllerDelegate>(
              context_menu_button_.get())));
}

InstalledExtensionMenuItemView::~InstalledExtensionMenuItemView() = default;

void InstalledExtensionMenuItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const auto* const color_provider = GetColorProvider();
  const SkColor icon_color = color_provider->GetColor(kColorExtensionMenuIcon);

  if (pin_button_)
    views::InkDrop::Get(pin_button_)->SetBaseColor(icon_color);

  SetButtonIconWithColor(
      context_menu_button_, kBrowserToolsIcon, icon_color,
      color_provider->GetColor(kColorExtensionMenuIconDisabled));

  UpdatePinButton();
}

void InstalledExtensionMenuItemView::Update() {
  view_controller()->UpdateState();
}

void InstalledExtensionMenuItemView::UpdatePinButton() {
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
  pin_button_->SetEnabled(!is_force_pinned &&
                          !browser_->profile()->IsOffTheRecord());

  if (!GetWidget())
    return;
  const auto* const color_provider = GetColorProvider();
  const SkColor icon_color = color_provider->GetColor(
      IsPinned() ? kColorExtensionMenuPinButtonIcon : kColorExtensionMenuIcon);
  const SkColor disabled_icon_color = color_provider->GetColor(
      IsPinned() ? kColorExtensionMenuPinButtonIconDisabled
                 : kColorExtensionMenuIconDisabled);
  SetButtonIconWithColor(pin_button_,
                         IsPinned() ? views::kUnpinIcon : views::kPinIcon,
                         icon_color, disabled_icon_color);
}

bool InstalledExtensionMenuItemView::IsPinned() const {
  // |model_| can be null in unit tests.
  return model_ && model_->IsActionPinned(controller_->GetId());
}

void InstalledExtensionMenuItemView::OnContextMenuPressed() {
  base::RecordAction(base::UserMetricsAction(
      "Extensions.Toolbar.MoreActionsButtonPressedFromMenu"));
  // TODO(crbug.com/998298): Cleanup the menu source type.
  context_menu_controller_->ShowContextMenuForViewImpl(
      context_menu_button_, context_menu_button_->GetMenuPosition(),
      ui::MenuSourceType::MENU_SOURCE_MOUSE);
}

void InstalledExtensionMenuItemView::OnPinButtonPressed() {
  base::RecordAction(
      base::UserMetricsAction("Extensions.Toolbar.PinButtonPressed"));
  model_->SetActionVisibility(controller_->GetId(), !IsPinned());
  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      IsPinned() ? IDS_EXTENSION_PINNED : IDS_EXTENSION_UNPINNED));
}

bool InstalledExtensionMenuItemView::IsContextMenuRunningForTesting() const {
  return context_menu_controller_->IsMenuRunning();
}

BEGIN_METADATA(SiteAccessMenuItemView, views::View)
END_METADATA

BEGIN_METADATA(InstalledExtensionMenuItemView, views::View)
END_METADATA
