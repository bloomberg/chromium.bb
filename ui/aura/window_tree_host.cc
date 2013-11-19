// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host.h"

#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_transformer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_delegate.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/display.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/point.h"
#include "ui/gfx/point3_f.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"

namespace aura {

float GetDeviceScaleFactorFromDisplay(Window* window) {
  gfx::Display display = gfx::Screen::GetScreenFor(window)->
      GetDisplayNearestWindow(window);
  DCHECK(display.is_valid());
  return display.device_scale_factor();
}

class SimpleRootWindowTransformer : public RootWindowTransformer {
 public:
  SimpleRootWindowTransformer(const Window* root_window,
                              const gfx::Transform& transform)
      : root_window_(root_window),
        transform_(transform) {
  }

  // RootWindowTransformer overrides:
  virtual gfx::Transform GetTransform() const OVERRIDE {
    return transform_;
  }

  virtual gfx::Transform GetInverseTransform() const OVERRIDE {
    gfx::Transform invert;
    if (!transform_.GetInverse(&invert))
      return transform_;
    return invert;
  }

  virtual gfx::Rect GetRootWindowBounds(
      const gfx::Size& host_size) const OVERRIDE {
    gfx::Rect bounds(host_size);
    gfx::RectF new_bounds(ui::ConvertRectToDIP(root_window_->layer(), bounds));
    transform_.TransformRect(&new_bounds);
    return gfx::Rect(gfx::ToFlooredSize(new_bounds.size()));
  }

  virtual gfx::Insets GetHostInsets() const OVERRIDE {
    return gfx::Insets();
  }

 private:
  virtual ~SimpleRootWindowTransformer() {}

  const Window* root_window_;
  const gfx::Transform transform_;

  DISALLOW_COPY_AND_ASSIGN(SimpleRootWindowTransformer);
};

////////////////////////////////////////////////////////////////////////////////
// RootWindowHost, public:

RootWindowHost::~RootWindowHost() {
  // TODO(beng): this represents an ordering change. In the old code, the
  //             compositor was reset before the window hierarchy was destroyed.
  //             verify that this has no adverse effects.
  // Make sure to destroy the compositor before terminating so that state is
  // cleared and we don't hit asserts.
  compositor_.reset();
}

void RootWindowHost::InitHost() {
  compositor_->SetScaleAndSize(GetDeviceScaleFactorFromDisplay(window()),
                               GetBounds().size());
  window()->Init(ui::LAYER_NOT_DRAWN);
  compositor_->SetRootLayer(window()->layer());
  transformer_.reset(
      new SimpleRootWindowTransformer(window(), gfx::Transform()));
  UpdateRootWindowSize(GetBounds().size());
  Env::GetInstance()->NotifyRootWindowInitialized(delegate_->AsRootWindow());
  window()->Show();
}

aura::Window* RootWindowHost::window() {
  return const_cast<Window*>(const_cast<const RootWindowHost*>(this)->window());
}

const aura::Window* RootWindowHost::window() const {
  return delegate_->AsRootWindow()->window();
}

void RootWindowHost::SetRootWindowTransformer(
    scoped_ptr<RootWindowTransformer> transformer) {
  transformer_ = transformer.Pass();
  SetInsets(transformer_->GetHostInsets());
  window()->SetTransform(transformer_->GetTransform());
  // If the layer is not animating, then we need to update the root window
  // size immediately.
  if (!window()->layer()->GetAnimator()->is_animating())
    UpdateRootWindowSize(GetBounds().size());
}

gfx::Transform RootWindowHost::GetRootTransform() const {
  float scale = ui::GetDeviceScaleFactor(window()->layer());
  gfx::Transform transform;
  transform.Scale(scale, scale);
  transform *= transformer_->GetTransform();
  return transform;
}

void RootWindowHost::SetTransform(const gfx::Transform& transform) {
  scoped_ptr<RootWindowTransformer> transformer(
      new SimpleRootWindowTransformer(window(), transform));
  SetRootWindowTransformer(transformer.Pass());
}

gfx::Transform RootWindowHost::GetInverseRootTransform() const {
  float scale = ui::GetDeviceScaleFactor(window()->layer());
  gfx::Transform transform;
  transform.Scale(1.0f / scale, 1.0f / scale);
  return transformer_->GetInverseTransform() * transform;
}

void RootWindowHost::UpdateRootWindowSize(const gfx::Size& host_size) {
  window()->SetBounds(transformer_->GetRootWindowBounds(host_size));
}

void RootWindowHost::ConvertPointToNativeScreen(gfx::Point* point) const {
  ConvertPointToHost(point);
  gfx::Point location = GetLocationOnNativeScreen();
  point->Offset(location.x(), location.y());
}

void RootWindowHost::ConvertPointFromNativeScreen(gfx::Point* point) const {
  gfx::Point location = GetLocationOnNativeScreen();
  point->Offset(-location.x(), -location.y());
  ConvertPointFromHost(point);
}

void RootWindowHost::ConvertPointToHost(gfx::Point* point) const {
  gfx::Point3F point_3f(*point);
  GetRootTransform().TransformPoint(&point_3f);
  *point = gfx::ToFlooredPoint(point_3f.AsPointF());
}

void RootWindowHost::ConvertPointFromHost(gfx::Point* point) const {
  gfx::Point3F point_3f(*point);
  GetInverseRootTransform().TransformPoint(&point_3f);
  *point = gfx::ToFlooredPoint(point_3f.AsPointF());
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowHost, protected:

RootWindowHost::RootWindowHost()
    : delegate_(NULL) {
}

void RootWindowHost::CreateCompositor(
    gfx::AcceleratedWidget accelerated_widget) {
  compositor_.reset(new ui::Compositor(GetAcceleratedWidget()));
  DCHECK(compositor_.get());
}

void RootWindowHost::NotifyHostResized(const gfx::Size& new_size) {
  // The compositor should have the same size as the native root window host.
  // Get the latest scale from display because it might have been changed.
  compositor_->SetScaleAndSize(GetDeviceScaleFactorFromDisplay(window()),
                               new_size);

  // The layer, and the observers should be notified of the
  // transformed size of the root window.
  UpdateRootWindowSize(new_size);
  delegate_->OnHostResized(new_size);
}

}  // namespace aura
