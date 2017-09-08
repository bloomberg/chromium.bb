// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_aura.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/painter.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

class NativeViewHostAura::ClippingWindowDelegate : public aura::WindowDelegate {
 public:
  ClippingWindowDelegate() : native_view_(NULL) {}
  ~ClippingWindowDelegate() override {}

  void set_native_view(aura::Window* native_view) {
    native_view_ = native_view;
  }

  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  gfx::Size GetMaximumSize() const override { return gfx::Size(); }
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    return gfx::kNullCursor;
  }
  int GetNonClientComponent(const gfx::Point& point) const override {
    return HTCLIENT;
  }
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return true;
  }
  bool CanFocus() override {
    // Ask the hosted native view's delegate because directly calling
    // aura::Window::CanFocus() will call back into this when checking whether
    // parents can focus.
    return native_view_ && native_view_->delegate()
        ? native_view_->delegate()->CanFocus()
        : true;
  }
  void OnCaptureLost() override {}
  void OnPaint(const ui::PaintContext& context) override {}
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override {}
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override { return false; }
  void GetHitTestMask(gfx::Path* mask) const override {}

 private:
  aura::Window* native_view_;
};

NativeViewHostAura::NativeViewHostAura(NativeViewHost* host)
    : host_(host),
      clipping_window_delegate_(new ClippingWindowDelegate()),
      clipping_window_(clipping_window_delegate_.get()) {
  // Set the type so descendant views (including popups) get positioned
  // appropriately.
  clipping_window_.SetType(aura::client::WINDOW_TYPE_CONTROL);
  clipping_window_.Init(ui::LAYER_NOT_DRAWN);
  clipping_window_.set_owned_by_parent(false);
  clipping_window_.SetName("NativeViewHostAuraClip");
  clipping_window_.layer()->SetMasksToBounds(true);
  clipping_window_.SetProperty(views::kHostViewKey, static_cast<View*>(host_));
}

NativeViewHostAura::~NativeViewHostAura() {
  if (host_->native_view()) {
    host_->native_view()->RemoveObserver(this);
    host_->native_view()->ClearProperty(views::kHostViewKey);
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);
    clipping_window_.ClearProperty(views::kHostViewKey);
    if (host_->native_view()->parent() == &clipping_window_)
      clipping_window_.RemoveChild(host_->native_view());
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostAura, NativeViewHostWrapper implementation:
void NativeViewHostAura::AttachNativeView() {
  clipping_window_delegate_->set_native_view(host_->native_view());
  host_->native_view()->AddObserver(this);
  host_->native_view()->SetProperty(views::kHostViewKey,
      static_cast<View*>(host_));
  AddClippingWindow();
  InstallMask();
}

void NativeViewHostAura::NativeViewDetaching(bool destroyed) {
  clipping_window_delegate_->set_native_view(NULL);
  RemoveClippingWindow();
  if (!destroyed) {
    host_->native_view()->RemoveObserver(this);
    host_->native_view()->ClearProperty(views::kHostViewKey);
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);
    host_->native_view()->Hide();
    if (host_->native_view()->parent())
      Widget::ReparentNativeView(host_->native_view(), NULL);
  }
}

void NativeViewHostAura::AddedToWidget() {
  if (!host_->native_view())
    return;

  AddClippingWindow();
  if (host_->IsDrawn())
    host_->native_view()->Show();
  else
    host_->native_view()->Hide();
  host_->Layout();
}

void NativeViewHostAura::RemovedFromWidget() {
  if (host_->native_view()) {
    host_->native_view()->Hide();
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);
    if (host_->native_view()->parent())
      host_->native_view()->parent()->RemoveChild(host_->native_view());
    RemoveClippingWindow();
  }
}

bool NativeViewHostAura::SetCornerRadius(int corner_radius) {
#if defined(OS_WIN)
  // Layer masks don't work on Windows. See crbug.com/713359
  return false;
#else
  mask_ = views::Painter::CreatePaintedLayer(
      views::Painter::CreateSolidRoundRectPainter(SK_ColorBLACK,
                                                  corner_radius));
  mask_->layer()->SetFillsBoundsOpaquely(false);
  InstallMask();
  return true;
#endif
}

void NativeViewHostAura::InstallClip(int x, int y, int w, int h) {
  clip_rect_.reset(
      new gfx::Rect(host_->ConvertRectToWidget(gfx::Rect(x, y, w, h))));
}

bool NativeViewHostAura::HasInstalledClip() {
  return !!clip_rect_;
}

void NativeViewHostAura::UninstallClip() {
  clip_rect_.reset();
}

void NativeViewHostAura::ShowWidget(int x, int y, int w, int h) {
  int width = w;
  int height = h;
  if (host_->fast_resize()) {
    gfx::Point origin(x, y);
    views::View::ConvertPointFromWidget(host_, &origin);
    InstallClip(origin.x(), origin.y(), w, h);
    width = host_->native_view()->bounds().width();
    height = host_->native_view()->bounds().height();
  }
  clipping_window_.SetBounds(clip_rect_ ? *clip_rect_
                                        : gfx::Rect(x, y, w, h));

  gfx::Point clip_offset = clipping_window_.bounds().origin();
  host_->native_view()->SetBounds(
      gfx::Rect(x - clip_offset.x(), y - clip_offset.y(), width, height));
  host_->native_view()->Show();
  clipping_window_.Show();
}

void NativeViewHostAura::HideWidget() {
  host_->native_view()->Hide();
  clipping_window_.Hide();
}

void NativeViewHostAura::SetFocus() {
  aura::Window* window = host_->native_view();
  aura::client::FocusClient* client = aura::client::GetFocusClient(window);
  if (client)
    client->FocusWindow(window);
}

gfx::NativeViewAccessible NativeViewHostAura::GetNativeViewAccessible() {
  return NULL;
}

gfx::NativeCursor NativeViewHostAura::GetCursor(int x, int y) {
  if (host_->native_view())
    return host_->native_view()->GetCursor(gfx::Point(x, y));
  return gfx::kNullCursor;
}

void NativeViewHostAura::OnWindowBoundsChanged(aura::Window* window,
                                               const gfx::Rect& old_bounds,
                                               const gfx::Rect& new_bounds) {
  if (mask_)
    mask_->layer()->SetBounds(gfx::Rect(host_->native_view()->bounds().size()));
}

void NativeViewHostAura::OnWindowDestroying(aura::Window* window) {
  DCHECK(window == host_->native_view());
  clipping_window_delegate_->set_native_view(NULL);
}

void NativeViewHostAura::OnWindowDestroyed(aura::Window* window) {
  DCHECK(window == host_->native_view());
  host_->NativeViewDestroyed();
}

// static
NativeViewHostWrapper* NativeViewHostWrapper::CreateWrapper(
    NativeViewHost* host) {
  return new NativeViewHostAura(host);
}

void NativeViewHostAura::AddClippingWindow() {
  RemoveClippingWindow();

  host_->native_view()->SetProperty(aura::client::kHostWindowKey,
                                    host_->GetWidget()->GetNativeView());
  Widget::ReparentNativeView(host_->native_view(),
                             &clipping_window_);
  if (host_->GetWidget()->GetNativeView()) {
    Widget::ReparentNativeView(&clipping_window_,
                               host_->GetWidget()->GetNativeView());
  }
}

void NativeViewHostAura::RemoveClippingWindow() {
  clipping_window_.Hide();
  if (host_->native_view())
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);

  if (host_->native_view()->parent() == &clipping_window_) {
    if (host_->GetWidget() && host_->GetWidget()->GetNativeView()) {
      Widget::ReparentNativeView(host_->native_view(),
                                 host_->GetWidget()->GetNativeView());
    } else {
      clipping_window_.RemoveChild(host_->native_view());
    }
    host_->native_view()->SetBounds(clipping_window_.bounds());
  }
  if (clipping_window_.parent())
    clipping_window_.parent()->RemoveChild(&clipping_window_);
}

void NativeViewHostAura::InstallMask() {
  if (!mask_)
    return;
  if (host_->native_view()) {
    mask_->layer()->SetBounds(gfx::Rect(host_->native_view()->bounds().size()));
    host_->native_view()->layer()->SetMaskLayer(mask_->layer());
  }
}

}  // namespace views
