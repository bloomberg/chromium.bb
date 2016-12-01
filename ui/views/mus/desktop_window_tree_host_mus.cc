// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/desktop_window_tree_host_mus.h"

#include "base/memory/ptr_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/window_manager_frame_values.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/native_cursor_manager.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace views {

namespace {

// As the window manager renderers the non-client decorations this class does
// very little but honor the client area insets from the window manager.
class ClientSideNonClientFrameView : public NonClientFrameView {
 public:
  explicit ClientSideNonClientFrameView(views::Widget* widget)
      : widget_(widget) {}
  ~ClientSideNonClientFrameView() override {}

 private:
  // Returns the default values of client area insets from the window manager.
  static gfx::Insets GetDefaultWindowManagerInsets(bool is_maximized) {
    const WindowManagerFrameValues& values =
        WindowManagerFrameValues::instance();
    return is_maximized ? values.maximized_insets : values.normal_insets;
  }

  // NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override {
    gfx::Rect result(GetLocalBounds());
    if (widget_->IsFullscreen())
      return result;
    result.Inset(GetDefaultWindowManagerInsets(widget_->IsMaximized()));
    return result;
  }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    if (widget_->IsFullscreen())
      return client_bounds;

    const gfx::Insets insets(
        GetDefaultWindowManagerInsets(widget_->IsMaximized()));
    return gfx::Rect(client_bounds.x() - insets.left(),
                     client_bounds.y() - insets.top(),
                     client_bounds.width() + insets.width(),
                     client_bounds.height() + insets.height());
  }
  int NonClientHitTest(const gfx::Point& point) override { return HTNOWHERE; }
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override {
    // The window manager provides the shape; do nothing.
  }
  void ResetWindowControls() override {
    // TODO(sky): push to wm?
  }

  // These have no implementation. The Window Manager handles the actual
  // rendering of the icon/title. See NonClientFrameViewMash. The values
  // associated with these methods are pushed to the server by the way of
  // NativeWidgetMus functions.
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

  gfx::Size GetPreferredSize() const override {
    return widget_->non_client_view()
        ->GetWindowBoundsForClientBounds(
            gfx::Rect(widget_->client_view()->GetPreferredSize()))
        .size();
  }
  gfx::Size GetMinimumSize() const override {
    return widget_->non_client_view()
        ->GetWindowBoundsForClientBounds(
            gfx::Rect(widget_->client_view()->GetMinimumSize()))
        .size();
  }
  gfx::Size GetMaximumSize() const override {
    gfx::Size max_size = widget_->client_view()->GetMaximumSize();
    gfx::Size converted_size =
        widget_->non_client_view()
            ->GetWindowBoundsForClientBounds(gfx::Rect(max_size))
            .size();
    return gfx::Size(max_size.width() == 0 ? 0 : converted_size.width(),
                     max_size.height() == 0 ? 0 : converted_size.height());
  }

  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(ClientSideNonClientFrameView);
};

class NativeCursorManagerMus : public wm::NativeCursorManager {
 public:
  explicit NativeCursorManagerMus(aura::Window* window) : window_(window) {}
  ~NativeCursorManagerMus() override {}

  // wm::NativeCursorManager:
  void SetDisplay(const display::Display& display,
                  wm::NativeCursorManagerDelegate* delegate) override {
    // We ignore this entirely, as cursor are set on the client.
  }

  void SetCursor(gfx::NativeCursor cursor,
                 wm::NativeCursorManagerDelegate* delegate) override {
    aura::WindowPortMus::Get(window_)->SetPredefinedCursor(
        ui::mojom::Cursor(cursor.native_type()));
    delegate->CommitCursor(cursor);
  }

  void SetVisibility(bool visible,
                     wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitVisibility(visible);

    if (visible) {
      SetCursor(delegate->GetCursor(), delegate);
    } else {
      aura::WindowPortMus::Get(window_)->SetPredefinedCursor(
          ui::mojom::Cursor::NONE);
    }
  }

  void SetCursorSet(ui::CursorSetType cursor_set,
                    wm::NativeCursorManagerDelegate* delegate) override {
    // TODO(erg): For now, ignore the difference between SET_NORMAL and
    // SET_LARGE here. This feels like a thing that mus should decide instead.
    //
    // Also, it's NOTIMPLEMENTED() in the desktop version!? Including not
    // acknowledging the call in the delegate.
    NOTIMPLEMENTED();
  }

  void SetMouseEventsEnabled(
      bool enabled,
      wm::NativeCursorManagerDelegate* delegate) override {
    // TODO(erg): How do we actually implement this?
    //
    // Mouse event dispatch is potentially done in a different process,
    // definitely in a different mojo service. Each app is fairly locked down.
    delegate->CommitMouseEventsEnabled(enabled);
    NOTIMPLEMENTED();
  }

 private:
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(NativeCursorManagerMus);
};
}  // namespace

DesktopWindowTreeHostMus::DesktopWindowTreeHostMus(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura,
    const std::map<std::string, std::vector<uint8_t>>* mus_properties)
    : aura::WindowTreeHostMus(MusClient::Get()->window_tree_client(),
                              mus_properties),
      native_widget_delegate_(native_widget_delegate),
      desktop_native_widget_aura_(desktop_native_widget_aura),
      fullscreen_restore_state_(ui::SHOW_STATE_DEFAULT),
      close_widget_factory_(this) {
  aura::Env::GetInstance()->AddObserver(this);
  MusClient::Get()->AddObserver(this);
  // TODO: use display id and bounds if available, likely need to pass in
  // InitParams for that.
}

DesktopWindowTreeHostMus::~DesktopWindowTreeHostMus() {
  MusClient::Get()->RemoveObserver(this);
  aura::Env::GetInstance()->RemoveObserver(this);
  desktop_native_widget_aura_->OnDesktopWindowTreeHostDestroyed(this);
}

bool DesktopWindowTreeHostMus::IsDocked() const {
  return window()->GetProperty(aura::client::kShowStateKey) ==
         ui::SHOW_STATE_DOCKED;
}

void DesktopWindowTreeHostMus::SendClientAreaToServer() {
  NonClientView* non_client_view =
      native_widget_delegate_->AsWidget()->non_client_view();
  if (!non_client_view || !non_client_view->client_view())
    return;

  const gfx::Rect client_area_rect(non_client_view->client_view()->bounds());
  SetClientArea(gfx::Insets(
      client_area_rect.y(), client_area_rect.x(),
      non_client_view->bounds().height() - client_area_rect.bottom(),
      non_client_view->bounds().width() - client_area_rect.right()));
}

void DesktopWindowTreeHostMus::SendHitTestMaskToServer() {
  if (!native_widget_delegate_->HasHitTestMask()) {
    SetHitTestMask(base::nullopt);
    return;
  }

  gfx::Path mask_path;
  native_widget_delegate_->GetHitTestMask(&mask_path);
  // TODO(jamescook): Use the full path for the mask.
  gfx::Rect mask_rect =
      gfx::ToEnclosingRect(gfx::SkRectToRectF(mask_path.getBounds()));
  SetHitTestMask(mask_rect);
}

float DesktopWindowTreeHostMus::GetScaleFactor() const {
  // TODO(sky): GetDisplayNearestWindow() should take a const aura::Window*.
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(const_cast<aura::Window*>(window()))
      .device_scale_factor();
}

void DesktopWindowTreeHostMus::SetBoundsInDIP(const gfx::Rect& bounds_in_dip) {
  SetBoundsInPixels(gfx::ConvertRectToPixel(GetScaleFactor(), bounds_in_dip));
}

void DesktopWindowTreeHostMus::Init(aura::Window* content_window,
                                    const Widget::InitParams& params) {
  // Needed so we don't render over the non-client area the window manager
  // renders to.
  content_window->layer()->SetFillsBoundsOpaquely(false);
  if (!params.bounds.IsEmpty())
    SetBoundsInDIP(params.bounds);

  cursor_manager_ = base::MakeUnique<wm::CursorManager>(
      base::MakeUnique<NativeCursorManagerMus>(window()));
  aura::client::SetCursorClient(window(), cursor_manager_.get());
}

void DesktopWindowTreeHostMus::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  if (params.parent && params.parent->GetHost()) {
    parent_ = static_cast<DesktopWindowTreeHostMus*>(params.parent->GetHost());
    parent_->children_.insert(this);
  }
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

  // If we have children, close them. Use a copy for iteration because they'll
  // remove themselves from |children_|.
  std::set<DesktopWindowTreeHostMus*> children_copy = children_;
  for (DesktopWindowTreeHostMus* child : children_copy)
    child->CloseNow();
  DCHECK(children_.empty());

  if (parent_) {
    parent_->children_.erase(this);
    parent_ = nullptr;
  }

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

  if (native_widget_delegate_->CanActivate()) {
    if (state != ui::SHOW_STATE_INACTIVE)
      Activate();

    // SetInitialFocus() should be always be called, even for
    // SHOW_STATE_INACTIVE. If the window has to stay inactive, the method will
    // do the right thing.
    // Activate() might fail if the window is non-activatable. In this case, we
    // should pass SHOW_STATE_INACTIVE to SetInitialFocus() to stop the initial
    // focused view from getting focused. See crbug.com/515594 for example.
    native_widget_delegate_->SetInitialFocus(
        IsActive() ? state : ui::SHOW_STATE_INACTIVE);
  }
}

void DesktopWindowTreeHostMus::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  window()->SetProperty(aura::client::kRestoreBoundsKey,
                        new gfx::Rect(restored_bounds));
  ShowWindowWithState(ui::SHOW_STATE_MAXIMIZED);
}

bool DesktopWindowTreeHostMus::IsVisible() const {
  // Go through the DesktopNativeWidgetAura::IsVisible() for checking
  // visibility of the parent as it has additional checks beyond checking the
  // aura::Window.
  return window()->IsVisible() &&
         (!parent_ ||
          static_cast<const internal::NativeWidgetPrivate*>(
              parent_->desktop_native_widget_aura_)
              ->IsVisible());
}

void DesktopWindowTreeHostMus::SetSize(const gfx::Size& size) {
  // Use GetBounds() as the origin of window() is always at 0, 0.
  gfx::Rect screen_bounds =
      gfx::ConvertRectToDIP(GetScaleFactor(), GetBoundsInPixels());
  screen_bounds.set_size(size);
  SetBoundsInDIP(screen_bounds);
}

void DesktopWindowTreeHostMus::StackAbove(aura::Window* window) {
  // TODO: implement window stacking, http://crbug.com/663617.
  NOTIMPLEMENTED();
}

void DesktopWindowTreeHostMus::StackAtTop() {
  // TODO: implement window stacking, http://crbug.com/663617.
  NOTIMPLEMENTED();
}

void DesktopWindowTreeHostMus::CenterWindow(const gfx::Size& size) {
  gfx::Rect bounds_to_center_in = GetWorkAreaBoundsInScreen();

  // If there is a transient parent and it fits |size|, then center over it.
  aura::Window* content_window = desktop_native_widget_aura_->content_window();
  if (wm::GetTransientParent(content_window)) {
    gfx::Rect transient_parent_bounds =
        wm::GetTransientParent(content_window)->GetBoundsInScreen();
    if (transient_parent_bounds.height() >= size.height() &&
        transient_parent_bounds.width() >= size.width()) {
      bounds_to_center_in = transient_parent_bounds;
    }
  }

  gfx::Rect resulting_bounds(bounds_to_center_in);
  resulting_bounds.ClampToCenteredSize(size);
  SetBoundsInDIP(resulting_bounds);
}

void DesktopWindowTreeHostMus::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  // Implementation matches that of NativeWidgetAura.
  *bounds = GetRestoredBounds();
  *show_state = window()->GetProperty(aura::client::kShowStateKey);
}

gfx::Rect DesktopWindowTreeHostMus::GetWindowBoundsInScreen() const {
  return gfx::ConvertRectToDIP(GetScaleFactor(), GetBoundsInPixels());
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
  aura::Env::GetInstance()->SetActiveFocusClient(
      aura::client::GetFocusClient(window()), window());
  if (is_active_) {
    window()->Focus();
    if (window()->GetProperty(aura::client::kDrawAttentionKey))
      window()->SetProperty(aura::client::kDrawAttentionKey, false);
  }
}

void DesktopWindowTreeHostMus::Deactivate() {
  // TODO: Deactivate() means focus next window, that needs to go to mus.
  // http://crbug.com/663618.
  NOTIMPLEMENTED();
}

bool DesktopWindowTreeHostMus::IsActive() const {
  return is_active_;
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
  // Capture state is held by DesktopNativeWidgetAura::content_window_.
  // DesktopNativeWidgetAura::HasCapture() calls content_window_->HasCapture(),
  // and this. That means this function can always return true.
  return true;
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
  if (window()->GetTitle() == title)
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

NonClientFrameView* DesktopWindowTreeHostMus::CreateNonClientFrameView() {
  return new ClientSideNonClientFrameView(native_widget_delegate_->AsWidget());
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
  // opacity. http://crbug.com/663619.
  window()->layer()->SetOpacity(opacity);
}

void DesktopWindowTreeHostMus::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                              const gfx::ImageSkia& app_icon) {
  NativeWidgetAura::AssignIconToAuraWindow(window(), window_icon, app_icon);
}

void DesktopWindowTreeHostMus::InitModalType(ui::ModalType modal_type) {
  window()->SetProperty(aura::client::kModalKey, modal_type);
}

void DesktopWindowTreeHostMus::FlashFrame(bool flash_frame) {
  window()->SetProperty(aura::client::kDrawAttentionKey, flash_frame);
}

bool DesktopWindowTreeHostMus::IsAnimatingClosed() const {
  return false;
}

bool DesktopWindowTreeHostMus::IsTranslucentWindowOpacitySupported() const {
  return true;
}

void DesktopWindowTreeHostMus::SizeConstraintsChanged() {
  int32_t behavior = ui::mojom::kResizeBehaviorNone;
  Widget* widget = native_widget_delegate_->AsWidget();
  if (widget->widget_delegate())
    behavior = widget->widget_delegate()->GetResizeBehavior();
  window()->SetProperty(aura::client::kResizeBehaviorKey, behavior);
}

bool DesktopWindowTreeHostMus::ShouldUpdateWindowTransparency() const {
  // Needed so the window manager can render the client decorations.
  return false;
}

bool DesktopWindowTreeHostMus::ShouldUseDesktopNativeCursorManager() const {
  // We manage the cursor ourself.
  return false;
}

void DesktopWindowTreeHostMus::OnWindowManagerFrameValuesChanged() {
  NonClientView* non_client_view =
      native_widget_delegate_->AsWidget()->non_client_view();
  if (non_client_view) {
    non_client_view->Layout();
    non_client_view->SchedulePaint();
  }

  SendClientAreaToServer();
  SendHitTestMaskToServer();
}

void DesktopWindowTreeHostMus::ShowImpl() {
  native_widget_delegate_->OnNativeWidgetVisibilityChanging(true);
  // Using ui::SHOW_STATE_NORMAL matches that of DesktopWindowTreeHostX11.
  ShowWindowWithState(ui::SHOW_STATE_NORMAL);
  WindowTreeHostMus::ShowImpl();
  native_widget_delegate_->OnNativeWidgetVisibilityChanged(true);
}

void DesktopWindowTreeHostMus::HideImpl() {
  native_widget_delegate_->OnNativeWidgetVisibilityChanging(false);
  WindowTreeHostMus::HideImpl();
  native_widget_delegate_->OnNativeWidgetVisibilityChanged(false);
}

void DesktopWindowTreeHostMus::SetBoundsInPixels(
    const gfx::Rect& bounds_in_pixels) {
  gfx::Rect final_bounds_in_pixels = bounds_in_pixels;
  if (GetBoundsInPixels().size() != bounds_in_pixels.size()) {
    gfx::Size size = bounds_in_pixels.size();
    size.SetToMax(gfx::ConvertSizeToPixel(
        GetScaleFactor(), native_widget_delegate_->GetMinimumSize()));
    const gfx::Size max_size_in_pixels = gfx::ConvertSizeToPixel(
        GetScaleFactor(), native_widget_delegate_->GetMaximumSize());
    if (!max_size_in_pixels.IsEmpty())
      size.SetToMin(max_size_in_pixels);
    final_bounds_in_pixels.set_size(size);
  }
  const gfx::Rect old_bounds_in_pixels = GetBoundsInPixels();
  WindowTreeHostMus::SetBoundsInPixels(final_bounds_in_pixels);
  if (old_bounds_in_pixels.size() != final_bounds_in_pixels.size()) {
    SendClientAreaToServer();
    SendHitTestMaskToServer();
  }
}

void DesktopWindowTreeHostMus::OnWindowInitialized(aura::Window* window) {}

void DesktopWindowTreeHostMus::OnActiveFocusClientChanged(
    aura::client::FocusClient* focus_client,
    aura::Window* window) {
  if (window == this->window()) {
    is_active_ = true;
    desktop_native_widget_aura_->HandleActivationChanged(true);
  } else if (is_active_) {
    is_active_ = false;
    desktop_native_widget_aura_->HandleActivationChanged(false);
  }
}

}  // namespace views
