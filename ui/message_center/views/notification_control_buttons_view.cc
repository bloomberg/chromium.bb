// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_control_buttons_view.h"

#include "base/memory/ptr_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// This value should be the same as the duration of reveal animation of
// the settings view of an Android notification.
constexpr auto kBackgroundColorChangeDuration =
    base::TimeDelta::FromMilliseconds(360);

// The initial background color of the view.
constexpr SkColor kInitialBackgroundColor =
    message_center::kControlButtonBackgroundColor;

}  // anonymous namespace

namespace message_center {

const char NotificationControlButtonsView::kViewClassName[] =
    "NotificationControlButtonsView";

// InnerView is the floaing view which contains buttons. InnerView can a
// dedicate layer as a direct child of the root layer, so that it can be shown
// above the NativeViewHost of ARC notification.
class NotificationControlButtonsView::InnerView : public views::View {
 public:
  InnerView(NotificationControlButtonsView* parent) : parent_(parent) {
    SetPaintToLayer(ui::LAYER_TEXTURED);
    layer()->SetFillsBoundsOpaquely(false);
  }
  ~InnerView() override = default;

#if defined(OS_CHROMEOS)
  // views::View override:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override {
    // Update the layer hierarchy.
    // See NotificationControlButtonsView::CalculateOffsetToAncestorWithLayer()
    // for detail.
    UpdateParentLayer();
  }
#endif  // defined(OS_CHROMEOS)

  void SetLayerVisible(bool visible) {
    DCHECK(layer());
    DCHECK_EQ(parent(), parent_);
    layer()->SetOpacity(visible ? 1.0 : 0.0);
  }

  bool GetLayerVisible() const {
    DCHECK(layer());
    DCHECK_EQ(parent(), parent_);
    return layer()->opacity() != 0.0;
  }

#if defined(OS_CHROMEOS)
  void AdjustLayerBounds() {
    DCHECK(layer());
    DCHECK_EQ(parent(), parent_);
    if (!visible() || layer()->opacity() == 0)
      return;

    // Adjust the position of this layer.
    layer()->SetBounds(GetLocalBounds() + gfx::Vector2d(GetMirroredX(), y()) +
                       parent_->CalculateOffsetToAncestorWithLayer(nullptr));
    SnapLayerToPixelBoundary();
  }
#endif  // defined(OS_CHROMEOS)

 private:
  NotificationControlButtonsView* const parent_;

  DISALLOW_COPY_AND_ASSIGN(InnerView);
};

NotificationControlButtonsView::NotificationControlButtonsView(
    MessageView* message_view)
    : message_view_(message_view),
      bgcolor_origin_(kInitialBackgroundColor),
      bgcolor_target_(kInitialBackgroundColor) {
  DCHECK(message_view);
  SetLayoutManager(new views::FillLayout());

  buttons_container_ = base::MakeUnique<InnerView>(this);
  buttons_container_->set_owned_by_client();
  buttons_container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal));
  buttons_container_->SetBackground(
      views::CreateSolidBackground(kInitialBackgroundColor));
  AddChildView(buttons_container_.get());
}

NotificationControlButtonsView::~NotificationControlButtonsView() = default;

void NotificationControlButtonsView::ShowCloseButton(bool show) {
  if (show && !close_button_) {
    close_button_ = base::MakeUnique<message_center::PaddedButton>(this);
    close_button_->set_owned_by_client();
    close_button_->SetImage(views::CustomButton::STATE_NORMAL,
                            message_center::GetCloseIcon());
    close_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_ACCESSIBLE_NAME));
    close_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_TOOLTIP));
    close_button_->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));

    // Add the button at the last.
    DCHECK_LE(buttons_container_->child_count(), 1);
    buttons_container_->AddChildView(close_button_.get());
  } else if (!show && close_button_) {
    DCHECK(buttons_container_->Contains(close_button_.get()));
    close_button_.reset();
  }
}

void NotificationControlButtonsView::ShowSettingsButton(bool show) {
  if (show && !settings_button_) {
    settings_button_ = base::MakeUnique<message_center::PaddedButton>(this);
    settings_button_->set_owned_by_client();
    settings_button_->SetImage(views::CustomButton::STATE_NORMAL,
                               message_center::GetSettingsIcon());
    settings_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    settings_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    settings_button_->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));

    // Add the button at the first.
    DCHECK_LE(buttons_container_->child_count(), 1);
    buttons_container_->AddChildViewAt(settings_button_.get(), 0);
  } else if (!show && settings_button_) {
    DCHECK(buttons_container_->Contains(settings_button_.get()));
    settings_button_.reset();
  }
}

void NotificationControlButtonsView::SetBackgroundColor(
    const SkColor& target_bgcolor) {
  DCHECK(buttons_container_->background());
  if (buttons_container_->background()->get_color() != target_bgcolor) {
    bgcolor_origin_ = buttons_container_->background()->get_color();
    bgcolor_target_ = target_bgcolor;

    if (bgcolor_animation_)
      bgcolor_animation_->End();
    bgcolor_animation_.reset(new gfx::LinearAnimation(this));
    bgcolor_animation_->SetDuration(kBackgroundColorChangeDuration);
    bgcolor_animation_->Start();
  }
}

void NotificationControlButtonsView::SetButtonsVisible(bool visible) {
  DCHECK(buttons_container_->layer());
  // Manipulate the opacity instead of changing the visibility to keep the tab
  // order even when the view is invisible.
  buttons_container_->SetLayerVisible(visible);
  set_can_process_events_within_subtree(visible);
}

bool NotificationControlButtonsView::GetButtonsVisible() const {
  return buttons_container_->GetLayerVisible();
}

void NotificationControlButtonsView::RequestFocusOnCloseButton() {
  if (close_button_)
    close_button_->RequestFocus();
}

bool NotificationControlButtonsView::IsCloseButtonFocused() const {
  return close_button_ && close_button_->HasFocus();
}

bool NotificationControlButtonsView::IsSettingsButtonFocused() const {
  return settings_button_ && settings_button_->HasFocus();
}

#if defined(OS_CHROMEOS)

void NotificationControlButtonsView::ReparentToWidgetLayer() {
  // Reparent the inner container from the MessageView's layer to the widget
  // layer. This calls this class's CalculateOffsetToAncestorWithLayer
  // internally and reparents the layer.
  buttons_container_->SetPaintToLayer(ui::LAYER_TEXTURED);
  is_layer_parent_widget_ = true;
}

void NotificationControlButtonsView::AdjustLayerBounds() {
  // If the flag is not set, we don need to adjust the bound explicitly.
  if (!is_layer_parent_widget_)
    return;

  DCHECK_EQ(1, child_count());
  DCHECK_EQ(buttons_container_.get(), child_at(0));
  buttons_container_->AdjustLayerBounds();
}

#endif  // defined(OS_CHROMEOS)

message_center::PaddedButton*
NotificationControlButtonsView::close_button_for_testing() const {
  return close_button_.get();
}

message_center::PaddedButton*
NotificationControlButtonsView::settings_button_for_testing() const {
  return settings_button_.get();
}

const char* NotificationControlButtonsView::GetClassName() const {
  return kViewClassName;
}

#if defined(OS_CHROMEOS)

void NotificationControlButtonsView::ReorderChildLayers(
    ui::Layer* parent_layer) {
  // If the flag is set, do nothing, since the layer of the child view is a
  // child of th widget.
  if (!is_layer_parent_widget_)
    return views::View::ReorderChildLayers(parent_layer);
}

gfx::Vector2d
NotificationControlButtonsView::CalculateOffsetToAncestorWithLayer(
    ui::Layer** layer_parent) {
  // If the flag is not set, do the default behavior.
  if (!is_layer_parent_widget_)
    return views::View::CalculateOffsetToAncestorWithLayer(layer_parent);

  if (!parent() || !GetWidget())
    return gfx::Vector2d();

  // Sets the widget layer as the parent layer.
  if (layer_parent)
    *layer_parent = GetWidget()->GetLayer();

  // Returns the position relative to the widget origin.
  gfx::Point pos(GetMirroredX(), y());
  views::View::ConvertPointToWidget(parent(), &pos);
  return gfx::Vector2d(pos.x(), pos.y());
}

#endif  // defined(OS_CHROMEOS)

void NotificationControlButtonsView::ButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
  if (close_button_ && sender == close_button_.get()) {
    message_view_->OnCloseButtonPressed();
  } else if (settings_button_ && sender == settings_button_.get()) {
    message_view_->OnSettingsButtonPressed();
  }
}

void NotificationControlButtonsView::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(animation, bgcolor_animation_.get());

  const SkColor color = gfx::Tween::ColorValueBetween(
      animation->GetCurrentValue(), bgcolor_origin_, bgcolor_target_);
  buttons_container_->SetBackground(views::CreateSolidBackground(color));
  buttons_container_->SchedulePaint();
}

void NotificationControlButtonsView::AnimationEnded(
    const gfx::Animation* animation) {
  DCHECK_EQ(animation, bgcolor_animation_.get());
  bgcolor_animation_.reset();
  bgcolor_origin_ = bgcolor_target_;
}

void NotificationControlButtonsView::AnimationCanceled(
    const gfx::Animation* animation) {
  // The animation is never cancelled explicitly.
  NOTREACHED();

  bgcolor_origin_ = bgcolor_target_;
  buttons_container_->SetBackground(
      views::CreateSolidBackground(bgcolor_target_));
  buttons_container_->SchedulePaint();
}

}  // namespace message_center
