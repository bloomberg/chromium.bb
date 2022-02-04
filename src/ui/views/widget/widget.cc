// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget.h"

#include <set>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/default_style.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/l10n/l10n_font_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_provider_manager.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/event_monitor.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_manager_factory.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/image_model_utils.h"
#include "ui/views/views_delegate.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/any_widget_observer_singleton.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_deletion_observer.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_removals_observer.h"
#include "ui/views/window/custom_frame_view.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/views/linux_ui/linux_ui.h"
#endif

namespace views {

namespace {

// If |view| has a layer the layer is added to |layers|. Else this recurses
// through the children. This is used to build a list of the layers created by
// views that are direct children of the Widgets layer.
void BuildViewsWithLayers(View* view, View::Views* views) {
  if (view->layer()) {
    views->push_back(view);
  } else {
    for (View* child : view->children())
      BuildViewsWithLayers(child, views);
  }
}

// Create a native widget implementation.
// First, use the supplied one if non-NULL.
// Finally, make a default one.
NativeWidget* CreateNativeWidget(const Widget::InitParams& params,
                                 internal::NativeWidgetDelegate* delegate) {
  if (params.native_widget)
    return params.native_widget;

  const auto& factory = ViewsDelegate::GetInstance()->native_widget_factory();
  if (!factory.is_null()) {
    NativeWidget* native_widget = factory.Run(params, delegate);
    if (native_widget)
      return native_widget;
  }
  return internal::NativeWidgetPrivate::CreateNativeWidget(delegate);
}

void NotifyCaretBoundsChanged(ui::InputMethod* input_method) {
  if (!input_method)
    return;
  ui::TextInputClient* client = input_method->GetTextInputClient();
  if (client)
    input_method->OnCaretBoundsChanged(client);
}

}  // namespace

// static
Widget::DisableActivationChangeHandlingType
    Widget::g_disable_activation_change_handling_ =
        Widget::DisableActivationChangeHandlingType::kNone;

// A default implementation of WidgetDelegate, used by Widget when no
// WidgetDelegate is supplied.
class DefaultWidgetDelegate : public WidgetDelegate {
 public:
  DefaultWidgetDelegate() {
    // In most situations where a Widget is used without a delegate the Widget
    // is used as a container, so that we want focus to advance to the top-level
    // widget. A good example of this is the find bar.
    SetOwnedByWidget(true);
    SetFocusTraversesOut(true);
  }

  DefaultWidgetDelegate(const DefaultWidgetDelegate&) = delete;
  DefaultWidgetDelegate& operator=(const DefaultWidgetDelegate&) = delete;

  ~DefaultWidgetDelegate() override = default;
};

////////////////////////////////////////////////////////////////////////////////
// Widget, PaintAsActiveLock:

Widget::PaintAsActiveLock::PaintAsActiveLock() = default;
Widget::PaintAsActiveLock::~PaintAsActiveLock() = default;

////////////////////////////////////////////////////////////////////////////////
// Widget, PaintAsActiveLockImpl:

class Widget::PaintAsActiveLockImpl : public Widget::PaintAsActiveLock {
 public:
  explicit PaintAsActiveLockImpl(base::WeakPtr<Widget>&& widget)
      : widget_(widget) {}

  ~PaintAsActiveLockImpl() override {
    Widget* const widget = widget_.get();
    if (widget)
      widget->UnlockPaintAsActive();
  }

 private:
  base::WeakPtr<Widget> widget_;
};

////////////////////////////////////////////////////////////////////////////////
// Widget, InitParams:

Widget::InitParams::InitParams() = default;

Widget::InitParams::InitParams(Type type) : type(type) {}

Widget::InitParams::InitParams(InitParams&& other) = default;

Widget::InitParams::~InitParams() = default;

bool Widget::InitParams::CanActivate() const {
  if (activatable != InitParams::Activatable::kDefault)
    return activatable == InitParams::Activatable::kYes;
  return type != InitParams::TYPE_CONTROL && type != InitParams::TYPE_POPUP &&
         type != InitParams::TYPE_MENU && type != InitParams::TYPE_TOOLTIP &&
         type != InitParams::TYPE_DRAG;
}

ui::ZOrderLevel Widget::InitParams::EffectiveZOrderLevel() const {
  if (z_order.has_value())
    return z_order.value();

  switch (type) {
    case TYPE_MENU:
      return ui::ZOrderLevel::kFloatingWindow;
    case TYPE_DRAG:
      return ui::ZOrderLevel::kFloatingUIElement;
    default:
      return ui::ZOrderLevel::kNormal;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

Widget::Widget() = default;

Widget::Widget(InitParams params) {
  Init(std::move(params));
}

Widget::~Widget() {
  if (widget_delegate_)
    widget_delegate_->WidgetDestroying();
  if (ownership_ == InitParams::WIDGET_OWNS_NATIVE_WIDGET) {
    delete native_widget_;
    DCHECK(native_widget_destroyed_);
  } else {
    // TODO(crbug.com/937381): Revert to DCHECK once we figure out the reason.
    CHECK(native_widget_destroyed_)
        << "Destroying a widget with a live native widget. "
        << "Widget probably should use WIDGET_OWNS_NATIVE_WIDGET ownership.";
  }
  // Destroy RootView after the native widget, so in case the WidgetDelegate is
  // a View in the RootView hierarchy it gets destroyed as a WidgetDelegate
  // first.
  // This makes destruction order for WidgetDelegate consistent between
  // different Widget/NativeWidget ownership models (WidgetDelegate is always
  // deleted before here, which may have removed it as a View from the
  // View hierarchy).
  DestroyRootView();
}

// static
Widget* Widget::CreateWindowWithParent(WidgetDelegate* delegate,
                                       gfx::NativeView parent,
                                       const gfx::Rect& bounds) {
  Widget::InitParams params;
  params.delegate = delegate;
  params.parent = parent;
  params.bounds = bounds;
  return new Widget(std::move(params));
}

Widget* Widget::CreateWindowWithParent(std::unique_ptr<WidgetDelegate> delegate,
                                       gfx::NativeView parent,
                                       const gfx::Rect& bounds) {
  DCHECK(delegate->owned_by_widget());
  return CreateWindowWithParent(delegate.release(), parent, bounds);
}

// static
Widget* Widget::CreateWindowWithContext(WidgetDelegate* delegate,
                                        gfx::NativeWindow context,
                                        const gfx::Rect& bounds) {
  Widget::InitParams params;
  params.delegate = delegate;
  params.context = context;
  params.bounds = bounds;
  return new Widget(std::move(params));
}

// static
Widget* Widget::CreateWindowWithContext(
    std::unique_ptr<WidgetDelegate> delegate,
    gfx::NativeWindow context,
    const gfx::Rect& bounds) {
  DCHECK(delegate->owned_by_widget());
  return CreateWindowWithContext(delegate.release(), context, bounds);
}

// static
Widget* Widget::GetWidgetForNativeView(gfx::NativeView native_view) {
  internal::NativeWidgetPrivate* native_widget =
      internal::NativeWidgetPrivate::GetNativeWidgetForNativeView(native_view);
  return native_widget ? native_widget->GetWidget() : nullptr;
}

// static
Widget* Widget::GetWidgetForNativeWindow(gfx::NativeWindow native_window) {
  internal::NativeWidgetPrivate* native_widget =
      internal::NativeWidgetPrivate::GetNativeWidgetForNativeWindow(
          native_window);
  return native_widget ? native_widget->GetWidget() : nullptr;
}

// static
Widget* Widget::GetTopLevelWidgetForNativeView(gfx::NativeView native_view) {
  internal::NativeWidgetPrivate* native_widget =
      internal::NativeWidgetPrivate::GetTopLevelNativeWidget(native_view);
  return native_widget ? native_widget->GetWidget() : nullptr;
}

// static
void Widget::GetAllChildWidgets(gfx::NativeView native_view,
                                Widgets* children) {
  internal::NativeWidgetPrivate::GetAllChildWidgets(native_view, children);
}

// static
void Widget::GetAllOwnedWidgets(gfx::NativeView native_view, Widgets* owned) {
  internal::NativeWidgetPrivate::GetAllOwnedWidgets(native_view, owned);
}

// static
void Widget::ReparentNativeView(gfx::NativeView native_view,
                                gfx::NativeView new_parent) {
  internal::NativeWidgetPrivate::ReparentNativeView(native_view, new_parent);
  Widget* child_widget = GetWidgetForNativeView(native_view);
  Widget* parent_widget =
      new_parent ? GetWidgetForNativeView(new_parent) : nullptr;
  if (child_widget)
    child_widget->SetParent(parent_widget);
}

// static
int Widget::GetLocalizedContentsWidth(int col_resource_id) {
  return ui::GetLocalizedContentsWidthForFontList(
      col_resource_id,
      ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
          ui::kMessageFontSizeDelta));
}

// static
int Widget::GetLocalizedContentsHeight(int row_resource_id) {
  return ui::GetLocalizedContentsHeightForFontList(
      row_resource_id,
      ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
          ui::kMessageFontSizeDelta));
}

// static
gfx::Size Widget::GetLocalizedContentsSize(int col_resource_id,
                                           int row_resource_id) {
  return gfx::Size(GetLocalizedContentsWidth(col_resource_id),
                   GetLocalizedContentsHeight(row_resource_id));
}

// static
bool Widget::RequiresNonClientView(InitParams::Type type) {
  return type == InitParams::TYPE_WINDOW || type == InitParams::TYPE_BUBBLE;
}

void Widget::Init(InitParams params) {
  TRACE_EVENT0("views", "Widget::Init");

  if (params.name.empty() && params.delegate) {
    params.name = params.delegate->internal_name();
    // If an internal name was not provided the class name of the contents view
    // is a reasonable default.
    if (params.name.empty() && params.delegate->GetContentsView())
      params.name = params.delegate->GetContentsView()->GetClassName();
  }

  parent_ = params.parent ? GetWidgetForNativeView(params.parent) : nullptr;

  // Subscripbe to parent's paint-as-active change.
  if (parent_) {
    parent_paint_as_active_subscription_ =
        parent_->RegisterPaintAsActiveChangedCallback(
            base::BindRepeating(&Widget::OnParentShouldPaintAsActiveChanged,
                                base::Unretained(this)));
  }

  params.child |= (params.type == InitParams::TYPE_CONTROL);
  is_top_level_ = !params.child;

  if (params.opacity == views::Widget::InitParams::WindowOpacity::kInferred &&
      params.type != views::Widget::InitParams::TYPE_WINDOW) {
    params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  }

  {
    // ViewsDelegate::OnBeforeWidgetInit() may change `params.delegate` either
    // by setting it to null or assigning a different value to it, so handle
    // both cases.
    auto default_widget_delegate = std::make_unique<DefaultWidgetDelegate>();
    widget_delegate_ =
        params.delegate ? params.delegate : default_widget_delegate.get();

    ViewsDelegate::GetInstance()->OnBeforeWidgetInit(&params, this);

    widget_delegate_ =
        params.delegate ? params.delegate : default_widget_delegate.release();
  }
  DCHECK(widget_delegate_);

  if (params.opacity == views::Widget::InitParams::WindowOpacity::kInferred)
    params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;

  bool can_activate = params.CanActivate();
  params.activatable = can_activate ? InitParams::Activatable::kYes
                                    : InitParams::Activatable::kNo;

  widget_delegate_->SetCanActivate(can_activate);

  // Henceforth, ensure the delegate outlives the Widget.
  widget_delegate_->can_delete_this_ = false;

  if (params.delegate)
    params.delegate->WidgetInitializing(this);

  ownership_ = params.ownership;
  native_widget_ = CreateNativeWidget(params, this)->AsNativeWidgetPrivate();
  root_view_.reset(CreateRootView());

  // Copy the elements of params that will be used after it is moved.
  const InitParams::Type type = params.type;
  const gfx::Rect bounds = params.bounds;
  const ui::WindowShowState show_state = params.show_state;
  WidgetDelegate* delegate = params.delegate;

  native_widget_->InitNativeWidget(std::move(params));
  if (type == InitParams::TYPE_MENU)
    is_mouse_button_pressed_ = native_widget_->IsMouseButtonDown();
  if (RequiresNonClientView(type)) {
    non_client_view_ =
        new NonClientView(widget_delegate_->CreateClientView(this));
    non_client_view_->SetFrameView(CreateNonClientFrameView());
    non_client_view_->SetOverlayView(widget_delegate_->CreateOverlayView());

    // Bypass the Layout() that happens in Widget::SetContentsView(). Layout()
    // will occur after setting the initial bounds below. The RootView's size is
    // not valid until that happens.
    root_view_->SetContentsView(non_client_view_);

    // Initialize the window's icon and title before setting the window's
    // initial bounds; the frame view's preferred height may depend on the
    // presence of an icon or a title.
    UpdateWindowIcon();
    UpdateWindowTitle();
    non_client_view_->ResetWindowControls();
    SetInitialBounds(bounds);

    // Perform the initial layout. This handles the case where the size might
    // not actually change when setting the initial bounds. If it did, child
    // views won't have a dirty Layout state, so won't do any work.
    root_view_->Layout();

    if (show_state == ui::SHOW_STATE_MAXIMIZED) {
      Maximize();
    } else if (show_state == ui::SHOW_STATE_MINIMIZED) {
      Minimize();
      saved_show_state_ = ui::SHOW_STATE_MINIMIZED;
    }
  } else if (delegate) {
    SetContentsView(delegate->TransferOwnershipOfContentsView());
    SetInitialBoundsForFramelessWindow(bounds);
  }

  native_theme_observation_.Observe(GetNativeTheme());
  native_widget_initialized_ = true;
  native_widget_->OnWidgetInitDone();

  if (delegate)
    delegate->WidgetInitialized();

  internal::AnyWidgetObserverSingleton::GetInstance()->OnAnyWidgetInitialized(
      this);
}

void Widget::ShowEmojiPanel() {
  native_widget_->ShowEmojiPanel();
}

// Unconverted methods (see header) --------------------------------------------

gfx::NativeView Widget::GetNativeView() const {
  if (native_widget_destroyed_)
    return gfx::kNullNativeView;
  return native_widget_->GetNativeView();
}

gfx::NativeWindow Widget::GetNativeWindow() const {
  if (native_widget_destroyed_)
    return gfx::kNullNativeWindow;
  return native_widget_->GetNativeWindow();
}

void Widget::AddObserver(WidgetObserver* observer) {
  // Make sure that there is no nullptr in observer list. crbug.com/471649.
  CHECK(observer);
  observers_.AddObserver(observer);
}

void Widget::RemoveObserver(WidgetObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool Widget::HasObserver(const WidgetObserver* observer) const {
  return observers_.HasObserver(observer);
}

void Widget::AddRemovalsObserver(WidgetRemovalsObserver* observer) {
  removals_observers_.AddObserver(observer);
}

void Widget::RemoveRemovalsObserver(WidgetRemovalsObserver* observer) {
  removals_observers_.RemoveObserver(observer);
}

bool Widget::HasRemovalsObserver(const WidgetRemovalsObserver* observer) const {
  return removals_observers_.HasObserver(observer);
}

bool Widget::GetAccelerator(int cmd_id, ui::Accelerator* accelerator) const {
  return false;
}

void Widget::ViewHierarchyChanged(const ViewHierarchyChangedDetails& details) {
  if (!details.is_add) {
    if (details.child == dragged_view_)
      dragged_view_ = nullptr;
    FocusManager* focus_manager = GetFocusManager();
    if (focus_manager)
      focus_manager->ViewRemoved(details.child);
    if (!native_widget_destroyed_)
      native_widget_->ViewRemoved(details.child);
  }
}

void Widget::NotifyNativeViewHierarchyWillChange() {
  // During tear-down the top-level focus manager becomes unavailable to
  // GTK tabbed panes and their children, so normal deregistration via
  // |FocusManager::ViewRemoved()| calls are fouled.  We clear focus here
  // to avoid these redundant steps and to avoid accessing deleted views
  // that may have been in focus.
  ClearFocusFromWidget();
  native_widget_->OnNativeViewHierarchyWillChange();
}

void Widget::NotifyNativeViewHierarchyChanged() {
  native_widget_->OnNativeViewHierarchyChanged();
  root_view_->NotifyNativeViewHierarchyChanged();
}

void Widget::NotifyWillRemoveView(View* view) {
  for (WidgetRemovalsObserver& observer : removals_observers_)
    observer.OnWillRemoveView(this, view);
}

// Converted methods (see header) ----------------------------------------------

Widget* Widget::GetTopLevelWidget() {
  return const_cast<Widget*>(
      static_cast<const Widget*>(this)->GetTopLevelWidget());
}

const Widget* Widget::GetTopLevelWidget() const {
  // GetTopLevelNativeWidget doesn't work during destruction because
  // property is gone after gobject gets deleted. Short circuit here
  // for toplevel so that InputMethod can remove itself from
  // focus manager.
  if (is_top_level())
    return this;
  return native_widget_destroyed_ ? nullptr
                                  : native_widget_->GetTopLevelWidget();
}

Widget* Widget::GetPrimaryWindowWidget() {
  return GetTopLevelWidget();
}

const Widget* Widget::GetPrimaryWindowWidget() const {
  return const_cast<Widget*>(this)->GetPrimaryWindowWidget();
}

void Widget::SetContentsView(View* view) {
  // Do not SetContentsView() again if it is already set to the same view.
  if (view == GetContentsView())
    return;

  // |non_client_view_| can only be non-null here if RequiresNonClientView() was
  // true when the widget was initialized. Creating widgets with non-client
  // views and then setting the contents view can cause subtle problems on
  // Windows, where the native widget thinks there is still a
  // |non_client_view_|. If you get this error, either use a different type when
  // initializing the widget, or don't call SetContentsView().
  DCHECK(!non_client_view_);

  root_view_->SetContentsView(view);

  // Force a layout now, since the attached hierarchy won't be ready for the
  // containing window's bounds. Note that we call Layout directly rather than
  // calling the widget's size changed handler, since the RootView's bounds may
  // not have changed, which will cause the Layout not to be done otherwise.
  root_view_->Layout();
}

View* Widget::GetContentsView() {
  return root_view_->GetContentsView();
}

gfx::Rect Widget::GetWindowBoundsInScreen() const {
  return native_widget_->GetWindowBoundsInScreen();
}

gfx::Rect Widget::GetClientAreaBoundsInScreen() const {
  return native_widget_->GetClientAreaBoundsInScreen();
}

gfx::Rect Widget::GetRestoredBounds() const {
  return native_widget_->GetRestoredBounds();
}

std::string Widget::GetWorkspace() const {
  return native_widget_->GetWorkspace();
}

void Widget::SetBounds(const gfx::Rect& bounds) {
  native_widget_->SetBounds(bounds);
}

void Widget::SetSize(const gfx::Size& size) {
  native_widget_->SetSize(size);
}

void Widget::CenterWindow(const gfx::Size& size) {
  native_widget_->CenterWindow(size);
}

void Widget::SetBoundsConstrained(const gfx::Rect& bounds) {
  native_widget_->SetBoundsConstrained(bounds);
}

void Widget::SetVisibilityChangedAnimationsEnabled(bool value) {
  native_widget_->SetVisibilityChangedAnimationsEnabled(value);
}

void Widget::SetVisibilityAnimationDuration(const base::TimeDelta& duration) {
  native_widget_->SetVisibilityAnimationDuration(duration);
}

void Widget::SetVisibilityAnimationTransition(VisibilityTransition transition) {
  native_widget_->SetVisibilityAnimationTransition(transition);
}

bool Widget::IsMoveLoopSupported() const {
  return native_widget_->IsMoveLoopSupported();
}

Widget::MoveLoopResult Widget::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    MoveLoopSource source,
    MoveLoopEscapeBehavior escape_behavior) {
  return native_widget_->RunMoveLoop(drag_offset, source, escape_behavior);
}

void Widget::EndMoveLoop() {
  native_widget_->EndMoveLoop();
}

void Widget::StackAboveWidget(Widget* widget) {
  native_widget_->StackAbove(widget->GetNativeView());
}

void Widget::StackAbove(gfx::NativeView native_view) {
  native_widget_->StackAbove(native_view);
}

void Widget::StackAtTop() {
  native_widget_->StackAtTop();
}

void Widget::SetShape(std::unique_ptr<ShapeRects> shape) {
  native_widget_->SetShape(std::move(shape));
}

void Widget::CloseWithReason(ClosedReason closed_reason) {
  if (widget_closed_) {
    // It appears we can hit this code path if you close a modal dialog then
    // close the last browser before the destructor is hit, which triggers
    // invoking Close again.
    return;
  }
  if (block_close_) {
    return;
  }
  if (non_client_view_ && non_client_view_->OnWindowCloseRequested() ==
                              CloseRequestResult::kCannotClose)
    return;

  // This is the last chance to cancel closing.
  if (widget_delegate_ && !widget_delegate_->OnCloseRequested(closed_reason))
    return;

  // Cancel widget close on focus lost. This is used in UI Devtools to lock
  // bubbles and in some tests where we want to ignore spurious deactivation.
  if (closed_reason == ClosedReason::kLostFocus &&
      (g_disable_activation_change_handling_ ==
           DisableActivationChangeHandlingType::kIgnore ||
       g_disable_activation_change_handling_ ==
           DisableActivationChangeHandlingType::kIgnoreDeactivationOnly))
    return;

  // The actions below can cause this function to be called again, so mark
  // |this| as closed early. See crbug.com/714334
  widget_closed_ = true;
  closed_reason_ = closed_reason;
  SaveWindowPlacement();
  ClearFocusFromWidget();

  for (WidgetObserver& observer : observers_)
    observer.OnWidgetClosing(this);

  internal::AnyWidgetObserverSingleton::GetInstance()->OnAnyWidgetClosing(this);

  if (widget_delegate_)
    widget_delegate_->WindowWillClose();

  native_widget_->Close();
}

void Widget::Close() {
  CloseWithReason(ClosedReason::kUnspecified);
}

void Widget::CloseNow() {
  for (WidgetObserver& observer : observers_)
    observer.OnWidgetClosing(this);
  internal::AnyWidgetObserverSingleton::GetInstance()->OnAnyWidgetClosing(this);
  native_widget_->CloseNow();
}

bool Widget::IsClosed() const {
  return widget_closed_;
}

void Widget::Show() {
  const ui::Layer* layer = GetLayer();
  TRACE_EVENT1("views", "Widget::Show", "layer",
               layer ? layer->name() : "none");
  ui::WindowShowState preferred_show_state =
      CanActivate() ? ui::SHOW_STATE_NORMAL : ui::SHOW_STATE_INACTIVE;
  if (non_client_view_) {
    // While initializing, the kiosk mode will go to full screen before the
    // widget gets shown. In that case we stay in full screen mode, regardless
    // of the |saved_show_state_| member.
    if (saved_show_state_ == ui::SHOW_STATE_MAXIMIZED &&
        !initial_restored_bounds_.IsEmpty() && !IsFullscreen()) {
      native_widget_->Show(ui::SHOW_STATE_MAXIMIZED, initial_restored_bounds_);
    } else {
      native_widget_->Show(
          IsFullscreen() ? ui::SHOW_STATE_FULLSCREEN : saved_show_state_,
          gfx::Rect());
    }
    // |saved_show_state_| only applies the first time the window is shown.
    // If we don't reset the value the window may be shown maximized every time
    // it is subsequently shown after being hidden.
    saved_show_state_ = preferred_show_state;
  } else {
    native_widget_->Show(preferred_show_state, gfx::Rect());
  }
  internal::AnyWidgetObserverSingleton::GetInstance()->OnAnyWidgetShown(this);
}

void Widget::Hide() {
  native_widget_->Hide();
  internal::AnyWidgetObserverSingleton::GetInstance()->OnAnyWidgetHidden(this);
}

void Widget::ShowInactive() {
  // If this gets called with saved_show_state_ == ui::SHOW_STATE_MAXIMIZED,
  // call SetBounds()with the restored bounds to set the correct size. This
  // normally should not happen, but if it does we should avoid showing unsized
  // windows.
  if (saved_show_state_ == ui::SHOW_STATE_MAXIMIZED &&
      !initial_restored_bounds_.IsEmpty()) {
    SetBounds(initial_restored_bounds_);
    saved_show_state_ = ui::SHOW_STATE_NORMAL;
  }
  native_widget_->Show(ui::SHOW_STATE_INACTIVE, gfx::Rect());
  internal::AnyWidgetObserverSingleton::GetInstance()->OnAnyWidgetShown(this);
}

void Widget::Activate() {
  if (CanActivate())
    native_widget_->Activate();
}

void Widget::Deactivate() {
  native_widget_->Deactivate();
}

bool Widget::IsActive() const {
  return native_widget_->IsActive();
}

void Widget::SetZOrderLevel(ui::ZOrderLevel order) {
  native_widget_->SetZOrderLevel(order);
}

ui::ZOrderLevel Widget::GetZOrderLevel() const {
  return native_widget_->GetZOrderLevel();
}

void Widget::SetVisibleOnAllWorkspaces(bool always_visible) {
  native_widget_->SetVisibleOnAllWorkspaces(always_visible);
}

bool Widget::IsVisibleOnAllWorkspaces() const {
  return native_widget_->IsVisibleOnAllWorkspaces();
}

void Widget::Maximize() {
  native_widget_->Maximize();
}

void Widget::Minimize() {
  native_widget_->Minimize();
}

void Widget::Restore() {
  native_widget_->Restore();
}

bool Widget::IsMaximized() const {
  return native_widget_->IsMaximized();
}

bool Widget::IsMinimized() const {
  return native_widget_->IsMinimized();
}

void Widget::SetFullscreen(bool fullscreen, base::TimeDelta delay) {
  if (IsFullscreen() == fullscreen)
    return;

  auto weak_ptr = GetWeakPtr();
  native_widget_->SetFullscreen(fullscreen, delay);
  if (!weak_ptr)
    return;

  if (non_client_view_)
    non_client_view_->InvalidateLayout();
}

bool Widget::IsFullscreen() const {
  return native_widget_->IsFullscreen();
}

void Widget::SetCanAppearInExistingFullscreenSpaces(
    bool can_appear_in_existing_fullscreen_spaces) {
  native_widget_->SetCanAppearInExistingFullscreenSpaces(
      can_appear_in_existing_fullscreen_spaces);
}

void Widget::SetOpacity(float opacity) {
  DCHECK(opacity >= 0.0f);
  DCHECK(opacity <= 1.0f);
  native_widget_->SetOpacity(opacity);
}

void Widget::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  native_widget_->SetAspectRatio(aspect_ratio);
}

void Widget::FlashFrame(bool flash) {
  native_widget_->FlashFrame(flash);
}

View* Widget::GetRootView() {
  return root_view_.get();
}

const View* Widget::GetRootView() const {
  return root_view_.get();
}

bool Widget::IsVisible() const {
  return native_widget_->IsVisible();
}

const ui::ThemeProvider* Widget::GetThemeProvider() const {
  const Widget* root_widget = GetTopLevelWidget();
  return (root_widget && root_widget != this) ? root_widget->GetThemeProvider()
                                              : nullptr;
}

ui::ColorProviderManager::InitializerSupplier* Widget::GetCustomTheme() const {
  return nullptr;
}

FocusManager* Widget::GetFocusManager() {
  Widget* toplevel_widget = GetTopLevelWidget();
  return toplevel_widget ? toplevel_widget->focus_manager_.get() : nullptr;
}

const FocusManager* Widget::GetFocusManager() const {
  const Widget* toplevel_widget = GetTopLevelWidget();
  return toplevel_widget ? toplevel_widget->focus_manager_.get() : nullptr;
}

ui::InputMethod* Widget::GetInputMethod() {
  if (is_top_level()) {
    // Only creates the shared the input method instance on top level widget.
    return native_widget_private()->GetInputMethod();
  } else {
    Widget* toplevel = GetTopLevelWidget();
    // If GetTopLevelWidget() returns itself which is not toplevel,
    // the widget is detached from toplevel widget.
    // TODO(oshima): Fix GetTopLevelWidget() to return NULL
    // if there is no toplevel. We probably need to add GetTopMostWidget()
    // to replace some use cases.
    return (toplevel && toplevel != this) ? toplevel->GetInputMethod()
                                          : nullptr;
  }
}

void Widget::RunShellDrag(View* view,
                          std::unique_ptr<ui::OSExchangeData> data,
                          const gfx::Point& location,
                          int operation,
                          ui::mojom::DragEventSource source) {
  dragged_view_ = view;
  OnDragWillStart();

  for (WidgetObserver& observer : observers_)
    observer.OnWidgetDragWillStart(this);

  WidgetDeletionObserver widget_deletion_observer(this);
  native_widget_->RunShellDrag(view, std::move(data), location, operation,
                               source);

  // The widget may be destroyed during the drag operation.
  if (!widget_deletion_observer.IsWidgetAlive())
    return;

  // If the view is removed during the drag operation, dragged_view_ is set to
  // NULL.
  if (view && dragged_view_ == view) {
    dragged_view_ = nullptr;
    view->OnDragDone();
  }
  OnDragComplete();

  for (WidgetObserver& observer : observers_)
    observer.OnWidgetDragComplete(this);
}

void Widget::SchedulePaintInRect(const gfx::Rect& rect) {
  // This happens when DestroyRootView removes all children from the
  // RootView which triggers a SchedulePaint that ends up here. This happens
  // after in ~Widget after native_widget_ is destroyed.
  if (native_widget_destroyed_)
    return;
  native_widget_->SchedulePaintInRect(rect);
}

void Widget::ScheduleLayout() {
  native_widget_->ScheduleLayout();
}

void Widget::SetCursor(gfx::NativeCursor cursor) {
  native_widget_->SetCursor(cursor);
}

bool Widget::IsMouseEventsEnabled() const {
  return native_widget_->IsMouseEventsEnabled();
}

void Widget::SetNativeWindowProperty(const char* name, void* value) {
  native_widget_->SetNativeWindowProperty(name, value);
}

void* Widget::GetNativeWindowProperty(const char* name) const {
  return native_widget_->GetNativeWindowProperty(name);
}

void Widget::UpdateWindowTitle() {
  if (!non_client_view_)
    return;

  // Update the native frame's text. We do this regardless of whether or not
  // the native frame is being used, since this also updates the taskbar, etc.
  std::u16string window_title = widget_delegate_->GetWindowTitle();
  base::i18n::AdjustStringForLocaleDirection(&window_title);
  if (!native_widget_->SetWindowTitle(window_title))
    return;

  non_client_view_->UpdateWindowTitle();
}

void Widget::UpdateWindowIcon() {
  if (non_client_view_)
    non_client_view_->UpdateWindowIcon();

  gfx::ImageSkia window_icon = GetImageSkiaFromImageModel(
      widget_delegate_->GetWindowIcon(), GetColorProvider());

  // In general, icon information is read from a |widget_delegate_| and then
  // passed to |native_widget_|. On ChromeOS, for lacros-chrome to support the
  // initial window state as minimized state, a valid icon is added to
  // |native_widget_| earlier stage of widget initialization. See
  // https://crbug.com/1189981. As only lacros-chrome on ChromeOS supports this
  // behavior other overrides of |native_widget_| will always have no icon
  // information. This is also true for |app_icon| referred below.
  if (window_icon.isNull()) {
    const gfx::ImageSkia* icon = native_widget_->GetWindowIcon();
    if (icon && !icon->isNull())
      window_icon = *icon;
  }

  gfx::ImageSkia app_icon = GetImageSkiaFromImageModel(
      widget_delegate_->GetWindowAppIcon(), GetColorProvider());
  if (app_icon.isNull()) {
    const gfx::ImageSkia* icon = native_widget_->GetWindowAppIcon();
    if (icon && !icon->isNull())
      app_icon = *icon;
  }

  native_widget_->SetWindowIcons(window_icon, app_icon);
}

FocusTraversable* Widget::GetFocusTraversable() {
  return static_cast<internal::RootView*>(root_view_.get());
}

void Widget::ThemeChanged() {
  root_view_->ThemeChanged();

  for (WidgetObserver& observer : observers_)
    observer.OnWidgetThemeChanged(this);

  NotifyColorProviderChanged();
}

void Widget::DeviceScaleFactorChanged(float old_device_scale_factor,
                                      float new_device_scale_factor) {
  root_view_->DeviceScaleFactorChanged(old_device_scale_factor,
                                       new_device_scale_factor);
}

void Widget::SetFocusTraversableParent(FocusTraversable* parent) {
  root_view_->SetFocusTraversableParent(parent);
}

void Widget::SetFocusTraversableParentView(View* parent_view) {
  root_view_->SetFocusTraversableParentView(parent_view);
}

void Widget::ClearNativeFocus() {
  native_widget_->ClearNativeFocus();
}

std::unique_ptr<NonClientFrameView> Widget::CreateNonClientFrameView() {
  auto frame_view = widget_delegate_->CreateNonClientFrameView(this);
  if (!frame_view)
    frame_view = native_widget_->CreateNonClientFrameView();
  if (!frame_view) {
    frame_view =
        ViewsDelegate::GetInstance()->CreateDefaultNonClientFrameView(this);
  }
  if (frame_view)
    return frame_view;

  return std::make_unique<CustomFrameView>(this);
}

bool Widget::ShouldUseNativeFrame() const {
  if (frame_type_ != FrameType::kDefault)
    return frame_type_ == FrameType::kForceNative;
  return native_widget_->ShouldUseNativeFrame();
}

bool Widget::ShouldWindowContentsBeTransparent() const {
  return native_widget_->ShouldWindowContentsBeTransparent();
}

void Widget::DebugToggleFrameType() {
  if (frame_type_ == FrameType::kDefault) {
    frame_type_ = ShouldUseNativeFrame() ? FrameType::kForceCustom
                                         : FrameType::kForceNative;
  } else {
    frame_type_ = frame_type_ == FrameType::kForceCustom
                      ? FrameType::kForceNative
                      : FrameType::kForceCustom;
  }
  FrameTypeChanged();
}

void Widget::FrameTypeChanged() {
  native_widget_->FrameTypeChanged();
}

const ui::Compositor* Widget::GetCompositor() const {
  if (native_widget_destroyed_)
    return nullptr;
  return native_widget_->GetCompositor();
}

const ui::Layer* Widget::GetLayer() const {
  if (native_widget_destroyed_)
    return nullptr;
  return native_widget_->GetLayer();
}

void Widget::ReorderNativeViews() {
  if (native_widget_destroyed_)
    return;
  native_widget_->ReorderNativeViews();
}

void Widget::LayerTreeChanged() {
  // Calculate the layers requires traversing the tree, and since nearly any
  // mutation of the tree can trigger this call we delay until absolutely
  // necessary.
  views_with_layers_dirty_ = true;
}

const NativeWidget* Widget::native_widget() const {
  return native_widget_;
}

NativeWidget* Widget::native_widget() {
  return native_widget_;
}

void Widget::SetCapture(View* view) {
  if (!native_widget_->HasCapture()) {
    native_widget_->SetCapture();

    // Early return if setting capture was unsuccessful.
    if (!native_widget_->HasCapture())
      return;
  }

  if (native_widget_->IsMouseButtonDown())
    is_mouse_button_pressed_ = true;
  root_view_->SetMouseAndGestureHandler(view);
}

void Widget::ReleaseCapture() {
  if (native_widget_->HasCapture())
    native_widget_->ReleaseCapture();
}

bool Widget::HasCapture() {
  return native_widget_->HasCapture();
}

TooltipManager* Widget::GetTooltipManager() {
  if (native_widget_destroyed_)
    return nullptr;
  return native_widget_->GetTooltipManager();
}

const TooltipManager* Widget::GetTooltipManager() const {
  return native_widget_->GetTooltipManager();
}

gfx::Rect Widget::GetWorkAreaBoundsInScreen() const {
  return native_widget_->GetWorkAreaBoundsInScreen();
}

void Widget::SynthesizeMouseMoveEvent() {
  // In screen coordinate.
  gfx::Point mouse_location =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  if (!GetWindowBoundsInScreen().Contains(mouse_location))
    return;

  // Convert: screen coordinate -> widget coordinate.
  View::ConvertPointFromScreen(root_view_.get(), &mouse_location);
  last_mouse_event_was_move_ = false;
  ui::MouseEvent mouse_event(ui::ET_MOUSE_MOVED, mouse_location, mouse_location,
                             ui::EventTimeForNow(), ui::EF_IS_SYNTHESIZED, 0);
  root_view_->OnMouseMoved(mouse_event);
}

bool Widget::IsTranslucentWindowOpacitySupported() const {
  return native_widget_->IsTranslucentWindowOpacitySupported();
}

ui::GestureRecognizer* Widget::GetGestureRecognizer() {
  return native_widget_->GetGestureRecognizer();
}

ui::GestureConsumer* Widget::GetGestureConsumer() {
  return native_widget_->GetGestureConsumer();
}

void Widget::OnSizeConstraintsChanged() {
  native_widget_->OnSizeConstraintsChanged();
  if (non_client_view_)
    non_client_view_->SizeConstraintsChanged();
}

void Widget::OnOwnerClosing() {}

std::string Widget::GetName() const {
  return native_widget_->GetName();
}

base::CallbackListSubscription Widget::RegisterPaintAsActiveChangedCallback(
    PaintAsActiveCallbackList::CallbackType callback) {
  return paint_as_active_callbacks_.Add(std::move(callback));
}

std::unique_ptr<Widget::PaintAsActiveLock> Widget::LockPaintAsActive() {
  const bool was_paint_as_active = ShouldPaintAsActive();
  ++paint_as_active_refcount_;
  if (ShouldPaintAsActive() != was_paint_as_active) {
    paint_as_active_callbacks_.Notify();
    if (parent() && !parent_paint_as_active_lock_)
      parent_paint_as_active_lock_ = parent()->LockPaintAsActive();
  }
  return std::make_unique<PaintAsActiveLockImpl>(
      weak_ptr_factory_.GetWeakPtr());
}

base::WeakPtr<Widget> Widget::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool Widget::ShouldPaintAsActive() const {
  // A transient bubble hits this code path when it loses focus.
  // Return false after Close() is called.
  if (widget_closed_)
    return false;

  return native_widget_active_ || paint_as_active_refcount_ ||
         (parent() && parent()->ShouldPaintAsActive());
}

void Widget::OnParentShouldPaintAsActiveChanged() {
  // |native_widget_| has already been deleted and |this| is being deleted so
  // that we don't have to handle the event and also it's unsafe to reference
  // |native_widget_| in this case.
  if (native_widget_destroyed_)
    return;

  // |native_widget_active| is being updated in
  // OnNativeWidgetActivationChanged(). Notification will be handled there.
  if (native_widget_active_ != native_widget_->IsActive())
    return;

  // this->ShouldPaintAsActive() changes iff the native widget is
  // inactive and there's no lock on this widget.
  if (!(native_widget_active_ || paint_as_active_refcount_))
    paint_as_active_callbacks_.Notify();
}

void Widget::SetNativeTheme(ui::NativeTheme* native_theme) {
  native_theme_ = native_theme;
  native_theme_observation_.Reset();
  if (native_theme)
    native_theme_observation_.Observe(native_theme);
  ThemeChanged();
}

int Widget::GetX() const {
  return GetRestoredBounds().x();
}

int Widget::GetY() const {
  return GetRestoredBounds().y();
}

int Widget::GetWidth() const {
  return GetRestoredBounds().width();
}

int Widget::GetHeight() const {
  return GetRestoredBounds().height();
}

bool Widget::GetVisible() const {
  return IsVisible();
}

void Widget::SetX(int x) {
  gfx::Rect bounds = GetRestoredBounds();
  if (x == bounds.x())
    return;
  bounds.set_x(x);
  SetBounds(bounds);
}

void Widget::SetY(int y) {
  gfx::Rect bounds = GetRestoredBounds();
  if (y == bounds.y())
    return;
  bounds.set_y(y);
  SetBounds(bounds);
}

void Widget::SetWidth(int width) {
  gfx::Rect bounds = GetRestoredBounds();
  if (width == bounds.width())
    return;
  bounds.set_width(width);
  SetBounds(bounds);
}

void Widget::SetHeight(int height) {
  gfx::Rect bounds = GetRestoredBounds();
  if (height == bounds.height())
    return;
  bounds.set_height(height);
  SetBounds(bounds);
}

void Widget::SetVisible(bool visible) {
  if (visible == IsVisible())
    return;
  if (visible)
    Show();
  else
    Hide();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, NativeWidgetDelegate implementation:

bool Widget::IsModal() const {
  return widget_delegate_->GetModalType() != ui::MODAL_TYPE_NONE;
}

bool Widget::IsDialogBox() const {
  return !!widget_delegate_->AsDialogDelegate();
}

bool Widget::CanActivate() const {
  // This may be called after OnNativeWidgetDestroyed(), which sets
  // |widget_delegate_| to null.
  return widget_delegate_ && widget_delegate_->CanActivate();
}

bool Widget::IsNativeWidgetInitialized() const {
  return native_widget_initialized_;
}

bool Widget::OnNativeWidgetActivationChanged(bool active) {
  if (!ShouldHandleNativeWidgetActivationChanged(active))
    return false;

  // On windows we may end up here before we've completed initialization (from
  // an WM_NCACTIVATE). If that happens the WidgetDelegate likely doesn't know
  // the Widget and will crash attempting to access it.
  if (!active && native_widget_initialized_)
    SaveWindowPlacement();

  for (WidgetObserver& observer : observers_)
    observer.OnWidgetActivationChanged(this, active);

  const bool was_paint_as_active = ShouldPaintAsActive();

  // Widgets in a widget tree should share the same ShouldPaintAsActive().
  // Lock the parent as paint-as-active when this widget becomes active.
  if (!active && !paint_as_active_refcount_)
    parent_paint_as_active_lock_.reset();
  else if (parent())
    parent_paint_as_active_lock_ = parent()->LockPaintAsActive();

  native_widget_active_ = active;

  // Notify controls (e.g. LabelButton) and children widgets about the
  // paint-as-active change.
  if (ShouldPaintAsActive() != was_paint_as_active)
    paint_as_active_callbacks_.Notify();

  return true;
}

bool Widget::ShouldHandleNativeWidgetActivationChanged(bool active) {
  return (g_disable_activation_change_handling_ !=
          DisableActivationChangeHandlingType::kIgnore) &&
         (g_disable_activation_change_handling_ !=
              DisableActivationChangeHandlingType::kIgnoreDeactivationOnly ||
          active);
}

void Widget::OnNativeFocus() {
  WidgetFocusManager::GetInstance()->OnNativeFocusChanged(GetNativeView());
}

void Widget::OnNativeBlur() {
  WidgetFocusManager::GetInstance()->OnNativeFocusChanged(nullptr);
}

void Widget::OnNativeWidgetVisibilityChanged(bool visible) {
  View* root = GetRootView();
  if (root)
    root->PropagateVisibilityNotifications(root, visible);
  for (WidgetObserver& observer : observers_)
    observer.OnWidgetVisibilityChanged(this, visible);
  if (GetCompositor() && root && root->layer())
    root->layer()->SetVisible(visible);
}

void Widget::OnNativeWidgetCreated() {
  if (is_top_level())
    focus_manager_ = FocusManagerFactory::Create(this);

  native_widget_->InitModalType(widget_delegate_->GetModalType());

  for (WidgetObserver& observer : observers_)
    observer.OnWidgetCreated(this);
}

void Widget::OnNativeWidgetDestroying() {
  // Tell the focus manager (if any) that root_view is being removed
  // in case that the focused view is under this root view.
  if (GetFocusManager() && root_view_)
    GetFocusManager()->ViewRemoved(root_view_.get());
  for (WidgetObserver& observer : observers_)
    observer.OnWidgetDestroying(this);
  if (non_client_view_)
    non_client_view_->WindowClosing();
  widget_delegate_->WindowClosing();
}

void Widget::OnNativeWidgetDestroyed() {
  for (WidgetObserver& observer : observers_)
    observer.OnWidgetDestroyed(this);
  widget_delegate_->can_delete_this_ = true;
  widget_delegate_->DeleteDelegate();
  widget_delegate_ = nullptr;
  // TODO(pbos): Replace this with native_widget_ = nullptr; and nullptr
  // checking. This currently breaks on reentrant calls to CloseNow() that I'm
  // too scared to fix right now.
  native_widget_destroyed_ = true;
}

void Widget::OnNativeWidgetParentChanged(gfx::NativeView parent) {
  Widget* parent_widget = parent ? GetWidgetForNativeView(parent) : nullptr;
  SetParent(parent_widget);
}

gfx::Size Widget::GetMinimumSize() const {
  return non_client_view_ ? non_client_view_->GetMinimumSize() : gfx::Size();
}

gfx::Size Widget::GetMaximumSize() const {
  return non_client_view_ ? non_client_view_->GetMaximumSize() : gfx::Size();
}

void Widget::OnNativeWidgetMove() {
  TRACE_EVENT0("ui", "Widget::OnNativeWidgetMove");

  widget_delegate_->OnWidgetMove();
  NotifyCaretBoundsChanged(GetInputMethod());

  for (WidgetObserver& observer : observers_)
    observer.OnWidgetBoundsChanged(this, GetWindowBoundsInScreen());
}

void Widget::OnNativeWidgetSizeChanged(const gfx::Size& new_size) {
  TRACE_EVENT0("ui", "Widget::OnNativeWidgetSizeChanged");

  View* root = GetRootView();
  if (root)
    root->SetSize(new_size);

  NotifyCaretBoundsChanged(GetInputMethod());
  SaveWindowPlacementIfInitialized();

  for (WidgetObserver& observer : observers_)
    observer.OnWidgetBoundsChanged(this, GetWindowBoundsInScreen());
}

void Widget::OnNativeWidgetWorkspaceChanged() {}

void Widget::OnNativeWidgetWindowShowStateChanged() {
  SaveWindowPlacementIfInitialized();
}

void Widget::OnNativeWidgetBeginUserBoundsChange() {
  widget_delegate_->OnWindowBeginUserBoundsChange();
}

void Widget::OnNativeWidgetEndUserBoundsChange() {
  widget_delegate_->OnWindowEndUserBoundsChange();
}

void Widget::OnNativeWidgetAddedToCompositor() {}

void Widget::OnNativeWidgetRemovingFromCompositor() {}

bool Widget::HasFocusManager() const {
  return !!focus_manager_.get();
}

void Widget::OnNativeWidgetPaint(const ui::PaintContext& context) {
  // On Linux Aura, we can get here during Init() because of the
  // SetInitialBounds call.
  if (!native_widget_initialized_)
    return;
  GetRootView()->PaintFromPaintRoot(context);
}

int Widget::GetNonClientComponent(const gfx::Point& point) {
  int component =
      non_client_view_ ? non_client_view_->NonClientHitTest(point) : HTNOWHERE;

  if (movement_disabled_ && (component == HTCAPTION || component == HTSYSMENU))
    return HTNOWHERE;

  return component;
}

void Widget::OnKeyEvent(ui::KeyEvent* event) {
  SendEventToSink(event);
  if (!event->handled() && GetFocusManager() &&
      !GetFocusManager()->OnKeyEvent(*event)) {
    event->StopPropagation();
  }
}

// TODO(tdanderson): We should not be calling the OnMouse*() functions on
//                   RootView from anywhere in Widget. Use
//                   SendEventToSink() instead. See crbug.com/348087.
void Widget::OnMouseEvent(ui::MouseEvent* event) {
  TRACE_EVENT0("ui", "Widget::OnMouseEvent");

  View* root_view = GetRootView();
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED: {
      last_mouse_event_was_move_ = false;

      // We may get deleted by the time we return from OnMousePressed. So we
      // use an observer to make sure we are still alive.
      WidgetDeletionObserver widget_deletion_observer(this);

      gfx::NativeView current_capture =
          internal::NativeWidgetPrivate::GetGlobalCapture(
              native_widget_->GetNativeView());
      // Make sure we're still visible before we attempt capture as the mouse
      // press processing may have made the window hide (as happens with menus).
      //
      // It is possible that capture has changed as a result of a mouse-press.
      // In these cases do not update internal state.
      //
      // A mouse-press may trigger a nested message-loop, and absorb the paired
      // release. If so the code returns here. So make sure that that
      // mouse-button is still down before attempting to do a capture.
      if (root_view && root_view->OnMousePressed(*event) &&
          widget_deletion_observer.IsWidgetAlive() && IsVisible() &&
          native_widget_->IsMouseButtonDown() &&
          current_capture == internal::NativeWidgetPrivate::GetGlobalCapture(
                                 native_widget_->GetNativeView())) {
        is_mouse_button_pressed_ = true;
        if (!native_widget_->HasCapture())
          native_widget_->SetCapture();
        event->SetHandled();
      }
      return;
    }

    case ui::ET_MOUSE_RELEASED:
      last_mouse_event_was_move_ = false;
      is_mouse_button_pressed_ = false;
      // Release capture first, to avoid confusion if OnMouseReleased blocks.
      if (auto_release_capture_ && native_widget_->HasCapture()) {
        base::AutoReset<bool> resetter(&ignore_capture_loss_, true);
        native_widget_->ReleaseCapture();
      }
      if (root_view)
        root_view->OnMouseReleased(*event);
      if ((event->flags() & ui::EF_IS_NON_CLIENT) == 0 &&
          // If none of the "normal" buttons are pressed, this event may be from
          // one of the newer mice that have buttons bound to browser forward
          // back actions. Don't squelch the event and let the default handler
          // process it.
          (event->flags() &
           (ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON |
            ui::EF_RIGHT_MOUSE_BUTTON)) != 0)
        event->SetHandled();
      return;

    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
      if (native_widget_->HasCapture() && is_mouse_button_pressed_) {
        last_mouse_event_was_move_ = false;
        if (root_view)
          root_view->OnMouseDragged(*event);
      } else if (!last_mouse_event_was_move_ ||
                 last_mouse_event_position_ != event->location()) {
        last_mouse_event_position_ = event->location();
        last_mouse_event_was_move_ = true;
        if (root_view)
          root_view->OnMouseMoved(*event);
      }
      return;

    case ui::ET_MOUSE_EXITED:
      last_mouse_event_was_move_ = false;
      if (root_view)
        root_view->OnMouseExited(*event);
      return;

    case ui::ET_MOUSEWHEEL:
      if (root_view && root_view->OnMouseWheel(
                           static_cast<const ui::MouseWheelEvent&>(*event)))
        event->SetHandled();
      return;

    default:
      return;
  }
}

void Widget::OnMouseCaptureLost() {
  if (ignore_capture_loss_)
    return;

  View* root_view = GetRootView();
  if (root_view)
    root_view->OnMouseCaptureLost();
  is_mouse_button_pressed_ = false;
}

void Widget::OnScrollEvent(ui::ScrollEvent* event) {
  ui::ScrollEvent event_copy(*event);
  SendEventToSink(&event_copy);

  // Convert unhandled ui::ET_SCROLL events into ui::ET_MOUSEWHEEL events.
  if (!event_copy.handled() && event_copy.type() == ui::ET_SCROLL) {
    ui::MouseWheelEvent wheel(*event);
    OnMouseEvent(&wheel);
  }
}

void Widget::OnGestureEvent(ui::GestureEvent* event) {
  // We explicitly do not capture here. Not capturing enables multiple widgets
  // to get tap events at the same time. Views (such as tab dragging) may
  // explicitly capture.
  SendEventToSink(event);
}

bool Widget::ExecuteCommand(int command_id) {
  return widget_delegate_->ExecuteWindowsCommand(command_id);
}

bool Widget::HasHitTestMask() const {
  return widget_delegate_->WidgetHasHitTestMask();
}

void Widget::GetHitTestMask(SkPath* mask) const {
  DCHECK(mask);
  widget_delegate_->GetWidgetHitTestMask(mask);
}

Widget* Widget::AsWidget() {
  return this;
}

const Widget* Widget::AsWidget() const {
  return this;
}

bool Widget::SetInitialFocus(ui::WindowShowState show_state) {
  FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager)
    return false;
  View* v = widget_delegate_->GetInitiallyFocusedView();
  if (!focus_on_creation_ || show_state == ui::SHOW_STATE_INACTIVE ||
      show_state == ui::SHOW_STATE_MINIMIZED) {
    // If not focusing the window now, tell the focus manager which view to
    // focus when the window is restored.
    if (v)
      focus_manager->SetStoredFocusView(v);
    return true;
  }
  if (v) {
    v->RequestFocus();
    // If the Widget is active (thus allowing its child Views to receive focus),
    // but the request for focus was unsuccessful, fall back to using the first
    // focusable View instead.
    if (focus_manager->GetFocusedView() == nullptr && IsActive())
      focus_manager->AdvanceFocus(false);
  }
  return !!focus_manager->GetFocusedView();
}

bool Widget::ShouldDescendIntoChildForEventHandling(
    ui::Layer* root_layer,
    gfx::NativeView child,
    ui::Layer* child_layer,
    const gfx::Point& location) {
  if (widget_delegate_ &&
      !widget_delegate_->ShouldDescendIntoChildForEventHandling(child,
                                                                location)) {
    return false;
  }

  const View::Views& views_with_layers = GetViewsWithLayers();
  if (views_with_layers.empty())
    return true;

  // Don't descend into |child| if there is a view with a Layer that contains
  // the point and is stacked above |child_layer|.
  auto child_layer_iter = std::find(root_layer->children().begin(),
                                    root_layer->children().end(), child_layer);
  if (child_layer_iter == root_layer->children().end())
    return true;

  for (View* view : base::Reversed(views_with_layers)) {
    // Skip views that don't process events.
    if (!view->GetCanProcessEventsWithinSubtree())
      continue;
    ui::Layer* layer = view->layer();
    DCHECK(layer);
    if (layer->visible() && layer->bounds().Contains(location)) {
      auto root_layer_iter = std::find(root_layer->children().begin(),
                                       root_layer->children().end(), layer);
      if (child_layer_iter > root_layer_iter) {
        // |child| is on top of the remaining layers, no need to continue.
        return true;
      }

      // TODO(pbos): Does this need to be made more robust through hit testing
      // or using ViewTargeter? This for instance does not take into account
      // whether the view is enabled/drawn/etc.
      //
      // Event targeting uses the visible bounds of the View, which may differ
      // from the bounds of the layer. Verify the view hosting the layer
      // actually contains |location|. Use GetVisibleBounds(), which is
      // effectively what event targetting uses.
      gfx::Rect vis_bounds = view->GetVisibleBounds();
      gfx::Point point_in_view = location;
      View::ConvertPointToTarget(GetRootView(), view, &point_in_view);
      if (vis_bounds.Contains(point_in_view))
        return false;
    }
  }
  return true;
}

void Widget::LayoutRootViewIfNecessary() {
  if (root_view_ && root_view_->needs_layout())
    root_view_->Layout();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, ui::EventSource implementation:
ui::EventSink* Widget::GetEventSink() {
  return root_view_.get();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, FocusTraversable implementation:

FocusSearch* Widget::GetFocusSearch() {
  return root_view_->GetFocusSearch();
}

FocusTraversable* Widget::GetFocusTraversableParent() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return nullptr;
}

View* Widget::GetFocusTraversableParentView() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Widget, ui::NativeThemeObserver implementation:

void Widget::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  TRACE_EVENT0("ui", "Widget::OnNativeThemeUpdated");
  ThemeChanged();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, ui::ColorProviderSource:

const ui::ColorProvider* Widget::GetColorProvider() const {
  return ui::ColorProviderManager::Get().GetColorProviderFor(
      GetNativeTheme()->GetColorProviderKey(GetCustomTheme()));
}

////////////////////////////////////////////////////////////////////////////////
// Widget, protected:

internal::RootView* Widget::CreateRootView() {
  return new internal::RootView(this);
}

void Widget::DestroyRootView() {
  ClearFocusFromWidget();
  NotifyWillRemoveView(root_view_.get());
  non_client_view_ = nullptr;
  // Remove all children before the unique_ptr reset so that
  // GetWidget()->GetRootView() doesn't return nullptr while the views hierarchy
  // is being torn down.
  root_view_->RemoveAllChildViews();
  root_view_.reset();
}

void Widget::OnDragWillStart() {}

void Widget::OnDragComplete() {}

const ui::NativeTheme* Widget::GetNativeTheme() const {
  if (native_theme_)
    return native_theme_;

  if (base::FeatureList::IsEnabled(
          features::kInheritNativeThemeFromParentWidget) &&
      parent_)
    return parent_->GetNativeTheme();

#if BUILDFLAG(IS_LINUX)
  if (const views::LinuxUI* linux_ui = views::LinuxUI::instance()) {
    if (auto* native_theme = linux_ui->GetNativeTheme(GetNativeWindow()))
      return native_theme;
  }
#endif

  return ui::NativeTheme::GetInstanceForNativeUi();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, private:

void Widget::SaveWindowPlacement() {
  // The window delegate does the actual saving for us. It seems like (judging
  // by go/crash) that in some circumstances we can end up here after
  // WM_DESTROY, at which point the window delegate is likely gone. So just
  // bail.
  if (!widget_delegate_)
    return;

  ui::WindowShowState show_state = ui::SHOW_STATE_NORMAL;
  gfx::Rect bounds;
  native_widget_->GetWindowPlacement(&bounds, &show_state);
  widget_delegate_->SaveWindowPlacement(bounds, show_state);
}

void Widget::SaveWindowPlacementIfInitialized() {
  if (native_widget_initialized_)
    SaveWindowPlacement();
}

void Widget::SetInitialBounds(const gfx::Rect& bounds) {
  if (!non_client_view_)
    return;

  gfx::Rect saved_bounds;
  if (GetSavedWindowPlacement(&saved_bounds, &saved_show_state_)) {
    if (saved_show_state_ == ui::SHOW_STATE_MAXIMIZED) {
      // If we're going to maximize, wait until Show is invoked to set the
      // bounds. That way we avoid a noticeable resize.
      initial_restored_bounds_ = saved_bounds;
    } else if (!saved_bounds.IsEmpty()) {
      // If the saved bounds are valid, use them.
      SetBounds(saved_bounds);
    }
  } else {
    if (bounds.IsEmpty()) {
      if (bounds.origin().IsOrigin()) {
        // No initial bounds supplied, so size the window to its content and
        // center over its parent.
        CenterWindow(non_client_view_->GetPreferredSize());
      } else {
        // Use the preferred size and the supplied origin.
        gfx::Rect preferred_bounds(bounds);
        preferred_bounds.set_size(non_client_view_->GetPreferredSize());
        SetBoundsConstrained(preferred_bounds);
      }
    } else {
      // Use the supplied initial bounds.
      SetBoundsConstrained(bounds);
    }
  }
}

void Widget::SetInitialBoundsForFramelessWindow(const gfx::Rect& bounds) {
  if (bounds.IsEmpty()) {
    View* contents_view = GetContentsView();
    DCHECK(contents_view);
    // No initial bounds supplied, so size the window to its content and
    // center over its parent if preferred size is provided.
    gfx::Size size = contents_view->GetPreferredSize();
    if (!size.IsEmpty())
      native_widget_->CenterWindow(size);
  } else {
    // Use the supplied initial bounds.
    SetBounds(bounds);
  }
}

void Widget::SetParent(Widget* parent) {
  if (parent == parent_)
    return;

  parent_ = parent;

  // Release the paint-as-active lock on the old parent.
  bool has_lock_on_parent = !!parent_paint_as_active_lock_;
  parent_paint_as_active_lock_.reset();
  parent_paint_as_active_subscription_ = base::CallbackListSubscription();

  // Lock and subscribe to parent's paint-as-active.
  if (parent) {
    if (has_lock_on_parent || native_widget_active_)
      parent_paint_as_active_lock_ = parent->LockPaintAsActive();
    parent_paint_as_active_subscription_ =
        parent->RegisterPaintAsActiveChangedCallback(
            base::BindRepeating(&Widget::OnParentShouldPaintAsActiveChanged,
                                base::Unretained(this)));
  }
}

bool Widget::GetSavedWindowPlacement(gfx::Rect* bounds,
                                     ui::WindowShowState* show_state) {
  // First we obtain the window's saved show-style and store it. We need to do
  // this here, rather than in Show() because by the time Show() is called,
  // the window's size will have been reset (below) and the saved maximized
  // state will have been lost. Sadly there's no way to tell on Windows when
  // a window is restored from maximized state, so we can't more accurately
  // track maximized state independently of sizing information.

  if (!widget_delegate_->GetSavedWindowPlacement(this, bounds, show_state))
    return false;

  gfx::Size minimum_size = GetMinimumSize();
  // Make sure the bounds are at least the minimum size.
  if (bounds->width() < minimum_size.width())
    bounds->set_width(minimum_size.width());

  if (bounds->height() < minimum_size.height())
    bounds->set_height(minimum_size.height());
  return true;
}

const View::Views& Widget::GetViewsWithLayers() {
  if (views_with_layers_dirty_) {
    views_with_layers_dirty_ = false;
    views_with_layers_.clear();
    BuildViewsWithLayers(GetRootView(), &views_with_layers_);
  }
  return views_with_layers_;
}

void Widget::UnlockPaintAsActive() {
  const bool was_paint_as_active = ShouldPaintAsActive();
  DCHECK_GT(paint_as_active_refcount_, 0U);
  --paint_as_active_refcount_;

  if (!paint_as_active_refcount_ && !native_widget_active_)
    parent_paint_as_active_lock_.reset();

  if (ShouldPaintAsActive() != was_paint_as_active)
    paint_as_active_callbacks_.Notify();
}

void Widget::ClearFocusFromWidget() {
  FocusManager* focus_manager = GetFocusManager();
  // We are being removed from a window hierarchy.  Treat this as
  // the root_view_ being removed.
  if (focus_manager)
    focus_manager->ViewRemoved(root_view_.get());
}

BEGIN_METADATA_BASE(Widget)
ADD_READONLY_PROPERTY_METADATA(const char*, ClassName)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, ClientAreaBoundsInScreen)
ADD_READONLY_PROPERTY_METADATA(std::string, Name)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, RestoredBounds)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, WindowBoundsInScreen)
ADD_PROPERTY_METADATA(int, X)
ADD_PROPERTY_METADATA(int, Y)
ADD_PROPERTY_METADATA(int, Width)
ADD_PROPERTY_METADATA(int, Height)
ADD_PROPERTY_METADATA(bool, Visible)
ADD_PROPERTY_METADATA(ui::ZOrderLevel, ZOrderLevel)
END_METADATA

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// internal::NativeWidgetPrivate, NativeWidget implementation:

internal::NativeWidgetPrivate* NativeWidgetPrivate::AsNativeWidgetPrivate() {
  return this;
}

}  // namespace internal
}  // namespace views

DEFINE_ENUM_CONVERTERS(ui::ZOrderLevel,
                       {ui::ZOrderLevel::kNormal, u"kNormal"},
                       {ui::ZOrderLevel::kFloatingWindow, u"kFloatingWindow"},
                       {ui::ZOrderLevel::kFloatingUIElement,
                        u"kFloatingUIElement"},
                       {ui::ZOrderLevel::kSecuritySurface, u"kSecuritySurface"})
