// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_font_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/screen.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_manager_factory.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/window/custom_frame_view.h"
#include "views/controls/menu/menu_controller.h"
#include "views/views_delegate.h"
#include "views/widget/default_theme_provider.h"
#include "views/widget/native_widget_private.h"
#include "views/widget/root_view.h"
#include "views/widget/tooltip_manager.h"
#include "views/widget/widget_delegate.h"

namespace {

// Set to true if a pure Views implementation is preferred
bool use_pure_views = false;

// True to enable debug paint that indicates where to be painted.
bool debug_paint = false;

}  // namespace

namespace views {

// This class is used to keep track of the event a Widget is processing, and
// restore any previously active event afterwards.
class ScopedEvent {
 public:
  ScopedEvent(Widget* widget, const Event& event)
      : widget_(widget),
        event_(&event) {
    widget->event_stack_.push(this);
  }

  ~ScopedEvent() {
    if (widget_)
      widget_->event_stack_.pop();
  }

  void reset() {
    widget_ = NULL;
  }

  const Event* event() {
    return event_;
  }

 private:
  Widget* widget_;
  const Event* event_;

  DISALLOW_COPY_AND_ASSIGN(ScopedEvent);
};

// A default implementation of WidgetDelegate, used by Widget when no
// WidgetDelegate is supplied.
class DefaultWidgetDelegate : public WidgetDelegate {
 public:
  DefaultWidgetDelegate(Widget* widget, const Widget::InitParams& params)
      : widget_(widget),
        can_activate_(!params.child &&
                      params.type != Widget::InitParams::TYPE_POPUP) {
  }
  virtual ~DefaultWidgetDelegate() {}

  // Overridden from WidgetDelegate:
  virtual void DeleteDelegate() OVERRIDE {
    delete this;
  }
  virtual Widget* GetWidget() {
    return widget_;
  }
  virtual const Widget* GetWidget() const {
    return widget_;
  }

  virtual bool CanActivate() const {
    return can_activate_;
  }

 private:
  Widget* widget_;
  bool can_activate_;

  DISALLOW_COPY_AND_ASSIGN(DefaultWidgetDelegate);
};

////////////////////////////////////////////////////////////////////////////////
// Widget, InitParams:

Widget::InitParams::InitParams()
    : type(TYPE_WINDOW),
      delegate(NULL),
      child(false),
      transient(false),
      transparent(false),
      accept_events(true),
      can_activate(true),
      keep_on_top(false),
      ownership(NATIVE_WIDGET_OWNS_WIDGET),
      mirror_origin_in_rtl(false),
      has_dropshadow(false),
      show_state(ui::SHOW_STATE_DEFAULT),
      double_buffer(false),
      parent(NULL),
      parent_widget(NULL),
      native_widget(NULL),
      top_level(false),
      create_texture_for_layer(true) {
}

Widget::InitParams::InitParams(Type type)
    : type(type),
      delegate(NULL),
      child(type == TYPE_CONTROL),
      transient(type == TYPE_BUBBLE || type == TYPE_POPUP || type == TYPE_MENU),
      transparent(false),
      accept_events(true),
      can_activate(type != TYPE_POPUP && type != TYPE_MENU),
      keep_on_top(type == TYPE_MENU),
      ownership(NATIVE_WIDGET_OWNS_WIDGET),
      mirror_origin_in_rtl(false),
      has_dropshadow(false),
      show_state(ui::SHOW_STATE_DEFAULT),
      double_buffer(false),
      parent(NULL),
      parent_widget(NULL),
      native_widget(NULL),
      top_level(false),
      create_texture_for_layer(true) {
}

gfx::NativeView Widget::InitParams::GetParent() const {
  return parent_widget ? parent_widget->GetNativeView() : parent;
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

Widget::Widget()
    : native_widget_(NULL),
      widget_delegate_(NULL),
      non_client_view_(NULL),
      dragged_view_(NULL),
      event_stack_(),
      ownership_(InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      is_secondary_widget_(true),
      frame_type_(FRAME_TYPE_DEFAULT),
      disable_inactive_rendering_(false),
      widget_closed_(false),
      saved_show_state_(ui::SHOW_STATE_DEFAULT),
      focus_on_creation_(true),
      is_top_level_(false),
      native_widget_initialized_(false),
      is_mouse_button_pressed_(false),
      last_mouse_event_was_move_(false) {
}

Widget::~Widget() {
  while (!event_stack_.empty()) {
    event_stack_.top()->reset();
    event_stack_.pop();
  }

  DestroyRootView();
  if (ownership_ == InitParams::WIDGET_OWNS_NATIVE_WIDGET)
    delete native_widget_;
}

// static
Widget* Widget::CreateWindow(WidgetDelegate* delegate) {
  return CreateWindowWithParentAndBounds(delegate, NULL, gfx::Rect());
}

// static
Widget* Widget::CreateWindowWithParent(WidgetDelegate* delegate,
                                       gfx::NativeWindow parent) {
  return CreateWindowWithParentAndBounds(delegate, parent, gfx::Rect());
}

// static
Widget* Widget::CreateWindowWithBounds(WidgetDelegate* delegate,
                                       const gfx::Rect& bounds) {
  return CreateWindowWithParentAndBounds(delegate, NULL, bounds);
}

// static
Widget* Widget::CreateWindowWithParentAndBounds(WidgetDelegate* delegate,
                                                gfx::NativeWindow parent,
                                                const gfx::Rect& bounds) {
  Widget* widget = new Widget;
  Widget::InitParams params;
  params.delegate = delegate;
#if defined(OS_WIN) || defined(USE_AURA)
  params.parent = parent;
#endif
  params.bounds = bounds;
  widget->Init(params);
  return widget;
}

// static
void Widget::SetPureViews(bool pure) {
  use_pure_views = pure;
}

// static
bool Widget::IsPureViews() {
#if defined(USE_AURA) || defined(TOUCH_UI)
  return true;
#else
  return use_pure_views;
#endif
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
void Widget::ReparentNativeView(gfx::NativeView native_view,
                                gfx::NativeView new_parent) {
  internal::NativeWidgetPrivate::ReparentNativeView(native_view, new_parent);
}

// static
int Widget::GetLocalizedContentsWidth(int col_resource_id) {
  return ui::GetLocalizedContentsWidthForFont(col_resource_id,
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont));
}

// static
int Widget::GetLocalizedContentsHeight(int row_resource_id) {
  return ui::GetLocalizedContentsHeightForFont(row_resource_id,
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont));
}

// static
gfx::Size Widget::GetLocalizedContentsSize(int col_resource_id,
                                           int row_resource_id) {
  return gfx::Size(GetLocalizedContentsWidth(col_resource_id),
                   GetLocalizedContentsHeight(row_resource_id));
}

// static
void Widget::SetDebugPaintEnabled(bool enabled) {
  debug_paint = enabled;
}

// static
bool Widget::IsDebugPaintEnabled() {
  return debug_paint;
}

// static
bool Widget::RequiresNonClientView(InitParams::Type type) {
  return type == InitParams::TYPE_WINDOW || type == InitParams::TYPE_BUBBLE;
}

void Widget::Init(const InitParams& params) {
  is_top_level_ = params.top_level ||
      (!params.child &&
       params.type != InitParams::TYPE_CONTROL &&
       params.type != InitParams::TYPE_TOOLTIP);
  widget_delegate_ = params.delegate ?
      params.delegate : new DefaultWidgetDelegate(this, params);
  ownership_ = params.ownership;
  native_widget_ = params.native_widget ?
      params.native_widget->AsNativeWidgetPrivate() :
      internal::NativeWidgetPrivate::CreateNativeWidget(this);
  GetRootView();
  default_theme_provider_.reset(new DefaultThemeProvider);
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
    SetContentsView(non_client_view_);
    SetInitialBounds(params.bounds);
    if (params.show_state == ui::SHOW_STATE_MAXIMIZED)
      Maximize();
    else if (params.show_state == ui::SHOW_STATE_MINIMIZED)
      Minimize();
    UpdateWindowTitle();
  }
  native_widget_initialized_ = true;
}

// Unconverted methods (see header) --------------------------------------------

gfx::NativeView Widget::GetNativeView() const {
  return native_widget_->GetNativeView();
}

gfx::NativeWindow Widget::GetNativeWindow() const {
  return native_widget_->GetNativeWindow();
}

void Widget::AddObserver(Widget::Observer* observer) {
  observers_.AddObserver(observer);
}

void Widget::RemoveObserver(Widget::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool Widget::HasObserver(Widget::Observer* observer) {
  return observers_.HasObserver(observer);
}

bool Widget::GetAccelerator(int cmd_id, ui::Accelerator* accelerator) {
  return false;
}

void Widget::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (!is_add) {
    if (child == dragged_view_)
      dragged_view_ = NULL;

    FocusManager* focus_manager = GetFocusManager();
    if (focus_manager)
      focus_manager->ViewRemoved(child);
    ViewStorage::GetInstance()->ViewRemoved(child);
    native_widget_->ViewRemoved(child);
  }
}

void Widget::NotifyNativeViewHierarchyChanged(bool attached,
                                              gfx::NativeView native_view) {
  if (!attached) {
    FocusManager* focus_manager = GetFocusManager();
    // We are being removed from a window hierarchy.  Treat this as
    // the root_view_ being removed.
    if (focus_manager)
      focus_manager->ViewRemoved(root_view_.get());
  }
  root_view_->NotifyNativeViewHierarchyChanged(attached, native_view);
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
  root_view_->SetContentsView(view);
  if (non_client_view_ != view)
    non_client_view_ = NULL;
}

View* Widget::GetContentsView() {
  return root_view_->GetContentsView();
}

gfx::Rect Widget::GetWindowScreenBounds() const {
  return native_widget_->GetWindowScreenBounds();
}

gfx::Rect Widget::GetClientAreaScreenBounds() const {
  return native_widget_->GetClientAreaScreenBounds();
}

gfx::Rect Widget::GetRestoredBounds() const {
  return native_widget_->GetRestoredBounds();
}

void Widget::SetBounds(const gfx::Rect& bounds) {
  native_widget_->SetBounds(bounds);
}

void Widget::SetSize(const gfx::Size& size) {
  native_widget_->SetSize(size);
}

void Widget::SetBoundsConstrained(const gfx::Rect& bounds) {
  gfx::Rect work_area =
      gfx::Screen::GetMonitorWorkAreaNearestPoint(bounds.origin());
  if (work_area.IsEmpty()) {
    SetBounds(bounds);
  } else {
    // Inset the work area slightly.
    work_area.Inset(10, 10, 10, 10);
    SetBounds(work_area.AdjustToFit(bounds));
  }
}

void Widget::MoveAboveWidget(Widget* widget) {
  native_widget_->MoveAbove(widget->GetNativeView());
}

void Widget::MoveAbove(gfx::NativeView native_view) {
  native_widget_->MoveAbove(native_view);
}

void Widget::MoveToTop() {
  native_widget_->MoveToTop();
}

void Widget::SetShape(gfx::NativeRegion shape) {
  native_widget_->SetShape(shape);
}

void Widget::Close() {
  if (widget_closed_) {
    // It appears we can hit this code path if you close a modal dialog then
    // close the last browser before the destructor is hit, which triggers
    // invoking Close again.
    return;
  }

  bool can_close = true;
  if (non_client_view_)
    can_close = non_client_view_->CanClose();
  if (can_close) {
    SaveWindowPlacement();

    // During tear-down the top-level focus manager becomes unavailable to
    // GTK tabbed panes and their children, so normal deregistration via
    // |FormManager::ViewRemoved()| calls are fouled.  We clear focus here
    // to avoid these redundant steps and to avoid accessing deleted views
    // that may have been in focus.
    if (is_top_level() && focus_manager_.get())
      focus_manager_->SetFocusedView(NULL);

    native_widget_->Close();
    widget_closed_ = true;
  }
}

void Widget::CloseNow() {
  native_widget_->CloseNow();
}

void Widget::EnableClose(bool enable) {
  if (non_client_view_)
    non_client_view_->EnableClose(enable);
  native_widget_->EnableClose(enable);
}

void Widget::Show() {
  if (non_client_view_) {
    if (saved_show_state_ == ui::SHOW_STATE_MAXIMIZED &&
        !initial_restored_bounds_.IsEmpty()) {
      native_widget_->ShowMaximizedWithBounds(initial_restored_bounds_);
    } else {
      native_widget_->ShowWithWindowState(saved_show_state_);
    }
    // |saved_show_state_| only applies the first time the window is shown.
    // If we don't reset the value the window may be shown maximized every time
    // it is subsequently shown after being hidden.
    saved_show_state_ = ui::SHOW_STATE_NORMAL;
  } else {
    native_widget_->Show();
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

void Widget::DisableInactiveRendering() {
  SetInactiveRenderingDisabled(true);
}

void Widget::SetAlwaysOnTop(bool on_top) {
  native_widget_->SetAlwaysOnTop(on_top);
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
  native_widget_->SetFullscreen(fullscreen);
}

bool Widget::IsFullscreen() const {
  return native_widget_->IsFullscreen();
}

void Widget::SetOpacity(unsigned char opacity) {
  native_widget_->SetOpacity(opacity);
}

void Widget::SetUseDragFrame(bool use_drag_frame) {
  native_widget_->SetUseDragFrame(use_drag_frame);
}

View* Widget::GetRootView() {
  if (!root_view_.get()) {
    // First time the root view is being asked for, create it now.
    root_view_.reset(CreateRootView());
  }
  return root_view_.get();
}

const View* Widget::GetRootView() const {
  return root_view_.get();
}

bool Widget::IsVisible() const {
  return native_widget_->IsVisible();
}

bool Widget::IsAccessibleWidget() const {
  return native_widget_->IsAccessibleWidget();
}

ThemeProvider* Widget::GetThemeProvider() const {
  const Widget* root_widget = GetTopLevelWidget();
  if (root_widget && root_widget != this) {
    // Attempt to get the theme provider, and fall back to the default theme
    // provider if not found.
    ThemeProvider* provider = root_widget->GetThemeProvider();
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

InputMethod* Widget::GetInputMethod() {
  if (is_top_level()) {
    if (!input_method_.get())
      input_method_.reset(native_widget_->CreateInputMethod());
    return input_method_.get();
  } else {
    Widget* toplevel = GetTopLevelWidget();
    // If GetTopLevelWidget() returns itself which is not toplevel,
    // the widget is detached from toplevel widget.
    // TODO(oshima): Fix GetTopLevelWidget() to return NULL
    // if there is no toplevel. We probably need to add GetTopMostWidget()
    // to replace some use cases.
    return (toplevel && toplevel != this) ? toplevel->GetInputMethod() : NULL;
  }
}

void Widget::RunShellDrag(View* view, const ui::OSExchangeData& data,
                          int operation) {
  dragged_view_ = view;
  native_widget_->RunShellDrag(view, data, operation);
  // If the view is removed during the drag operation, dragged_view_ is set to
  // NULL.
  if (view && dragged_view_ == view) {
    dragged_view_ = NULL;
    view->OnDragDone();
  }
}

void Widget::SchedulePaintInRect(const gfx::Rect& rect) {
  native_widget_->SchedulePaintInRect(rect);
}

void Widget::SetCursor(gfx::NativeCursor cursor) {
  native_widget_->SetCursor(cursor);
}

void Widget::ResetLastMouseMoveFlag() {
  last_mouse_event_was_move_ = false;
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

  // If the non-client view is rendering its own title, it'll need to relayout
  // now.
  non_client_view_->Layout();

  // Update the native frame's text. We do this regardless of whether or not
  // the native frame is being used, since this also updates the taskbar, etc.
  string16 window_title;
  if (native_widget_->IsScreenReaderActive()) {
    window_title = widget_delegate_->GetAccessibleWindowTitle();
  } else {
    window_title = widget_delegate_->GetWindowTitle();
  }
  base::i18n::AdjustStringForLocaleDirection(&window_title);
  native_widget_->SetWindowTitle(window_title);
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

void Widget::LocaleChanged() {
  root_view_->LocaleChanged();
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

void Widget::FocusNativeView(gfx::NativeView native_view) {
  native_widget_->FocusNativeView(native_view);
}

void Widget::UpdateFrameAfterFrameChange() {
  native_widget_->UpdateFrameAfterFrameChange();
}

NonClientFrameView* Widget::CreateNonClientFrameView() {
  NonClientFrameView* frame_view = widget_delegate_->CreateNonClientFrameView();
  if (!frame_view)
    frame_view = native_widget_->CreateNonClientFrameView();
  return frame_view ? frame_view : new CustomFrameView(this);
}

bool Widget::ShouldUseNativeFrame() const {
  if (frame_type_ != FRAME_TYPE_DEFAULT)
    return frame_type_ == FRAME_TYPE_FORCE_NATIVE;
  return native_widget_->ShouldUseNativeFrame();
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

ui::Compositor* Widget::GetCompositor() {
  return native_widget_->GetCompositor();
}

void Widget::CalculateOffsetToAncestorWithLayer(gfx::Point* offset,
                                                ui::Layer** layer_parent) {
  native_widget_->CalculateOffsetToAncestorWithLayer(offset, layer_parent);
}

void Widget::ReorderLayers() {
  native_widget_->ReorderLayers();
}

void Widget::NotifyAccessibilityEvent(
    View* view,
    ui::AccessibilityTypes::Event event_type,
    bool send_native_event) {
  // Send the notification to the delegate.
  if (ViewsDelegate::views_delegate)
    ViewsDelegate::views_delegate->NotifyAccessibilityEvent(view, event_type);

  if (send_native_event)
    native_widget_->SendNativeAccessibilityEvent(view, event_type);
}

const NativeWidget* Widget::native_widget() const {
  return native_widget_;
}

NativeWidget* Widget::native_widget() {
  return native_widget_;
}

const Event* Widget::GetCurrentEvent() {
  return event_stack_.empty() ? NULL : event_stack_.top()->event();
}

void Widget::TooltipTextChanged(View* view) {
  TooltipManager* manager = native_widget_private()->GetTooltipManager();
  if (manager)
    manager->TooltipTextChanged(view);
}

bool Widget::SetInitialFocus() {
  if (!focus_on_creation_)
    return true;
  View* v = widget_delegate_->GetInitiallyFocusedView();
  if (v)
    v->RequestFocus();
  return !!v;
}

bool Widget::ConvertPointFromAncestor(
    const Widget* ancestor, gfx::Point* point) const {
  return native_widget_->ConvertPointFromAncestor(ancestor, point);
}

View* Widget::GetChildViewParent() {
  return GetContentsView() ? GetContentsView() : GetRootView();
}

gfx::Rect Widget::GetWorkAreaBoundsInScreen() const {
  return native_widget_->GetWorkAreaBoundsInScreen();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, NativeWidgetDelegate implementation:

bool Widget::IsModal() const {
  return widget_delegate_->IsModal();
}

bool Widget::IsDialogBox() const {
  return !!widget_delegate_->AsDialogDelegate();
}

bool Widget::CanActivate() const {
  return widget_delegate_->CanActivate();
}

bool Widget::IsInactiveRenderingDisabled() const {
  return disable_inactive_rendering_;
}

void Widget::EnableInactiveRendering() {
  SetInactiveRenderingDisabled(false);
}

void Widget::OnNativeWidgetActivationChanged(bool active) {
  if (!active) {
    SaveWindowPlacement();

    // Close any open menus.
    MenuController* menu_controller = MenuController::GetActiveInstance();
    if (menu_controller)
      menu_controller->OnWidgetActivationChanged();
  }

  FOR_EACH_OBSERVER(Observer, observers_,
                    OnWidgetActivationChanged(this, active));
}

void Widget::OnNativeFocus(gfx::NativeView focused_view) {
  WidgetFocusManager::GetInstance()->OnWidgetFocusEvent(focused_view,
                                                        GetNativeView());
}

void Widget::OnNativeBlur(gfx::NativeView focused_view) {
  WidgetFocusManager::GetInstance()->OnWidgetFocusEvent(GetNativeView(),
                                                        focused_view);
}

void Widget::OnNativeWidgetVisibilityChanged(bool visible) {
  View* root = GetRootView();
  root->PropagateVisibilityNotifications(root, visible);
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnWidgetVisibilityChanged(this, visible));
  if (GetCompositor() && root->layer())
    root->layer()->SetVisible(visible);
}

void Widget::OnNativeWidgetCreated() {
  if (is_top_level())
    focus_manager_.reset(FocusManagerFactory::Create(this));

  native_widget_->SetAccessibleRole(
      widget_delegate_->GetAccessibleWindowRole());
  native_widget_->SetAccessibleState(
      widget_delegate_->GetAccessibleWindowState());

  if (widget_delegate_->IsModal())
    native_widget_->BecomeModal();
}

void Widget::OnNativeWidgetDestroying() {
  FOR_EACH_OBSERVER(Observer, observers_, OnWidgetClosing(this));
  if (non_client_view_)
    non_client_view_->WindowClosing();
  widget_delegate_->WindowClosing();
}

void Widget::OnNativeWidgetDestroyed() {
  widget_delegate_->DeleteDelegate();
  widget_delegate_ = NULL;
}

gfx::Size Widget::GetMinimumSize() {
  return non_client_view_ ? non_client_view_->GetMinimumSize() : gfx::Size();
}

void Widget::OnNativeWidgetSizeChanged(const gfx::Size& new_size) {
  root_view_->SetSize(new_size);

  // Size changed notifications can fire prior to full initialization
  // i.e. during session restore.  Avoid saving session state during these
  // startup procedures.
  if (native_widget_initialized_)
    SaveWindowPlacement();
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

bool Widget::OnNativeWidgetPaintAccelerated(const gfx::Rect& dirty_region) {
  ui::Compositor* compositor = GetCompositor();
  if (!compositor)
    return false;

  // If the root view is animating, it is likely that it does not cover the same
  // set of pixels it did at the last frame, so we must clear when compositing
  // to avoid leaving ghosts.
  bool force_clear = false;
  if (GetRootView()->layer()) {
    const ui::Transform& layer_transform = GetRootView()->layer()->transform();
    if (layer_transform != GetRootView()->GetTransform()) {
      // The layer has not caught up to the view (i.e., the layer is still
      // animating), and so a clear is required.
      force_clear = true;
    } else {
      // Determine if the layer fills the client area.
      gfx::Rect layer_bounds = GetRootView()->layer()->bounds();
      layer_transform.TransformRect(&layer_bounds);
      gfx::Rect client_bounds = GetClientAreaScreenBounds();
      // Translate bounds to origin (client area bounds are offset to account
      // for buttons, etc).
      client_bounds.set_origin(gfx::Point(0, 0));
      if (!layer_bounds.Contains(client_bounds)) {
        // It doesn't, and so a clear is required.
        force_clear = true;
      }
    }
  }

  compositor->Draw(force_clear);
  return true;
}

void Widget::OnNativeWidgetPaint(gfx::Canvas* canvas) {
  GetRootView()->Paint(canvas);
}

int Widget::GetNonClientComponent(const gfx::Point& point) {
  return non_client_view_ ?
      non_client_view_->NonClientHitTest(point) :
      HTNOWHERE;
}

bool Widget::OnKeyEvent(const KeyEvent& event) {
  ScopedEvent scoped(this, event);
  return static_cast<internal::RootView*>(GetRootView())->OnKeyEvent(event);
}

bool Widget::OnMouseEvent(const MouseEvent& event) {
  ScopedEvent scoped(this, event);
  switch (event.type()) {
    case ui::ET_MOUSE_PRESSED:
      last_mouse_event_was_move_ = false;
      // Make sure we're still visible before we attempt capture as the mouse
      // press processing may have made the window hide (as happens with menus).
      if (GetRootView()->OnMousePressed(event) && IsVisible()) {
        is_mouse_button_pressed_ = true;
        if (!native_widget_->HasMouseCapture())
          native_widget_->SetMouseCapture();
        return true;
      }
      return false;
    case ui::ET_MOUSE_RELEASED:
      last_mouse_event_was_move_ = false;
      is_mouse_button_pressed_ = false;
      // Release capture first, to avoid confusion if OnMouseReleased blocks.
      if (native_widget_->HasMouseCapture() &&
          ShouldReleaseCaptureOnMouseReleased()) {
        native_widget_->ReleaseMouseCapture();
      }
      GetRootView()->OnMouseReleased(event);
      return (event.flags() & ui::EF_IS_NON_CLIENT) ? false : true;
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
      if (native_widget_->HasMouseCapture() && is_mouse_button_pressed_) {
        last_mouse_event_was_move_ = false;
        GetRootView()->OnMouseDragged(event);
      } else if (!last_mouse_event_was_move_ ||
                 last_mouse_event_position_ != event.location()) {
        last_mouse_event_position_ = event.location();
        last_mouse_event_was_move_ = true;
        GetRootView()->OnMouseMoved(event);
      }
      return false;
    case ui::ET_MOUSE_EXITED:
      last_mouse_event_was_move_ = false;
      GetRootView()->OnMouseExited(event);
      return false;
    case ui::ET_MOUSEWHEEL:
      return GetRootView()->OnMouseWheel(
          reinterpret_cast<const MouseWheelEvent&>(event));
    default:
      return false;
  }
  return true;
}

void Widget::OnMouseCaptureLost() {
  if (is_mouse_button_pressed_)
    GetRootView()->OnMouseCaptureLost();
  is_mouse_button_pressed_ = false;
}

ui::TouchStatus Widget::OnTouchEvent(const TouchEvent& event) {
  ScopedEvent scoped(this, event);
  return static_cast<internal::RootView*>(GetRootView())->OnTouchEvent(event);
}

bool Widget::ExecuteCommand(int command_id) {
  return widget_delegate_->ExecuteWindowsCommand(command_id);
}

InputMethod* Widget::GetInputMethodDirect() {
  return input_method_.get();
}

Widget* Widget::AsWidget() {
  return this;
}

const Widget* Widget::AsWidget() const {
  return this;
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
// Widget, protected:

internal::RootView* Widget::CreateRootView() {
  return new internal::RootView(this);
}

void Widget::DestroyRootView() {
  non_client_view_ = NULL;
  root_view_.reset();
  // Input method has to be destroyed before focus manager.
  input_method_.reset();
  // Defer focus manager's destruction. This is for the case when the
  // focus manager is referenced by a child NativeWidgetGtk (e.g. TabbedPane in
  // a dialog). When gtk_widget_destroy is called on the parent, the destroy
  // signal reaches parent first and then the child. Thus causing the parent
  // NativeWidgetGtk's dtor executed before the child's. If child's view
  // hierarchy references this focus manager, it crashes. This will defer focus
  // manager's destruction after child NativeWidgetGtk's dtor.
  FocusManager* focus_manager = focus_manager_.release();
  if (focus_manager)
    MessageLoop::current()->DeleteSoon(FROM_HERE, focus_manager);
}

////////////////////////////////////////////////////////////////////////////////
// Widget, private:

bool Widget::ShouldReleaseCaptureOnMouseReleased() const {
  return true;
}

void Widget::SetInactiveRenderingDisabled(bool value) {
  disable_inactive_rendering_ = value;
  // We need to always notify the NonClientView so that it can trigger a paint.
  // TODO: what's really needed is a way to know when either the active state
  // changes or |disable_inactive_rendering_| changes.
  if (non_client_view_)
    non_client_view_->SetInactiveRenderingDisabled(value);
  native_widget_->SetInactiveRenderingDisabled(value);
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

void Widget::SetInitialBounds(const gfx::Rect& bounds) {
  if (!non_client_view_)
    return;

  gfx::Rect saved_bounds;
  if (GetSavedWindowPlacement(&saved_bounds, &saved_show_state_)) {
    if (saved_show_state_ == ui::SHOW_STATE_MAXIMIZED) {
      // If we're going to maximize, wait until Show is invoked to set the
      // bounds. That way we avoid a noticable resize.
      initial_restored_bounds_ = saved_bounds;
    } else {
      SetBounds(saved_bounds);
    }
  } else {
    if (bounds.IsEmpty()) {
      // No initial bounds supplied, so size the window to its content and
      // center over its parent.
      native_widget_->CenterWindow(non_client_view_->GetPreferredSize());
    } else {
      // Use the supplied initial bounds.
      SetBoundsConstrained(bounds);
    }
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
  if (widget_delegate_->GetSavedWindowPlacement(bounds, show_state)) {
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

void Widget::ReplaceInputMethod(InputMethod* input_method) {
  input_method_.reset(input_method);
  // TODO(oshima): Gtk's textfield doesn't need views InputMethod.
  // Remove this check once gtk is removed.
  if (input_method) {
    input_method->set_delegate(native_widget_);
    input_method->Init(this);
  }
}

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// internal::NativeWidgetPrivate, NativeWidget implementation:

internal::NativeWidgetPrivate* NativeWidgetPrivate::AsNativeWidgetPrivate() {
  return this;
}

}  // namespace internal
}  // namespace views
