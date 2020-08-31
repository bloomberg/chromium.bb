// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hover_button.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/hover_button_controller.h"
#include "ui/events/event_constants.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

std::unique_ptr<views::Border> CreateBorderWithVerticalSpacing(
    int vertical_spacing) {
  const int horizontal_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUTTON_HORIZONTAL_PADDING);
  return views::CreateEmptyBorder(
      gfx::Insets(vertical_spacing, horizontal_spacing));
}

// Wrapper class for the icon that maintains consistent spacing for both badged
// and unbadged icons.
// Badging may make the icon slightly wider (but not taller). However, the
// layout should be the same whether or not the icon is badged, so allow the
// badged part of the icon to extend into the padding.
class IconWrapper : public views::View {
 public:
  explicit IconWrapper(std::unique_ptr<views::View> icon, int vertical_spacing)
      : icon_(AddChildView(std::move(icon))) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    // Make sure hovering over the icon also hovers the |HoverButton|.
    set_can_process_events_within_subtree(false);
    // Don't cover |icon| when the ink drops are being painted.
    // |MenuButton| already does this with its own image.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetProperty(views::kMarginsKey, gfx::Insets(vertical_spacing, 0));
  }

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    const int icon_height = icon_->GetPreferredSize().height();
    const int icon_label_spacing =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_LABEL_HORIZONTAL);
    return gfx::Size(icon_height + icon_label_spacing, icon_height);
  }

  views::View* icon() { return icon_; }

 private:
  views::View* icon_;
};

}  // namespace

HoverButton::HoverButton(views::ButtonListener* button_listener,
                         const base::string16& text)
    : views::LabelButton(button_listener, text, views::style::CONTEXT_BUTTON) {
  SetButtonController(std::make_unique<HoverButtonController>(
      this, button_listener,
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(this)));

  views::InstallRectHighlightPathGenerator(this);

  SetInstallFocusRingOnFocus(false);
  SetFocusBehavior(FocusBehavior::ALWAYS);

  const int vert_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_CONTROL_LIST_VERTICAL) /
                           2;
  SetBorder(CreateBorderWithVerticalSpacing(vert_spacing));

  SetInkDropMode(InkDropMode::ON);

  set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                              ui::EF_RIGHT_MOUSE_BUTTON);
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnRelease);
}

HoverButton::HoverButton(views::ButtonListener* button_listener,
                         const gfx::ImageSkia& icon,
                         const base::string16& text)
    : HoverButton(button_listener, text) {
  SetImage(STATE_NORMAL, icon);
}

HoverButton::HoverButton(views::ButtonListener* button_listener,
                         std::unique_ptr<views::View> icon_view,
                         const base::string16& title,
                         const base::string16& subtitle,
                         std::unique_ptr<views::View> secondary_view,
                         bool resize_row_for_secondary_view,
                         bool secondary_view_can_process_events)
    : HoverButton(button_listener, base::string16()) {
  label()->SetHandlesTooltips(false);

  // Set the layout manager to ignore the ink_drop_container to ensure the ink
  // drop tracks the bounds of its parent.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetChildViewIgnoredByLayout(ink_drop_container(), true);

  // The vertical space that must exist on the top and the bottom of the item
  // to ensure the proper spacing is maintained between items when stacking
  // vertically.
  const int vertical_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
                                   DISTANCE_CONTROL_LIST_VERTICAL) /
                               2;

  icon_view_ = AddChildView(std::make_unique<IconWrapper>(std::move(icon_view),
                                                          vertical_spacing))
                   ->icon();

  // |label_wrapper| will hold both the title and subtitle if it exists.
  auto label_wrapper = std::make_unique<views::View>();

  title_ = label_wrapper->AddChildView(
      std::make_unique<views::StyledLabel>(title, nullptr));
  // Allow the StyledLabel for title to assume its preferred size on a single
  // line and let the flex layout attenuate its width if necessary.
  title_->SizeToFit(0);
  // Hover the whole button when hovering |title_|. This is OK because |title_|
  // will never have a link in it.
  title_->set_can_process_events_within_subtree(false);

  if (!subtitle.empty()) {
    auto subtitle_label = std::make_unique<views::Label>(
        subtitle, views::style::CONTEXT_BUTTON, views::style::STYLE_SECONDARY);
    subtitle_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    subtitle_label->SetAutoColorReadabilityEnabled(false);
    subtitle_ = label_wrapper->AddChildView(std::move(subtitle_label));
  }

  label_wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  label_wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  label_wrapper->set_can_process_events_within_subtree(false);
  label_wrapper->SetProperty(views::kMarginsKey,
                             gfx::Insets(vertical_spacing, 0));
  label_wrapper_ = AddChildView(std::move(label_wrapper));
  // Observe |label_wrapper_| bounds changes to ensure the HoverButton tooltip
  // is kept in sync with the size.
  observed_label_.Add(label_wrapper_);

  if (secondary_view) {
    secondary_view->set_can_process_events_within_subtree(
        secondary_view_can_process_events);
    // |secondary_view| needs a layer otherwise it's obscured by the layer
    // used in drawing ink drops.
    secondary_view->SetPaintToLayer();
    secondary_view->layer()->SetFillsBoundsOpaquely(false);
    const int icon_label_spacing =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_LABEL_HORIZONTAL);

    // If |resize_row_for_secondary_view| is true set vertical margins such that
    // the vertical distance between HoverButtons is maintained.
    // Otherwise set vertical margins to 0 and allow the secondary view to grow
    // into the vertical margins that would otherwise exist due to |icon_view_|
    // and the |label_wrapper_|.
    const int secondary_spacing =
        resize_row_for_secondary_view ? vertical_spacing : 0;
    secondary_view->SetProperty(
        views::kMarginsKey, gfx::Insets(secondary_spacing, icon_label_spacing,
                                        secondary_spacing, 0));
    secondary_view_ = AddChildView(std::move(secondary_view));
  }

  // Create the appropriate border with no vertical insets. The required spacing
  // will be met via margins set on the containing views.
  SetBorder(CreateBorderWithVerticalSpacing(0));
}

HoverButton::~HoverButton() = default;

// static
SkColor HoverButton::GetInkDropColor(const views::View* view) {
  return views::style::GetColor(*view, views::style::CONTEXT_BUTTON,
                                views::style::STYLE_SECONDARY);
}

void HoverButton::SetBorder(std::unique_ptr<views::Border> b) {
  LabelButton::SetBorder(std::move(b));
  // Make sure the minimum size is correct according to the layout (if any).
  if (GetLayoutManager())
    SetMinSize(GetLayoutManager()->GetPreferredSize(this));
}

void HoverButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Button::GetAccessibleNodeData(node_data);
}

void HoverButton::OnViewBoundsChanged(View* observed_view) {
  LabelButton::OnViewBoundsChanged(observed_view);
  if (observed_view == label_wrapper_)
    SetTooltipAndAccessibleName();
}

void HoverButton::SetTitleTextStyle(views::style::TextStyle text_style,
                                    SkColor background_color) {
  title_->SetDisplayedOnBackgroundColor(background_color);
  title_->SetDefaultTextStyle(text_style);
}

void HoverButton::SetTooltipAndAccessibleName() {
  const base::string16 accessible_name =
      subtitle_ == nullptr
          ? title_->GetText()
          : base::JoinString({title_->GetText(), subtitle_->GetText()},
                             base::ASCIIToUTF16("\n"));

  // |views::StyledLabel|s only add tooltips for any links they may have.
  // However, since |HoverButton| will never insert a link inside its child
  // |StyledLabel|, decide whether it needs a tooltip by checking whether the
  // available space is smaller than its preferred size.
  const bool needs_tooltip =
      label_wrapper_->GetPreferredSize().width() > label_wrapper_->width();
  SetTooltipText(needs_tooltip ? accessible_name : base::string16());
  SetAccessibleName(accessible_name);
}

views::Button::KeyClickAction HoverButton::GetKeyClickActionForEvent(
    const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN) {
    // As the hover button is presented in the user menu, it triggers a
    // kOnKeyPress action every time the user clicks on enter on all platforms.
    // (it ignores the value of PlatformStyle::kReturnClicksFocusedControl)
    return KeyClickAction::kOnKeyPress;
  }
  return LabelButton::GetKeyClickActionForEvent(event);
}

void HoverButton::StateChanged(ButtonState old_state) {
  LabelButton::StateChanged(old_state);

  // |HoverButtons| are designed for use in a list, so ensure only one button
  // can have a hover background at any time by requesting focus on hover.
  if (state() == STATE_HOVERED && old_state != STATE_PRESSED) {
    RequestFocus();
  } else if (state() == STATE_NORMAL && HasFocus()) {
    GetFocusManager()->SetFocusedView(nullptr);
  }
}

SkColor HoverButton::GetInkDropBaseColor() const {
  return GetInkDropColor(this);
}

std::unique_ptr<views::InkDrop> HoverButton::CreateInkDrop() {
  std::unique_ptr<views::InkDrop> ink_drop = LabelButton::CreateInkDrop();
  // Turn on highlighting when the button is focused only - hovering the button
  // will request focus.
  ink_drop->SetShowHighlightOnFocus(true);
  ink_drop->SetShowHighlightOnHover(false);
  return ink_drop;
}

views::View* HoverButton::GetTooltipHandlerForPoint(const gfx::Point& point) {
  if (!HitTestPoint(point))
    return nullptr;

  // Let the secondary control handle it if it has a tooltip.
  if (secondary_view_) {
    gfx::Point point_in_secondary_view(point);
    ConvertPointToTarget(this, secondary_view_, &point_in_secondary_view);
    View* handler =
        secondary_view_->GetTooltipHandlerForPoint(point_in_secondary_view);
    if (handler) {
      gfx::Point point_in_handler_view(point);
      ConvertPointToTarget(this, handler, &point_in_handler_view);
      if (!handler->GetTooltipText(point_in_secondary_view).empty()) {
        return handler;
      }
    }
  }

  return this;
}
