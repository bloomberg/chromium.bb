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
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/transform.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/toast_contents_view.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace arc {

namespace {

// This value should be the same as the duration of reveal animation of
// the settings view of an Android notification.
constexpr int kBackgroundColorChangeDuration = 360;

SkColor GetControlButtonBackgroundColor(
    const mojom::ArcNotificationShownContents& shown_contents) {
  if (shown_contents == mojom::ArcNotificationShownContents::CONTENTS_SHOWN)
    return message_center::kControlButtonBackgroundColor;
  else
    return SK_ColorTRANSPARENT;
}

}  // namespace

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
    if (event->type() == ui::ET_MOUSE_PRESSED)
      owner_->ActivateToast();

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
    if (owner_->surface_ && owner_->surface_->GetWindow())
      owner_->surface_->GetWindow()->layer()->SetOpacity(1.0f);
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
    if (!owner_->surface_ || !owner_->surface_->GetWindow())
      return;
    surface_copy_ = ::wm::RecreateLayers(owner_->surface_->GetWindow());
    // |surface_copy_| is at (0, 0) in owner_->layer().
    surface_copy_->root()->SetBounds(gfx::Rect(surface_copy_->root()->size()));
    owner_->layer()->Add(surface_copy_->root());
    owner_->surface_->GetWindow()->layer()->SetOpacity(0.0f);
  }

  void OnSlideEnd() {
    if (!owner_->surface_ || !owner_->surface_->GetWindow())
      return;
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
    if (!owner_->close_button_)
      return false;
    return owner_->close_button_->HasFocus();
  }

  void RequestFocusOnCloseButton() override {
    if (owner_->close_button_)
      owner_->close_button_->RequestFocus();
    owner_->UpdateControlButtonsVisibility();
  }

  void UpdateControlButtonsVisibility() override {
    owner_->UpdateControlButtonsVisibility();
  }

  void OnSlideChanged() override {
    if (owner_->slide_helper_)
      owner_->slide_helper_->Update();
  }

 private:
  ArcNotificationContentView* const owner_;

  DISALLOW_COPY_AND_ASSIGN(ContentViewDelegate);
};

ArcNotificationContentView::ControlButton::ControlButton(
    ArcNotificationContentView* owner)
    : message_center::PaddedButton(owner), owner_(owner) {
  if (owner_->item_) {
    SetBackground(views::CreateSolidBackground(
        GetControlButtonBackgroundColor(owner_->item_->GetShownContents())));
  } else {
    SetBackground(views::CreateSolidBackground(
        message_center::kControlButtonBackgroundColor));
  }
}

void ArcNotificationContentView::ControlButton::OnFocus() {
  message_center::PaddedButton::OnFocus();
  owner_->UpdateControlButtonsVisibility();
}

void ArcNotificationContentView::ControlButton::OnBlur() {
  message_center::PaddedButton::OnBlur();
  owner_->UpdateControlButtonsVisibility();
}

// static, for ArcNotificationContentView::GetClassName().
const char ArcNotificationContentView::kViewClassName[] =
    "ArcNotificationContentView";

ArcNotificationContentView::ArcNotificationContentView(
    ArcNotificationItem* item)
    : item_(item),
      notification_key_(item->GetNotificationKey()),
      event_forwarder_(new EventForwarder(this)) {
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
  return base::MakeUnique<ArcNotificationContentView::ContentViewDelegate>(
      this);
}

void ArcNotificationContentView::CreateCloseButton() {
  DCHECK(control_buttons_view_);
  DCHECK(item_);

  close_button_ = base::MakeUnique<ControlButton>(this);
  close_button_->SetImage(views::CustomButton::STATE_NORMAL,
                          message_center::GetCloseIcon());
  close_button_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_ACCESSIBLE_NAME));
  close_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_TOOLTIP));
  close_button_->set_owned_by_client();
  control_buttons_view_->AddChildView(close_button_.get());
}

void ArcNotificationContentView::CreateSettingsButton() {
  DCHECK(control_buttons_view_);
  DCHECK(item_);

  settings_button_ = new ControlButton(this);
  settings_button_->SetImage(views::CustomButton::STATE_NORMAL,
                             message_center::GetSettingsIcon());
  settings_button_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
  settings_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
  control_buttons_view_->AddChildView(settings_button_);
}

void ArcNotificationContentView::MaybeCreateFloatingControlButtons() {
  // Floating close button is a transient child of |surface_| and also part
  // of the hosting widget's focus chain. It could only be created when both
  // are present. Further, if we are being destroyed (|item_| is null), don't
  // create the control buttons.
  if (!surface_ || !GetWidget() || !item_)
    return;

  // Creates the control_buttons_view_, which collects all control buttons into
  // a horizontal box.
  control_buttons_view_ = new views::View();
  control_buttons_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal));

  if (item_->IsOpeningSettingsSupported())
    CreateSettingsButton();
  if (!item_->GetPinned())
    CreateCloseButton();

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = surface_->GetWindow();

  floating_control_buttons_widget_.reset(new views::Widget);
  floating_control_buttons_widget_->Init(params);
  floating_control_buttons_widget_->SetContentsView(control_buttons_view_);

  // Put the close button into the focus chain.
  floating_control_buttons_widget_->SetFocusTraversableParent(
      GetWidget()->GetFocusTraversable());
  floating_control_buttons_widget_->SetFocusTraversableParentView(this);

  Layout();
}

void ArcNotificationContentView::SetSurface(ArcNotificationSurface* surface) {
  if (surface_ == surface)
    return;

  // Reset |floating_control_buttons_widget_| when |surface_| is changed.
  floating_control_buttons_widget_.reset();
  control_buttons_view_ = nullptr;
  settings_button_ = nullptr;
  close_button_.reset();

  if (surface_ && surface_->GetWindow()) {
    surface_->GetWindow()->RemoveObserver(this);
    surface_->GetWindow()->RemovePreTargetHandler(event_forwarder_.get());

    if (GetWidget() && surface_->IsAttached())
      surface_->Detach();
  }

  surface_ = surface;

  if (surface_ && surface_->GetWindow()) {
    surface_->GetWindow()->AddObserver(this);
    surface_->GetWindow()->AddPreTargetHandler(event_forwarder_.get());

    if (GetWidget())
      AttachSurface();
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
  if (!floating_control_buttons_widget_)
    return;

  const bool target_visiblity =
      IsMouseHovered() || (close_button_ && close_button_->HasFocus()) ||
      (settings_button_ && settings_button_->HasFocus());
  if (target_visiblity == floating_control_buttons_widget_->IsVisible())
    return;

  if (target_visiblity)
    floating_control_buttons_widget_->Show();
  else
    floating_control_buttons_widget_->Hide();
}

void ArcNotificationContentView::UpdatePinnedState() {
  if (!item_)
    return;

  // Surface is not attached yet.
  if (!control_buttons_view_)
    return;

  if (item_->GetPinned() && close_button_) {
    control_buttons_view_->RemoveChildView(close_button_.get());
    close_button_.reset();
    Layout();
  } else if (!item_->GetPinned() && !close_button_) {
    CreateCloseButton();
    Layout();
  }
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

void ArcNotificationContentView::StartControlButtonsColorAnimation() {
  if (control_button_color_animation_)
    control_button_color_animation_->End();
  control_button_color_animation_.reset(new gfx::LinearAnimation(this));
  control_button_color_animation_->SetDuration(kBackgroundColorChangeDuration);
  control_button_color_animation_->Start();
}

bool ArcNotificationContentView::ShouldUpdateControlButtonsColor() const {
  // Don't update the control button color when we are about to be destroyed.
  if (!item_)
    return false;

  if (settings_button_ &&
      settings_button_->background()->get_color() !=
          GetControlButtonBackgroundColor(item_->GetShownContents()))
    return true;
  if (close_button_ &&
      close_button_->background()->get_color() !=
          GetControlButtonBackgroundColor(item_->GetShownContents()))
    return true;
  return false;
}

void ArcNotificationContentView::UpdateAccessibleName() {
  // Don't update the accessible name when we are about to be destroyed.
  if (!item_)
    return;

  accessible_name_ = item_->GetAccessibleName();
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
  const gfx::Size contents_size = contents_bounds.size();
  if (!surface_size.IsEmpty() && !contents_size.IsEmpty()) {
    transform.Scale(
        static_cast<float>(contents_size.width()) / surface_size.width(),
        static_cast<float>(contents_size.height()) / surface_size.height());
  }

  // Apply the transform to the surface content so that close button can
  // be positioned without the need to consider the transform.
  surface_->GetContentWindow()->SetTransform(transform);

  if (!floating_control_buttons_widget_)
    return;

  gfx::Rect control_buttons_bounds(contents_bounds);
  int buttons_width = 0;
  int buttons_height = 0;
  if (close_button_) {
    buttons_width += close_button_->GetPreferredSize().width();
    buttons_height = close_button_->GetPreferredSize().height();
  }
  if (settings_button_) {
    buttons_width += settings_button_->GetPreferredSize().width();
    buttons_height = settings_button_->GetPreferredSize().height();
  }
  control_buttons_bounds.set_x(control_buttons_bounds.right() - buttons_width -
                               message_center::kControlButtonPadding);
  control_buttons_bounds.set_y(control_buttons_bounds.y() +
                               message_center::kControlButtonPadding);
  control_buttons_bounds.set_width(buttons_width);
  control_buttons_bounds.set_height(buttons_height);
  floating_control_buttons_widget_->SetBounds(control_buttons_bounds);

  UpdateControlButtonsVisibility();

  ash::wm::SnapWindowToPixelBoundary(surface_->GetWindow());
}

void ArcNotificationContentView::OnPaint(gfx::Canvas* canvas) {
  views::NativeViewHost::OnPaint(canvas);

  // Bail if there is a |surface_| or no item or no snapshot image.
  if (surface_ || !item_ || item_->GetSnapshot().isNull())
    return;
  const gfx::Rect contents_bounds = GetContentsBounds();
  canvas->DrawImageInt(item_->GetSnapshot(), 0, 0, item_->GetSnapshot().width(),
                       item_->GetSnapshot().height(), contents_bounds.x(),
                       contents_bounds.y(), contents_bounds.width(),
                       contents_bounds.height(), false);
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

void ArcNotificationContentView::ActivateToast() {
  if (message_center::ToastContentsView::kViewClassName ==
      parent()->parent()->GetClassName()) {
    static_cast<message_center::ToastContentsView*>(parent()->parent())
        ->ActivateToast();
  }
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
  node_data->role = ui::AX_ROLE_BUTTON;
  node_data->AddStringAttribute(
      ui::AX_ATTR_ROLE_DESCRIPTION,
      l10n_util::GetStringUTF8(
          IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
  node_data->SetName(accessible_name_);
}

void ArcNotificationContentView::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  if (item_ && !item_->GetPinned() && sender == close_button_.get()) {
    CHECK_EQ(ArcNotificationView::kViewClassName, parent()->GetClassName());
    static_cast<ArcNotificationView*>(parent())->OnCloseButtonPressed();
  }
  if (item_ && settings_button_ && sender == settings_button_) {
    item_->OpenSettings();
  }
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
  UpdatePinnedState();
  UpdateSnapshot();
  if (ShouldUpdateControlButtonsColor())
    StartControlButtonsColorAnimation();
}

void ArcNotificationContentView::OnNotificationSurfaceAdded(
    ArcNotificationSurface* surface) {
  if (surface->GetNotificationKey() != notification_key_)
    return;

  SetSurface(surface);
}

void ArcNotificationContentView::OnNotificationSurfaceRemoved(
    ArcNotificationSurface* surface) {
  if (surface->GetNotificationKey() != notification_key_)
    return;

  SetSurface(nullptr);
}

void ArcNotificationContentView::AnimationEnded(
    const gfx::Animation* animation) {
  DCHECK_EQ(animation, control_button_color_animation_.get());
  control_button_color_animation_.reset();
}

void ArcNotificationContentView::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(animation, control_button_color_animation_.get());

  if (item_) {
    const SkColor target =
        GetControlButtonBackgroundColor(item_->GetShownContents());
    const SkColor start =
        target == message_center::kControlButtonBackgroundColor
            ? SK_ColorTRANSPARENT
            : message_center::kControlButtonBackgroundColor;
    const SkColor current_color = gfx::Tween::ColorValueBetween(
        animation->GetCurrentValue(), start, target);
    if (settings_button_) {
      settings_button_->SetBackground(
          views::CreateSolidBackground(current_color));
      settings_button_->SchedulePaint();
    }
    if (close_button_) {
      close_button_->SetBackground(views::CreateSolidBackground(current_color));
      close_button_->SchedulePaint();
    }
  }
}

}  // namespace arc
