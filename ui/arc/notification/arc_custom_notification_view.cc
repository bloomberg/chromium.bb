// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_custom_notification_view.h"

#include "components/exo/notification_surface.h"
#include "components/exo/surface.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/display/screen.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/transform.h"
#include "ui/message_center/message_center_style.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace arc {

class ArcCustomNotificationView::EventForwarder : public ui::EventHandler {
 public:
  explicit EventForwarder(ArcCustomNotificationView* owner) : owner_(owner) {}
  ~EventForwarder() override = default;

 private:
  // ui::EventHandler
  void OnEvent(ui::Event* event) override { owner_->OnEvent(event); }

  ArcCustomNotificationView* const owner_;

  DISALLOW_COPY_AND_ASSIGN(EventForwarder);
};

class ArcCustomNotificationView::SlideHelper
    : public ui::LayerAnimationObserver {
 public:
  explicit SlideHelper(ArcCustomNotificationView* owner) : owner_(owner) {
    owner_->parent()->layer()->GetAnimator()->AddObserver(this);

    // Reset opacity to 1 to handle to case when the surface is sliding before
    // getting managed by this class, e.g. sliding in a popup before showing
    // in a message center view.
    if (owner_->surface_ && owner_->surface_->window())
      owner_->surface_->window()->layer()->SetOpacity(1.0f);
  }
  ~SlideHelper() override {
    owner_->parent()->layer()->GetAnimator()->RemoveObserver(this);
  }

  void Update() {
    const bool has_animation =
        owner_->parent()->layer()->GetAnimator()->is_animating();
    const bool has_transform = !owner_->parent()->GetTransform().IsIdentity();
    const bool sliding = has_transform || has_animation;
    if (sliding_ == sliding)
      return;

    sliding_ = sliding;

    if (sliding_)
      OnSlideStart();
    else
      OnSlideEnd();
  }

 private:
  void OnSlideStart() {
    if (!owner_->surface_ || !owner_->surface_->window())
      return;
    surface_copy_ = ::wm::RecreateLayers(owner_->surface_->window(), nullptr);
    owner_->layer()->Add(surface_copy_->root());
    owner_->surface_->window()->layer()->SetOpacity(0.0f);
  }

  void OnSlideEnd() {
    if (!owner_->surface_ || !owner_->surface_->window())
      return;
    owner_->surface_->window()->layer()->SetOpacity(1.0f);
    owner_->Layout();
    surface_copy_.reset();
  }

  // ui::LayerAnimationObserver
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* seq) override {
    Update();
  }
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* seq) override {
    Update();
  }
  void OnLayerAnimationScheduled(ui::LayerAnimationSequence* seq) override {}

  ArcCustomNotificationView* const owner_;
  bool sliding_ = false;
  std::unique_ptr<ui::LayerTreeOwner> surface_copy_;

  DISALLOW_COPY_AND_ASSIGN(SlideHelper);
};

ArcCustomNotificationView::ArcCustomNotificationView(
    ArcCustomNotificationItem* item,
    exo::NotificationSurface* surface)
    : item_(item), event_forwarder_(new EventForwarder(this)) {
  SetSurface(surface);
  item_->AddObserver(this);
  OnItemPinnedChanged();

  // Create a layer as an anchor to insert surface copy during a slide.
  SetPaintToLayer(true);
}

ArcCustomNotificationView::~ArcCustomNotificationView() {
  SetSurface(nullptr);
  if (item_)
    item_->RemoveObserver(this);
}

void ArcCustomNotificationView::CreateFloatingCloseButton() {
  floating_close_button_ = new views::ImageButton(this);
  floating_close_button_->set_background(
      views::Background::CreateSolidBackground(SK_ColorTRANSPARENT));
  floating_close_button_->SetBorder(
      views::Border::CreateEmptyBorder(5, 5, 5, 5));

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  floating_close_button_->SetImage(
      views::CustomButton::STATE_NORMAL,
      rb.GetImageSkiaNamed(IDR_NOTIFICATION_CLOSE));
  floating_close_button_->SetImage(
      views::CustomButton::STATE_HOVERED,
      rb.GetImageSkiaNamed(IDR_NOTIFICATION_CLOSE_HOVER));
  floating_close_button_->SetImage(
      views::CustomButton::STATE_PRESSED,
      rb.GetImageSkiaNamed(IDR_NOTIFICATION_CLOSE_PRESSED));
  floating_close_button_->set_animate_on_state_change(false);
  floating_close_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_ACCESSIBLE_NAME));

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = surface_->window();

  floating_close_button_widget_.reset(new views::Widget);
  floating_close_button_widget_->Init(params);
  floating_close_button_widget_->SetContentsView(floating_close_button_);

  Layout();
}

void ArcCustomNotificationView::SetSurface(exo::NotificationSurface* surface) {
  if (surface_ == surface)
    return;

  if (surface_ && surface_->window()) {
    surface_->window()->RemoveObserver(this);
    surface_->window()->RemovePreTargetHandler(event_forwarder_.get());
  }

  surface_ = surface;

  if (surface_ && surface_->window()) {
    surface_->window()->AddObserver(this);
    surface_->window()->AddPreTargetHandler(event_forwarder_.get());
  }
}

void ArcCustomNotificationView::UpdatePreferredSize() {
  gfx::Size preferred_size = surface_->GetSize();
  if (preferred_size.width() != message_center::kNotificationWidth) {
    const float scale = static_cast<float>(message_center::kNotificationWidth) /
                        preferred_size.width();
    preferred_size.SetSize(message_center::kNotificationWidth,
                           preferred_size.height() * scale);
  }

  SetPreferredSize(preferred_size);
}

void ArcCustomNotificationView::UpdateCloseButtonVisiblity() {
  if (!surface_ || !floating_close_button_widget_)
    return;

  const bool target_visiblity =
      surface_->window()->GetBoundsInScreen().Contains(
          display::Screen::GetScreen()->GetCursorScreenPoint());
  if (target_visiblity == floating_close_button_widget_->IsVisible())
    return;

  if (target_visiblity)
    floating_close_button_widget_->Show();
  else
    floating_close_button_widget_->Hide();
}

void ArcCustomNotificationView::ViewHierarchyChanged(
    const views::View::ViewHierarchyChangedDetails& details) {
  views::Widget* widget = GetWidget();

  if (!details.is_add) {
    // Resets slide helper when this view is removed from its parent.
    slide_helper_.reset();
  }

  // Bail if native_view() has attached to a different widget.
  if (widget && native_view() &&
      views::Widget::GetTopLevelWidgetForNativeView(native_view()) != widget) {
    return;
  }

  views::NativeViewHost::ViewHierarchyChanged(details);

  if (!widget || !surface_ || !details.is_add)
    return;

  UpdatePreferredSize();
  Attach(surface_->window());

  // Creates slide helper after this view is added to its parent.
  slide_helper_.reset(new SlideHelper(this));
}

void ArcCustomNotificationView::Layout() {
  views::NativeViewHost::Layout();

  if (!surface_ || !GetWidget())
    return;

  // Scale notification surface if necessary.
  gfx::Transform transform;
  const gfx::Size surface_size = surface_->GetSize();
  const gfx::Size contents_size = GetContentsBounds().size();
  if (!surface_size.IsEmpty() && !contents_size.IsEmpty()) {
    transform.Scale(
        static_cast<float>(contents_size.width()) / surface_size.width(),
        static_cast<float>(contents_size.height()) / surface_size.height());
  }
  surface_->window()->SetTransform(transform);

  if (!floating_close_button_widget_)
    return;

  gfx::Rect surface_local_bounds(surface_->GetSize());
  gfx::Rect close_button_bounds(floating_close_button_->GetPreferredSize());
  close_button_bounds.set_x(surface_local_bounds.right() -
                            close_button_bounds.width());
  close_button_bounds.set_y(surface_local_bounds.y());
  floating_close_button_widget_->SetBounds(close_button_bounds);

  UpdateCloseButtonVisiblity();
}

void ArcCustomNotificationView::OnKeyEvent(ui::KeyEvent* event) {
  // Forward to parent CustomNotificationView to handle keyboard dismissal.
  parent()->OnKeyEvent(event);
}

void ArcCustomNotificationView::OnGestureEvent(ui::GestureEvent* event) {
  // Forward to parent CustomNotificationView to handle sliding out.
  parent()->OnGestureEvent(event);
  slide_helper_->Update();
}

void ArcCustomNotificationView::OnMouseEntered(const ui::MouseEvent&) {
  UpdateCloseButtonVisiblity();
}

void ArcCustomNotificationView::OnMouseExited(const ui::MouseEvent&) {
  UpdateCloseButtonVisiblity();
}

void ArcCustomNotificationView::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  if (item_ && !item_->pinned() && sender == floating_close_button_) {
    item_->CloseFromCloseButton();
  }
}

void ArcCustomNotificationView::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  UpdatePreferredSize();
}

void ArcCustomNotificationView::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
}

void ArcCustomNotificationView::OnItemDestroying() {
  item_->RemoveObserver(this);
  item_ = nullptr;

  // Reset |surface_| with |item_| since no one is observing the |surface_|
  // after |item_| is gone and this view should be removed soon.
  SetSurface(nullptr);
}

void ArcCustomNotificationView::OnItemPinnedChanged() {
  if (item_->pinned() && floating_close_button_widget_) {
    floating_close_button_widget_.reset();
  } else if (!item_->pinned() && !floating_close_button_widget_) {
    CreateFloatingCloseButton();
  }
}

void ArcCustomNotificationView::OnItemNotificationSurfaceRemoved() {
  SetSurface(nullptr);
}

}  // namespace arc
