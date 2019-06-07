// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_H_
#define UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_H_

#include <memory>

#include "base/feature_list.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_event_handler.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

class SkPath;

namespace gfx {
class Size;
}  // namespace gfx

namespace ui {
class Layer;
class PaintContext;
}  // namespace ui

namespace views {

class View;

extern const VIEWS_EXPORT base::Feature kInstallableInkDropFeature;

// Stub for future InkDrop implementation that will be installable on any View
// without needing InkDropHostView. This is currently non-functional and fails
// on some method calls. TODO(crbug.com/931964): implement the necessary parts
// of the API and remove the rest from the InkDrop interface.
class VIEWS_EXPORT InstallableInkDrop : public InkDrop,
                                        public InkDropEventHandler::Delegate,
                                        public ui::LayerDelegate,
                                        public ViewObserver {
 public:
  // Create ink drop for |view|. Note that |view| must live longer than us.
  explicit InstallableInkDrop(View* view);
  ~InstallableInkDrop() override;

  // Should only be used for inspecting properties of the layer in tests.
  const ui::Layer* layer_for_testing() const { return layer_.get(); }

  // InkDrop:
  void HostSizeChanged(const gfx::Size& new_size) override;
  InkDropState GetTargetInkDropState() const override;
  void AnimateToState(InkDropState ink_drop_state) override;
  void SetHoverHighlightFadeDurationMs(int duration_ms) override;
  void UseDefaultHoverHighlightFadeDuration() override;
  void SnapToActivated() override;
  void SnapToHidden() override;
  void SetHovered(bool is_hovered) override;
  void SetFocused(bool is_focused) override;
  bool IsHighlightFadingInOrVisible() const override;
  void SetShowHighlightOnHover(bool show_highlight_on_hover) override;
  void SetShowHighlightOnFocus(bool show_highlight_on_focus) override;

  // InkDropEventHandler::Delegate:
  InkDrop* GetInkDrop() override;
  bool HasInkDrop() const override;
  bool SupportsGestureEvents() const override;

  // ViewObserver:
  void OnViewIsDeleting(View* observed_view) override;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // Gets the path that the ink drop fills in for the highlight. This uses
  // |kHighlightPathKey| if provided but falls back to a pill-shaped path.
  static SkPath GetHighlightPathForView(const View* view);

 private:
  // The view this ink drop is showing for. |layer_| is added to the layer
  // hierarchy that |view_| belongs to. We track events on |view_| to update our
  // visual state.
  View* const view_;

  // Observes |view_| and updates our visual state accordingly.
  InkDropEventHandler event_handler_;

  // The layer we paint to.
  std::unique_ptr<ui::Layer> layer_;

  InkDropState current_state_ = InkDropState::HIDDEN;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_H_
