// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_LAYER_ANIMATION_SEQUENCE_H_
#define UI_GFX_COMPOSITOR_LAYER_ANIMATION_SEQUENCE_H_
#pragma once

#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/time.h"
#include "ui/gfx/compositor/compositor_export.h"
#include "ui/gfx/compositor/layer_animation_element.h"

namespace ui {

class LayerAnimationDelegate;

// Contains a collection of layer animation elements to be played one after
// another. Although it has a similar interface to LayerAnimationElement, it is
// not a LayerAnimationElement (i.e., it is not permitted to have a sequence in
// a sequence). Sequences own their elements, and sequences are themselves owned
// by a LayerAnimator.
//
// TODO(vollick) Create a 'blended' sequence for transitioning between
// sequences.
class COMPOSITOR_EXPORT LayerAnimationSequence {
 public:
  LayerAnimationSequence();
  // Takes ownership of the given element and adds it to the sequence.
  explicit LayerAnimationSequence(LayerAnimationElement* element);
  virtual ~LayerAnimationSequence();

  // Updates the delegate to the appropriate value for |elapsed|, which is in
  // the range [0, Duration()].  If the animation is not aborted, it is
  // guaranteed that Animate will be called with elapsed = Duration().
  void Progress(base::TimeDelta elapsed, LayerAnimationDelegate* delegate);

  // Sets the target value to the value that would have been set had
  // the sequence completed. Does nothing if the sequence is cyclic.
  void GetTargetValue(LayerAnimationElement::TargetValue* target) const;

  // Aborts the given animation.
  void Abort();

  // All properties modified by the sequence.
  const LayerAnimationElement::AnimatableProperties& properties() const {
    return properties_;
  }

  // The total, finite duration of one cycle of the sequence.
  base::TimeDelta duration() const {
    return duration_;
  }

  // Adds an element to the sequence. The sequences takes ownership of this
  // element.
  void AddElement(LayerAnimationElement* element);

  // Sequences can be looped indefinitely.
  void set_is_cyclic(bool is_cyclic) { is_cyclic_ = is_cyclic; }
  bool is_cyclic() const { return is_cyclic_; }

  // Returns true if this sequence has at least one element affecting a
  // property in |other|.
  bool HasCommonProperty(
      const LayerAnimationElement::AnimatableProperties& other) const;

 private:
  typedef std::vector<linked_ptr<LayerAnimationElement> > Elements;

  // The sum of the durations of all the elements in the sequence.
  base::TimeDelta duration_;

  // The union of all the properties modified by all elements in the sequence.
  LayerAnimationElement::AnimatableProperties properties_;

  // The elements in the sequence.
  Elements elements_;

  // True if the sequence should be looped forever.
  bool is_cyclic_;

  // These are used when animating to efficiently find the next element.
  size_t last_element_;
  base::TimeDelta last_start_;

  DISALLOW_COPY_AND_ASSIGN(LayerAnimationSequence);
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_LAYER_ANIMATION_SEQUENCE_H_
