// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JAVASCRIPT_DIALOGS_VIEWS_LAYER_DIMMER_H_
#define COMPONENTS_JAVASCRIPT_DIALOGS_VIEWS_LAYER_DIMMER_H_

#include <memory>
#include "ui/aura/window_observer.h"

namespace ui {
class Layer;
}  // namespace ui

namespace javascript_dialogs {

class LayerDimmer : public aura::WindowObserver {
 public:
  explicit LayerDimmer(aura::Window* parent, aura::Window* dialog);

  LayerDimmer(const LayerDimmer&) = delete;
  LayerDimmer& operator=(const LayerDimmer&) = delete;

  ~LayerDimmer() override;

  void Show();
  void Hide();

  // NOTE: WindowDimmer is an observer for both |parent_| and |dialog_|.
  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowStackingChanged(aura::Window* window) override;

  // Test only functions
  ui::Layer* GetLayerForTest() const { return layer_.get(); }

 private:
  void StackLayerUnderDialog();

  std::unique_ptr<ui::Layer> layer_;

  aura::Window* parent_;
  aura::Window* dialog_;
};

}  // namespace javascript_dialogs

#endif  // COMPONENTS_JAVASCRIPT_DIALOGS_VIEWS_LAYER_DIMMER_H_
