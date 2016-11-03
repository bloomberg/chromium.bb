// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/desktop_window_tree_host_mus.h"

#include "base/memory/ptr_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace views {

DesktopWindowTreeHostMus::DesktopWindowTreeHostMus(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : aura::WindowTreeHostMus(MusClient::Get()->window_tree_client()),
      native_widget_delegate_(native_widget_delegate),
      desktop_native_widget_aura_(desktop_native_widget_aura),
      fullscreen_restore_state_(ui::SHOW_STATE_DEFAULT),
      close_widget_factory_(this) {
  // TODO: use display id and bounds if available, likely need to pass in
  // InitParams for that.
}

DesktopWindowTreeHostMus::~DesktopWindowTreeHostMus() {
  desktop_native_widget_aura_->OnDesktopWindowTreeHostDestroyed(this);
}

bool DesktopWindowTreeHostMus::IsDocked() const {
  return window()->GetProperty(aura::client::kShowStateKey) ==
         ui::SHOW_STATE_DOCKED;
}

void DesktopWindowTreeHostMus::Init(aura::Window* content_window,
                                    const Widget::InitParams& params) {}

void DesktopWindowTreeHostMus::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  native_widget_delegate_->OnNativeWidgetCreated(true);
}

std::unique_ptr<corewm::Tooltip> DesktopWindowTreeHostMus::CreateTooltip() {
  return base::MakeUnique<corewm::TooltipAura>();
}

std::unique_ptr<aura::client::DragDropClient>
DesktopWindowTreeHostMus::CreateDragDropClient(
    DesktopNativeCursorManager* cursor_manager) {
  // aura-mus handles installing a DragDropClient.
  return nullptr;
}

void DesktopWindowTreeHostMus::Close() {
  if (close_widget_factory_.HasWeakPtrs())
    return;

  // Close doesn't delete this immediately, as 'this' may still be on the stack
  // resulting in possible crashes when the stack unwindes.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DesktopWindowTreeHostMus::CloseNow,
                            close_widget_factory_.GetWeakPtr()));
}

void DesktopWindowTreeHostMus::CloseNow() {
  native_widget_delegate_->OnNativeWidgetDestroying();
  DestroyCompositor();
  desktop_native_widget_aura_->OnHostClosed();
}

aura::WindowTreeHost* DesktopWindowTreeHostMus::AsWindowTreeHost() {
  return this;
}

void DesktopWindowTreeHostMus::ShowWindowWithState(ui::WindowShowState state) {
  if (state == ui::SHOW_STATE_MAXIMIZED || state == ui::SHOW_STATE_FULLSCREEN ||
      state == ui::SHOW_STATE_DOCKED) {
    window()->SetProperty(aura::client::kShowStateKey, state);
  }
  window()->Show();
  if (compositor())
    compositor()->SetVisible(true);
  // TODO: likely needs code to activate, but after content_window_ is shown.
  // See NativeWidgetAura::ShowWithWindowState().
}

void DesktopWindowTreeHostMus::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  window()->SetProperty(aura::client::kRestoreBoundsKey,
                        new gfx::Rect(restored_bounds));
  ShowWindowWithState(ui::SHOW_STATE_MAXIMIZED);
}

bool DesktopWindowTreeHostMus::IsVisible() const {
  return window()->IsVisible();
}

void DesktopWindowTreeHostMus::SetSize(const gfx::Size& size) {
  window()->SetBounds(gfx::Rect(window()->bounds().origin(), size));
}

void DesktopWindowTreeHostMus::StackAbove(aura::Window* window) {
  NOTIMPLEMENTED();
}

void DesktopWindowTreeHostMus::StackAtTop() {
  NOTIMPLEMENTED();
}

void DesktopWindowTreeHostMus::CenterWindow(const gfx::Size& size) {
  NOTIMPLEMENTED();
}
void DesktopWindowTreeHostMus::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  NOTIMPLEMENTED();
}
gfx::Rect DesktopWindowTreeHostMus::GetWindowBoundsInScreen() const {
  // TODO: convert to dips.
  return GetBounds();
}

gfx::Rect DesktopWindowTreeHostMus::GetClientAreaBoundsInScreen() const {
  // View-to-screen coordinate system transformations depend on this returning
  // the full window bounds, for example View::ConvertPointToScreen().
  return GetWindowBoundsInScreen();
}

gfx::Rect DesktopWindowTreeHostMus::GetRestoredBounds() const {
  // Restored bounds should only be relevant if the window is minimized,
  // maximized, fullscreen or docked. However, in some places the code expects
  // GetRestoredBounds() to return the current window bounds if the window is
  // not in either state.
  if (IsMinimized() || IsMaximized() || IsFullscreen()) {
    // Restore bounds are in screen coordinates, no need to convert.
    gfx::Rect* restore_bounds =
        window()->GetProperty(aura::client::kRestoreBoundsKey);
    if (restore_bounds)
      return *restore_bounds;
  }
  gfx::Rect bounds = GetWindowBoundsInScreen();
  if (IsDocked()) {
    // Restore bounds are in screen coordinates, no need to convert.
    gfx::Rect* restore_bounds =
        window()->GetProperty(aura::client::kRestoreBoundsKey);
    // Use current window horizontal offset origin in order to preserve docked
    // alignment but preserve restored size and vertical offset for the time
    // when the |window_| gets undocked.
    if (restore_bounds) {
      bounds.set_size(restore_bounds->size());
      bounds.set_y(restore_bounds->y());
    }
  }
  return bounds;
}

std::string DesktopWindowTreeHostMus::GetWorkspace() const {
  // Only used on x11.
  return std::string();
}

gfx::Rect DesktopWindowTreeHostMus::GetWorkAreaBoundsInScreen() const {
  // TODO(sky): GetDisplayNearestWindow() should take a const aura::Window*.
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(const_cast<aura::Window*>(window()))
      .work_area();
}

void DesktopWindowTreeHostMus::SetShape(
    std::unique_ptr<SkRegion> native_region) {
  NOTIMPLEMENTED();
}

void DesktopWindowTreeHostMus::Activate() {
  window()->Focus();
  if (window()->GetProperty(aura::client::kDrawAttentionKey))
    window()->SetProperty(aura::client::kDrawAttentionKey, false);
}

void DesktopWindowTreeHostMus::Deactivate() {
  aura::client::GetActivationClient(window()->GetRootWindow())
      ->DeactivateWindow(window());
}

bool DesktopWindowTreeHostMus::IsActive() const {
  return wm::IsActiveWindow(const_cast<aura::Window*>(window()));
}

void DesktopWindowTreeHostMus::Maximize() {
  window()->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
}
void DesktopWindowTreeHostMus::Minimize() {
  window()->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
}

void DesktopWindowTreeHostMus::Restore() {
  window()->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
}

bool DesktopWindowTreeHostMus::IsMaximized() const {
  return window()->GetProperty(aura::client::kShowStateKey) ==
         ui::SHOW_STATE_MAXIMIZED;
}

bool DesktopWindowTreeHostMus::IsMinimized() const {
  return window()->GetProperty(aura::client::kShowStateKey) ==
         ui::SHOW_STATE_MINIMIZED;
}

bool DesktopWindowTreeHostMus::HasCapture() const {
  return const_cast<aura::Window*>(window())->HasCapture();
}

void DesktopWindowTreeHostMus::SetAlwaysOnTop(bool always_on_top) {
  window()->SetProperty(aura::client::kAlwaysOnTopKey, always_on_top);
}

bool DesktopWindowTreeHostMus::IsAlwaysOnTop() const {
  return window()->GetProperty(aura::client::kAlwaysOnTopKey);
}

void DesktopWindowTreeHostMus::SetVisibleOnAllWorkspaces(bool always_visible) {
  // Not applicable to chromeos.
}

bool DesktopWindowTreeHostMus::IsVisibleOnAllWorkspaces() const {
  return false;
}

bool DesktopWindowTreeHostMus::SetWindowTitle(const base::string16& title) {
  if (window()->title() == title)
    return false;
  window()->SetTitle(title);
  return true;
}

void DesktopWindowTreeHostMus::ClearNativeFocus() {
  aura::client::FocusClient* client = aura::client::GetFocusClient(window());
  if (client && window()->Contains(client->GetFocusedWindow()))
    client->ResetFocusWithinActiveWindow(window());
}

Widget::MoveLoopResult DesktopWindowTreeHostMus::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  NOTIMPLEMENTED();
  return Widget::MOVE_LOOP_CANCELED;
}

void DesktopWindowTreeHostMus::EndMoveLoop() {
  NOTIMPLEMENTED();
}

void DesktopWindowTreeHostMus::SetVisibilityChangedAnimationsEnabled(
    bool value) {
  window()->SetProperty(aura::client::kAnimationsDisabledKey, !value);
}

bool DesktopWindowTreeHostMus::ShouldUseNativeFrame() const {
  return false;
}

bool DesktopWindowTreeHostMus::ShouldWindowContentsBeTransparent() const {
  return false;
}

void DesktopWindowTreeHostMus::FrameTypeChanged() {}

void DesktopWindowTreeHostMus::SetFullscreen(bool fullscreen) {
  if (IsFullscreen() == fullscreen)
    return;  // Nothing to do.

  // Save window state before entering full screen so that it could restored
  // when exiting full screen.
  if (fullscreen) {
    fullscreen_restore_state_ =
        window()->GetProperty(aura::client::kShowStateKey);
  }

  window()->SetProperty(
      aura::client::kShowStateKey,
      fullscreen ? ui::SHOW_STATE_FULLSCREEN : fullscreen_restore_state_);
}

bool DesktopWindowTreeHostMus::IsFullscreen() const {
  return window()->GetProperty(aura::client::kShowStateKey) ==
         ui::SHOW_STATE_FULLSCREEN;
}
void DesktopWindowTreeHostMus::SetOpacity(float opacity) {
  // TODO: this likely need to go to server so that non-client decorations get
  // opacity.
  window()->layer()->SetOpacity(opacity);
}

void DesktopWindowTreeHostMus::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                              const gfx::ImageSkia& app_icon) {
  if (window_icon.isNull() && app_icon.isNull()) {
    window()->ClearProperty(aura::client::kWindowIconKey);
    return;
  }

  window()->SetProperty(
      aura::client::kWindowIconKey,
      new gfx::ImageSkia(!window_icon.isNull() ? window_icon : app_icon));
}

void DesktopWindowTreeHostMus::InitModalType(ui::ModalType modal_type) {
  window()->SetProperty(aura::client::kModalKey, modal_type);
}

void DesktopWindowTreeHostMus::FlashFrame(bool flash_frame) {
  window()->SetProperty(aura::client::kDrawAttentionKey, flash_frame);
}

void DesktopWindowTreeHostMus::OnRootViewLayout() {}

bool DesktopWindowTreeHostMus::IsAnimatingClosed() const {
  return false;
}

bool DesktopWindowTreeHostMus::IsTranslucentWindowOpacitySupported() const {
  return true;
}

void DesktopWindowTreeHostMus::SizeConstraintsChanged() {
  Widget* widget = native_widget_delegate_->AsWidget();
  window()->SetProperty(aura::client::kCanMaximizeKey,
                        widget->widget_delegate()->CanMaximize());
  window()->SetProperty(aura::client::kCanMinimizeKey,
                        widget->widget_delegate()->CanMinimize());
  window()->SetProperty(aura::client::kCanResizeKey,
                        widget->widget_delegate()->CanResize());
}

}  // namespace views
