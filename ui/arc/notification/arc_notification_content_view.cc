// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_notification_content_view.h"

#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/memory/ptr_util.h"
#include "components/exo/notification_surface.h"
#include "components/exo/surface.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/arc/notification/arc_notification_surface.h"
#include "ui/arc/notification/arc_notification_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/transform.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_util.h"

namespace arc {

namespace {

SkColor GetControlButtonBackgroundColor(
    const mojom::ArcNotificationShownContents& shown_contents) {
  if (shown_contents == mojom::ArcNotificationShownContents::CONTENTS_SHOWN)
    return message_center::kControlButtonBackgroundColor;
  else
    return SK_ColorTRANSPARENT;
}

}  // namespace

class ArcNotificationContentView::MouseEnterExitHandler
    : public ui::EventHandler {
 public:
  explicit MouseEnterExitHandler(ArcNotificationContentView* owner)
      : owner_(owner) {
    DCHECK(owner);
  }
  ~MouseEnterExitHandler() override = default;

  // ui::EventHandler
  void OnMouseEvent(ui::MouseEvent* event) override {
    ui::EventHandler::OnMouseEvent(event);
    if (event->type() == ui::ET_MOUSE_ENTERED ||
        event->type() == ui::ET_MOUSE_EXITED) {
      owner_->UpdateControlButtonsVisibility();
    }
  }

 private:
  ArcNotificationContentView* const owner_;

  DISALLOW_COPY_AND_ASSIGN(MouseEnterExitHandler);
};

class ArcNotificationContentView::EventForwarder : public ui::EventHandler {
 public:
  explicit EventForwarder(ArcNotificationContentView* owner) : owner_(owner) {}
  ~EventForwarder() override = default;

 private:
  // ui::EventHandler
  void OnEvent(ui::Event* event) override {
    // Do not forward event targeted to the floating close button so that
    // keyboard press and tap are handled properly.
    if (owner_->floating_control_buttons_widget_ && event->target() &&
        owner_->floating_control_buttons_widget_->GetNativeWindow() ==
            event->target()) {
      return;
    }

    // TODO(yoshiki): Use a better tigger (eg. focusing EditText on
    // notification) than clicking (crbug.com/697379).
    if (event->type() == ui::ET_MOUSE_PRESSED ||
        event->type() == ui::ET_GESTURE_TAP)
      owner_->Activate();

    views::Widget* widget = owner_->GetWidget();
    if (!widget)
      return;

    // Forward the events to the containing widget, except for:
    // 1. Touches, because View should no longer receive touch events.
    //    See View::OnTouchEvent.
    // 2. Tap gestures are handled on the Android side, so ignore them.
    //    See crbug.com/709911.
    // 3. Key events. These are already forwarded by NotificationSurface's
    //    WindowDelegate.
    if (event->IsLocatedEvent()) {
      ui::LocatedEvent* located_event = event->AsLocatedEvent();
      located_event->target()->ConvertEventToTarget(widget->GetNativeWindow(),
                                                    located_event);
      if (located_event->type() == ui::ET_MOUSE_ENTERED ||
          located_event->type() == ui::ET_MOUSE_EXITED) {
        owner_->UpdateControlButtonsVisibility();
        return;
      }

      if (located_event->type() == ui::ET_MOUSE_MOVED ||
          located_event->IsMouseWheelEvent()) {
        widget->OnMouseEvent(located_event->AsMouseEvent());
      } else if (located_event->IsScrollEvent()) {
        widget->OnScrollEvent(located_event->AsScrollEvent());
      } else if (located_event->IsGestureEvent() &&
                 event->type() != ui::ET_GESTURE_TAP) {
        widget->OnGestureEvent(located_event->AsGestureEvent());
      }
    }

    // If AXTree is attached to notification content view, notification surface
    // always gets focus. Tab key events are consumed by the surface, and tab
    // focus traversal gets stuck at Android notification. To prevent it, always
    // pass tab key event to focus manager of content view.
    // TODO(yawano): include elements inside Android notification in tab focus
    // traversal rather than skipping them.
    if (owner_->surface_ && owner_->surface_->GetAXTreeId() != -1 &&
        event->IsKeyEvent()) {
      ui::KeyEvent* key_event = event->AsKeyEvent();
      if (key_event->key_code() == ui::VKEY_TAB &&
          (key_event->flags() == ui::EF_NONE ||
           key_event->flags() == ui::EF_SHIFT_DOWN)) {
        widget->GetFocusManager()->OnKeyEvent(*key_event);
      }
    }
  }

  ArcNotificationContentView* const owner_;

  DISALLOW_COPY_AND_ASSIGN(EventForwarder);
};

class ArcNotificationContentView::SlideHelper
    : public ui::LayerAnimationObserver {
 public:
  explicit SlideHelper(ArcNotificationContentView* owner) : owner_(owner) {
    GetSlideOutLayer()->GetAnimator()->AddObserver(this);

    // Reset opacity to 1 to handle to case when the surface is sliding before
    // getting managed by this class, e.g. sliding in a popup before showing
    // in a message center view.
    if (owner_->surface_) {
      DCHECK(owner_->surface_->GetWindow());
      owner_->surface_->GetWindow()->layer()->SetOpacity(1.0f);
    }
  }
  ~SlideHelper() override {
    if (GetSlideOutLayer())
      GetSlideOutLayer()->GetAnimator()->RemoveObserver(this);
  }

  void Update() {
    const bool has_animation =
        GetSlideOutLayer()->GetAnimator()->is_animating();
    const bool has_transform = !GetSlideOutLayer()->transform().IsIdentity();
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
  // This is a temporary hack to address crbug.com/718965
  ui::Layer* GetSlideOutLayer() {
    ui::Layer* layer = owner_->parent()->layer();
    return layer ? layer : owner_->GetWidget()->GetLayer();
  }

  void OnSlideStart() {
    if (!owner_->surface_)
      return;
    DCHECK(owner_->surface_->GetWindow());
    surface_copy_ = ::wm::RecreateLayers(owner_->surface_->GetWindow());
    // |surface_copy_| is at (0, 0) in owner_->layer().
    surface_copy_->root()->SetBounds(gfx::Rect(surface_copy_->root()->size()));
    owner_->layer()->Add(surface_copy_->root());
    owner_->surface_->GetWindow()->layer()->SetOpacity(0.0f);
  }

  void OnSlideEnd() {
    if (!owner_->surface_)
      return;
    DCHECK(owner_->surface_->GetWindow());
    owner_->surface_->GetWindow()->layer()->SetOpacity(1.0f);
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

  ArcNotificationContentView* const owner_;
  bool sliding_ = false;
  std::unique_ptr<ui::LayerTreeOwner> surface_copy_;

  DISALLOW_COPY_AND_ASSIGN(SlideHelper);
};

class ArcNotificationContentView::ContentViewDelegate
    : public ArcNotificationContentViewDelegate {
 public:
  explicit ContentViewDelegate(ArcNotificationContentView* owner)
      : owner_(owner) {}

  bool IsCloseButtonFocused() const override {
    if (!owner_->control_buttons_view_)
      return false;
    return owner_->control_buttons_view_->IsCloseButtonFocused();
  }

  void RequestFocusOnCloseButton() override {
    if (owner_->control_buttons_view_)
      owner_->control_buttons_view_->RequestFocusOnCloseButton();
    owner_->UpdateControlButtonsVisibility();
  }

  void UpdateControlButtonsVisibility() override {
    owner_->UpdateControlButtonsVisibility();
  }

  void OnSlideChanged() override {
    if (owner_->slide_helper_)
      owner_->slide_helper_->Update();
  }

  message_center::NotificationControlButtonsView* GetControlButtonsView()
      const override {
    return owner_->control_buttons_view_;
  }

  bool IsExpanded() const override { return owner_->IsExpanded(); }

  void SetExpanded(bool expanded) override { owner_->SetExpanded(expanded); }

 private:
  ArcNotificationContentView* const owner_;

  DISALLOW_COPY_AND_ASSIGN(ContentViewDelegate);
};

// static, for ArcNotificationContentView::GetClassName().
const char ArcNotificationContentView::kViewClassName[] =
    "ArcNotificationContentView";

ArcNotificationContentView::ArcNotificationContentView(
    ArcNotificationItem* item)
    : item_(item),
      notification_key_(item->GetNotificationKey()),
      event_forwarder_(new EventForwarder(this)),
      mouse_enter_exit_handler_(new MouseEnterExitHandler(this)) {
  // kNotificationWidth must be 360, since this value is separately defiend in
  // ArcNotificationWrapperView class in Android side.
  DCHECK_EQ(360, message_center::kNotificationWidth);

  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_notify_enter_exit_on_child(true);

  item_->IncrementWindowRefCount();
  item_->AddObserver(this);

  auto* surface_manager = ArcNotificationSurfaceManager::Get();
  if (surface_manager) {
    surface_manager->AddObserver(this);
    ArcNotificationSurface* surface =
        surface_manager->GetArcSurface(notification_key_);
    if (surface)
      OnNotificationSurfaceAdded(surface);
  }

  // Create a layer as an anchor to insert surface copy during a slide.
  SetPaintToLayer();
  UpdatePreferredSize();
  UpdateAccessibleName();
}

ArcNotificationContentView::~ArcNotificationContentView() {
  SetSurface(nullptr);

  auto* surface_manager = ArcNotificationSurfaceManager::Get();
  if (surface_manager)
    surface_manager->RemoveObserver(this);
  if (item_) {
    item_->RemoveObserver(this);
    item_->DecrementWindowRefCount();
  }
}

const char* ArcNotificationContentView::GetClassName() const {
  return kViewClassName;
}

std::unique_ptr<ArcNotificationContentViewDelegate>
ArcNotificationContentView::CreateContentViewDelegate() {
  return std::make_unique<ArcNotificationContentView::ContentViewDelegate>(
      this);
}

void ArcNotificationContentView::MaybeCreateFloatingControlButtons() {
  // Floating close button is a transient child of |surface_| and also part
  // of the hosting widget's focus chain. It could only be created when both
  // are present. Further, if we are being destroyed (|item_| is null), don't
  // create the control buttons.
  if (!surface_ || !GetWidget() || !item_)
    return;

  DCHECK(!control_buttons_view_);
  DCHECK(!floating_control_buttons_widget_);

  CHECK_EQ(ArcNotificationView::kViewClassName, parent()->GetClassName());
  auto* notification_view = static_cast<ArcNotificationView*>(parent());

  // Creates the control_buttons_view_, which collects all control buttons into
  // a horizontal box.
  control_buttons_view_ =
      new message_center::NotificationControlButtonsView(notification_view);
  control_buttons_view_->SetBackgroundColor(
      GetControlButtonBackgroundColor(item_->GetShownContents()));
  control_buttons_view_->ShowSettingsButton(
      item_->IsOpeningSettingsSupported());
  control_buttons_view_->ShowCloseButton(!notification_view->GetPinned());

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = surface_->GetWindow();

  floating_control_buttons_widget_.reset(new views::Widget);
  floating_control_buttons_widget_->Init(params);
  floating_control_buttons_widget_->SetContentsView(control_buttons_view_);
  floating_control_buttons_widget_->GetNativeWindow()->AddPreTargetHandler(
      mouse_enter_exit_handler_.get());

  // Put the close button into the focus chain.
  floating_control_buttons_widget_->SetFocusTraversableParent(
      GetWidget()->GetFocusTraversable());
  floating_control_buttons_widget_->SetFocusTraversableParentView(this);

  Layout();
}

void ArcNotificationContentView::SetSurface(ArcNotificationSurface* surface) {
  if (surface_ == surface)
    return;

  // Put null to |control_buttos|view_| before deleting the widget, since it may
  // be referred while deletion.
  control_buttons_view_ = nullptr;
  // Reset |floating_control_buttons_widget_| when |surface_| is changed.
  floating_control_buttons_widget_.reset();

  if (surface_) {
    DCHECK(surface_->GetWindow());
    DCHECK(surface_->GetContentWindow());
    surface_->GetContentWindow()->RemoveObserver(this);
    surface_->GetWindow()->RemovePreTargetHandler(event_forwarder_.get());

    if (surface_->GetAttachedHost() == this) {
      DCHECK_EQ(this, surface_->GetAttachedHost());
      surface_->Detach();
    }
  }

  surface_ = surface;

  if (surface_) {
    DCHECK(surface_->GetWindow());
    DCHECK(surface_->GetContentWindow());
    surface_->GetContentWindow()->AddObserver(this);
    surface_->GetWindow()->AddPreTargetHandler(event_forwarder_.get());

    if (GetWidget()) {
      // Force to detach the surface.
      if (surface_->IsAttached()) {
        // The attached host must not be this. Since if it is, this should
        // already be detached above.
        DCHECK_NE(this, surface_->GetAttachedHost());
        surface_->Detach();
      }
      AttachSurface();
    }
  }
}

void ArcNotificationContentView::UpdatePreferredSize() {
  gfx::Size preferred_size;
  if (surface_)
    preferred_size = surface_->GetSize();
  else if (item_)
    preferred_size = item_->GetSnapshot().size();

  if (preferred_size.IsEmpty())
    return;

  if (preferred_size.width() != message_center::kNotificationWidth) {
    const float scale = static_cast<float>(message_center::kNotificationWidth) /
                        preferred_size.width();
    preferred_size.SetSize(message_center::kNotificationWidth,
                           preferred_size.height() * scale);
  }

  SetPreferredSize(preferred_size);
}

void ArcNotificationContentView::UpdateControlButtonsVisibility() {
  if (!control_buttons_view_)
    return;

  // If the visibility change is ongoing, skip this method to prevent an
  // infinite loop.
  if (updating_control_buttons_visibility_)
    return;

  DCHECK(floating_control_buttons_widget_);

  const bool target_visiblity =
      IsMouseHovered() || (control_buttons_view_->IsCloseButtonFocused()) ||
      (control_buttons_view_->IsSettingsButtonFocused());

  if (target_visiblity == floating_control_buttons_widget_->IsVisible())
    return;

  // Add the guard to prevent an infinite loop. Changing visibility may generate
  // an event and it may call thie method again.
  base::AutoReset<bool> reset(&updating_control_buttons_visibility_, true);

  if (target_visiblity)
    floating_control_buttons_widget_->Show();
  else
    floating_control_buttons_widget_->Hide();
}

void ArcNotificationContentView::UpdateSnapshot() {
  // Bail if we have a |surface_| because it controls the sizes and paints UI.
  if (surface_)
    return;

  UpdatePreferredSize();
  SchedulePaint();
}

void ArcNotificationContentView::AttachSurface() {
  DCHECK(!native_view());

  if (!GetWidget())
    return;

  UpdatePreferredSize();
  surface_->Attach(this);

  // The texture for this window can be placed at subpixel position
  // with fractional scale factor. Force to align it at the pixel
  // boundary here, and when layout is updated in Layout().
  ash::wm::SnapWindowToPixelBoundary(surface_->GetWindow());

  // Creates slide helper after this view is added to its parent.
  slide_helper_.reset(new SlideHelper(this));

  // Invokes Update() in case surface is attached during a slide.
  slide_helper_->Update();

  // (Re-)create the floating buttons after |surface_| is attached to a widget.
  MaybeCreateFloatingControlButtons();
}

void ArcNotificationContentView::UpdateAccessibleName() {
  // Don't update the accessible name when we are about to be destroyed.
  if (!item_)
    return;

  accessible_name_ = item_->GetAccessibleName();
}

bool ArcNotificationContentView::IsExpanded() const {
  return item_->GetExpandState() == mojom::ArcNotificationExpandState::EXPANDED;
}

void ArcNotificationContentView::SetExpanded(bool expanded) {
  auto expand_state = item_->GetExpandState();
  if (expanded) {
    if (expand_state == mojom::ArcNotificationExpandState::COLLAPSED)
      item_->ToggleExpansion();
  } else {
    if (expand_state == mojom::ArcNotificationExpandState::EXPANDED)
      item_->ToggleExpansion();
  }
}

void ArcNotificationContentView::ViewHierarchyChanged(
    const views::View::ViewHierarchyChangedDetails& details) {
  views::Widget* widget = GetWidget();

  if (!details.is_add) {
    // Resets slide helper when this view is removed from its parent.
    slide_helper_.reset();

    // Bail if this view is no longer attached to a widget or native_view() has
    // attached to a different widget.
    if (!widget ||
        (native_view() && views::Widget::GetTopLevelWidgetForNativeView(
                              native_view()) != widget)) {
      return;
    }
  }

  views::NativeViewHost::ViewHierarchyChanged(details);

  if (!widget || !surface_ || !details.is_add)
    return;

  if (surface_->IsAttached())
    surface_->Detach();
  AttachSurface();
}

void ArcNotificationContentView::Layout() {
  base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);

  views::NativeViewHost::Layout();

  if (!surface_ || !GetWidget())
    return;

  const gfx::Rect contents_bounds = GetContentsBounds();

  // Scale notification surface if necessary.
  gfx::Transform transform;
  const gfx::Size surface_size = surface_->GetSize();
  if (!surface_size.IsEmpty()) {
    const float factor =
        static_cast<float>(message_center::kNotificationWidth) /
        surface_size.width();
    transform.Scale(factor, factor);
  }

  // Apply the transform to the surface content so that close button can
  // be positioned without the need to consider the transform.
  surface_->GetContentWindow()->SetTransform(transform);

  if (control_buttons_view_) {
    DCHECK(floating_control_buttons_widget_);
    gfx::Rect control_buttons_bounds(contents_bounds);
    int buttons_width = control_buttons_view_->GetPreferredSize().width();
    int buttons_height = control_buttons_view_->GetPreferredSize().height();

    control_buttons_bounds.set_x(control_buttons_bounds.right() -
                                 buttons_width -
                                 message_center::kControlButtonPadding);
    control_buttons_bounds.set_y(control_buttons_bounds.y() +
                                 message_center::kControlButtonPadding);
    control_buttons_bounds.set_width(buttons_width);
    control_buttons_bounds.set_height(buttons_height);
    floating_control_buttons_widget_->SetBounds(control_buttons_bounds);
  }

  UpdateControlButtonsVisibility();

  ash::wm::SnapWindowToPixelBoundary(surface_->GetWindow());
}

void ArcNotificationContentView::OnPaint(gfx::Canvas* canvas) {
  views::NativeViewHost::OnPaint(canvas);

  if (!surface_ && item_ && !item_->GetSnapshot().isNull()) {
    // Draw the snapshot if there is no surface and the snapshot is available.
    const gfx::Rect contents_bounds = GetContentsBounds();
    canvas->DrawImageInt(
        item_->GetSnapshot(), 0, 0, item_->GetSnapshot().width(),
        item_->GetSnapshot().height(), contents_bounds.x(), contents_bounds.y(),
        contents_bounds.width(), contents_bounds.height(), false);
  } else {
    // Draw a blank background otherwise. The height of the view and surface are
    // not exactly synced and user may see the blank area out of the surface.
    // This code prevetns an ugly blank area and show white color instead.
    // This should be removed after b/35786193 is done.
    canvas->DrawColor(SK_ColorWHITE);
  }
}

void ArcNotificationContentView::OnMouseEntered(const ui::MouseEvent&) {
  UpdateControlButtonsVisibility();
}

void ArcNotificationContentView::OnMouseExited(const ui::MouseEvent&) {
  UpdateControlButtonsVisibility();
}

void ArcNotificationContentView::OnFocus() {
  CHECK_EQ(ArcNotificationView::kViewClassName, parent()->GetClassName());

  NativeViewHost::OnFocus();
  static_cast<ArcNotificationView*>(parent())->OnContentFocused();

  if (surface_ && surface_->GetAXTreeId() != -1)
    Activate();
}

void ArcNotificationContentView::OnBlur() {
  if (!parent()) {
    // OnBlur may be called when this view is being removed.
    return;
  }

  CHECK_EQ(ArcNotificationView::kViewClassName, parent()->GetClassName());

  NativeViewHost::OnBlur();
  static_cast<ArcNotificationView*>(parent())->OnContentBlured();
}

void ArcNotificationContentView::Activate() {
  if (!GetWidget())
    return;

  // Make the widget active.
  if (!GetWidget()->IsActive()) {
    GetWidget()->widget_delegate()->set_can_activate(true);
    GetWidget()->Activate();
  }

  // Focus the surface window.
  surface_->FocusSurfaceWindow();
}

views::FocusTraversable* ArcNotificationContentView::GetFocusTraversable() {
  if (floating_control_buttons_widget_)
    return static_cast<views::internal::RootView*>(
        floating_control_buttons_widget_->GetRootView());
  return nullptr;
}

bool ArcNotificationContentView::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  if (item_ && action_data.action == ui::AX_ACTION_DO_DEFAULT) {
    item_->ToggleExpansion();
    return true;
  }
  return false;
}

void ArcNotificationContentView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  if (surface_ && surface_->GetAXTreeId() != -1) {
    node_data->role = ui::AX_ROLE_CLIENT;
    node_data->AddIntAttribute(ui::AX_ATTR_CHILD_TREE_ID,
                               surface_->GetAXTreeId());
  } else {
    node_data->role = ui::AX_ROLE_BUTTON;
    node_data->AddStringAttribute(
        ui::AX_ATTR_ROLE_DESCRIPTION,
        l10n_util::GetStringUTF8(
            IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
  }
  node_data->SetName(accessible_name_);
}

void ArcNotificationContentView::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  if (in_layout_)
    return;

  UpdatePreferredSize();
  Layout();
}

void ArcNotificationContentView::OnWindowDestroying(aura::Window* window) {
  SetSurface(nullptr);
}

void ArcNotificationContentView::OnItemDestroying() {
  item_->RemoveObserver(this);
  item_ = nullptr;

  // Reset |surface_| with |item_| since no one is observing the |surface_|
  // after |item_| is gone and this view should be removed soon.
  SetSurface(nullptr);
}

void ArcNotificationContentView::OnItemUpdated() {
  UpdateAccessibleName();
  UpdateSnapshot();
  if (control_buttons_view_) {
    DCHECK(floating_control_buttons_widget_);
    control_buttons_view_->SetBackgroundColor(
        GetControlButtonBackgroundColor(item_->GetShownContents()));
  }
}

void ArcNotificationContentView::OnNotificationSurfaceAdded(
    ArcNotificationSurface* surface) {
  if (surface->GetNotificationKey() != notification_key_)
    return;

  SetSurface(surface);

  // Notify AX_EVENT_CHILDREN_CHANGED to force AXNodeData of this view updated.
  // As order of OnNotificationSurfaceAdded call is not guaranteed, we are
  // dispatching the event in both ArcNotificationContentView and
  // ArcAccessibilityHelperBridge.
  NotifyAccessibilityEvent(ui::AX_EVENT_CHILDREN_CHANGED, false);
}

void ArcNotificationContentView::OnNotificationSurfaceRemoved(
    ArcNotificationSurface* surface) {
  if (surface->GetNotificationKey() != notification_key_)
    return;

  SetSurface(nullptr);
}

}  // namespace arc
