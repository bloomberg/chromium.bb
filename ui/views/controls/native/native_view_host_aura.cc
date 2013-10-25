// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_aura.h"

#include "base/logging.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/gfx/safe_integer_conversions.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

// Watches for the clipping window to enter its destructor, so it can clear the
// NVHA's reference to the clipping window.
class ClippingWindowObserver : public aura::WindowObserver {
 public:
  explicit ClippingWindowObserver(NativeViewHostAura* host) :
      host_(host),
      clipping_window_(NULL) {}

  virtual ~ClippingWindowObserver() {
    if (clipping_window_)
      clipping_window_->RemoveObserver(this);
  }

  void SetClippingWindow(aura::Window* window) {
    if (clipping_window_)
      clipping_window_->RemoveObserver(this);
    clipping_window_ = window;
    if (clipping_window_)
      clipping_window_->AddObserver(this);
  }

  // Overridden from aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    host_->clipping_window_ = NULL;
    SetClippingWindow(NULL);
  }

 private:
  NativeViewHostAura* host_;
  aura::Window* clipping_window_;

  DISALLOW_COPY_AND_ASSIGN(ClippingWindowObserver);
};

NativeViewHostAura::NativeViewHostAura(NativeViewHost* host)
    : host_(host),
      installed_clip_(false),
      clipping_window_(NULL),
      clipping_window_observer_(new ClippingWindowObserver(this)) {}

NativeViewHostAura::~NativeViewHostAura() {
  if (clipping_window_)
    host_->native_view()->ClearProperty(views::kHostViewKey);

  if (host_->native_view()) {
    host_->native_view()->RemoveObserver(this);
    host_->native_view()->ClearProperty(views::kHostViewKey);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostAura, NativeViewHostWrapper implementation:
void NativeViewHostAura::AttachNativeView() {
  host_->native_view()->AddObserver(this);
  host_->native_view()->SetProperty(views::kHostViewKey,
                                    static_cast<View*>(host_));
  AddClippingWindow();
}

void NativeViewHostAura::NativeViewDetaching(bool destroyed) {
  if (!destroyed) {
    host_->native_view()->RemoveObserver(this);
    host_->native_view()->ClearProperty(views::kHostViewKey);
    host_->native_view()->Hide();
    RemoveClippingWindow();
  } else {
    if (clipping_window_)
      delete clipping_window_;
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
  }
  RemoveClippingWindow();
}

void NativeViewHostAura::InstallClip(int x, int y, int w, int h) {
  installed_clip_ = true;
  clip_rect_ = gfx::Rect(x + orig_bounds_.x(),
                         y + orig_bounds_.y(),
                         w,
                         h);
  UpdateClippingWindow();
  clipping_window_->layer()->SetMasksToBounds(true);
}

bool NativeViewHostAura::HasInstalledClip() {
  return installed_clip_;
}

void NativeViewHostAura::UninstallClip() {
  if (installed_clip_ == false)
    return;
  installed_clip_ = false;
  clipping_window_->layer()->SetMasksToBounds(false);
}

void NativeViewHostAura::ShowWidget(int x, int y, int w, int h) {
  if (host_->fast_resize()) {
    gfx::Rect native_view_bounds = host_->native_view()->bounds();
    gfx::Point native_view_origin =
        CalculateNativeViewOrigin(clipping_window_->bounds(),
                                  native_view_bounds);
    orig_bounds_ = gfx::Rect(x + native_view_origin.x(),
                             y + native_view_origin.y(),
                             native_view_bounds.width(),
                             native_view_bounds.height());

    InstallClip(x - orig_bounds_.x(),
                y - orig_bounds_.y(),
                w,
                h);
  } else {
    clip_rect_.Offset(x - orig_bounds_.x(), y - orig_bounds_.y());
    orig_bounds_ = gfx::Rect(x, y, w, h);
    UpdateClippingWindow();
  }
  host_->native_view()->Show();
}

void NativeViewHostAura::HideWidget() {
  host_->native_view()->Hide();
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

void NativeViewHostAura::OnWindowDestroyed(aura::Window* window) {
  DCHECK(window == host_->native_view());
  host_->NativeViewDestroyed();
}

// static
NativeViewHostWrapper* NativeViewHostWrapper::CreateWrapper(
    NativeViewHost* host) {
  return new NativeViewHostAura(host);
}

gfx::Point NativeViewHostAura::CalculateNativeViewOrigin(
    const gfx::Rect& input_rect,
    const gfx::Rect& native_rect) const {
  int new_x = gfx::ToRoundedInt(host_->GetWidthScaleFactor() *
                                (input_rect.width() -
                                 native_rect.width()));
  int new_y = gfx::ToRoundedInt(host_->GetHeightScaleFactor() *
                                (input_rect.height() -
                                 native_rect.height()));
  return gfx::Point(new_x, new_y);
}

void NativeViewHostAura::AddClippingWindow() {
  RemoveClippingWindow();
  clipping_window_ = new aura::Window(NULL);
  clipping_window_->SetTransparent(true);
  clipping_window_->Init(ui::LAYER_NOT_DRAWN);
  clipping_window_->layer()->set_name("NativeViewHostAuraClip");
  clipping_window_->layer()->SetMasksToBounds(false);
  clipping_window_observer_->SetClippingWindow(clipping_window_);

  gfx::Rect bounds = host_->native_view()->bounds();
  orig_bounds_ = bounds;
  clipping_window_->SetBounds(bounds);
  bounds.set_origin(gfx::Point(0, 0));
  host_->native_view()->SetBounds(bounds);

  clipping_window_->AddChild(host_->native_view());
  clipping_window_->SetProperty(views::kHostViewKey,
      static_cast<View*>(host_));
  Widget::ReparentNativeView(clipping_window_,
                             host_->GetWidget()->GetNativeView());
  clipping_window_->Show();
}

void NativeViewHostAura::RemoveClippingWindow() {
  if (clipping_window_) {
    if (host_->native_view()->parent() == clipping_window_) {
      host_->native_view()->SetBounds(clipping_window_->bounds());
      if (clipping_window_->parent()) {
        Widget::ReparentNativeView(host_->native_view(), NULL);
      } else {
        clipping_window_->RemoveChild(host_->native_view());
      }
    }
    delete clipping_window_;
  }
}

void NativeViewHostAura::UpdateClippingWindow() {
  if (!installed_clip_)
    clip_rect_ = orig_bounds_;

  clipping_window_->SetBounds(clip_rect_);

  gfx::Rect native_view_bounds = orig_bounds_;
  native_view_bounds.Offset(-clip_rect_.x(), -clip_rect_.y());
  host_->native_view()->SetBounds(native_view_bounds);
}

}  // namespace views
