// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/default_style.h"
#include "ui/base/default_theme_provider.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/l10n/l10n_font_util.h"
#include "ui/base/resource/resource_bundle.h"
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
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_deletion_observer.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_removals_observer.h"
#include "ui/views/window/custom_frame_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {

namespace {

// If |view| has a layer the layer is added to |layers|. Else this recurses
// through the children. This is used to build a list of the layers created by
// views that are direct children of the Widgets layer.
void BuildViewsWithLayers(View* view, View::Views* views) {
  if (view->layer()) {
    views->push_back(view);
  } else {
    for (int i = 0; i < view->child_count(); ++i)
      BuildViewsWithLayers(view->child_at(i), views);
  }
}

// Create a native widget implementation.
// First, use the supplied one if non-NULL.
// Finally, make a default one.
NativeWidget* CreateNativeWidget(const Widget::InitParams& params,
                                 internal::NativeWidgetDelegate* delegate) {
  if (params.native_widget)
    return params.native_widget;

  ViewsDelegate* views_delegate = ViewsDelegate::GetInstance();
  if (views_delegate && !views_delegate->native_widget_factory().is_null()) {
    NativeWidget* native_widget =
        views_delegate->native_widget_factory().Run(params, delegate);
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

// A default implementation of WidgetDelegate, used by Widget when no
// WidgetDelegate is supplied.
class DefaultWidgetDelegate : public WidgetDelegate {
 public:
  explicit DefaultWidgetDelegate(Widget* widget) : widget_(widget) {
  }
  ~DefaultWidgetDelegate() override {}

  // Overridden from WidgetDelegate:
  void DeleteDelegate() override { delete this; }
  Widget* GetWidget() override { return widget_; }
  const Widget* GetWidget() const override { return widget_; }
  bool ShouldAdvanceFocusToTopLevelWidget() const override {
    // In most situations where a Widget is used without a delegate the Widget
    // is used as a container, so that we want focus to advance to the top-level
    // widget. A good example of this is the find bar.
    return true;
  }

 private:
  Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(DefaultWidgetDelegate);
};

////////////////////////////////////////////////////////////////////////////////
// Widget, InitParams:

Widget::InitParams::InitParams() : InitParams(TYPE_WINDOW) {}

Widget::InitParams::InitParams(Type type)
    : type(type),
      delegate(nullptr),
      child(false),
      opacity(INFER_OPACITY),
      accept_events(true),
      activatable(ACTIVATABLE_DEFAULT),
      keep_on_top(type == TYPE_MENU || type == TYPE_DRAG),
      visible_on_all_workspaces(false),
      ownership(NATIVE_WIDGET_OWNS_WIDGET),
      mirror_origin_in_rtl(false),
      shadow_type(SHADOW_TYPE_DEFAULT),
      remove_standard_frame(false),
      use_system_default_icon(false),
      show_state(ui::SHOW_STATE_DEFAULT),
      parent(nullptr),
      native_widget(nullptr),
      desktop_window_tree_host(nullptr),
      layer_type(ui::LAYER_TEXTURED),
      context(nullptr),
      force_show_in_taskbar(false),
      force_software_compositing(false) {}

Widget::InitParams::InitParams(const InitParams& other) = default;

Widget::InitParams::~InitParams() {
}

bool Widget::InitParams::CanActivate() const {
  if (activatable != InitParams::ACTIVATABLE_DEFAULT)
    return activatable == InitParams::ACTIVATABLE_YES;
  return type != InitParams::TYPE_CONTROL && type != InitParams::TYPE_POPUP &&
         type != InitParams::TYPE_MENU && type != InitParams::TYPE_TOOLTIP &&
         type != InitParams::TYPE_DRAG;
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

Widget::Widget()
    : native_widget_(nullptr),
      widget_delegate_(nullptr),
      non_client_view_(nullptr),
      dragged_view_(nullptr),
      ownership_(InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      is_secondary_widget_(true),
      frame_type_(FRAME_TYPE_DEFAULT),
      always_render_as_active_(false),
      widget_closed_(false),
      saved_show_state_(ui::SHOW_STATE_DEFAULT),
      focus_on_creation_(true),
      is_top_level_(false),
      native_widget_initialized_(false),
      native_widget_destroyed_(false),
      is_mouse_button_pressed_(false),
      ignore_capture_loss_(false),
      last_mouse_event_was_move_(false),
      auto_release_capture_(true),
      views_with_layers_dirty_(false),
      movement_disabled_(false),
      observer_manager_(this) {}

Widget::~Widget() {
  DestroyRootView();
  if (ownership_ == InitParams::WIDGET_OWNS_NATIVE_WIDGET) {
    delete native_widget_;
  } else {
    DCHECK(native_widget_destroyed_)
        << "Destroying a widget with a live native widget. "
        << "Widget probably should use WIDGET_OWNS_NATIVE_WIDGET ownership.";
  }
}

// static
Widget* Widget::CreateWindowWithParent(WidgetDelegate* delegate,
                                       gfx::NativeView parent) {
  return CreateWindowWithParentAndBounds(delegate, parent, gfx::Rect());
}

// static
Widget* Widget::CreateWindowWithParentAndBounds(WidgetDelegate* delegate,
                                                gfx::NativeView parent,
                                                const gfx::Rect& bounds) {
  Widget* widget = new Widget;
  Widget::InitParams params;
  params.delegate = delegate;
  params.parent = parent;
  params.bounds = bounds;
  widget->Init(params);
  return widget;
}

// static
Widget* Widget::CreateWindowWithContext(WidgetDelegate* delegate,
                                        gfx::NativeWindow context) {
  return CreateWindowWithContextAndBounds(delegate, context, gfx::Rect());
}

// static
Widget* Widget::CreateWindowWithContextAndBounds(WidgetDelegate* delegate,
                                                 gfx::NativeWindow context,
                                                 const gfx::Rect& bounds) {
  Widget* widget = new Widget;
  Widget::InitParams params;
  params.delegate = delegate;
  params.context = context;
  params.bounds = bounds;
  widget->Init(params);
  return widget;
}

// static
Widget* Widget::GetWidgetForNativeView(gfx::NativeView native_view) {
  internal::NativeWidgetPrivate* native_widget =
      internal::NativeWidgetPrivate::GetNativeWidgetForNativeView(native_view);
  return native_widget ? native_widget->GetWidget() : NULL;
}

// static
Widget* Widget::GetWidgetForNativeWindow(gfx::NativeWindow native_window) {
  internal::NativeWidgetPrivate* native_widget =
      internal::NativeWidgetPrivate::GetNativeWidgetForNativeWindow(
          native_window);
  return native_widget ? native_widget->GetWidget() : NULL;
}

// static
Widget* Widget::GetTopLevelWidgetForNativeView(gfx::NativeView native_view) {
  internal::NativeWidgetPrivate* native_widget =
      internal::NativeWidgetPrivate::GetTopLevelNativeWidget(native_view);
  return native_widget ? native_widget->GetWidget() : NULL;
}

// static
void Widget::GetAllChildWidgets(gfx::NativeView native_view,
                                Widgets* children) {
  internal::NativeWidgetPrivate::GetAllChildWidgets(native_view, children);
}

// static
void Widget::GetAllOwnedWidgets(gfx::NativeView native_view,
                                Widgets* owned) {
  internal::NativeWidgetPrivate::GetAllOwnedWidgets(native_view, owned);
}

// static
void Widget::ReparentNativeView(gfx::NativeView native_view,
                                gfx::NativeView new_parent) {
  internal::NativeWidgetPrivate::ReparentNativeView(native_view, new_parent);
}

// static
int Widget::GetLocalizedContentsWidth(int col_resource_id) {
  return ui::GetLocalizedContentsWidthForFont(
      col_resource_id, ResourceBundle::GetSharedInstance().GetFontWithDelta(
                           ui::kMessageFontSizeDelta));
}

// static
int Widget::GetLocalizedContentsHeight(int row_resource_id) {
  return ui::GetLocalizedContentsHeightForFont(
      row_resource_id, ResourceBundle::GetSharedInstance().GetFontWithDelta(
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
  return type == InitParams::TYPE_WINDOW ||
         type == InitParams::TYPE_PANEL ||
         type == InitParams::TYPE_BUBBLE;
}

void Widget::Init(const InitParams& in_params) {
  TRACE_EVENT0("views", "Widget::Init");
  InitParams params = in_params;

  // If an internal name was not provided the class name of the contents view
  // is a reasonable default.
  if (params.name.empty() && params.delegate &&
      params.delegate->GetContentsView())
    params.name = params.delegate->GetContentsView()->GetClassName();

  params.child |= (params.type == InitParams::TYPE_CONTROL);
  is_top_level_ = !params.child;

  if (params.opacity == views::Widget::InitParams::INFER_OPACITY &&
      params.type != views::Widget::InitParams::TYPE_WINDOW &&
      params.type != views::Widget::InitParams::TYPE_PANEL)
    params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;

  if (ViewsDelegate::GetInstance())
    ViewsDelegate::GetInstance()->OnBeforeWidgetInit(&params, this);

  if (params.opacity == views::Widget::InitParams::INFER_OPACITY)
    params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;

  bool can_activate = params.CanActivate();
  params.activatable =
      can_activate ? InitParams::ACTIVATABLE_YES : InitParams::ACTIVATABLE_NO;

  widget_delegate_ = params.delegate ?
      params.delegate : new DefaultWidgetDelegate(this);
  widget_delegate_->set_can_activate(can_activate);

  ownership_ = params.ownership;
  native_widget_ = CreateNativeWidget(params, this)->AsNativeWidgetPrivate();
  root_view_.reset(CreateRootView());
  default_theme_provider_.reset(new ui::DefaultThemeProvider);
  if (params.type == InitParams::TYPE_MENU) {
    is_mouse_button_pressed_ =
        internal::NativeWidgetPrivate::IsMouseButtonDown();
  }
  native_widget_->InitNativeWidget(params);
  if (RequiresNonClientView(params.type)) {
    non_client_view_ = new NonClientView;
    non_client_view_->SetFrameView(CreateNonClientFrameView());
    // Create the ClientView, add it to the NonClientView and add the
    // NonClientView to the RootView. This will cause everything to be parented.
    non_client_view_->set_client_view(widget_delegate_->CreateClientView(this));
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
    SetInitialBounds(params.bounds);

    // Perform the initial layout. This handles the case where the size might
    // not actually change when setting the initial bounds. If it did, child
    // views won't have a dirty Layout state, so won't do any work.
    root_view_->Layout();

    if (params.show_state == ui::SHOW_STATE_MAXIMIZED) {
      Maximize();
    } else if (params.show_state == ui::SHOW_STATE_MINIMIZED) {
      Minimize();
      saved_show_state_ = ui::SHOW_STATE_MINIMIZED;
    }
  } else if (params.delegate) {
    SetContentsView(params.delegate->GetContentsView());
    SetInitialBoundsForFramelessWindow(params.bounds);
  }
  // This must come after SetContentsView() or it might not be able to find
  // the correct NativeTheme (on Linux). See http://crbug.com/384492
  observer_manager_.Add(GetNativeTheme());
  native_widget_initialized_ = true;
  native_widget_->OnWidgetInitDone();
}

// Unconverted methods (see header) --------------------------------------------

gfx::NativeView Widget::GetNativeView() const {
  return native_widget_->GetNativeView();
}

gfx::NativeWindow Widget::GetNativeWindow() const {
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

void Widget::ViewHierarchyChanged(
    const View::ViewHierarchyChangedDetails& details) {
  if (!details.is_add) {
    if (details.child == dragged_view_)
      dragged_view_ = NULL;
    FocusManager* focus_manager = GetFocusManager();
    if (focus_manager)
      focus_manager->ViewRemoved(details.child);
    native_widget_->ViewRemoved(details.child);
  }
}

void Widget::NotifyNativeViewHierarchyWillChange() {
  FocusManager* focus_manager = GetFocusManager();
  // We are being removed from a window hierarchy.  Treat this as
  // the root_view_ being removed.
  if (focus_manager)
    focus_manager->ViewRemoved(root_view_.get());
}

void Widget::NotifyNativeViewHierarchyChanged() {
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
  return is_top_level() ? this : native_widget_->GetTopLevelWidget();
}

void Widget::SetContentsView(View* view) {
  // Do not SetContentsView() again if it is already set to the same view.
  if (view == GetContentsView())
    return;

  root_view_->SetContentsView(view);

  // Force a layout now, since the attached hierarchy won't be ready for the
  // containing window's bounds. Note that we call Layout directly rather than
  // calling the widget's size changed handler, since the RootView's bounds may
  // not have changed, which will cause the Layout not to be done otherwise.
  root_view_->Layout();

  if (non_client_view_ != view) {
    // |non_client_view_| can only be non-NULL here if RequiresNonClientView()
    // was true when the widget was initialized. Creating widgets with non
    // client views and then setting the contents view can cause subtle
    // problems on Windows, where the native widget thinks there is still a
    // |non_client_view_|. If you get this error, either use a different type
    // when initializing the widget, or don't call SetContentsView().
    DCHECK(!non_client_view_);
    non_client_view_ = NULL;
  }
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
  gfx::Rect work_area = display::Screen::GetScreen()
                            ->GetDisplayNearestPoint(bounds.origin())
                            .work_area();
  if (work_area.IsEmpty()) {
    SetBounds(bounds);
  } else {
    // Inset the work area slightly.
    work_area.Inset(10, 10, 10, 10);
    work_area.AdjustToFit(bounds);
    SetBounds(work_area);
  }
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

void Widget::Close() {
  if (widget_closed_) {
    // It appears we can hit this code path if you close a modal dialog then
    // close the last browser before the destructor is hit, which triggers
    // invoking Close again.
    return;
  }

  if (non_client_view_ && !non_client_view_->CanClose())
    return;

  // The actions below can cause this function to be called again, so mark
  // |this| as closed early. See crbug.com/714334
  widget_closed_ = true;
  SaveWindowPlacement();

  // During tear-down the top-level focus manager becomes unavailable to
  // GTK tabbed panes and their children, so normal deregistration via
  // |FocusManager::ViewRemoved()| calls are fouled.  We clear focus here
  // to avoid these redundant steps and to avoid accessing deleted views
  // that may have been in focus.
  if (is_top_level() && focus_manager_)
    focus_manager_->SetFocusedView(nullptr);

  for (WidgetObserver& observer : observers_)
    observer.OnWidgetClosing(this);

  native_widget_->Close();
}

void Widget::CloseNow() {
  for (WidgetObserver& observer : observers_)
    observer.OnWidgetClosing(this);
  native_widget_->CloseNow();
}

bool Widget::IsClosed() const {
  return widget_closed_;
}

void Widget::Show() {
  const ui::Layer* layer = GetLayer();
  TRACE_EVENT1("views", "Widget::Show", "layer",
               layer ? layer->name() : "none");
  if (non_client_view_) {
    // While initializing, the kiosk mode will go to full screen before the
    // widget gets shown. In that case we stay in full screen mode, regardless
    // of the |saved_show_state_| member.
    if (saved_show_state_ == ui::SHOW_STATE_MAXIMIZED &&
        !initial_restored_bounds_.IsEmpty() &&
        !IsFullscreen()) {
      native_widget_->ShowMaximizedWithBounds(initial_restored_bounds_);
    } else {
      native_widget_->ShowWithWindowState(
          IsFullscreen() ? ui::SHOW_STATE_FULLSCREEN : saved_show_state_);
    }
    // |saved_show_state_| only applies the first time the window is shown.
    // If we don't reset the value the window may be shown maximized every time
    // it is subsequently shown after being hidden.
    saved_show_state_ = ui::SHOW_STATE_NORMAL;
  } else {
    CanActivate()
        ? native_widget_->Show()
        : native_widget_->ShowWithWindowState(ui::SHOW_STATE_INACTIVE);
  }
}

void Widget::Hide() {
  native_widget_->Hide();
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
  native_widget_->ShowWithWindowState(ui::SHOW_STATE_INACTIVE);
}

void Widget::Activate() {
  native_widget_->Activate();
}

void Widget::Deactivate() {
  native_widget_->Deactivate();
}

bool Widget::IsActive() const {
  return native_widget_->IsActive();
}

void Widget::SetAlwaysOnTop(bool on_top) {
  native_widget_->SetAlwaysOnTop(on_top);
}

bool Widget::IsAlwaysOnTop() const {
  return native_widget_->IsAlwaysOnTop();
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

void Widget::SetFullscreen(bool fullscreen) {
  if (IsFullscreen() == fullscreen)
    return;

  native_widget_->SetFullscreen(fullscreen);

  if (non_client_view_)
    non_client_view_->Layout();
}

bool Widget::IsFullscreen() const {
  return native_widget_->IsFullscreen();
}

void Widget::SetOpacity(float opacity) {
  DCHECK(opacity >= 0.0f);
  DCHECK(opacity <= 1.0f);
  native_widget_->SetOpacity(opacity);
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
  if (root_widget && root_widget != this) {
    // Attempt to get the theme provider, and fall back to the default theme
    // provider if not found.
    const ui::ThemeProvider* provider = root_widget->GetThemeProvider();
    if (provider)
      return provider;

    provider = root_widget->default_theme_provider_.get();
    if (provider)
      return provider;
  }
  return default_theme_provider_.get();
}

FocusManager* Widget::GetFocusManager() {
  Widget* toplevel_widget = GetTopLevelWidget();
  return toplevel_widget ? toplevel_widget->focus_manager_.get() : NULL;
}

const FocusManager* Widget::GetFocusManager() const {
  const Widget* toplevel_widget = GetTopLevelWidget();
  return toplevel_widget ? toplevel_widget->focus_manager_.get() : NULL;
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
                          const ui::OSExchangeData& data,
                          const gfx::Point& location,
                          int operation,
                          ui::DragDropTypes::DragEventSource source) {
  dragged_view_ = view;
  OnDragWillStart();

  WidgetDeletionObserver widget_deletion_observer(this);
  native_widget_->RunShellDrag(view, data, location, operation, source);

  // The widget may be destroyed during the drag operation.
  if (!widget_deletion_observer.IsWidgetAlive())
    return;

  // If the view is removed during the drag operation, dragged_view_ is set to
  // NULL.
  if (view && dragged_view_ == view) {
    dragged_view_ = NULL;
    view->OnDragDone();
  }
  OnDragComplete();
}

void Widget::SchedulePaintInRect(const gfx::Rect& rect) {
  native_widget_->SchedulePaintInRect(rect);
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
  base::string16 window_title = widget_delegate_->GetWindowTitle();
  base::i18n::AdjustStringForLocaleDirection(&window_title);
  if (!native_widget_->SetWindowTitle(window_title))
    return;
  non_client_view_->UpdateWindowTitle();

  // If the non-client view is rendering its own title, it'll need to relayout
  // now and to get a paint update later on.
  non_client_view_->Layout();
}

void Widget::UpdateWindowIcon() {
  if (non_client_view_)
    non_client_view_->UpdateWindowIcon();
  native_widget_->SetWindowIcons(widget_delegate_->GetWindowIcon(),
                                 widget_delegate_->GetWindowAppIcon());
}

FocusTraversable* Widget::GetFocusTraversable() {
  return static_cast<internal::RootView*>(root_view_.get());
}

void Widget::ThemeChanged() {
  root_view_->ThemeChanged();
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

NonClientFrameView* Widget::CreateNonClientFrameView() {
  NonClientFrameView* frame_view =
      widget_delegate_->CreateNonClientFrameView(this);
  if (!frame_view)
    frame_view = native_widget_->CreateNonClientFrameView();
  if (!frame_view && ViewsDelegate::GetInstance()) {
    frame_view =
        ViewsDelegate::GetInstance()->CreateDefaultNonClientFrameView(this);
  }
  if (frame_view)
    return frame_view;

  CustomFrameView* custom_frame_view = new CustomFrameView;
  custom_frame_view->Init(this);
  return custom_frame_view;
}

bool Widget::ShouldUseNativeFrame() const {
  if (frame_type_ != FRAME_TYPE_DEFAULT)
    return frame_type_ == FRAME_TYPE_FORCE_NATIVE;
  return native_widget_->ShouldUseNativeFrame();
}

bool Widget::ShouldWindowContentsBeTransparent() const {
  return native_widget_->ShouldWindowContentsBeTransparent();
}

void Widget::DebugToggleFrameType() {
  if (frame_type_ == FRAME_TYPE_DEFAULT) {
    frame_type_ = ShouldUseNativeFrame() ? FRAME_TYPE_FORCE_CUSTOM :
        FRAME_TYPE_FORCE_NATIVE;
  } else {
    frame_type_ = frame_type_ == FRAME_TYPE_FORCE_CUSTOM ?
        FRAME_TYPE_FORCE_NATIVE : FRAME_TYPE_FORCE_CUSTOM;
  }
  FrameTypeChanged();
}

void Widget::FrameTypeChanged() {
  native_widget_->FrameTypeChanged();
}

const ui::Compositor* Widget::GetCompositor() const {
  return native_widget_->GetCompositor();
}

const ui::Layer* Widget::GetLayer() const {
  return native_widget_->GetLayer();
}

void Widget::ReorderNativeViews() {
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

  if (internal::NativeWidgetPrivate::IsMouseButtonDown())
    is_mouse_button_pressed_ = true;
  root_view_->SetMouseHandler(view);
}

void Widget::ReleaseCapture() {
  if (native_widget_->HasCapture())
    native_widget_->ReleaseCapture();
}

bool Widget::HasCapture() {
  return native_widget_->HasCapture();
}

TooltipManager* Widget::GetTooltipManager() {
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
  gfx::Point mouse_location = EventMonitor::GetLastMouseLocation();
  if (!GetWindowBoundsInScreen().Contains(mouse_location))
    return;

  // Convert: screen coordinate -> widget coordinate.
  View::ConvertPointFromScreen(root_view_.get(), &mouse_location);
  last_mouse_event_was_move_ = false;
  ui::MouseEvent mouse_event(ui::ET_MOUSE_MOVED, mouse_location,
                             mouse_location, ui::EventTimeForNow(),
                             ui::EF_IS_SYNTHESIZED, 0);
  root_view_->OnMouseMoved(mouse_event);
}

bool Widget::IsTranslucentWindowOpacitySupported() const {
  return native_widget_->IsTranslucentWindowOpacitySupported();
}

void Widget::OnSizeConstraintsChanged() {
  native_widget_->OnSizeConstraintsChanged();
  non_client_view_->SizeConstraintsChanged();
}

void Widget::OnOwnerClosing() {}

std::string Widget::GetName() const {
  return native_widget_->GetName();
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
  return widget_delegate_->CanActivate();
}

bool Widget::IsAlwaysRenderAsActive() const {
  return always_render_as_active_;
}

void Widget::OnNativeWidgetActivationChanged(bool active) {
  // On windows we may end up here before we've completed initialization (from
  // an WM_NCACTIVATE). If that happens the WidgetDelegate likely doesn't know
  // the Widget and will crash attempting to access it.
  if (!active && native_widget_initialized_)
    SaveWindowPlacement();

  for (WidgetObserver& observer : observers_)
    observer.OnWidgetActivationChanged(this, active);

  if (non_client_view())
    non_client_view()->frame_view()->ActivationChanged(active);
}

void Widget::OnNativeFocus() {
  WidgetFocusManager::GetInstance()->OnNativeFocusChanged(GetNativeView());
}

void Widget::OnNativeBlur() {
  WidgetFocusManager::GetInstance()->OnNativeFocusChanged(nullptr);
}

void Widget::OnNativeWidgetVisibilityChanging(bool visible) {
  for (WidgetObserver& observer : observers_)
    observer.OnWidgetVisibilityChanging(this, visible);
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

void Widget::OnNativeWidgetCreated(bool desktop_widget) {
  if (is_top_level())
    focus_manager_ = FocusManagerFactory::Create(this, desktop_widget);

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
  widget_delegate_->DeleteDelegate();
  widget_delegate_ = NULL;
  native_widget_destroyed_ = true;
}

gfx::Size Widget::GetMinimumSize() const {
  return non_client_view_ ? non_client_view_->GetMinimumSize() : gfx::Size();
}

gfx::Size Widget::GetMaximumSize() const {
  return non_client_view_ ? non_client_view_->GetMaximumSize() : gfx::Size();
}

void Widget::OnNativeWidgetMove() {
  widget_delegate_->OnWidgetMove();
  NotifyCaretBoundsChanged(GetInputMethod());

  for (WidgetObserver& observer : observers_)
    observer.OnWidgetBoundsChanged(this, GetWindowBoundsInScreen());
}

void Widget::OnNativeWidgetSizeChanged(const gfx::Size& new_size) {
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
  int component = non_client_view_ ?
      non_client_view_->NonClientHitTest(point) :
      HTNOWHERE;

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
          internal::NativeWidgetPrivate::IsMouseButtonDown() &&
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

void Widget::GetHitTestMask(gfx::Path* mask) const {
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
  View* v = widget_delegate_->GetInitiallyFocusedView();
  if (!focus_on_creation_ || show_state == ui::SHOW_STATE_INACTIVE ||
      show_state == ui::SHOW_STATE_MINIMIZED) {
    // If not focusing the window now, tell the focus manager which view to
    // focus when the window is restored.
    if (v && focus_manager)
      focus_manager->SetStoredFocusView(v);
    return true;
  }
  if (v) {
    v->RequestFocus();
    // If the Widget is active (thus allowing its child Views to receive focus),
    // but the request for focus was unsuccessful, fall back to using the first
    // focusable View instead.
    if (focus_manager && focus_manager->GetFocusedView() == nullptr &&
        IsActive()) {
      focus_manager->AdvanceFocus(false);
    }
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

  for (auto iter = views_with_layers.rbegin(); iter != views_with_layers.rend();
       ++iter) {
    ui::Layer* layer = (*iter)->layer();
    DCHECK(layer);
    if (layer->visible() && layer->bounds().Contains(location)) {
      auto root_layer_iter = std::find(root_layer->children().begin(),
                                       root_layer->children().end(), layer);
      if (child_layer_iter > root_layer_iter) {
        // |child| is on top of the remaining layers, no need to continue.
        return true;
      }

      // Event targeting uses the visible bounds of the View, which may differ
      // from the bounds of the layer. Verify the view hosting the layer
      // actually contains |location|. Use GetVisibleBounds(), which is
      // effectively what event targetting uses.
      View* view = *iter;
      gfx::Rect vis_bounds = view->GetVisibleBounds();
      gfx::Point point_in_view = location;
      View::ConvertPointToTarget(GetRootView(), view, &point_in_view);
      if (vis_bounds.Contains(point_in_view))
        return false;
    }
  }
  return true;
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
  return NULL;
}

View* Widget::GetFocusTraversableParentView() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Widget, ui::NativeThemeObserver implementation:

void Widget::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  DCHECK(observer_manager_.IsObserving(observed_theme));

  ui::NativeTheme* current_native_theme = GetNativeTheme();
  if (!observer_manager_.IsObserving(current_native_theme)) {
    observer_manager_.RemoveAll();
    observer_manager_.Add(current_native_theme);
  }

  root_view_->PropagateNativeThemeChanged(current_native_theme);
}

////////////////////////////////////////////////////////////////////////////////
// Widget, protected:

internal::RootView* Widget::CreateRootView() {
  return new internal::RootView(this);
}

void Widget::DestroyRootView() {
  if (is_top_level() && focus_manager_)
    focus_manager_->SetFocusedView(nullptr);
  NotifyWillRemoveView(root_view_.get());
  non_client_view_ = NULL;
  root_view_.reset();
}

void Widget::OnDragWillStart() {}

void Widget::OnDragComplete() {}

////////////////////////////////////////////////////////////////////////////////
// Widget, private:

void Widget::SetAlwaysRenderAsActive(bool always_render_as_active) {
  if (always_render_as_active_ == always_render_as_active)
    return;

  always_render_as_active_ = always_render_as_active;

  // If active, the frame should already be painted. Otherwise,
  // |always_render_as_active_| just changed, and the widget is inactive, so
  // schedule a repaint.
  if (non_client_view_ && !IsActive())
    non_client_view_->frame_view()->SchedulePaint();
}

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
        native_widget_->CenterWindow(non_client_view_->GetPreferredSize());
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

bool Widget::GetSavedWindowPlacement(gfx::Rect* bounds,
                                     ui::WindowShowState* show_state) {
  // First we obtain the window's saved show-style and store it. We need to do
  // this here, rather than in Show() because by the time Show() is called,
  // the window's size will have been reset (below) and the saved maximized
  // state will have been lost. Sadly there's no way to tell on Windows when
  // a window is restored from maximized state, so we can't more accurately
  // track maximized state independently of sizing information.

  // Restore the window's placement from the controller.
  if (widget_delegate_->GetSavedWindowPlacement(this, bounds, show_state)) {
    if (!widget_delegate_->ShouldRestoreWindowSize()) {
      bounds->set_size(non_client_view_->GetPreferredSize());
    } else {
      gfx::Size minimum_size = GetMinimumSize();
      // Make sure the bounds are at least the minimum size.
      if (bounds->width() < minimum_size.width())
        bounds->set_width(minimum_size.width());

      if (bounds->height() < minimum_size.height())
        bounds->set_height(minimum_size.height());
    }
    return true;
  }
  return false;
}

const View::Views& Widget::GetViewsWithLayers() {
  if (views_with_layers_dirty_) {
    views_with_layers_dirty_ = false;
    views_with_layers_.clear();
    BuildViewsWithLayers(GetRootView(), &views_with_layers_);
  }
  return views_with_layers_;
}

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// internal::NativeWidgetPrivate, NativeWidget implementation:

internal::NativeWidgetPrivate* NativeWidgetPrivate::AsNativeWidgetPrivate() {
  return this;
}

}  // namespace internal
}  // namespace views
