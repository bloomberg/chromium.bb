// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/x11_window.h"

#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/buildflags.h"
#include "ui/base/x/x11_desktop_window_move_client.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/display/screen.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/x/x11_event_translation.h"
#include "ui/gfx/x/x11.h"
#include "ui/platform_window/common/platform_window_defaults.h"
#include "ui/platform_window/extensions/workspace_extension_delegate.h"
#include "ui/platform_window/extensions/x11_extension_delegate.h"
#include "ui/platform_window/x11/x11_window_manager.h"

#if defined(USE_OZONE)
#include "ui/events/ozone/events_ozone.h"
#endif  // defined(USE_OZONE)

#if BUILDFLAG(USE_ATK)
#include "ui/platform_window/x11/atk_event_conversion.h"
#endif

namespace ui {

namespace {

XWindow::WindowOpacity GetXWindowOpacity(PlatformWindowOpacity opacity) {
  using WindowOpacity = XWindow::WindowOpacity;
  switch (opacity) {
    case PlatformWindowOpacity::kInferOpacity:
      return WindowOpacity::kInferOpacity;
    case PlatformWindowOpacity::kOpaqueWindow:
      return WindowOpacity::kOpaqueWindow;
    case PlatformWindowOpacity::kTranslucentWindow:
      return WindowOpacity::kTranslucentWindow;
  }
  NOTREACHED() << "Uknown window opacity.";
  return WindowOpacity::kInferOpacity;
}

XWindow::WindowType GetXWindowType(PlatformWindowType window_type) {
  using WindowType = XWindow::WindowType;
  switch (window_type) {
    case PlatformWindowType::kWindow:
      return WindowType::kWindow;
    case PlatformWindowType::kMenu:
      return WindowType::kMenu;
    case PlatformWindowType::kTooltip:
      return WindowType::kTooltip;
    case PlatformWindowType::kPopup:
      return WindowType::kPopup;
    case PlatformWindowType::kDrag:
      return WindowType::kDrag;
    case PlatformWindowType::kBubble:
      return WindowType::kBubble;
  }
  NOTREACHED() << "Uknown window type.";
  return WindowType::kWindow;
}

ui::XWindow::Configuration ConvertInitPropertiesToXWindowConfig(
    const PlatformWindowInitProperties& properties) {
  ui::XWindow::Configuration config;
  config.type = GetXWindowType(properties.type);
  config.opacity = GetXWindowOpacity(properties.opacity);
  config.bounds = properties.bounds;
  config.icon = properties.icon;
  config.force_show_in_taskbar = properties.force_show_in_taskbar;
  config.keep_on_top = properties.keep_on_top;
  config.visible_on_all_workspaces = properties.visible_on_all_workspaces;
  config.remove_standard_frame = properties.remove_standard_frame;
  config.workspace = properties.workspace;
  config.wm_class_name = properties.wm_class_name;
  config.wm_class_class = properties.wm_class_class;
  config.wm_role_name = properties.wm_role_name;
  config.activatable = properties.activatable;
  config.prefer_dark_theme = properties.prefer_dark_theme;
  config.background_color = properties.background_color;
  return config;
}

// Coalesce touch/mouse events if needed
bool CoalesceEventsIfNeeded(XEvent* const xev, EventType type, XEvent* out) {
  if (xev->type == MotionNotify ||
      (xev->type == GenericEvent &&
       (type == ui::ET_TOUCH_MOVED || type == ui::ET_MOUSE_MOVED ||
        type == ui::ET_MOUSE_DRAGGED))) {
    return ui::CoalescePendingMotionEvents(xev, out) > 0;
  }
  return false;
}

}  // namespace

X11Window::X11Window(PlatformWindowDelegate* platform_window_delegate)
    : platform_window_delegate_(platform_window_delegate) {
  // Set a class property key, which allows |this| to be used for interactive
  // events, e.g. move or resize.
  SetWmMoveResizeHandler(this, static_cast<WmMoveResizeHandler*>(this));

  // Set extensions property key that extends the interface of this platform
  // implementation.
  SetWorkspaceExtension(this, static_cast<WorkspaceExtension*>(this));
  SetX11Extension(this, static_cast<X11Extension*>(this));
}

X11Window::~X11Window() {
  PrepareForShutdown();
  Close();
}

void X11Window::Initialize(PlatformWindowInitProperties properties) {
  XWindow::Configuration config =
      ConvertInitPropertiesToXWindowConfig(properties);

  gfx::Size adjusted_size_in_pixels =
      AdjustSizeForDisplay(config.bounds.size());
  config.bounds.set_size(adjusted_size_in_pixels);
  config.override_redirect =
      properties.x11_extension_delegate &&
      properties.x11_extension_delegate->IsOverrideRedirect(IsWmTiling());

  workspace_extension_delegate_ = properties.workspace_extension_delegate;
  x11_extension_delegate_ = properties.x11_extension_delegate;

  Init(config);
}

void X11Window::SetXEventDelegate(XEventDelegate* delegate) {
  DCHECK(!x_event_delegate_);
  x_event_delegate_ = delegate;
}

void X11Window::OnXWindowLostCapture() {
  platform_window_delegate_->OnLostCapture();
}

void X11Window::OnMouseEnter() {
  platform_window_delegate_->OnMouseEnter();
}

gfx::AcceleratedWidget X11Window::GetWidget() const {
  // In spite of being defined in Xlib as `unsigned long`, XID (|window()|'s
  // type) is fixed at 32-bits (CARD32) in X11 Protocol, therefore can't be
  // larger than 32 bits values on the wire (see https://crbug.com/607014 for
  // more details). So, It's safe to use static_cast here.
  return static_cast<gfx::AcceleratedWidget>(window());
}

void X11Window::Show(bool inactive) {
  if (mapped_in_client())
    return;

  XWindow::Map(inactive);
}

void X11Window::Hide() {
  XWindow::Hide();
}

void X11Window::Close() {
  if (is_shutting_down_)
    return;

  X11WindowManager::GetInstance()->RemoveWindow(this);

  is_shutting_down_ = true;
  XWindow::Close();
  platform_window_delegate_->OnClosed();
}

bool X11Window::IsVisible() const {
  return XWindow::IsXWindowVisible();
}

void X11Window::PrepareForShutdown() {
  DCHECK(X11EventSource::HasInstance());
  X11EventSource::GetInstance()->RemoveXEventDispatcher(this);
}

void X11Window::SetBounds(const gfx::Rect& bounds) {
  gfx::Rect current_bounds_in_pixels = GetBounds();
  gfx::Rect bounds_in_pixels(bounds.origin(),
                             AdjustSizeForDisplay(bounds.size()));

  bool size_changed =
      current_bounds_in_pixels.size() != bounds_in_pixels.size();

  if (size_changed) {
    // Only cancel the delayed resize task if we're already about to call
    // OnHostResized in this function.
    XWindow::CancelResize();
  }

  // Assume that the resize will go through as requested, which should be the
  // case if we're running without a window manager.  If there's a window
  // manager, it can modify or ignore the request, but (per ICCCM) we'll get a
  // (possibly synthetic) ConfigureNotify about the actual size and correct
  // |bounds_| later.
  XWindow::SetBounds(bounds_in_pixels);
}

gfx::Rect X11Window::GetBounds() {
  return XWindow::bounds();
}

void X11Window::SetTitle(const base::string16& title) {
  XWindow::SetTitle(title);
}

void X11Window::SetCapture() {
  if (HasCapture())
    return;
  X11WindowManager::GetInstance()->GrabEvents(this);
  GrabPointer();
}

void X11Window::ReleaseCapture() {
  if (!HasCapture())
    return;
  ReleasePointerGrab();
  X11WindowManager::GetInstance()->UngrabEvents(this);
}

bool X11Window::HasCapture() const {
  return X11WindowManager::GetInstance()->located_events_grabber() == this;
}

void X11Window::ToggleFullscreen() {
  // Check if we need to fullscreen the window or not.
  bool fullscreen = state_ != PlatformWindowState::kFullScreen;
  if (fullscreen)
    CancelResize();

  // Work around a bug where if we try to unfullscreen, metacity immediately
  // fullscreens us again. This is a little flickery and not necessary if
  // there's a gnome-panel, but it's not easy to detect whether there's a
  // panel or not.
  bool unmaximize_and_remaximize = !fullscreen && IsMaximized() &&
                                   ui::GuessWindowManager() == ui::WM_METACITY;

  if (unmaximize_and_remaximize)
    Restore();

  // Fullscreen state changes have to be handled manually and then checked
  // against configuration events, which come from a compositor. The reason
  // of manually changing the |state_| is that the compositor answers
  // about state changes asynchronously, which leads to a wrong return value in
  // DesktopWindowTreeHostPlatform::IsFullscreen, for example, and media
  // files can never be set to fullscreen. Wayland does the same.
  auto new_state = PlatformWindowState::kNormal;
  if (fullscreen)
    new_state = PlatformWindowState::kFullScreen;
  else if (IsMaximized())
    new_state = PlatformWindowState::kMaximized;

  bool was_fullscreen = IsFullscreen();
  state_ = new_state;
  SetFullscreen(fullscreen);

  if (unmaximize_and_remaximize)
    Maximize();

  // Try to guess the size we will have after the switch to/from fullscreen:
  // - (may) avoid transient states
  // - works around Flash content which expects to have the size updated
  //   synchronously.
  // See https://crbug.com/361408
  gfx::Rect bounds_in_pixels = GetBounds();
  if (fullscreen) {
    display::Screen* screen = display::Screen::GetScreen();
    const display::Display display =
        screen->GetDisplayMatching(bounds_in_pixels);
    SetRestoredBoundsInPixels(bounds_in_pixels);
    bounds_in_pixels =
        gfx::Rect(gfx::ScaleToFlooredPoint(display.bounds().origin(),
                                           display.device_scale_factor()),
                  display.GetSizeInPixel());
  } else {
    // Exiting "browser fullscreen mode", but the X11 window is not necessarily
    // in fullscreen state (e.g: a WM keybinding might have been used to toggle
    // fullscreen state). So check whether the window is in fullscreen state
    // before trying to restore its bounds (saved before entering in browser
    // fullscreen mode).
    if (was_fullscreen)
      bounds_in_pixels = GetRestoredBoundsInPixels();
    else
      SetRestoredBoundsInPixels({});
  }
  // Do not go through SetBounds as long as it adjusts bounds and sets them to X
  // Server. Instead, we just store the bounds and notify the client that the
  // window occupies the entire screen.
  XWindow::set_bounds(bounds_in_pixels);
  platform_window_delegate_->OnBoundsChanged(bounds_in_pixels);
}

void X11Window::Maximize() {
  if (IsFullscreen()) {
    // Unfullscreen the window if it is fullscreen.
    SetFullscreen(false);

    // Resize the window so that it does not have the same size as a monitor.
    // (Otherwise, some window managers immediately put the window back in
    // fullscreen mode).
    gfx::Rect bounds_in_pixels = GetBounds();
    gfx::Rect adjusted_bounds_in_pixels(
        bounds_in_pixels.origin(),
        AdjustSizeForDisplay(bounds_in_pixels.size()));
    if (adjusted_bounds_in_pixels != bounds_in_pixels)
      SetBounds(adjusted_bounds_in_pixels);
  }

  // When we are in the process of requesting to maximize a window, we can
  // accurately keep track of our restored bounds instead of relying on the
  // heuristics that are in the PropertyNotify and ConfigureNotify handlers.
  SetRestoredBoundsInPixels(GetBounds());

  XWindow::Maximize();
}

void X11Window::Minimize() {
  XWindow::Minimize();
}

void X11Window::Restore() {
  XWindow::Unmaximize();
  XWindow::Unhide();
}

PlatformWindowState X11Window::GetPlatformWindowState() const {
  return state_;
}

void X11Window::Activate() {
  XWindow::Activate();
}

void X11Window::Deactivate() {
  XWindow::Deactivate();
}

void X11Window::SetUseNativeFrame(bool use_native_frame) {
  XWindow::SetUseNativeFrame(use_native_frame);
}

bool X11Window::ShouldUseNativeFrame() const {
  return XWindow::use_native_frame();
}

void X11Window::SetCursor(PlatformCursor cursor) {
  // X11PlatformWindowOzone has different type of PlatformCursor. Thus, use this
  // only for X11 and Ozone will manage this by itself.
#if defined(USE_X11)
  XWindow::SetCursor(cursor);
#endif
}

void X11Window::MoveCursorTo(const gfx::Point& location) {
  XWindow::MoveCursorTo(location);
}

void X11Window::ConfineCursorToBounds(const gfx::Rect& bounds) {
  XWindow::ConfineCursorTo(bounds);
}

void X11Window::SetRestoredBoundsInPixels(const gfx::Rect& bounds) {
  restored_bounds_in_pixels_ = bounds;
}

gfx::Rect X11Window::GetRestoredBoundsInPixels() const {
  return restored_bounds_in_pixels_;
}

bool X11Window::ShouldWindowContentsBeTransparent() const {
  return XWindow::has_alpha();
}

void X11Window::SetZOrderLevel(ZOrderLevel order) {
  z_order_ = order;

  // Emulate the multiple window levels provided by other platforms by
  // collapsing the z-order enum into kNormal = normal, everything else = always
  // on top.
  XWindow::SetAlwaysOnTop(order != ui::ZOrderLevel::kNormal);
}

ZOrderLevel X11Window::GetZOrderLevel() const {
  bool window_always_on_top = is_always_on_top();
  bool level_always_on_top = z_order_ != ui::ZOrderLevel::kNormal;

  if (window_always_on_top == level_always_on_top)
    return z_order_;

  // If something external has forced a window to be always-on-top, map it to
  // kFloatingWindow as a reasonable equivalent.
  return window_always_on_top ? ui::ZOrderLevel::kFloatingWindow
                              : ui::ZOrderLevel::kNormal;
}

void X11Window::StackAbove(gfx::AcceleratedWidget widget) {
  // Check comment in the GetWidget method about this this cast.
  XWindow::StackXWindowAbove(static_cast<::Window>(widget));
}

void X11Window::StackAtTop() {
  XWindow::StackXWindowAtTop();
}

void X11Window::FlashFrame(bool flash_frame) {
  XWindow::SetFlashFrameHint(flash_frame);
}

void X11Window::SetShape(std::unique_ptr<ShapeRects> native_shape,
                         const gfx::Transform& transform) {
  return XWindow::SetXWindowShape(std::move(native_shape), transform);
}

void X11Window::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  XWindow::SetXWindowAspectRatio(aspect_ratio);
}

void X11Window::SetWindowIcons(const gfx::ImageSkia& window_icon,
                               const gfx::ImageSkia& app_icon) {
  XWindow::SetXWindowIcons(window_icon, app_icon);
}

void X11Window::SizeConstraintsChanged() {
  XWindow::UpdateMinAndMaxSize();
}

bool X11Window::IsTranslucentWindowOpacitySupported() const {
  // This function may be called before InitX11Window() (which
  // initializes |visual_has_alpha_|), so we cannot simply return
  // |visual_has_alpha_|.
  return ui::XVisualManager::GetInstance()->ArgbVisualAvailable();
}

void X11Window::SetOpacity(float opacity) {
  XWindow::SetXWindowOpacity(opacity);
}

std::string X11Window::GetWorkspace() const {
  base::Optional<int> workspace_id = XWindow::workspace();
  return workspace_id.has_value() ? base::NumberToString(workspace_id.value())
                                  : std::string();
}

void X11Window::SetVisibleOnAllWorkspaces(bool always_visible) {
  XWindow::SetXWindowVisibleOnAllWorkspaces(always_visible);
}

bool X11Window::IsVisibleOnAllWorkspaces() const {
  return XWindow::IsXWindowVisibleOnAllWorkspaces();
}

void X11Window::SetWorkspaceExtensionDelegate(
    WorkspaceExtensionDelegate* delegate) {
  workspace_extension_delegate_ = delegate;
}

bool X11Window::IsSyncExtensionAvailable() const {
  return ui::IsSyncExtensionAvailable();
}

bool X11Window::IsWmTiling() const {
  return ui::IsWmTiling(ui::GuessWindowManager());
}

void X11Window::OnCompleteSwapAfterResize() {
  XWindow::NotifySwapAfterResize();
}

gfx::Rect X11Window::GetXRootWindowOuterBounds() const {
  return XWindow::GetOutterBounds();
}

bool X11Window::ContainsPointInXRegion(const gfx::Point& point) const {
  return XWindow::ContainsPointInRegion(point);
}

void X11Window::LowerXWindow() {
  XWindow::LowerWindow();
}

void X11Window::SetOverrideRedirect(bool override_redirect) {
  XWindow::SetOverrideRedirect(override_redirect);
}

void X11Window::SetX11ExtensionDelegate(X11ExtensionDelegate* delegate) {
  x11_extension_delegate_ = delegate;
}

bool X11Window::HandleAsAtkEvent(XEvent* xev) {
#if !BUILDFLAG(USE_ATK)
  // TODO(crbug.com/1014934): Support ATK in Ozone/X11.
  NOTREACHED();
  return false;
#else
  DCHECK(xev);
  if (!x11_extension_delegate_ ||
      (xev->type != KeyPress && xev->type != KeyRelease))
    return false;
  auto atk_key_event = AtkKeyEventFromXEvent(xev);
  return x11_extension_delegate_->OnAtkKeyEvent(atk_key_event.get());
#endif
}

// CheckCanDispatchNextPlatformEvent is called by X11EventSourceLibevent to
// determine whether X11Window instance (XEventDispatcher implementation) is
// able to process next translated event sent by it. So, it's done through
// |handle_next_event_| internal flag, used in subsequent CanDispatchEvent
// call.
void X11Window::CheckCanDispatchNextPlatformEvent(XEvent* xev) {
  if (is_shutting_down_)
    return;
  current_xevent_ = XWindow::IsTargetedBy(*xev) ? xev : nullptr;
}

void X11Window::PlatformEventDispatchFinished() {
  current_xevent_ = nullptr;
}

PlatformEventDispatcher* X11Window::GetPlatformEventDispatcher() {
  return this;
}

bool X11Window::DispatchXEvent(XEvent* xev) {
  if (!XWindow::IsTargetedBy(*xev))
    return false;
  XWindow::ProcessEvent(xev);
  return true;
}

bool X11Window::CanDispatchEvent(const PlatformEvent& xev) {
  DCHECK_NE(window(), x11::None);
  return !!current_xevent_;
}

uint32_t X11Window::DispatchEvent(const PlatformEvent& event) {
  TRACE_EVENT1("views", "X11PlatformWindow::Dispatch", "event->type()",
               event->type());

  DCHECK_NE(window(), x11::None);
  DCHECK(event);
  DCHECK(current_xevent_);

  if (event->IsMouseEvent())
    X11WindowManager::GetInstance()->MouseOnWindow(this);
#if BUILDFLAG(USE_ATK)
  // TODO(crbug.com/1014934): Support ATK in Ozone/X11.
  if (HandleAsAtkEvent(current_xevent_))
    return POST_DISPATCH_STOP_PROPAGATION;
#endif

  DispatchUiEvent(event, current_xevent_);
  return POST_DISPATCH_STOP_PROPAGATION;
}

void X11Window::DispatchUiEvent(ui::Event* event, XEvent* xev) {
  auto* window_manager = X11WindowManager::GetInstance();
  DCHECK(window_manager);

  if (DispatchDraggingUiEvent(event))
    return;

  // Process X11-specific bits
  if (XWindow::IsTargetedBy(*xev))
    XWindow::ProcessEvent(xev);

  // If |event| is a located event (mouse, touch, etc) and another X11 window
  // is set as the current located events grabber, the |event| must be
  // re-routed to that grabber. Otherwise, just send the event.
  auto* located_events_grabber = window_manager->located_events_grabber();
  if (event->IsLocatedEvent() && located_events_grabber &&
      located_events_grabber != this) {
    if (event->IsMouseEvent() ||
        (event->IsTouchEvent() && event->type() == ui::ET_TOUCH_PRESSED)) {
      // Another X11Window has installed itself as capture. Translate the
      // event's location and dispatch to the other.
      ConvertEventLocationToTargetLocation(located_events_grabber->GetBounds(),
                                           GetBounds(),
                                           event->AsLocatedEvent());
    }
    return located_events_grabber->DispatchUiEvent(event, xev);
  }

  XEvent last_xev;
  std::unique_ptr<ui::Event> last_motion;
  bool coalesced = CoalesceEventsIfNeeded(xev, event->type(), &last_xev);
  if (coalesced) {
    last_motion = ui::BuildEventFromXEvent(last_xev);
    event = last_motion.get();
  }

  // If after CoalescePendingMotionEvents the type of xev is resolved to
  // UNKNOWN, i.e: xevent translation returns nullptr, don't dispatch the
  // event. TODO(804418): investigate why ColescePendingMotionEvents can
  // include mouse wheel events as well. Investigation showed that events on
  // Linux are checked with cmt-device path, and can include DT_CMT_SCROLL_
  // data. See more discussion in https://crrev.com/c/853953
  if (event) {
    XWindow::UpdateWMUserTime(event);
#if defined(USE_OZONE)
    DispatchEventFromNativeUiEvent(
        event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                              base::Unretained(platform_window_delegate())));
#else
    platform_window_delegate_->DispatchEvent(event);
#endif
  }

  if (coalesced)
    XFreeEventData(last_xev.xgeneric.display, &last_xev.xcookie);
}

void X11Window::OnXWindowCreated() {
  X11WindowManager::GetInstance()->AddWindow(this);

  DCHECK(X11EventSource::HasInstance());
  X11EventSource::GetInstance()->AddXEventDispatcher(this);

  x11_window_move_client_ =
      std::make_unique<ui::X11DesktopWindowMoveClient>(this);

  // Set a class property key, which allows |this| to be used for move loop aka
  // tab dragging.
  SetWmMoveLoopHandler(this, static_cast<WmMoveLoopHandler*>(this));

  platform_window_delegate_->OnAcceleratedWidgetAvailable(GetWidget());
}

bool X11Window::DispatchDraggingUiEvent(ui::Event* event) {
  return false;
}

void X11Window::OnXWindowStateChanged() {
  // Determine the new window state information to be propagated to the client.
  // Note that the order of checks is important here, because window can have
  // several properties at the same time.
  auto new_state = PlatformWindowState::kNormal;
  if (IsMinimized())
    new_state = PlatformWindowState::kMinimized;
  else if (IsFullscreen())
    new_state = PlatformWindowState::kFullScreen;
  else if (IsMaximized())
    new_state = PlatformWindowState::kMaximized;

  // fullscreen state is set syschronously at ToggleFullscreen() and must be
  // kept and propagated to the client only when explicitly requested by upper
  // layers, as it means we are in "browser fullscreen mode" (where
  // decorations, omnibar, buttons, etc are hidden), which is different from
  // the case where the request comes from the window manager (or any other
  // process), handled by this method. In this case, we follow EWMH guidelines:
  // Optimize the whole application for fullscreen usage. Window decorations
  // (e.g. borders) should be hidden, but the functionalily of the application
  // should not change. Further details:
  // https://specifications.freedesktop.org/wm-spec/wm-spec-1.3.html
  bool browser_fullscreen_mode = state_ == PlatformWindowState::kFullScreen;
  bool window_fullscreen_mode = new_state == PlatformWindowState::kFullScreen;
  // So, we ignore fullscreen state transitions in 2 cases:
  // 1. If |new_state| is kFullScreen but |state_| is not, which means the
  // fullscreen request is coming from an external process. So the browser
  // window must occupies the entire screen but not transitioning to browser
  // fullscreen mode.
  // 2. if |state_| is kFullScreen but |new_state| is not, we have been
  // requested to exit fullscreen by other process (e.g: via WM keybinding),
  // in this case we must keep on "browser fullscreen mode" bug the platform
  // window gets back to its previous state (e.g: unmaximized, tiled in TWMs,
  // etc).
  if (window_fullscreen_mode != browser_fullscreen_mode)
    return;

  if (GetRestoredBoundsInPixels().IsEmpty()) {
    if (IsMaximized()) {
      // The request that we become maximized originated from a different
      // process. |bounds_in_pixels_| already contains our maximized bounds. Do
      // a best effort attempt to get restored bounds by setting it to our
      // previously set bounds (and if we get this wrong, we aren't any worse
      // off since we'd otherwise be returning our maximized bounds).
      SetRestoredBoundsInPixels(previous_bounds());
    }
  } else if (!IsMaximized() && !IsFullscreen()) {
    // If we have restored bounds, but WM_STATE no longer claims to be
    // maximized or fullscreen, we should clear our restored bounds.
    SetRestoredBoundsInPixels(gfx::Rect());
  }

  if (new_state != state_) {
    state_ = new_state;
    platform_window_delegate_->OnWindowStateChanged(state_);
  }
}

void X11Window::OnXWindowDamageEvent(const gfx::Rect& damage_rect) {
  platform_window_delegate_->OnDamageRect(damage_rect);
}

void X11Window::OnXWindowBoundsChanged(const gfx::Rect& bounds) {
  platform_window_delegate_->OnBoundsChanged(bounds);
}

void X11Window::OnXWindowCloseRequested() {
  platform_window_delegate_->OnCloseRequest();
}

void X11Window::OnXWindowIsActiveChanged(bool active) {
  platform_window_delegate_->OnActivationChanged(active);
}

void X11Window::OnXWindowWorkspaceChanged() {
  if (workspace_extension_delegate_)
    workspace_extension_delegate_->OnWorkspaceChanged();
}

void X11Window::OnXWindowLostPointerGrab() {
  if (x11_extension_delegate_)
    x11_extension_delegate_->OnLostMouseGrab();
}

void X11Window::OnXWindowSelectionEvent(XEvent* xev) {
  if (x_event_delegate_)
    x_event_delegate_->OnXWindowSelectionEvent(xev);
}

void X11Window::OnXWindowDragDropEvent(XEvent* xev) {
  if (x_event_delegate_)
    x_event_delegate_->OnXWindowDragDropEvent(xev);
}

base::Optional<gfx::Size> X11Window::GetMinimumSizeForXWindow() {
  return platform_window_delegate_->GetMinimumSizeForWindow();
}

base::Optional<gfx::Size> X11Window::GetMaximumSizeForXWindow() {
  return platform_window_delegate_->GetMaximumSizeForWindow();
}

void X11Window::GetWindowMaskForXWindow(const gfx::Size& size,
                                        SkPath* window_mask) {
  if (x11_extension_delegate_)
    x11_extension_delegate_->GetWindowMask(size, window_mask);
}

void X11Window::DispatchHostWindowDragMovement(
    int hittest,
    const gfx::Point& pointer_location_in_px) {
  XWindow::WmMoveResize(hittest, pointer_location_in_px);
}

bool X11Window::RunMoveLoop(const gfx::Vector2d& drag_offset) {
  return x11_window_move_client_->RunMoveLoop(!HasCapture(), drag_offset);
}

void X11Window::EndMoveLoop() {
  x11_window_move_client_->EndMoveLoop();
}

gfx::Size X11Window::AdjustSizeForDisplay(
    const gfx::Size& requested_size_in_pixels) {
#if defined(OS_CHROMEOS)
  // We do not need to apply the workaround for the ChromeOS.
  return requested_size_in_pixels;
#else
  auto* screen = display::Screen::GetScreen();
  if (screen && !UseTestConfigForPlatformWindows()) {
    std::vector<display::Display> displays = screen->GetAllDisplays();
    // Compare against all monitor sizes. The window manager can move the window
    // to whichever monitor it wants.
    for (const auto& display : displays) {
      if (requested_size_in_pixels == display.GetSizeInPixel()) {
        return gfx::Size(requested_size_in_pixels.width() - 1,
                         requested_size_in_pixels.height() - 1);
      }
    }
  }

  // Do not request a 0x0 window size. It causes an XError.
  gfx::Size size_in_pixels = requested_size_in_pixels;
  size_in_pixels.SetToMax(gfx::Size(1, 1));
  return size_in_pixels;
#endif
}

void X11Window::ConvertEventLocationToTargetLocation(
    const gfx::Rect& target_window_bounds,
    const gfx::Rect& current_window_bounds,
    ui::LocatedEvent* located_event) {
  // TODO(msisov): for ozone, we need to access PlatformScreen instead and get
  // the displays.
  auto* display = display::Screen::GetScreen();
  DCHECK(display);
  auto display_window_target =
      display->GetDisplayMatching(target_window_bounds);
  auto display_window_current =
      display->GetDisplayMatching(current_window_bounds);
  DCHECK_EQ(display_window_target.device_scale_factor(),
            display_window_current.device_scale_factor());

  ConvertEventLocationToTargetWindowLocation(target_window_bounds.origin(),
                                             current_window_bounds.origin(),
                                             located_event);
}

}  // namespace ui
