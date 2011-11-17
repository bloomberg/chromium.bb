// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/native_widget_wayland.h"

#include <EGL/egl.h>
#include <GL/gl.h>
#include <cairo-gl.h>
#include <cairo.h>
#include <wayland-egl.h>

#include <algorithm>
#include <list>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/wayland/wayland_event.h"
#include "ui/base/view_prop.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/gl/gl_surface.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#include "ui/views/ime/input_method_wayland.h"
#include "ui/wayland/wayland_display.h"
#include "ui/wayland/wayland_input_device.h"
#include "ui/wayland/wayland_screen.h"
#include "ui/wayland/wayland_window.h"
#include "views/views_delegate.h"
#include "views/widget/native_widget_views.h"
#include "views/widget/root_view.h"
#include "views/widget/tooltip_manager_views.h"

using ui::ViewProp;

using base::wayland::WaylandEvent;

namespace views {

namespace {

// Links the WaylandWidget to its NativeWidget.
const char* const kNativeWidgetKey = "__VIEWS_NATIVE_WIDGET__";

}  // namespace

NativeWidgetWayland::NativeWidgetWayland(
                      internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_widget_factory_(this)),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      has_mouse_capture_(false),
      wayland_display_(
       ui:: WaylandDisplay::GetDisplay(gfx::GLSurfaceEGL::GetNativeDisplay())),
      wayland_window_(new ui::WaylandWindow(this, wayland_display_)),
      surface_data_key_(),
      damage_area_() {
}

NativeWidgetWayland::~NativeWidgetWayland() {
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete delegate_;

  if (!View::get_use_acceleration_when_possible()) {
    cairo_surface_destroy(cairo_surface_);
    cairo_device_destroy(device_);
  }

  if (egl_window_)
    wl_egl_window_destroy(egl_window_);

  if (wayland_window_)
    delete wayland_window_;
}

void NativeWidgetWayland::InitNativeWidget(const Widget::InitParams& params) {
  // Cannot create a window with a size smaller than 1x1
  allocation_.set_width(std::max(params.bounds.width(), 1));
  allocation_.set_height(std::max(params.bounds.height(), 1));

  egl_window_ = wl_egl_window_create(wayland_window_->surface(),
                                     allocation_.width(),
                                     allocation_.height());

  SetNativeWindowProperty(kNativeWidgetKey, this);

  if (View::get_use_acceleration_when_possible()) {
    if (ui::Compositor::compositor_factory()) {
      compositor_ = (*ui::Compositor::compositor_factory())(this);
    } else {
      compositor_ = ui::Compositor::Create(this,
                                           egl_window_,
                                           allocation_.size());
    }
    if (compositor_.get())
      delegate_->AsWidget()->GetRootView()->SetPaintToLayer(true);
  } else {
    surface_ = gfx::GLSurface::CreateViewGLSurface(false, egl_window_);
    context_ = gfx::GLContext::CreateGLContext(
        NULL,
        surface_.get(),
        gfx::PreferIntegratedGpu);

    if (!context_->MakeCurrent(surface_.get()))
      DLOG(ERROR) << "Failed to make surface current";

    device_ = cairo_egl_device_create(gfx::GLSurfaceEGL::GetHardwareDisplay(),
                                      context_->GetHandle());
    if (cairo_device_status(device_) != CAIRO_STATUS_SUCCESS)
      DLOG(ERROR) << "Failed to create cairo egl device";

    cairo_surface_ = cairo_gl_surface_create_for_egl(device_,
                                                     surface_->GetHandle(),
                                                     allocation_.width(),
                                                     allocation_.height());
    cairo_surface_set_user_data(cairo_surface_,
                                &surface_data_key_,
                                this,
                                NULL);
  }

  delegate_->OnNativeWidgetCreated();

  if (params.type != Widget::InitParams::TYPE_TOOLTIP) {
    // TODO(dnicoara) Enable this once it works with Wayland
    /*
    views::TooltipManagerViews* manager = new views::TooltipManagerViews(
        static_cast<internal::RootView*>(GetWidget()->GetRootView()));
    tooltip_manager_.reset(manager);
    */
  }

  // TODO(dnicoara) This should be removed when we can specify the (x, y)
  // coordinates for a window. We use fullscreen since it will center the
  // window rather than give it random (x, y) coordinates.
  wayland_window_->set_fullscreen(true);
  Show();
  OnPaint(allocation_);
}

NonClientFrameView* NativeWidgetWayland::CreateNonClientFrameView() {
  return NULL;
}

void NativeWidgetWayland::UpdateFrameAfterFrameChange() {
  NOTIMPLEMENTED();
}

bool NativeWidgetWayland::ShouldUseNativeFrame() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetWayland::FrameTypeChanged() {
  // Called when the Theme has changed, so forward the event to the root
  // widget
  GetWidget()->ThemeChanged();
  GetWidget()->GetRootView()->SchedulePaint();
}

Widget* NativeWidgetWayland::GetWidget() {
  return delegate_->AsWidget();
}

const Widget* NativeWidgetWayland::GetWidget() const {
  return delegate_->AsWidget();
}

gfx::NativeView NativeWidgetWayland::GetNativeView() const {
  return wayland_window_;
}

gfx::NativeWindow NativeWidgetWayland::GetNativeWindow() const {
  return wayland_window_;
}

Widget* NativeWidgetWayland::GetTopLevelWidget() {
  NativeWidgetPrivate* native_widget = GetTopLevelNativeWidget(GetNativeView());
  return native_widget ? native_widget->GetWidget() : NULL;
}

const ui::Compositor* NativeWidgetWayland::GetCompositor() const {
  return compositor_.get();
}

ui::Compositor* NativeWidgetWayland::GetCompositor() {
  return compositor_.get();
}

void NativeWidgetWayland::CalculateOffsetToAncestorWithLayer(
    gfx::Point* offset,
    ui::Layer** layer_parent) {
}

void NativeWidgetWayland::ReorderLayers() {
}

void NativeWidgetWayland::ViewRemoved(View* view) {
  NOTIMPLEMENTED();
}

void NativeWidgetWayland::SetNativeWindowProperty(const char* name,
                                                  void* value) {
  // Remove the existing property (if any).
  for (ViewProps::iterator i = props_.begin(); i != props_.end(); ++i) {
    if ((*i)->Key() == name) {
      props_.erase(i);
      break;
    }
  }

  if (value)
    props_.push_back(new ViewProp(wayland_window_, name, value));
}

void* NativeWidgetWayland::GetNativeWindowProperty(const char* name) const {
  return ViewProp::GetValue(wayland_window_, name);
}

TooltipManager* NativeWidgetWayland::GetTooltipManager() const {
  return tooltip_manager_.get();
}

bool NativeWidgetWayland::IsScreenReaderActive() const {
  return false;
}

void NativeWidgetWayland::SendNativeAccessibilityEvent(
    View* view,
    ui::AccessibilityTypes::Event event_type) {
  NOTIMPLEMENTED();
}

void NativeWidgetWayland::SetMouseCapture() {
  NOTIMPLEMENTED();
  has_mouse_capture_ = true;
}

void NativeWidgetWayland::ReleaseMouseCapture() {
  NOTIMPLEMENTED();
  has_mouse_capture_ = false;
}

bool NativeWidgetWayland::HasMouseCapture() const {
  NOTIMPLEMENTED();
  return has_mouse_capture_;
}

InputMethod* NativeWidgetWayland::CreateInputMethod() {
  return new InputMethodWayland(this);
}

void NativeWidgetWayland::CenterWindow(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

void NativeWidgetWayland::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  NOTIMPLEMENTED();
}

void NativeWidgetWayland::SetWindowTitle(const string16& title) {
}

void NativeWidgetWayland::SetWindowIcons(const SkBitmap& window_icon,
                                         const SkBitmap& app_icon) {
}

void NativeWidgetWayland::SetAccessibleName(const string16& name) {
}

void NativeWidgetWayland::SetAccessibleRole(
    ui::AccessibilityTypes::Role role) {
}

void NativeWidgetWayland::SetAccessibleState(
    ui::AccessibilityTypes::State state) {
}

void NativeWidgetWayland::BecomeModal() {
  NOTIMPLEMENTED();
}

gfx::Rect NativeWidgetWayland::GetWindowScreenBounds() const {
  return GetClientAreaScreenBounds();
}

gfx::Rect NativeWidgetWayland::GetClientAreaScreenBounds() const {
  return allocation_;
}

gfx::Rect NativeWidgetWayland::GetRestoredBounds() const {
  return GetWindowScreenBounds();
}

void NativeWidgetWayland::SetBounds(const gfx::Rect& bounds) {
  saved_allocation_ = allocation_;
  allocation_ = bounds;

  // TODO(dnicoara) This needs to be updated to include (x, y).
  wl_egl_window_resize(egl_window_,
                       allocation_.width(),
                       allocation_.height(),
                       0, 0);
  if (!View::get_use_acceleration_when_possible()) {
    cairo_gl_surface_set_size(cairo_surface_,
                              allocation_.width(),
                              allocation_.height());
  }

  if (compositor_.get())
    compositor_->WidgetSizeChanged(allocation_.size());
  delegate_->OnNativeWidgetSizeChanged(allocation_.size());
}

void NativeWidgetWayland::SetSize(const gfx::Size& size) {
  gfx::Rect new_alloc = allocation_;
  new_alloc.set_size(size);

  SetBounds(new_alloc);
}

void NativeWidgetWayland::MoveAbove(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

void NativeWidgetWayland::MoveToTop() {
  NOTIMPLEMENTED();
}

void NativeWidgetWayland::SetShape(gfx::NativeRegion shape) {
  NOTIMPLEMENTED();
}

void NativeWidgetWayland::Close() {
  Hide();
  if (!close_widget_factory_.HasWeakPtrs()) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&NativeWidgetWayland::CloseNow,
                   close_widget_factory_.GetWeakPtr()));
  }
}

void NativeWidgetWayland::CloseNow() {
  delegate_->OnNativeWidgetDestroying();
  delegate_->OnNativeWidgetDestroyed();
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete this;
}

void NativeWidgetWayland::EnableClose(bool enable) {
  NOTIMPLEMENTED();
}

void NativeWidgetWayland::Show() {
  wayland_window_->SetVisible(true);
  delegate_->OnNativeWidgetVisibilityChanged(true);
}

void NativeWidgetWayland::Hide() {
  wayland_window_->SetVisible(false);
  delegate_->OnNativeWidgetVisibilityChanged(false);
}

void NativeWidgetWayland::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  Show();
  Maximize();
  saved_allocation_ = restored_bounds;
}

void NativeWidgetWayland::ShowWithWindowState(ui::WindowShowState state) {
  NOTIMPLEMENTED();
}

bool NativeWidgetWayland::IsVisible() const {
  return wayland_window_->IsVisible();
}

void NativeWidgetWayland::Activate() {
  NOTIMPLEMENTED();
}

void NativeWidgetWayland::Deactivate() {
  NOTIMPLEMENTED();
}

bool NativeWidgetWayland::IsActive() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetWayland::SetAlwaysOnTop(bool always_on_top) {
  NOTIMPLEMENTED();
}

void NativeWidgetWayland::Maximize() {
  std::list<ui::WaylandScreen*> screens = wayland_display_->GetScreenList();

  if (screens.empty())
    return;

  // TODO(dnicoara) We need to intersect the current coordinates with the
  // screen ones and decide the correct screen to fullscreen on.
  ui::WaylandScreen* screen = screens.front();

  SetBounds(screen->GetAllocation());
}

void NativeWidgetWayland::Minimize() {
  NOTIMPLEMENTED();
}

bool NativeWidgetWayland::IsMaximized() const {
  NOTIMPLEMENTED();
  return true;
}

bool NativeWidgetWayland::IsMinimized() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetWayland::Restore() {
  NOTIMPLEMENTED();
}

void NativeWidgetWayland::SetFullscreen(bool fullscreen) {
  gfx::Rect new_allocation = allocation_;

  if (fullscreen) {
    std::list<ui::WaylandScreen*> screens = wayland_display_->GetScreenList();

    if (screens.empty())
      return;

    // TODO(dnicoara) What does it mean to be fullscreen when having multiple
    // monitors? If we're going fullscreen only on one screen then we need to
    // intersect the current coordinates with the screen ones and decide the
    // correct screen to fullscreen on.
    ui::WaylandScreen* screen = screens.front();
    new_allocation = screen->GetAllocation();
  } else {
    new_allocation = saved_allocation_;
  }

  wayland_window_->set_fullscreen(fullscreen);
  SetBounds(new_allocation);
}

bool NativeWidgetWayland::IsFullscreen() const {
  return wayland_window_->fullscreen();
}

void NativeWidgetWayland::SetOpacity(unsigned char opacity) {
  NOTIMPLEMENTED();
}

void NativeWidgetWayland::SetUseDragFrame(bool use_drag_frame) {
  NOTIMPLEMENTED();
}

bool NativeWidgetWayland::IsAccessibleWidget() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetWayland::RunShellDrag(View* view,
                                       const ui::OSExchangeData& data,
                                       int operation) {
  NOTIMPLEMENTED();
}

gboolean NativeWidgetWayland::IdleRedraw(void* ptr) {
  NativeWidgetWayland* widget = static_cast<NativeWidgetWayland*>(ptr);
  gfx::Rect damage_area = widget->damage_area_;
  widget->damage_area_ = gfx::Rect();

  widget->OnPaint(damage_area);

  return FALSE;
}

void NativeWidgetWayland::SchedulePaintInRect(const gfx::Rect& rect) {
  if (damage_area_.IsEmpty())
    g_idle_add(NativeWidgetWayland::IdleRedraw, this);

  damage_area_ = damage_area_.Union(rect);
}

void NativeWidgetWayland::SetCursor(gfx::NativeCursor cursor) {
  NOTIMPLEMENTED();
}


void NativeWidgetWayland::ClearNativeFocus() {
  NOTIMPLEMENTED();
}


void NativeWidgetWayland::FocusNativeView(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

bool NativeWidgetWayland::ConvertPointFromAncestor(
    const Widget* ancestor, gfx::Point* point) const {
  NOTREACHED();
  return false;
}

void NativeWidgetWayland::ScheduleDraw() {
  SchedulePaintInRect(allocation_);
}

// Overridden from NativeWidget
gfx::AcceleratedWidget NativeWidgetWayland::GetAcceleratedWidget() {
  return egl_window_;
}


// Overridden from internal::InputMethodDelegate
void NativeWidgetWayland::DispatchKeyEventPostIME(const KeyEvent& key) {
  NOTIMPLEMENTED();
  delegate_->OnKeyEvent(key);
}

/////////////////////////////////////////////////////////////////////////////
// NativeWidgetWayland, private, event handlers

void NativeWidgetWayland::OnPaint(gfx::Rect damage_area) {
  if (!delegate_->OnNativeWidgetPaintAccelerated(damage_area)) {
    // This is required since the CanvasSkiaPaint damages the surface
    // in the destructor so we need to have this done before calling
    // swapbuffers.
    {
      cairo_rectangle_int_t region = damage_area.ToCairoRectangle();
      gfx::CanvasSkiaPaint canvas(cairo_surface_, &region);
      if (!canvas.is_empty()) {
        canvas.set_composite_alpha(false);
        delegate_->OnNativeWidgetPaint(&canvas);
      }
    }

    // Have cairo swap buffers, then let Wayland know of the damaged area.
    cairo_gl_surface_swapbuffers(cairo_surface_);
    wl_surface_damage(wayland_window_->surface(),
                      damage_area.x(), damage_area.y(),
                      damage_area.width(), damage_area.height());
  }
}

void NativeWidgetWayland::OnMotionNotify(WaylandEvent event) {
  MouseEvent mouse_event(&event);
  delegate_->OnMouseEvent(mouse_event);
}

void NativeWidgetWayland::OnButtonNotify(WaylandEvent event) {
  if (event.button.button == ui::SCROLL_UP ||
      event.button.button == ui::SCROLL_DOWN) {
    MouseWheelEvent mouse_event(&event);
    delegate_->OnMouseEvent(mouse_event);
  } else {
    MouseEvent mouse_event(&event);
    delegate_->OnMouseEvent(mouse_event);
  }
}

void NativeWidgetWayland::OnKeyNotify(WaylandEvent event) {
  KeyEvent key_event(&event);
  InputMethod* input_method = GetWidget()->GetInputMethodDirect();
  if (input_method)
    input_method->DispatchKeyEvent(key_event);
  else
    DispatchKeyEventPostIME(key_event);
}

void NativeWidgetWayland::OnPointerFocus(WaylandEvent event) {
  MouseEvent mouse_event(&event);
  delegate_->OnMouseEvent(mouse_event);
}

void NativeWidgetWayland::OnKeyboardFocus(WaylandEvent event) {
  InputMethod* input_method = GetWidget()->GetInputMethodDirect();
  if (input_method) {
    if (event.keyboard_focus.state)
      input_method->OnFocus();
    else
      input_method->OnBlur();
  }
}

void NativeWidgetWayland::OnGeometryChange(WaylandEvent event) {
  SetSize(gfx::Size(event.geometry_change.width,
                    event.geometry_change.height));
}

/////////////////////////////////////////////////////////////////////////////
// Widget

// static
bool Widget::ConvertRect(const Widget* source,
                         const Widget* target,
                         gfx::Rect* rect) {
  DCHECK(source);
  DCHECK(target);
  DCHECK(rect);

  gfx::NativeView source_widget = source->GetNativeView();
  gfx::NativeView target_widget = target->GetNativeView();
  if (source_widget == target_widget)
    return true;

  if (!source_widget || !target_widget)
    return false;

  NOTIMPLEMENTED();
  return false;
}

namespace internal {

/////////////////////////////////////////////////////////////////////////////
// NativeWidget

// static
NativeWidgetPrivate* NativeWidgetPrivate::CreateNativeWidget(
    internal::NativeWidgetDelegate* delegate) {
  if (Widget::IsPureViews() &&
      ViewsDelegate::views_delegate->GetDefaultParentView()) {
    return new NativeWidgetViews(delegate);
  }
  return new NativeWidgetWayland(delegate);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeView(
    gfx::NativeView native_view) {
  return reinterpret_cast<NativeWidgetWayland*>(
      ViewProp::GetValue(native_view, kNativeWidgetKey));
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeWindow(
    gfx::NativeWindow native_window) {
  return GetNativeWidgetForNativeView(native_window);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetTopLevelNativeWidget(
    gfx::NativeView native_view) {
  // TODO(dnicoara) What would be the best way to implement this?
  // Since there isn't any actual parenting concept in Wayland, we could
  // implement it using WaylandWindow->SetParent/GetParent calls.
  return GetNativeWidgetForNativeView(native_view);
}

// static
void NativeWidgetPrivate::GetAllChildWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* children) {
  NOTIMPLEMENTED();
  if (!native_view)
    return;
}

// static
void NativeWidgetPrivate::ReparentNativeView(gfx::NativeView native_view,
                                             gfx::NativeView new_parent) {
  NOTIMPLEMENTED();
  if (!native_view)
    return;
}

// static
bool NativeWidgetPrivate::IsMouseButtonDown() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace internal

}  // namespace views
