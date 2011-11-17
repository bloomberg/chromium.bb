// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/native_widget_views.h"

#include "base/bind.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "views/view.h"
#include "views/views_delegate.h"
#include "views/widget/native_widget_view.h"
#include "views/widget/root_view.h"
#include "views/widget/tooltip_manager_views.h"
#include "views/widget/window_manager.h"

#if defined(HAVE_IBUS)
#include "ui/views/ime/input_method_ibus.h"
#else
#include "ui/views/ime/mock_input_method.h"
#endif

namespace {

gfx::Rect AdjustRectOriginForParentWidget(const gfx::Rect& rect,
                                          const views::Widget* parent) {
  if (!parent)
    return rect;

  gfx::Rect adjusted = rect;
  gfx::Rect parent_bounds = parent->GetWindowScreenBounds();
  adjusted.Offset(-parent_bounds.x(), -parent_bounds.y());
  return adjusted;
}

}  // namespace

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetViews, public:

NativeWidgetViews::NativeWidgetViews(internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate),
      parent_(NULL),
      view_(NULL),
      active_(false),
      window_state_(ui::SHOW_STATE_DEFAULT),
      always_on_top_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_widget_factory_(this)),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      delete_native_view_(true) {
}

NativeWidgetViews::~NativeWidgetViews() {
  delegate_->OnNativeWidgetDestroying();

  if (view_ && delete_native_view_) {
    // We must prevent the NativeWidgetView from attempting to delete us.
    view_->set_delete_native_widget(false);
    delete view_;
  }

  delegate_->OnNativeWidgetDestroyed();
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete delegate_;
}

View* NativeWidgetViews::GetView() {
  return view_;
}

const View* NativeWidgetViews::GetView() const {
  return view_;
}

void NativeWidgetViews::OnActivate(bool active) {
  // TODO(oshima): find out if we should check toplevel here.
  if (active_ == active)
    return;
  active_ = active;
  delegate_->OnNativeWidgetActivationChanged(active);

  // TODO(oshima): Focus change should be separated from window activation.
  // This will be fixed when we have WM API.
  Widget* widget = GetWidget();
  if (widget->is_top_level()) {
    InputMethod* input_method = widget->GetInputMethod();
    if (active) {
      input_method->OnFocus();
      // See description of got_initial_focus_in_ for details on this.
      widget->GetFocusManager()->RestoreFocusedView();
    } else {
      input_method->OnBlur();
      widget->GetFocusManager()->StoreFocusedView();
    }
  }
  view_->SchedulePaint();
}

bool NativeWidgetViews::OnKeyEvent(const KeyEvent& key_event) {
  InputMethod* input_method = GetWidget()->GetInputMethod();
  DCHECK(input_method);
  input_method->DispatchKeyEvent(key_event);
  return true;
}

void NativeWidgetViews::DispatchKeyEventPostIME(const KeyEvent& key) {
  // TODO(oshima): GTK impl handles menu_key in special way. This may be
  // necessary when running under NativeWidgetX.
  if (delegate_->OnKeyEvent(key))
    return;
  if (key.type() == ui::ET_KEY_PRESSED && GetWidget()->GetFocusManager())
    GetWidget()->GetFocusManager()->OnKeyEvent(key);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetViews, protected:

void NativeWidgetViews::OnBoundsChanged(const gfx::Rect& new_bounds,
                                        const gfx::Rect& old_bounds) {
  delegate_->OnNativeWidgetSizeChanged(new_bounds.size());
}

bool NativeWidgetViews::OnMouseEvent(const MouseEvent& event) {
#if defined(TOUCH_UI) || defined(USE_AURA)
  TooltipManagerViews* tooltip_manager =
      static_cast<TooltipManagerViews*>(GetTooltipManager());
  if (tooltip_manager)
    tooltip_manager->UpdateForMouseEvent(event);
#endif
  return HandleWindowOperation(event) ? true : delegate_->OnMouseEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetViews, NativeWidget implementation:

void NativeWidgetViews::InitNativeWidget(const Widget::InitParams& params) {
  parent_ = params.parent_widget;
  ownership_ = params.ownership;
  always_on_top_ = params.keep_on_top;
  View* parent_view = NULL;
  if (params.parent_widget) {
    parent_view = params.parent_widget->GetChildViewParent();
  } else if (ViewsDelegate::views_delegate &&
             ViewsDelegate::views_delegate->GetDefaultParentView() &&
             !params.child) {
    parent_view = ViewsDelegate::views_delegate->GetDefaultParentView();
  } else if (params.parent) {
    Widget* widget = Widget::GetWidgetForNativeView(params.parent);
    parent_view = widget->GetChildViewParent();
  }

  gfx::Rect bounds = GetWidget()->is_top_level() ?
      AdjustRectOriginForParentWidget(params.bounds, parent_) : params.bounds;
  view_ = new internal::NativeWidgetView(this);
  view_->SetBoundsRect(bounds);
  view_->SetVisible(params.type == Widget::InitParams::TYPE_CONTROL);

  // With the default NATIVE_WIDGET_OWNS_WIDGET ownership, the
  // deletion of either of the NativeWidgetViews or NativeWidgetView
  // (e.g. via View hierarchy destruction) will delete the other.
  // With WIDGET_OWNS_NATIVE_WIDGET, NativeWidgetViews should only
  // be deleted by its Widget.
  if (ownership_ == Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET)
    view_->set_delete_native_widget(false);

  if (parent_view)
    parent_view->AddChildView(view_);
  view_->SetPaintToLayer(true);
  if (View::get_use_acceleration_when_possible())
    view_->SetFillsBoundsOpaquely(!params.transparent);
  // TODO(beng): SetInitParams().
}

NonClientFrameView* NativeWidgetViews::CreateNonClientFrameView() {
  return NULL;
}

void NativeWidgetViews::UpdateFrameAfterFrameChange() {
}

bool NativeWidgetViews::ShouldUseNativeFrame() const {
//  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetViews::FrameTypeChanged() {
}

Widget* NativeWidgetViews::GetWidget() {
  return delegate_->AsWidget();
}

const Widget* NativeWidgetViews::GetWidget() const {
  return delegate_->AsWidget();
}

gfx::NativeView NativeWidgetViews::GetNativeView() const {
  return GetParentNativeWidget() ?
      GetParentNativeWidget()->GetNativeView() : NULL;
}

gfx::NativeWindow NativeWidgetViews::GetNativeWindow() const {
  return GetParentNativeWidget() ?
      GetParentNativeWidget()->GetNativeWindow() : NULL;
}

Widget* NativeWidgetViews::GetTopLevelWidget() {
  // This can get called when this is in the process of being destroyed, and
  // view_ has already been unset.
  if (!view_)
    return GetWidget();
  if (ViewsDelegate::views_delegate &&
      view_->parent() == ViewsDelegate::views_delegate->GetDefaultParentView())
    return GetWidget();
  // During Widget destruction, this function may be called after |view_| is
  // detached from a Widget, at which point this NativeWidget's Widget becomes
  // the effective toplevel.
  Widget* containing_widget = view_->GetWidget();
  return containing_widget ? containing_widget->GetTopLevelWidget()
                           : GetWidget();
}

const ui::Compositor* NativeWidgetViews::GetCompositor() const {
  return view_->GetWidget() ? view_->GetWidget()->GetCompositor() : NULL;
}

ui::Compositor* NativeWidgetViews::GetCompositor() {
  return view_->GetWidget() ? view_->GetWidget()->GetCompositor() : NULL;
}

void NativeWidgetViews::CalculateOffsetToAncestorWithLayer(
    gfx::Point* offset,
    ui::Layer** layer_parent) {
  view_->CalculateOffsetToAncestorWithLayer(offset, layer_parent);
}

void NativeWidgetViews::ReorderLayers() {
  view_->ReorderLayers();
}

void NativeWidgetViews::ViewRemoved(View* view) {
  internal::NativeWidgetPrivate* parent = GetParentNativeWidget();
  if (parent)
    parent->ViewRemoved(view);
}

void NativeWidgetViews::SetNativeWindowProperty(const char* name, void* value) {
  if (value)
    props_map_[name] = value;
  else
    props_map_.erase(name);
}

void* NativeWidgetViews::GetNativeWindowProperty(const char* name) const {
  PropsMap::const_iterator iter = props_map_.find(name);
  return iter != props_map_.end() ? iter->second : NULL;
}

TooltipManager* NativeWidgetViews::GetTooltipManager() const {
  const internal::NativeWidgetPrivate* parent = GetParentNativeWidget();
  return parent ? parent->GetTooltipManager() : NULL;
}

bool NativeWidgetViews::IsScreenReaderActive() const {
  return GetParentNativeWidget()->IsScreenReaderActive();
}

void NativeWidgetViews::SendNativeAccessibilityEvent(
    View* view,
    ui::AccessibilityTypes::Event event_type) {
  return GetParentNativeWidget()->SendNativeAccessibilityEvent(view,
                                                               event_type);
}

void NativeWidgetViews::SetMouseCapture() {
  WindowManager::Get()->SetMouseCapture(GetWidget());
}

void NativeWidgetViews::ReleaseMouseCapture() {
  WindowManager::Get()->ReleaseMouseCapture(GetWidget());
}

bool NativeWidgetViews::HasMouseCapture() const {
  return WindowManager::Get()->HasMouseCapture(GetWidget());
}

InputMethod* NativeWidgetViews::CreateInputMethod() {
#if defined(HAVE_IBUS)
  InputMethod* input_method = new InputMethodIBus(this);
#else
  InputMethod* input_method = new MockInputMethod(this);
#endif
  input_method->Init(GetWidget());
  return input_method;
}

void NativeWidgetViews::CenterWindow(const gfx::Size& size) {
  const gfx::Size parent_size = GetView()->parent()->size();
  GetView()->SetBounds((parent_size.width() - size.width())/2,
                       (parent_size.height() - size.height())/2,
                       size.width(), size.height());
}

void NativeWidgetViews::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  *bounds = GetView()->bounds();
  *show_state = ui::SHOW_STATE_NORMAL;
}

void NativeWidgetViews::SetWindowTitle(const string16& title) {
}

void NativeWidgetViews::SetWindowIcons(const SkBitmap& window_icon,
                                       const SkBitmap& app_icon) {
}

void NativeWidgetViews::SetAccessibleName(const string16& name) {
}

void NativeWidgetViews::SetAccessibleRole(ui::AccessibilityTypes::Role role) {
}

void NativeWidgetViews::SetAccessibleState(
    ui::AccessibilityTypes::State state) {
}

void NativeWidgetViews::BecomeModal() {
  NOTIMPLEMENTED();
}

gfx::Rect NativeWidgetViews::GetWindowScreenBounds() const {
  if (GetWidget() == GetWidget()->GetTopLevelWidget() &&
      !parent_)
    return view_->bounds();
  gfx::Point origin = view_->bounds().origin();
  View::ConvertPointToScreen(view_->parent(), &origin);
  return gfx::Rect(origin.x(), origin.y(), view_->width(), view_->height());
}

gfx::Rect NativeWidgetViews::GetClientAreaScreenBounds() const {
  return GetWindowScreenBounds();
}

gfx::Rect NativeWidgetViews::GetRestoredBounds() const {
  return GetWindowScreenBounds();
}

void NativeWidgetViews::SetBounds(const gfx::Rect& bounds) {
  // |bounds| are supplied in the coordinates of the parent.
  if (GetWidget()->is_top_level())
    view_->SetBoundsRect(AdjustRectOriginForParentWidget(bounds, parent_));
  else
    view_->SetBoundsRect(bounds);
}

void NativeWidgetViews::SetSize(const gfx::Size& size) {
  view_->SetSize(size);
}

void NativeWidgetViews::MoveAbove(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

void NativeWidgetViews::MoveToTop() {
  view_->parent()->ReorderChildView(view_, -1);
}

void NativeWidgetViews::SetShape(gfx::NativeRegion region) {
  NOTIMPLEMENTED();
}

void NativeWidgetViews::Close() {
  Hide();
  if (!close_widget_factory_.HasWeakPtrs()) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&NativeWidgetViews::CloseNow,
                   close_widget_factory_.GetWeakPtr()));
  }
}

void NativeWidgetViews::CloseNow() {
  delete view_;
  view_ = NULL;
}

void NativeWidgetViews::EnableClose(bool enable) {
}

void NativeWidgetViews::Show() {
  if (always_on_top_)
    MoveToTop();
  view_->SetVisible(true);
  GetWidget()->SetInitialFocus();
}

void NativeWidgetViews::Hide() {
  view_->SetVisible(false);
  if (HasMouseCapture())
    ReleaseMouseCapture();

  // This is necessary because views desktop's window manager doesn't
  // set the focus back to parent.
  if (GetWidget()->is_top_level()) {
    Widget* parent_widget = view_->GetWidget();
    if (parent_widget && parent_widget->GetInputMethod())
      parent_widget->GetInputMethod()->OnFocus();
  }
}

void NativeWidgetViews::ShowWithWindowState(ui::WindowShowState show_state) {
  Show();
}

void NativeWidgetViews::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  Show();
}

bool NativeWidgetViews::IsVisible() const {
  return view_->IsVisible() && (GetWidget()->is_top_level() ||
                                GetWidget()->GetTopLevelWidget()->IsVisible());
}

void NativeWidgetViews::Activate() {
  // Enable WidgetObserverTest.ActivationChange when this is implemented.
  MoveToTop();
  OnActivate(true);
}

void NativeWidgetViews::Deactivate() {
  OnActivate(false);
}

bool NativeWidgetViews::IsActive() const {
  return active_;
}

void NativeWidgetViews::SetAlwaysOnTop(bool on_top) {
  always_on_top_ = on_top;
  // This is not complete yet. At least |MoveToTop| will need to be updated to
  // make sure a 'normal' window does not get on top of a window with
  // |always_on_top_| set.
  NOTIMPLEMENTED();
}

void NativeWidgetViews::Maximize() {
  if (window_state_ == ui::SHOW_STATE_MAXIMIZED)
    return;

  if (window_state_ != ui::SHOW_STATE_MINIMIZED) {
    // Remember bounds and transform to use when unmaximized.
    restored_bounds_ = view_->bounds();
    restored_transform_ = view_->GetTransform();
  }

  window_state_ = ui::SHOW_STATE_MAXIMIZED;
  gfx::Size size = GetParentNativeWidget()->GetWindowScreenBounds().size();
  SetBounds(gfx::Rect(gfx::Point(), size));
}

void NativeWidgetViews::Minimize() {
  if (view_->layer() && view_->layer()->GetAnimator()->is_animating())
    return;

  gfx::Rect view_bounds = view_->bounds();
  gfx::Rect parent_bounds = view_->parent()->bounds();

  if (window_state_ != ui::SHOW_STATE_MAXIMIZED) {
    restored_bounds_ = view_bounds;
    restored_transform_ = view_->GetTransform();
  }

  float aspect_ratio = static_cast<float>(view_bounds.width()) /
                       static_cast<float>(view_bounds.height());
  int target_size = 100;
  int target_height = target_size;
  int target_width = static_cast<int>(aspect_ratio * target_height);
  if (target_width > target_size) {
    target_width = target_size;
    target_height = static_cast<int>(target_width / aspect_ratio);
  }

  int target_x = 20;
  int target_y = parent_bounds.height() - target_size - 20;

  view_->SetBounds(
      target_x, target_y, view_bounds.width(), view_bounds.height());

  ui::Transform transform;
  transform.SetScale((float)target_width / (float)view_bounds.width(),
                     (float)target_height / (float)view_bounds.height());
  view_->SetTransform(transform);

  window_state_ = ui::SHOW_STATE_MINIMIZED;
}

bool NativeWidgetViews::IsMaximized() const {
  return window_state_ == ui::SHOW_STATE_MAXIMIZED;
}

bool NativeWidgetViews::IsMinimized() const {
  return window_state_ == ui::SHOW_STATE_MINIMIZED;
}

void NativeWidgetViews::Restore() {
  if (view_->layer() && view_->layer()->GetAnimator()->is_animating())
    return;

  window_state_ = ui::SHOW_STATE_NORMAL;
  view_->SetBoundsRect(restored_bounds_);
  view_->SetTransform(restored_transform_);
}

void NativeWidgetViews::SetFullscreen(bool fullscreen) {
  NOTIMPLEMENTED();
}

bool NativeWidgetViews::IsFullscreen() const {
  // NOTIMPLEMENTED();
  return false;
}

void NativeWidgetViews::SetOpacity(unsigned char opacity) {
  NOTIMPLEMENTED();
}

void NativeWidgetViews::SetUseDragFrame(bool use_drag_frame) {
  NOTIMPLEMENTED();
}

bool NativeWidgetViews::IsAccessibleWidget() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetViews::RunShellDrag(View* view,
                                     const ui::OSExchangeData& data,
                                     int operation) {
  GetParentNativeWidget()->RunShellDrag(view, data, operation);
}

void NativeWidgetViews::SchedulePaintInRect(const gfx::Rect& rect) {
  view_->SchedulePaintInRect(rect);
}

void NativeWidgetViews::SetCursor(gfx::NativeCursor cursor) {
  view_->set_cursor(cursor);
  GetParentNativeWidget()->SetCursor(cursor);
}

void NativeWidgetViews::ClearNativeFocus() {
  GetParentNativeWidget()->ClearNativeFocus();
}

void NativeWidgetViews::FocusNativeView(gfx::NativeView native_view) {
  GetParentNativeWidget()->FocusNativeView(native_view);
}

bool NativeWidgetViews::ConvertPointFromAncestor(
    const Widget* ancestor, gfx::Point* point) const {
  // This method converts the point from ancestor's coordinates to
  // this widget's coordinate using recursion as the widget hierachy
  // is usually shallow.

  if (ancestor == GetWidget())
    return true;  // no conversion necessary

  const Widget* parent_widget = view_->GetWidget();
  if (!parent_widget)  // couldn't reach the ancestor.
    return false;

  if (parent_widget == ancestor ||
      parent_widget->ConvertPointFromAncestor(ancestor, point)) {
    View::ConvertPointToView(parent_widget->GetRootView(), GetView(), point);
    return true;
  }
  return false;
}

gfx::Rect NativeWidgetViews::GetWorkAreaBoundsInScreen() const {
  return GetParentNativeWidget()->GetWorkAreaBoundsInScreen();
}

void NativeWidgetViews::SetInactiveRenderingDisabled(bool value) {
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetViews, private:

internal::NativeWidgetPrivate* NativeWidgetViews::GetParentNativeWidget() {
  Widget* containing_widget = view_ ? view_->GetWidget() : NULL;
  return containing_widget ? static_cast<internal::NativeWidgetPrivate*>(
      containing_widget->native_widget()) :
      NULL;
}

const internal::NativeWidgetPrivate*
    NativeWidgetViews::GetParentNativeWidget() const {
  const Widget* containing_widget = view_ ? view_->GetWidget() : NULL;
  return containing_widget ? static_cast<const internal::NativeWidgetPrivate*>(
      containing_widget->native_widget()) :
      NULL;
}

bool NativeWidgetViews::HandleWindowOperation(const MouseEvent& event) {
  if (event.type() != ui::ET_MOUSE_PRESSED)
    return false;

  Widget* widget = GetWidget();
  if (widget->non_client_view()) {
    int hittest_code = widget->non_client_view()->NonClientHitTest(
        event.location());
    switch (hittest_code) {
      case HTCAPTION: {
        if (!event.IsOnlyRightMouseButton()) {
          WindowManager::Get()->StartMoveDrag(widget, event.location());
          return true;
        }
        break;
      }
      case HTBOTTOM:
      case HTBOTTOMLEFT:
      case HTBOTTOMRIGHT:
      case HTGROWBOX:
      case HTLEFT:
      case HTRIGHT:
      case HTTOP:
      case HTTOPLEFT:
      case HTTOPRIGHT: {
        WindowManager::Get()->StartResizeDrag(
            widget, event.location(), hittest_code);
        return true;
      }
      default:
        // Everything else falls into standard client event handling.
        break;
    }
  }
  return false;
}

}  // namespace views
