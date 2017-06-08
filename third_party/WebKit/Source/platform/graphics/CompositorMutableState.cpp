// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorMutableState.h"

#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "platform/graphics/CompositorMutation.h"

namespace blink {
CompositorMutableState::CompositorMutableState(CompositorMutation* mutation,
                                               cc::LayerImpl* main,
                                               cc::LayerImpl* scroll)
    : mutation_(mutation), main_layer_(main), scroll_layer_(scroll) {}

CompositorMutableState::~CompositorMutableState() {}

double CompositorMutableState::Opacity() const {
  return main_layer_->Opacity();
}

void CompositorMutableState::SetOpacity(double opacity) {
  if (!main_layer_)
    return;
  main_layer_->layer_tree_impl()->SetOpacityMutated(main_layer_->element_id(),
                                                    opacity);
  mutation_->SetOpacity(opacity);
}

const SkMatrix44& CompositorMutableState::Transform() const {
  return main_layer_ ? main_layer_->Transform().matrix() : SkMatrix44::I();
}

void CompositorMutableState::SetTransform(const SkMatrix44& matrix) {
  if (!main_layer_)
    return;
  main_layer_->layer_tree_impl()->SetTransformMutated(main_layer_->element_id(),
                                                      gfx::Transform(matrix));
  mutation_->SetTransform(matrix);
}

float CompositorMutableState::ScrollLeft() const {
  return scroll_layer_ ? scroll_layer_->CurrentScrollOffset().x() : 0.0;
}

void CompositorMutableState::SetScrollLeft(float scroll_left) {
  if (!scroll_layer_)
    return;

  gfx::ScrollOffset offset = scroll_layer_->CurrentScrollOffset();
  offset.set_x(scroll_left);
  scroll_layer_->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.OnScrollOffsetAnimated(
          scroll_layer_->element_id(), scroll_layer_->scroll_tree_index(),
          offset, scroll_layer_->layer_tree_impl());
  mutation_->SetScrollLeft(scroll_left);
}

float CompositorMutableState::ScrollTop() const {
  return scroll_layer_ ? scroll_layer_->CurrentScrollOffset().y() : 0.0;
}

void CompositorMutableState::SetScrollTop(float scroll_top) {
  if (!scroll_layer_)
    return;
  gfx::ScrollOffset offset = scroll_layer_->CurrentScrollOffset();
  offset.set_y(scroll_top);
  scroll_layer_->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.OnScrollOffsetAnimated(
          scroll_layer_->element_id(), scroll_layer_->scroll_tree_index(),
          offset, scroll_layer_->layer_tree_impl());
  mutation_->SetScrollTop(scroll_top);
}

}  // namespace blink
