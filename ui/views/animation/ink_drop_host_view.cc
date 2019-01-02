// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_host_view.h"

#include "build/build_config.h"
#include "ui/events/event.h"
#include "ui/events/scoped_target_handler.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_stub.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_properties.h"

namespace views {

// An EventHandler that is guaranteed to be invoked and is not prone to
// InkDropHostView descendents who do not call
// InkDropHostView::OnGestureEvent() and InkDropHostView::OnMouseEvent().
// Only one instance of this class can exist at any given time for each ink drop
// host view.
class InkDropHostView::InkDropEventHandler : public ui::EventHandler {
 public:
  explicit InkDropEventHandler(InkDropHostView* host_view)
      : target_handler_(
            std::make_unique<ui::ScopedTargetHandler>(host_view, this)),
        host_view_(host_view) {}

  ~InkDropEventHandler() override = default;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override {
    if (!host_view_->enabled() || host_view_->ink_drop_mode_ != InkDropMode::ON)
      return;

    InkDropState current_ink_drop_state =
        host_view_->GetInkDrop()->GetTargetInkDropState();

    InkDropState ink_drop_state = InkDropState::HIDDEN;
    switch (event->type()) {
      case ui::ET_GESTURE_TAP_DOWN:
        if (current_ink_drop_state == InkDropState::ACTIVATED)
          return;
        ink_drop_state = InkDropState::ACTION_PENDING;
        // The ui::ET_GESTURE_TAP_DOWN event needs to be marked as handled so
        // that subsequent events for the gesture are sent to |this|.
        event->SetHandled();
        break;
      case ui::ET_GESTURE_LONG_PRESS:
        if (current_ink_drop_state == InkDropState::ACTIVATED)
          return;
        ink_drop_state = InkDropState::ALTERNATE_ACTION_PENDING;
        break;
      case ui::ET_GESTURE_LONG_TAP:
        ink_drop_state = InkDropState::ALTERNATE_ACTION_TRIGGERED;
        break;
      case ui::ET_GESTURE_END:
      case ui::ET_GESTURE_SCROLL_BEGIN:
      case ui::ET_GESTURE_TAP_CANCEL:
        if (current_ink_drop_state == InkDropState::ACTIVATED)
          return;
        ink_drop_state = InkDropState::HIDDEN;
        break;
      default:
        return;
    }

    if (ink_drop_state == InkDropState::HIDDEN &&
        (current_ink_drop_state == InkDropState::ACTION_TRIGGERED ||
         current_ink_drop_state == InkDropState::ALTERNATE_ACTION_TRIGGERED ||
         current_ink_drop_state == InkDropState::DEACTIVATED ||
         current_ink_drop_state == InkDropState::HIDDEN)) {
      // These InkDropStates automatically transition to the HIDDEN state so we
      // don't make an explicit call. Explicitly animating to HIDDEN in this
      // case would prematurely pre-empt these animations.
      return;
    }
    host_view_->AnimateInkDrop(ink_drop_state, event);
  }

  void OnMouseEvent(ui::MouseEvent* event) override {
    switch (event->type()) {
      case ui::ET_MOUSE_ENTERED:
        host_view_->GetInkDrop()->SetHovered(true);
        break;
      case ui::ET_MOUSE_EXITED:
        host_view_->GetInkDrop()->SetHovered(false);
        break;
      case ui::ET_MOUSE_DRAGGED:
        host_view_->GetInkDrop()->SetHovered(
            host_view_->GetLocalBounds().Contains(event->location()));
        break;
      default:
        break;
    }
  }

 private:
  // Allows |this| to handle all GestureEvents on |host_view_|.
  std::unique_ptr<ui::ScopedTargetHandler> target_handler_;

  // The host view to cache ui::Events to when animating the ink drop.
  InkDropHostView* host_view_;

  DISALLOW_COPY_AND_ASSIGN(InkDropEventHandler);
};

InkDropHostView::InkDropHostView()
    : ink_drop_event_handler_(std::make_unique<InkDropEventHandler>(this)) {}

InkDropHostView::~InkDropHostView() {
  // TODO(bruthig): Improve InkDropImpl to be safer about calling back to
  // potentially destroyed InkDropHosts and remove |destroying_|.
  destroying_ = true;
}

void InkDropHostView::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  old_paint_to_layer_ = layer() != nullptr;
  if (!old_paint_to_layer_)
    SetPaintToLayer();

  layer()->SetFillsBoundsOpaquely(false);
  InstallInkDropMask(ink_drop_layer);
  layer()->Add(ink_drop_layer);
  layer()->StackAtBottom(ink_drop_layer);
}

void InkDropHostView::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  // No need to do anything when called during shutdown, and if a derived
  // class has overridden Add/RemoveInkDropLayer, running this implementation
  // would be wrong.
  if (destroying_)
    return;
  layer()->Remove(ink_drop_layer);
  // Layers safely handle destroying a mask layer before the masked layer.
  ink_drop_mask_.reset();
  if (!old_paint_to_layer_)
    DestroyLayer();
}

std::unique_ptr<InkDrop> InkDropHostView::CreateInkDrop() {
  return CreateDefaultFloodFillInkDropImpl();
}

std::unique_ptr<InkDropRipple> InkDropHostView::CreateInkDropRipple() const {
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), gfx::Insets(), GetInkDropCenterBasedOnLastEvent(),
      GetInkDropBaseColor(), ink_drop_visible_opacity());
}

std::unique_ptr<InkDropHighlight> InkDropHostView::CreateInkDropHighlight()
    const {
  auto highlight = std::make_unique<views::InkDropHighlight>(
      size(), 0, gfx::RectF(GetMirroredRect(GetLocalBounds())).CenterPoint(),
      GetInkDropBaseColor());
  // TODO(pbos): Once |ink_drop_highlight_opacity_| is either always set or
  // callers are using the default InkDropHighlight value then make this a
  // constructor argument to InkDropHighlight.
  if (ink_drop_highlight_opacity_)
    highlight->set_visible_opacity(*ink_drop_highlight_opacity_);

  return highlight;
}

std::unique_ptr<views::InkDropMask> InkDropHostView::CreateInkDropMask() const {
  return std::make_unique<views::PathInkDropMask>(size(),
                                                  GetHighlightPath(this));
}

SkColor InkDropHostView::GetInkDropBaseColor() const {
  NOTREACHED();
  return gfx::kPlaceholderColor;
}

void InkDropHostView::SetInkDropMode(InkDropMode ink_drop_mode) {
  ink_drop_mode_ = ink_drop_mode;
  ink_drop_ = nullptr;
}

void InkDropHostView::AnimateInkDrop(InkDropState state,
                                     const ui::LocatedEvent* event) {
#if defined(OS_WIN)
  // On Windows, don't initiate ink-drops for touch/gesture events.
  // Additionally, certain event states should dismiss existing ink-drop
  // animations. If the state is already other than HIDDEN, presumably from
  // a mouse or keyboard event, then the state should be allowed. Conversely,
  // if the requested state is ACTIVATED, then it should always be allowed.
  if (event && (event->IsTouchEvent() || event->IsGestureEvent()) &&
      GetInkDrop()->GetTargetInkDropState() == InkDropState::HIDDEN &&
      state != InkDropState::ACTIVATED)
    return;
#endif
  last_ripple_triggering_event_.reset(
      event ? ui::Event::Clone(*event).release()->AsLocatedEvent() : nullptr);
  GetInkDrop()->AnimateToState(state);
}

void InkDropHostView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  // If we're being removed hide the ink-drop so if we're highlighted now the
  // highlight won't be active if we're added back again.
  if (!details.is_add && details.child == this && ink_drop_) {
    GetInkDrop()->SnapToHidden();
    GetInkDrop()->SetHovered(false);
  }
  View::ViewHierarchyChanged(details);
}

void InkDropHostView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (ink_drop_)
    ink_drop_->HostSizeChanged(size());
}

void InkDropHostView::VisibilityChanged(View* starting_from, bool is_visible) {
  View::VisibilityChanged(starting_from, is_visible);
  if (GetWidget() && !is_visible) {
    GetInkDrop()->AnimateToState(InkDropState::HIDDEN);
    GetInkDrop()->SetHovered(false);
  }
}

void InkDropHostView::OnFocus() {
  views::View::OnFocus();
  GetInkDrop()->SetFocused(true);
}

void InkDropHostView::OnBlur() {
  views::View::OnBlur();
  GetInkDrop()->SetFocused(false);
}

std::unique_ptr<InkDropImpl> InkDropHostView::CreateDefaultInkDropImpl() {
  auto ink_drop = std::make_unique<InkDropImpl>(this, size());
  ink_drop->SetAutoHighlightMode(
      InkDropImpl::AutoHighlightMode::HIDE_ON_RIPPLE);
  return ink_drop;
}

std::unique_ptr<InkDropImpl>
InkDropHostView::CreateDefaultFloodFillInkDropImpl() {
  auto ink_drop = std::make_unique<InkDropImpl>(this, size());
  ink_drop->SetAutoHighlightMode(
      views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
  return ink_drop;
}

std::unique_ptr<InkDropRipple> InkDropHostView::CreateDefaultInkDropRipple(
    const gfx::Point& center_point,
    const gfx::Size& size) const {
  return CreateSquareInkDropRipple(center_point, size);
}

std::unique_ptr<InkDropRipple> InkDropHostView::CreateSquareInkDropRipple(
    const gfx::Point& center_point,
    const gfx::Size& size) const {
  auto ripple = std::make_unique<SquareInkDropRipple>(
      CalculateLargeInkDropSize(size), ink_drop_large_corner_radius_, size,
      ink_drop_small_corner_radius_, center_point, GetInkDropBaseColor(),
      ink_drop_visible_opacity());
  return ripple;
}

std::unique_ptr<InkDropHighlight>
InkDropHostView::CreateDefaultInkDropHighlight(const gfx::PointF& center_point,
                                               const gfx::Size& size) const {
  return CreateSquareInkDropHighlight(center_point, size);
}

std::unique_ptr<InkDropHighlight> InkDropHostView::CreateSquareInkDropHighlight(
    const gfx::PointF& center_point,
    const gfx::Size& size) const {
  auto highlight = std::make_unique<InkDropHighlight>(
      size, ink_drop_small_corner_radius_, center_point, GetInkDropBaseColor());
  highlight->set_explode_size(gfx::SizeF(CalculateLargeInkDropSize(size)));
  return highlight;
}

bool InkDropHostView::HasInkDrop() const {
  return !!ink_drop_;
}

InkDrop* InkDropHostView::GetInkDrop() {
  if (!ink_drop_) {
    if (ink_drop_mode_ == InkDropMode::OFF)
      ink_drop_ = std::make_unique<InkDropStub>();
    else
      ink_drop_ = CreateInkDrop();
    OnInkDropCreated();
  }
  return ink_drop_.get();
}

gfx::Point InkDropHostView::GetInkDropCenterBasedOnLastEvent() const {
  return last_ripple_triggering_event_
             ? last_ripple_triggering_event_->location()
             : GetMirroredRect(GetContentsBounds()).CenterPoint();
}

void InkDropHostView::InstallInkDropMask(ui::Layer* ink_drop_layer) {
  ink_drop_mask_ = CreateInkDropMask();
  if (ink_drop_mask_)
    ink_drop_layer->SetMaskLayer(ink_drop_mask_->layer());
}

void InkDropHostView::ResetInkDropMask() {
  ink_drop_mask_.reset();
}

// static
gfx::Size InkDropHostView::CalculateLargeInkDropSize(
    const gfx::Size& small_size) {
  // The scale factor to compute the large size of the default
  // SquareInkDropRipple.
  constexpr float kLargeInkDropScale = 1.333f;
  return gfx::ScaleToCeiledSize(gfx::Size(small_size), kLargeInkDropScale);
}

}  // namespace views
