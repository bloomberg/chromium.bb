// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/base/hit_test.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_handler/wm_move_resize_handler.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/window_event_filter.h"
#include "ui/views/widget/widget_aura_utils.h"
#include "ui/views/window/native_frame_view.h"
#include "ui/wm/core/window_util.h"

namespace views {

namespace {

ui::PlatformWindowInitProperties ConvertWidgetInitParamsToInitProperties(
    const Widget::InitParams& params) {
  ui::PlatformWindowInitProperties properties;

  switch (params.type) {
    case Widget::InitParams::TYPE_WINDOW:
      properties.type = ui::PlatformWindowType::kWindow;
      break;

    case Widget::InitParams::TYPE_MENU:
      properties.type = ui::PlatformWindowType::kMenu;
      break;

    case Widget::InitParams::TYPE_TOOLTIP:
      properties.type = ui::PlatformWindowType::kTooltip;
      break;

    case Widget::InitParams::TYPE_DRAG:
      properties.type = ui::PlatformWindowType::kDrag;
      break;

    case Widget::InitParams::TYPE_BUBBLE:
      properties.type = ui::PlatformWindowType::kBubble;
      break;

    default:
      properties.type = ui::PlatformWindowType::kPopup;
      break;
  }

  properties.bounds = params.bounds;
  properties.activatable =
      params.activatable == Widget::InitParams::ACTIVATABLE_YES;
  properties.force_show_in_taskbar = params.force_show_in_taskbar;
  properties.keep_on_top =
      params.EffectiveZOrderLevel() != ui::ZOrderLevel::kNormal;
  properties.visible_on_all_workspaces = params.visible_on_all_workspaces;
  properties.remove_standard_frame = params.remove_standard_frame;
  properties.workspace = params.workspace;

  if (params.parent && params.parent->GetHost())
    properties.parent_widget = params.parent->GetHost()->GetAcceleratedWidget();

  switch (params.opacity) {
    case Widget::InitParams::WindowOpacity::INFER_OPACITY:
      properties.opacity = ui::PlatformWindowOpacity::kInferOpacity;
      break;
    case Widget::InitParams::WindowOpacity::OPAQUE_WINDOW:
      properties.opacity = ui::PlatformWindowOpacity::kOpaqueWindow;
      break;
    case Widget::InitParams::WindowOpacity::TRANSLUCENT_WINDOW:
      properties.opacity = ui::PlatformWindowOpacity::kTranslucentWindow;
      break;
  }

  return properties;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostPlatform:

DesktopWindowTreeHostPlatform::DesktopWindowTreeHostPlatform(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : native_widget_delegate_(native_widget_delegate),
      desktop_native_widget_aura_(desktop_native_widget_aura) {}

DesktopWindowTreeHostPlatform::~DesktopWindowTreeHostPlatform() {
// TODO(msisov): Once destruction goes from DWTHX11 to DWTHPlatform, remove this
// guard.
#if !defined(USE_X11)
  DCHECK(got_on_closed_);
  desktop_native_widget_aura_->OnDesktopWindowTreeHostDestroyed(this);
  DestroyDispatcher();
#endif
}

void DesktopWindowTreeHostPlatform::Init(const Widget::InitParams& params) {
  if (params.type == Widget::InitParams::TYPE_WINDOW)
    content_window()->SetProperty(aura::client::kAnimationsDisabledKey, true);

  ui::PlatformWindowInitProperties properties =
      ConvertWidgetInitParamsToInitProperties(params);
  AddAdditionalInitProperties(params, &properties);

  CreateAndSetPlatformWindow(std::move(properties));
  // Disable compositing on tooltips as a workaround for
  // https://crbug.com/442111.
  CreateCompositor(viz::FrameSinkId(),
                   params.force_software_compositing ||
                       params.type == Widget::InitParams::TYPE_TOOLTIP);

  WindowTreeHost::OnAcceleratedWidgetAvailable();
  InitHost();
  window()->Show();
}

void DesktopWindowTreeHostPlatform::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  native_widget_delegate_->OnNativeWidgetCreated();

  platform_window()->SetUseNativeFrame(params.type ==
                                           Widget::InitParams::TYPE_WINDOW &&
                                       !params.remove_standard_frame);

#if defined(OS_LINUX)
  // Setup a non_client_window_event_filter, which handles resize/move, double
  // click and other events.
  DCHECK(!non_client_window_event_filter_);
  std::unique_ptr<WindowEventFilter> window_event_filter =
      std::make_unique<WindowEventFilter>(this);
  auto* wm_move_resize_handler = GetWmMoveResizeHandler(*platform_window());
  if (wm_move_resize_handler)
    window_event_filter->SetWmMoveResizeHandler(
        GetWmMoveResizeHandler(*(platform_window())));

  non_client_window_event_filter_ = std::move(window_event_filter);
  window()->AddPreTargetHandler(non_client_window_event_filter_.get());
#endif
}

void DesktopWindowTreeHostPlatform::OnWidgetInitDone() {}

void DesktopWindowTreeHostPlatform::OnActiveWindowChanged(bool active) {}

std::unique_ptr<corewm::Tooltip>
DesktopWindowTreeHostPlatform::CreateTooltip() {
  return std::make_unique<corewm::TooltipAura>();
}

std::unique_ptr<aura::client::DragDropClient>
DesktopWindowTreeHostPlatform::CreateDragDropClient(
    DesktopNativeCursorManager* cursor_manager) {
#if !defined(USE_X11)
  ui::WmDragHandler* drag_handler = ui::GetWmDragHandler(*(platform_window()));
  std::unique_ptr<DesktopDragDropClientOzone> drag_drop_client =
      std::make_unique<DesktopDragDropClientOzone>(window(), cursor_manager,
                                                   drag_handler);
  // Set a class property key, which allows |drag_drop_client| to be used for
  // drop action.
  SetWmDropHandler(platform_window(), drag_drop_client.get());
  return std::move(drag_drop_client);
#else
  // TODO(https://crbug.com/990756): Move the X11 initialization of dnd here.
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
#endif
}

void DesktopWindowTreeHostPlatform::Close() {
  if (waiting_for_close_now_)
    return;

  desktop_native_widget_aura_->content_window()->Hide();

  // Hide while waiting for the close.
  // Please note that it's better to call WindowTreeHost::Hide, which also calls
  // PlatformWindow::Hide and Compositor::SetVisible(false).
  Hide();

  waiting_for_close_now_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DesktopWindowTreeHostPlatform::CloseNow,
                                weak_factory_.GetWeakPtr()));
}

void DesktopWindowTreeHostPlatform::CloseNow() {
  auto weak_ref = weak_factory_.GetWeakPtr();
  SetWmDropHandler(platform_window(), nullptr);
  // Deleting the PlatformWindow may not result in OnClosed() being called, if
  // not behave as though it was.
  SetPlatformWindow(nullptr);
  if (!weak_ref || got_on_closed_)
    return;

  RemoveNonClientEventFilter();

  native_widget_delegate_->OnNativeWidgetDestroying();

  got_on_closed_ = true;
  desktop_native_widget_aura_->OnHostClosed();
}

aura::WindowTreeHost* DesktopWindowTreeHostPlatform::AsWindowTreeHost() {
  return this;
}

void DesktopWindowTreeHostPlatform::Show(ui::WindowShowState show_state,
                                         const gfx::Rect& restore_bounds) {
  if (show_state == ui::SHOW_STATE_MAXIMIZED && !restore_bounds.IsEmpty())
    platform_window()->SetRestoredBoundsInPixels(ToPixelRect(restore_bounds));

  if (compositor()) {
    platform_window()->Show();
    compositor()->SetVisible(true);
  }

  switch (show_state) {
    case ui::SHOW_STATE_MAXIMIZED:
      platform_window()->Maximize();
      break;
    case ui::SHOW_STATE_MINIMIZED:
      platform_window()->Minimize();
      break;
    case ui::SHOW_STATE_FULLSCREEN:
      // TODO(sky): this isn't necessarily the same as explicitly setting
      // fullscreen.
      platform_window()->ToggleFullscreen();
      break;
    default:
      break;
  }

  if (native_widget_delegate_->CanActivate()) {
    if (show_state != ui::SHOW_STATE_INACTIVE)
      Activate();

    // SetInitialFocus() should be always be called, even for
    // SHOW_STATE_INACTIVE. If the window has to stay inactive, the method will
    // do the right thing.
    // Activate() might fail if the window is non-activatable. In this case, we
    // should pass SHOW_STATE_INACTIVE to SetInitialFocus() to stop the initial
    // focused view from getting focused. See https://crbug.com/515594 for
    // example.
    native_widget_delegate_->SetInitialFocus(
        IsActive() ? show_state : ui::SHOW_STATE_INACTIVE);
  }

  desktop_native_widget_aura_->content_window()->Show();
}

bool DesktopWindowTreeHostPlatform::IsVisible() const {
  // TODO: needs PlatformWindow support.
  NOTIMPLEMENTED_LOG_ONCE();
  return true;
}

void DesktopWindowTreeHostPlatform::SetSize(const gfx::Size& size) {
  gfx::Rect screen_bounds =
      gfx::ConvertRectToDIP(device_scale_factor(), GetBoundsInPixels());
  screen_bounds.set_size(size);
  SetBoundsInDIP(screen_bounds);
}

void DesktopWindowTreeHostPlatform::StackAbove(aura::Window* window) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopWindowTreeHostPlatform::StackAtTop() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopWindowTreeHostPlatform::CenterWindow(const gfx::Size& size) {
  gfx::Size size_in_pixels = ToPixelRect(gfx::Rect(size)).size();
  gfx::Rect parent_bounds_in_pixels = ToPixelRect(GetWorkAreaBoundsInScreen());

  // If |window_|'s transient parent bounds are big enough to contain |size|,
  // use them instead.
  if (wm::GetTransientParent(content_window())) {
    gfx::Rect transient_parent_rect =
        wm::GetTransientParent(content_window())->GetBoundsInScreen();
    if (transient_parent_rect.height() >= size.height() &&
        transient_parent_rect.width() >= size.width()) {
      parent_bounds_in_pixels = ToPixelRect(transient_parent_rect);
    }
  }

  gfx::Rect window_bounds_in_pixels(
      parent_bounds_in_pixels.x() +
          (parent_bounds_in_pixels.width() - size_in_pixels.width()) / 2,
      parent_bounds_in_pixels.y() +
          (parent_bounds_in_pixels.height() - size_in_pixels.height()) / 2,
      size_in_pixels.width(), size_in_pixels.height());
  // Don't size the window bigger than the parent, otherwise the user may not be
  // able to close or move it.
  window_bounds_in_pixels.AdjustToFit(parent_bounds_in_pixels);

  SetBoundsInPixels(window_bounds_in_pixels);
}

void DesktopWindowTreeHostPlatform::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  NOTIMPLEMENTED_LOG_ONCE();
  *bounds = gfx::Rect(0, 0, 640, 840);
  *show_state = ui::SHOW_STATE_NORMAL;
}

gfx::Rect DesktopWindowTreeHostPlatform::GetWindowBoundsInScreen() const {
  return ToDIPRect(GetBoundsInPixels());
}

gfx::Rect DesktopWindowTreeHostPlatform::GetClientAreaBoundsInScreen() const {
  // Attempts to calculate the rect by asking the NonClientFrameView what it
  // thought its GetBoundsForClientView() were broke combobox drop down
  // placement.
  return GetWindowBoundsInScreen();
}

gfx::Rect DesktopWindowTreeHostPlatform::GetRestoredBounds() const {
  gfx::Rect restored_bounds = platform_window()->GetRestoredBoundsInPixels();
  // When window is resized, |restored bounds| is not set and empty.
  // If |restored bounds| is empty, it returns the current window size.
  gfx::Rect bounds =
      !restored_bounds.IsEmpty() ? restored_bounds : GetBoundsInPixels();
  return ToDIPRect(bounds);
}

std::string DesktopWindowTreeHostPlatform::GetWorkspace() const {
  return std::string();
}

gfx::Rect DesktopWindowTreeHostPlatform::GetWorkAreaBoundsInScreen() const {
  // TODO(sky): GetDisplayNearestWindow() should take a const aura::Window*.
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(const_cast<aura::Window*>(window()))
      .work_area();
}

void DesktopWindowTreeHostPlatform::SetShape(
    std::unique_ptr<Widget::ShapeRects> native_shape) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopWindowTreeHostPlatform::Activate() {
  platform_window()->Activate();
}

void DesktopWindowTreeHostPlatform::Deactivate() {
  ReleaseCapture();
  platform_window()->Deactivate();
}

bool DesktopWindowTreeHostPlatform::IsActive() const {
  return is_active_;
}

void DesktopWindowTreeHostPlatform::Maximize() {
  platform_window()->Maximize();
}

void DesktopWindowTreeHostPlatform::Minimize() {
  platform_window()->Minimize();
}

void DesktopWindowTreeHostPlatform::Restore() {
  platform_window()->Restore();
}

bool DesktopWindowTreeHostPlatform::IsMaximized() const {
  return platform_window()->GetPlatformWindowState() ==
         ui::PlatformWindowState::kMaximized;
}

bool DesktopWindowTreeHostPlatform::IsMinimized() const {
  return platform_window()->GetPlatformWindowState() ==
         ui::PlatformWindowState::kMinimized;
}

bool DesktopWindowTreeHostPlatform::HasCapture() const {
  return platform_window()->HasCapture();
}

void DesktopWindowTreeHostPlatform::SetZOrderLevel(ui::ZOrderLevel order) {
  // TODO: needs PlatformWindow support.
  NOTIMPLEMENTED_LOG_ONCE();
}

ui::ZOrderLevel DesktopWindowTreeHostPlatform::GetZOrderLevel() const {
  // TODO: needs PlatformWindow support.
  return ui::ZOrderLevel::kNormal;
}

void DesktopWindowTreeHostPlatform::SetVisibleOnAllWorkspaces(
    bool always_visible) {
  // TODO: needs PlatformWindow support.
  NOTIMPLEMENTED_LOG_ONCE();
}

bool DesktopWindowTreeHostPlatform::IsVisibleOnAllWorkspaces() const {
  // TODO: needs PlatformWindow support.
  return false;
}

bool DesktopWindowTreeHostPlatform::SetWindowTitle(
    const base::string16& title) {
  if (window_title_ == title)
    return false;

  window_title_ = title;
  platform_window()->SetTitle(window_title_);
  return true;
}

void DesktopWindowTreeHostPlatform::ClearNativeFocus() {
  // This method is weird and misnamed. Instead of clearing the native focus,
  // it sets the focus to our content_window(), which will trigger a cascade
  // of focus changes into views.
  if (content_window() && aura::client::GetFocusClient(content_window()) &&
      content_window()->Contains(
          aura::client::GetFocusClient(content_window())->GetFocusedWindow())) {
    aura::client::GetFocusClient(content_window())
        ->FocusWindow(content_window());
  }
}

Widget::MoveLoopResult DesktopWindowTreeHostPlatform::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  // TODO: needs PlatformWindow support.
  NOTIMPLEMENTED_LOG_ONCE();
  return Widget::MOVE_LOOP_CANCELED;
}

void DesktopWindowTreeHostPlatform::EndMoveLoop() {
  // TODO: needs PlatformWindow support.
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopWindowTreeHostPlatform::SetVisibilityChangedAnimationsEnabled(
    bool value) {
  // TODO: needs PlatformWindow support.
  NOTIMPLEMENTED_LOG_ONCE();
}

NonClientFrameView* DesktopWindowTreeHostPlatform::CreateNonClientFrameView() {
  return ShouldUseNativeFrame() ? new NativeFrameView(GetWidget()) : nullptr;
}

bool DesktopWindowTreeHostPlatform::ShouldUseNativeFrame() const {
  return false;
}

bool DesktopWindowTreeHostPlatform::ShouldWindowContentsBeTransparent() const {
  return false;
}

void DesktopWindowTreeHostPlatform::FrameTypeChanged() {
  Widget::FrameType new_type =
      native_widget_delegate_->AsWidget()->frame_type();
  if (new_type == Widget::FrameType::kDefault) {
    // The default is determined by Widget::InitParams::remove_standard_frame
    // and does not change.
    return;
  }
  platform_window()->SetUseNativeFrame(new_type ==
                                       Widget::FrameType::kForceNative);
  // Replace the frame and layout the contents. Even though we don't have a
  // swappable glass frame like on Windows, we still replace the frame because
  // the button assets don't update otherwise.
  native_widget_delegate_->AsWidget()->non_client_view()->UpdateFrame();
}

void DesktopWindowTreeHostPlatform::SetFullscreen(bool fullscreen) {
  if (IsFullscreen() != fullscreen)
    platform_window()->ToggleFullscreen();
}

bool DesktopWindowTreeHostPlatform::IsFullscreen() const {
  return platform_window()->GetPlatformWindowState() ==
         ui::PlatformWindowState::kFullScreen;
}

void DesktopWindowTreeHostPlatform::SetOpacity(float opacity) {
  // TODO: needs PlatformWindow support.
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopWindowTreeHostPlatform::SetAspectRatio(
    const gfx::SizeF& aspect_ratio) {
  // TODO: needs PlatformWindow support.
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopWindowTreeHostPlatform::SetWindowIcons(
    const gfx::ImageSkia& window_icon,
    const gfx::ImageSkia& app_icon) {
  // TODO: needs PlatformWindow support.
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopWindowTreeHostPlatform::InitModalType(ui::ModalType modal_type) {
  // TODO: needs PlatformWindow support (alternatively, remove as
  // DesktopWindowTreeHostX11 doesn't support at all).
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopWindowTreeHostPlatform::FlashFrame(bool flash_frame) {
  // TODO: needs PlatformWindow support.
  NOTIMPLEMENTED_LOG_ONCE();
}

bool DesktopWindowTreeHostPlatform::IsAnimatingClosed() const {
  // TODO: needs PlatformWindow support.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool DesktopWindowTreeHostPlatform::IsTranslucentWindowOpacitySupported()
    const {
  // TODO: needs PlatformWindow support.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void DesktopWindowTreeHostPlatform::SizeConstraintsChanged() {
  // TODO: needs PlatformWindow support.
  NOTIMPLEMENTED_LOG_ONCE();
}

bool DesktopWindowTreeHostPlatform::ShouldUpdateWindowTransparency() const {
  return false;
}

bool DesktopWindowTreeHostPlatform::ShouldUseDesktopNativeCursorManager()
    const {
  return true;
}

bool DesktopWindowTreeHostPlatform::ShouldCreateVisibilityController() const {
  return true;
}

gfx::Transform DesktopWindowTreeHostPlatform::GetRootTransform() const {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  if (IsVisible()) {
    display = display::Screen::GetScreen()->GetDisplayNearestWindow(
        GetWidget()->GetNativeWindow());
  }

  float scale = display.device_scale_factor();
  gfx::Transform transform;
  transform.Scale(scale, scale);
  return transform;
}

void DesktopWindowTreeHostPlatform::DispatchEvent(ui::Event* event) {
#if defined(USE_OZONE)
  // Make sure the |event| is marked as a non-client if it's a non-client
  // mouse down event. This is needed to make sure the WindowEventDispatcher
  // does not set a |mouse_pressed_handler_| for such events, because they are
  // not always followed with non-client mouse up events in case of
  // Ozone/Wayland or Ozone/X11.
  //
  // Also see the comment in WindowEventDispatcher::PreDispatchMouseEvent..
  aura::Window* content_window = desktop_native_widget_aura_->content_window();
  if (content_window && content_window->delegate()) {
    if (event->IsMouseEvent()) {
      ui::MouseEvent* mouse_event = event->AsMouseEvent();
      int flags = mouse_event->flags();
      int hit_test_code = content_window->delegate()->GetNonClientComponent(
          mouse_event->location());
      if (hit_test_code != HTCLIENT && hit_test_code != HTNOWHERE)
        flags |= ui::EF_IS_NON_CLIENT;
      mouse_event->set_flags(flags);
    }
  }
#endif

  WindowTreeHostPlatform::DispatchEvent(event);
}

void DesktopWindowTreeHostPlatform::OnClosed() {
  RemoveNonClientEventFilter();
  got_on_closed_ = true;
  desktop_native_widget_aura_->OnHostClosed();
}

void DesktopWindowTreeHostPlatform::OnWindowStateChanged(
    ui::PlatformWindowState new_state) {
  // Propagate minimization/restore to compositor to avoid drawing 'blank'
  // frames that could be treated as previews, which show content even if a
  // window is minimized.
  bool visible = new_state != ui::PlatformWindowState::kMinimized;
  if (visible != compositor()->IsVisible()) {
    compositor()->SetVisible(visible);
    native_widget_delegate_->OnNativeWidgetVisibilityChanged(visible);
  }

  // It might require relayouting when state property has been changed.
  if (visible)
    Relayout();
}

void DesktopWindowTreeHostPlatform::OnCloseRequest() {
  GetWidget()->Close();
}

void DesktopWindowTreeHostPlatform::OnActivationChanged(bool active) {
  is_active_ = active;
  aura::WindowTreeHostPlatform::OnActivationChanged(active);
  desktop_native_widget_aura_->HandleActivationChanged(active);
}

base::Optional<gfx::Size>
DesktopWindowTreeHostPlatform::GetMinimumSizeForWindow() {
  return ToPixelRect(gfx::Rect(native_widget_delegate()->GetMinimumSize()))
      .size();
}

base::Optional<gfx::Size>
DesktopWindowTreeHostPlatform::GetMaximumSizeForWindow() {
  return ToPixelRect(gfx::Rect(native_widget_delegate()->GetMaximumSize()))
      .size();
}

aura::Window* DesktopWindowTreeHostPlatform::content_window() {
  return desktop_native_widget_aura_->content_window();
}

gfx::Rect DesktopWindowTreeHostPlatform::ToDIPRect(
    const gfx::Rect& rect_in_pixels) const {
  gfx::RectF rect_in_dip = gfx::RectF(rect_in_pixels);
  GetRootTransform().TransformRectReverse(&rect_in_dip);
  return gfx::ToEnclosingRect(rect_in_dip);
}

gfx::Rect DesktopWindowTreeHostPlatform::ToPixelRect(
    const gfx::Rect& rect_in_dip) const {
  gfx::RectF rect_in_pixels = gfx::RectF(rect_in_dip);
  GetRootTransform().TransformRect(&rect_in_pixels);
  return gfx::ToEnclosingRect(rect_in_pixels);
}

void DesktopWindowTreeHostPlatform::Relayout() {
  Widget* widget = native_widget_delegate_->AsWidget();
  NonClientView* non_client_view = widget->non_client_view();
  // non_client_view may be NULL, especially during creation.
  if (non_client_view) {
    non_client_view->client_view()->InvalidateLayout();
    non_client_view->InvalidateLayout();
  }
}

void DesktopWindowTreeHostPlatform::RemoveNonClientEventFilter() {
#if defined(OS_LINUX)
  if (!non_client_window_event_filter_)
    return;

  window()->RemovePreTargetHandler(non_client_window_event_filter_.get());
  non_client_window_event_filter_.reset();
#endif
}

Widget* DesktopWindowTreeHostPlatform::GetWidget() {
  return native_widget_delegate_->AsWidget();
}

const Widget* DesktopWindowTreeHostPlatform::GetWidget() const {
  return native_widget_delegate_->AsWidget();
}

void DesktopWindowTreeHostPlatform::AddAdditionalInitProperties(
    const Widget::InitParams& params,
    ui::PlatformWindowInitProperties* properties) {}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHost:

// As DWTHX11 subclasses DWTHPlatform now (during transition period. see
// https://crbug.com/990756), we need to guard this factory method.
// TODO(msisov): remove this guard once DWTHX11 is finally merged into
// DWTHPlatform.
#if !defined(USE_X11)
// static
DesktopWindowTreeHost* DesktopWindowTreeHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  return new DesktopWindowTreeHostPlatform(native_widget_delegate,
                                           desktop_native_widget_aura);
}
#endif

}  // namespace views
