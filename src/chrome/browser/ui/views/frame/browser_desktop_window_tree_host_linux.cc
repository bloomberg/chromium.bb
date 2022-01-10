// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_linux.h"

#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_linux.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/extensions/x11_extension.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/views/frame/desktop_browser_frame_lacros.h"
#else  // defined(OS_LINUX)
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura_linux.h"
#endif

#if defined(USE_DBUS_MENU)

namespace {

bool CreateGlobalMenuBar() {
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformProperties()
      .supports_global_application_menus;
}

}  // namespace

#endif  // defined(USE_DBUS_MENU)

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLinux, public:

BrowserDesktopWindowTreeHostLinux::BrowserDesktopWindowTreeHostLinux(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame)
    : DesktopWindowTreeHostLinux(native_widget_delegate,
                                 desktop_native_widget_aura),
      browser_view_(browser_view),
      browser_frame_(browser_frame) {
  native_frame_ = static_cast<DesktopBrowserFrameAuraPlatform*>(
      browser_frame->native_browser_frame());
  native_frame_->set_host(this);

  browser_frame->set_frame_type(browser_frame->UseCustomFrame()
                                    ? views::Widget::FrameType::kForceCustom
                                    : views::Widget::FrameType::kForceNative);

  theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
  if (auto* linux_ui = views::LinuxUI::instance())
    scale_observation_.Observe(linux_ui);
}

BrowserDesktopWindowTreeHostLinux::~BrowserDesktopWindowTreeHostLinux() =
    default;

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLinux,
//     BrowserDesktopWindowTreeHost implementation:

views::DesktopWindowTreeHost*
BrowserDesktopWindowTreeHostLinux::AsDesktopWindowTreeHost() {
  return this;
}

int BrowserDesktopWindowTreeHostLinux::GetMinimizeButtonOffset() const {
  return 0;
}

bool BrowserDesktopWindowTreeHostLinux::UsesNativeSystemMenu() const {
  return false;
}

void BrowserDesktopWindowTreeHostLinux::FrameTypeChanged() {
  DesktopWindowTreeHostPlatform::FrameTypeChanged();
  UpdateFrameHints();
}

bool BrowserDesktopWindowTreeHostLinux::SupportsMouseLock() {
  auto* wayland_extension = ui::GetWaylandExtension(*platform_window());
  if (!wayland_extension)
    return false;

  return wayland_extension->SupportsPointerLock();
}

void BrowserDesktopWindowTreeHostLinux::LockMouse(aura::Window* window) {
  DesktopWindowTreeHostLinux::LockMouse(window);

  if (SupportsMouseLock()) {
    auto* wayland_extension = ui::GetWaylandExtension(*platform_window());
    wayland_extension->LockPointer(true /*enabled*/);
  }
}

void BrowserDesktopWindowTreeHostLinux::UnlockMouse(aura::Window* window) {
  DesktopWindowTreeHostLinux::UnlockMouse(window);

  if (SupportsMouseLock()) {
    auto* wayland_extension = ui::GetWaylandExtension(*platform_window());
    wayland_extension->LockPointer(false /*enabled*/);
  }
}

void BrowserDesktopWindowTreeHostLinux::TabDraggingKindChanged(
    TabDragKind tab_drag_kind) {
  // If there's no tabs left, the browser window is about to close, so don't
  // call SetOverrideRedirect() to prevent the window from flashing.
  if (!browser_view_->tabstrip()->GetModelCount())
    return;

  auto* x11_extension = GetX11Extension();
  if (x11_extension && x11_extension->IsWmTiling() &&
      x11_extension->CanResetOverrideRedirect()) {
    bool was_dragging_window =
        browser_frame_->tab_drag_kind() == TabDragKind::kAllTabs;
    bool is_dragging_window = tab_drag_kind == TabDragKind::kAllTabs;
    if (is_dragging_window != was_dragging_window)
      x11_extension->SetOverrideRedirect(is_dragging_window);
  }

  if (auto* wayland_extension = ui::GetWaylandExtension(*platform_window())) {
    if (tab_drag_kind != TabDragKind::kNone)
      wayland_extension->StartWindowDraggingSessionIfNeeded();
  }
}

bool BrowserDesktopWindowTreeHostLinux::SupportsClientFrameShadow() const {
  return platform_window()->CanSetDecorationInsets() &&
         platform_window()->IsTranslucentWindowOpacitySupported();
}

void BrowserDesktopWindowTreeHostLinux::UpdateFrameHints() {
#if defined(OS_LINUX)
  auto* view = static_cast<BrowserFrameViewLinux*>(
      native_frame_->browser_frame()->GetFrameView());
  auto* layout = view->layout();
  auto* window = platform_window();
  float scale = device_scale_factor();
  bool showing_frame =
      browser_frame_->native_browser_frame()->UseCustomFrame() &&
      !view->IsFrameCondensed();
  const gfx::Size widget_size =
      view->GetWidget()->GetWindowBoundsInScreen().size();

  if (SupportsClientFrameShadow()) {
    // Set the frame decoration insets.
    const gfx::Insets insets =
        (window->GetPlatformWindowState() == ui::PlatformWindowState::kNormal)
            ? layout->MirroredFrameBorderInsets()
            : gfx::Insets();
    const gfx::Insets insets_px = gfx::ScaleToCeiledInsets(insets, scale);
    window->SetDecorationInsets(showing_frame ? &insets_px : nullptr);

    // Set the input region.
    gfx::Rect input_bounds(widget_size);
    input_bounds.Inset(insets + layout->GetInputInsets());
    input_bounds = gfx::ScaleToEnclosingRect(input_bounds, scale);
    window->SetInputRegion(showing_frame ? &input_bounds : nullptr);
  }

  if (window->IsTranslucentWindowOpacitySupported()) {
    // Set the opaque region.
    if (showing_frame) {
      // The opaque region is a list of rectangles that contain only fully
      // opaque pixels of the window.  We need to convert the clipping
      // rounded-rect into this format.
      SkRRect rrect = view->GetRestoredClipRegion();
      gfx::RectF rectf = gfx::SkRectToRectF(rrect.rect());
      rectf.Scale(scale);
      // It is acceptable to omit some pixels that are opaque, but the region
      // must not include any translucent pixels.  Therefore, we must
      // conservatively scale to the enclosed rectangle.
      gfx::Rect rect = gfx::ToEnclosedRect(rectf);

      // Create the initial region from the clipping rectangle without rounded
      // corners.
      SkRegion region(gfx::RectToSkIRect(rect));

      // Now subtract out the small rectangles that cover the corners.
      struct {
        SkRRect::Corner corner;
        bool left;
        bool upper;
      } kCorners[] = {
          {SkRRect::kUpperLeft_Corner, true, true},
          {SkRRect::kUpperRight_Corner, false, true},
          {SkRRect::kLowerLeft_Corner, true, false},
          {SkRRect::kLowerRight_Corner, false, false},
      };
      for (const auto& corner : kCorners) {
        auto radii = rrect.radii(corner.corner);
        auto rx = std::ceil(scale * radii.x());
        auto ry = std::ceil(scale * radii.y());
        auto corner_rect = SkIRect::MakeXYWH(
            corner.left ? rect.x() : rect.right() - rx,
            corner.upper ? rect.y() : rect.bottom() - ry, rx, ry);
        region.op(corner_rect, SkRegion::kDifference_Op);
      }

      // Convert the region to a list of rectangles.
      std::vector<gfx::Rect> opaque_region;
      for (SkRegion::Iterator i(region); !i.done(); i.next())
        opaque_region.push_back(gfx::SkIRectToRect(i.rect()));
      window->SetOpaqueRegion(&opaque_region);
    } else {
      window->SetOpaqueRegion(nullptr);
    }
  }

  SizeConstraintsChanged();
#endif
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLinux,
//     DesktopWindowTreeHostLinuxImpl implementation:

void BrowserDesktopWindowTreeHostLinux::Init(
    const views::Widget::InitParams& params) {
  DesktopWindowTreeHostLinux::Init(std::move(params));

#if defined(USE_DBUS_MENU)
  // We have now created our backing X11 window.  We now need to (possibly)
  // alert the desktop environment that there's a menu bar attached to it.
  if (CreateGlobalMenuBar()) {
    dbus_appmenu_ =
        std::make_unique<DbusAppmenu>(browser_view_, GetAcceleratedWidget());
  }
#endif
}

void BrowserDesktopWindowTreeHostLinux::OnWidgetInitDone() {
  DesktopWindowTreeHostLinux::OnWidgetInitDone();

  UpdateFrameHints();
}

void BrowserDesktopWindowTreeHostLinux::CloseNow() {
#if defined(USE_DBUS_MENU)
  dbus_appmenu_.reset();
#endif
  DesktopWindowTreeHostLinux::CloseNow();
}

bool BrowserDesktopWindowTreeHostLinux::IsOverrideRedirect() const {
  auto* x11_extension = GetX11Extension();
  return (browser_frame_->tab_drag_kind() == TabDragKind::kAllTabs) &&
         x11_extension && x11_extension->IsWmTiling() &&
         x11_extension->CanResetOverrideRedirect();
}

void BrowserDesktopWindowTreeHostLinux::OnBoundsChanged(
    const BoundsChange& change) {
  DesktopWindowTreeHostLinux::OnBoundsChanged(change);

  UpdateFrameHints();
}

void BrowserDesktopWindowTreeHostLinux::OnWindowStateChanged(
    ui::PlatformWindowState old_window_show_state,
    ui::PlatformWindowState new_window_show_state) {
  DesktopWindowTreeHostLinux::OnWindowStateChanged(old_window_show_state,
                                                   new_window_show_state);

  bool fullscreen_changed =
      new_window_show_state == ui::PlatformWindowState::kFullScreen ||
      old_window_show_state == ui::PlatformWindowState::kFullScreen;
  if (old_window_show_state != new_window_show_state && fullscreen_changed) {
    // If the browser view initiated this state change,
    // BrowserView::ProcessFullscreen will no-op, so this call is harmless.
    browser_view_->FullscreenStateChanging();
  }

  UpdateFrameHints();
}

void BrowserDesktopWindowTreeHostLinux::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  UpdateFrameHints();
}

void BrowserDesktopWindowTreeHostLinux::OnDeviceScaleFactorChanged() {
  UpdateFrameHints();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHost, public:

// TODO(crbug.com/1221374): Separate Lacros specific codes into
// browser_desktop_window_tree_host_lacros.cc.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// static
BrowserDesktopWindowTreeHost*
BrowserDesktopWindowTreeHost::CreateBrowserDesktopWindowTreeHost(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame) {
  return new BrowserDesktopWindowTreeHostLinux(native_widget_delegate,
                                               desktop_native_widget_aura,
                                               browser_view, browser_frame);
}
#endif
