// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/easy_resize_window_targeter.h"

#include <algorithm>

#include "services/ws/public/mojom/window_manager.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect.h"

namespace wm {
namespace {

gfx::Insets InsetsWithOnlyPositiveValues(const gfx::Insets& insets) {
  return gfx::Insets(std::max(0, insets.top()), std::max(0, insets.left()),
                     std::max(0, insets.bottom()), std::max(0, insets.right()));
}

}  // namespace

// HitMaskSetter is responsible for setting the hit-test mask on a Window.
class EasyResizeWindowTargeter::HitMaskSetter : public aura::WindowObserver {
 public:
  explicit HitMaskSetter(aura::Window* window) : window_(window) {
    window_->AddObserver(this);
  }
  ~HitMaskSetter() override {
    if (window_) {
      aura::WindowPortMus::Get(window_)->SetHitTestMask(base::nullopt);
      window_->RemoveObserver(this);
    }
  }

  void SetHitMaskInsets(const gfx::Insets& insets) {
    if (insets == insets_)
      return;

    insets_ = insets;
    ApplyHitTestMask();
  }

 private:
  void ApplyHitTestMask() {
    base::Optional<gfx::Rect> hit_test_mask(
        gfx::Rect(window_->bounds().size()));
    hit_test_mask->Inset(insets_);
    aura::WindowPortMus::Get(window_)->SetHitTestMask(hit_test_mask);
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window_->RemoveObserver(this);
    window_ = nullptr;
  }
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    ApplyHitTestMask();
  }

 private:
  aura::Window* window_;
  gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(HitMaskSetter);
};

EasyResizeWindowTargeter::EasyResizeWindowTargeter(
    const gfx::Insets& mouse_extend,
    const gfx::Insets& touch_extend) {
  SetInsets(mouse_extend, touch_extend);
}

EasyResizeWindowTargeter::~EasyResizeWindowTargeter() {}

void EasyResizeWindowTargeter::OnInstalled(aura::Window* w) {
  aura::WindowTargeter::OnInstalled(w);
  UpdateHitMaskSetter();
}

void EasyResizeWindowTargeter::OnSetInsets(
    const gfx::Insets& last_mouse_extend,
    const gfx::Insets& last_touch_extend) {
  UpdateHitMaskSetter();
}

void EasyResizeWindowTargeter::UpdateHitMaskSetter() {
  if (!window() || window()->env()->mode() != aura::Env::Mode::MUS) {
    hit_mask_setter_.reset();
    return;
  }

  // Positive values equate to a hit test mask.
  const gfx::Insets positive_mouse_insets =
      InsetsWithOnlyPositiveValues(mouse_extend());
  if (positive_mouse_insets.IsEmpty()) {
    hit_mask_setter_.reset();
  } else {
    if (!hit_mask_setter_)
      hit_mask_setter_ = std::make_unique<HitMaskSetter>(window());
    hit_mask_setter_->SetHitMaskInsets(positive_mouse_insets);
  }
}

bool EasyResizeWindowTargeter::EventLocationInsideBounds(
    aura::Window* target,
    const ui::LocatedEvent& event) const {
  return WindowTargeter::EventLocationInsideBounds(target, event);
}

bool EasyResizeWindowTargeter::ShouldUseExtendedBounds(
    const aura::Window* w) const {
  DCHECK(window());
  // Use the extended bounds only for immediate child windows of window().
  // Use the default targeter otherwise.
  if (w->parent() != window())
    return false;

  // Only resizable windows benefit from the extended hit-test region.
  if ((w->GetProperty(aura::client::kResizeBehaviorKey) &
       ws::mojom::kResizeBehaviorCanResize) == 0) {
    return false;
  }

  // For transient children use extended bounds if a transient parent or if
  // transient parent's parent is a top level window in window().
  aura::client::TransientWindowClient* transient_window_client =
      aura::client::GetTransientWindowClient();
  const aura::Window* transient_parent =
      transient_window_client ? transient_window_client->GetTransientParent(w)
                              : nullptr;
  return !transient_parent || transient_parent == window() ||
         transient_parent->parent() == window();
}

}  // namespace wm
