// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockLayoutAlgorithm_h
#define NGBlockLayoutAlgorithm_h

#include "wtf/RefPtr.h"

namespace blink {

class ComputedStyle;
class LayoutBox;
class NGConstraintSpace;
class NGFragment;

// A class for general block layout (e.g. a <div> with no special style).
// Lays out the children in sequence.
class NGBlockLayoutAlgorithm {
 public:
  NGBlockLayoutAlgorithm(const ComputedStyle*);

  // Actual layout implementation. Lays out the children in sequence within the
  // constraints given by the NGConstraintSpace. Returns a fragment with the
  // resulting layout information.
  // This function can not be const because for interruptible layout, we have
  // to be able to store state information.
  NGFragment* layout(const NGConstraintSpace&);

 private:
  RefPtr<const ComputedStyle> m_style;
};

}  // namespace blink

#endif  // NGBlockLayoutAlgorithm_h
