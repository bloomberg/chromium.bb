// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_item_view.h"

#include <algorithm>
#include <memory>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/wm_highlight_item_border.h"
#include "base/containers/contains.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Duration of the show/hide animation of the header.
constexpr base::TimeDelta kHeaderFadeDuration =
    base::TimeDelta::FromMilliseconds(167);

// Delay before the show animation of the header.
constexpr base::TimeDelta kHeaderFadeInDelay =
    base::TimeDelta::FromMilliseconds(83);

// Duration of the slow show animation of the close button.
constexpr base::TimeDelta kCloseButtonSlowFadeInDuration =
    base::TimeDelta::FromMilliseconds(300);

// Delay before the slow show animation of the close button.
constexpr base::TimeDelta kCloseButtonSlowFadeInDelay =
    base::TimeDelta::FromMilliseconds(750);

constexpr int kCloseButtonInkDropRadiusDp = 18;

// Value should match the one in
// ash/resources/vector_icons/overview_window_close.icon.
constexpr int kCloseButtonIconMarginDp = 5;

// Animates |layer| from 0 -> 1 opacity if |visible| and 1 -> 0 opacity
// otherwise. The tween type differs for |visible| and if |visible| is true
// there is a slight delay before the animation begins. Does not animate if
// opacity matches |visible|.
void AnimateLayerOpacity(ui::Layer* layer, bool visible) {
  float target_opacity = visible ? 1.f : 0.f;
  if (layer->GetTargetOpacity() == target_opacity)
    return;

  layer->SetOpacity(1.f - target_opacity);
  gfx::Tween::Type tween =
      visible ? gfx::Tween::LINEAR_OUT_SLOW_IN : gfx::Tween::FAST_OUT_LINEAR_IN;
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetTransitionDuration(kHeaderFadeDuration);
  settings.SetTweenType(tween);
  settings.SetPreemptionStrategy(ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  if (visible) {
    layer->GetAnimator()->SchedulePauseForProperties(
        kHeaderFadeInDelay, ui::LayerAnimationElement::OPACITY);
  }
  layer->SetOpacity(target_opacity);
}

// The close button for the overview item. It has a custom ink drop.
class OverviewCloseButton : public views::ImageButton {
 public:
  explicit OverviewCloseButton(PressedCallback callback)
      : views::ImageButton(std::move(callback)) {
    ink_drop()->SetMode(views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);
    views::InkDrop::UseInkDropForFloodFillRipple(ink_drop());
    SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    SetMinimumImageSize(gfx::Size(kHeaderHeightDp, kHeaderHeightDp));
    SetAccessibleName(l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
    SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
    SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);

    views::InstallFixedSizeCircleHighlightPathGenerator(
        this, kCloseButtonInkDropRadiusDp);
  }
  OverviewCloseButton(const OverviewCloseButton&) = delete;
  OverviewCloseButton& operator=(const OverviewCloseButton&) = delete;
  ~OverviewCloseButton() override = default;

  // Resets the listener so that the listener can go out of scope.
  void ResetListener() { SetCallback(views::Button::PressedCallback()); }

 protected:
  // views::ImageButton:
  void OnThemeChanged() override {
    views::ImageButton::OnThemeChanged();
    auto* color_provider = AshColorProvider::Get();
    const SkColor color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonIconColor);
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(kOverviewWindowCloseIcon, color));

    const auto ripple_attributes = color_provider->GetRippleAttributes(color);
    ink_drop()->SetBaseColor(ripple_attributes.base_color);
    ink_drop()->SetVisibleOpacity(ripple_attributes.inkdrop_opacity);
  }
};

}  // namespace

OverviewItemView::OverviewItemView(
    OverviewItem* overview_item,
    views::Button::PressedCallback close_callback,
    aura::Window* window,
    bool show_preview)
    : WindowMiniView(window), overview_item_(overview_item) {
  DCHECK(overview_item_);
  // This should not be focusable. It's also to avoid accessibility error when
  // |window->GetTitle()| is empty.
  SetFocusBehavior(FocusBehavior::NEVER);

  close_button_ = header_view()->AddChildView(
      std::make_unique<OverviewCloseButton>(std::move(close_callback)));
  close_button_->SetPaintToLayer();
  close_button_->layer()->SetFillsBoundsOpaquely(false);

  // Call this last as it calls |Layout()| which relies on the some of the other
  // elements existing.
  SetShowPreview(show_preview);
  // Do not show header if the current overview item is the drop target widget.
  if (show_preview || overview_item_->overview_grid()->IsDropTargetWindow(
                          overview_item_->GetWindow())) {
    header_view()->layer()->SetOpacity(0.f);
    current_header_visibility_ = HeaderVisibility::kInvisible;
  }

  UpdateIconView();
}

OverviewItemView::~OverviewItemView() = default;

void OverviewItemView::SetHeaderVisibility(HeaderVisibility visibility) {
  DCHECK(header_view()->layer());
  if (visibility == current_header_visibility_)
    return;
  const HeaderVisibility previous_visibility = current_header_visibility_;
  current_header_visibility_ = visibility;

  const bool all_invisible = visibility == HeaderVisibility::kInvisible;
  AnimateLayerOpacity(header_view()->layer(), !all_invisible);

  // If |header_view()| is fading out, we are done. Depending on if the close
  // button was visible, it will fade out with |header_view()| or stay hidden.
  if (all_invisible || !close_button_)
    return;

  const bool close_button_visible = visibility == HeaderVisibility::kVisible;
  // If |header_view()| was hidden and is fading in, set the opacity of
  // |close_button_| depending on whether the close button should fade in with
  // |header_view()| or stay hidden.
  if (previous_visibility == HeaderVisibility::kInvisible) {
    close_button_->layer()->SetOpacity(close_button_visible ? 1.f : 0.f);
    return;
  }

  AnimateLayerOpacity(close_button_->layer(), close_button_visible);
}

void OverviewItemView::HideCloseInstantlyAndThenShowItSlowly() {
  DCHECK(close_button_);
  DCHECK_NE(HeaderVisibility::kInvisible, current_header_visibility_);
  ui::Layer* layer = close_button_->layer();
  DCHECK(layer);
  current_header_visibility_ = HeaderVisibility::kVisible;
  layer->SetOpacity(0.f);
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetTransitionDuration(kCloseButtonSlowFadeInDuration);
  settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings.SetPreemptionStrategy(ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  layer->GetAnimator()->SchedulePauseForProperties(
      kCloseButtonSlowFadeInDelay, ui::LayerAnimationElement::OPACITY);
  layer->SetOpacity(1.f);
}

void OverviewItemView::OnOverviewItemWindowRestoring() {
  overview_item_ = nullptr;
  static_cast<OverviewCloseButton*>(close_button_)->ResetListener();
}

void OverviewItemView::RefreshPreviewView() {
  if (!preview_view())
    return;

  preview_view()->RecreatePreviews();
  Layout();
}

gfx::Rect OverviewItemView::GetHeaderBounds() const {
  // We want to align the edges of the image as shown below in the diagram. The
  // resource itself contains some padding, which is the distance from the edges
  // of the image to the edges of the vector icon. The icon keeps its size in
  // dips and is centered in the middle of the preferred width, so the
  // additional padding would be equal to half the difference in width between
  // the preferred width and the image size. The resulting padding would be that
  // number plus the padding in the resource, in dips.
  const int image_width = kIconSize.width();
  const int close_button_width = close_button_->GetPreferredSize().width();
  const int right_padding =
      (close_button_width - image_width) / 2 + kCloseButtonIconMarginDp;

  // Positions the header in a way so that the right aligned close button is
  // aligned so that the edge of its icon, not the button lines up with the
  // margins. In the diagram below, a represents the the right edge of the
  // provided icon (which contains some padding), b represents the right edge of
  // |close_button_| and c represents the right edge of the local bounds.
  // ---------------------------+---------+
  //                            |  +---+  |
  //      |title_label_|        |  |   a  b
  //                            |  +---+  |
  // ---------------------------+---------+
  //                                   c
  //                                   |

  // The size of this view is larger than that of the visible elements. Position
  // the header so that the margin is accounted for, then shift the right bounds
  // by a bit so that the close button resource lines up with the right edge of
  // the visible region.
  const gfx::Rect contents_bounds(GetContentsBounds());
  return gfx::Rect(contents_bounds.x(), contents_bounds.y(),
                   contents_bounds.width() + right_padding, kHeaderHeightDp);
}

gfx::Size OverviewItemView::GetPreviewViewSize() const {
  // The preview should expand to fit the bounds allocated for the content,
  // except if it is letterboxed or pillarboxed.
  const gfx::SizeF preview_pref_size(preview_view()->GetPreferredSize());
  const float aspect_ratio =
      preview_pref_size.width() / preview_pref_size.height();
  gfx::SizeF target_size(GetContentAreaBounds().size());
  OverviewGridWindowFillMode fill_mode =
      overview_item_ ? overview_item_->GetWindowDimensionsType()
                     : OverviewGridWindowFillMode::kNormal;
  switch (fill_mode) {
    case OverviewGridWindowFillMode::kNormal:
      break;
    case OverviewGridWindowFillMode::kLetterBoxed:
      target_size.set_height(target_size.width() / aspect_ratio);
      break;
    case OverviewGridWindowFillMode::kPillarBoxed:
      target_size.set_width(target_size.height() * aspect_ratio);
      break;
  }

  return gfx::ToRoundedSize(target_size);
}

views::View* OverviewItemView::GetView() {
  return this;
}

void OverviewItemView::MaybeActivateHighlightedView() {
  if (overview_item_)
    overview_item_->OnHighlightedViewActivated();
}

void OverviewItemView::MaybeCloseHighlightedView() {
  if (overview_item_)
    overview_item_->OnHighlightedViewClosed();
}

void OverviewItemView::MaybeSwapHighlightedView(bool right) {}

bool OverviewItemView::MaybeActivateHighlightedViewOnOverviewExit(
    OverviewSession* overview_session) {
  DCHECK(overview_session);
  overview_session->SelectWindow(overview_item_);
  return true;
}

void OverviewItemView::OnViewHighlighted() {
  UpdateBorderState(/*show=*/true);
}

void OverviewItemView::OnViewUnhighlighted() {
  UpdateBorderState(/*show=*/false);
}

gfx::Point OverviewItemView::GetMagnifierFocusPointInScreen() {
  // When this item is tabbed into, put the magnifier focus on the front of the
  // title, so that users can read the title first thing.
  const gfx::Rect title_bounds = title_label()->GetBoundsInScreen();
  return gfx::Point(GetMirroredXInView(title_bounds.x()),
                    title_bounds.CenterPoint().y());
}

const char* OverviewItemView::GetClassName() const {
  return "OverviewItemView";
}

bool OverviewItemView::OnMousePressed(const ui::MouseEvent& event) {
  if (!overview_item_)
    return views::View::OnMousePressed(event);
  overview_item_->HandleMouseEvent(event);
  return true;
}

bool OverviewItemView::OnMouseDragged(const ui::MouseEvent& event) {
  if (!overview_item_)
    return views::View::OnMouseDragged(event);
  overview_item_->HandleMouseEvent(event);
  return true;
}

void OverviewItemView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!overview_item_) {
    views::View::OnMouseReleased(event);
    return;
  }
  overview_item_->HandleMouseEvent(event);
}

void OverviewItemView::OnGestureEvent(ui::GestureEvent* event) {
  if (!overview_item_)
    return;

  overview_item_->HandleGestureEvent(event);
  event->SetHandled();
}

bool OverviewItemView::CanAcceptEvent(const ui::Event& event) {
  bool accept_events = true;
  // Do not process or accept press down events that are on the border.
  static ui::EventType press_types[] = {ui::ET_GESTURE_TAP_DOWN,
                                        ui::ET_MOUSE_PRESSED};
  if (event.IsLocatedEvent() && base::Contains(press_types, event.type())) {
    const gfx::Rect content_bounds = GetContentsBounds();
    if (!content_bounds.Contains(event.AsLocatedEvent()->location()))
      accept_events = false;
  }

  return accept_events && views::View::CanAcceptEvent(event);
}

void OverviewItemView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  WindowMiniView::GetAccessibleNodeData(node_data);

  // TODO: This doesn't allow |this| to be navigated by ChromeVox, find a way
  // to allow |this| as well as the title and close button.
  node_data->role = ax::mojom::Role::kGenericContainer;
  node_data->AddStringAttribute(
      ax::mojom::StringAttribute::kDescription,
      l10n_util::GetStringUTF8(
          IDS_ASH_OVERVIEW_CLOSABLE_HIGHLIGHT_ITEM_A11Y_EXTRA_TIP));
}

}  // namespace ash
