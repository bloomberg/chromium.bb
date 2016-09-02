// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBox_h
#define NGBox_h

#include "core/layout/LayoutBox.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class ComputedStyle;
class LayoutBox;
class NGBlockLayoutAlgorithm;
class NGConstraintSpace;
class NGFragment;
class NGPhysicalFragment;

// Represents a node to be laid out.
class CORE_EXPORT NGBox final : public GarbageCollectedFinalized<NGBox> {
 public:
  explicit NGBox(LayoutObject*);

  // TODO(layout-ng): make it private and declare a friend class to use in tests
  explicit NGBox(ComputedStyle*);

  // Returns true when done; when this function returns false, it has to be
  // called again. The out parameter will only be set when this function
  // returns true. The same constraint space has to be passed each time.
  // TODO(layout-ng): Should we have a StartLayout function to avoid passing
  // the same space for each Layout iteration?
  bool Layout(const NGConstraintSpace*, NGFragment**);
  const ComputedStyle* Style() const;

  NGBox* NextSibling();

  NGBox* FirstChild();

  void SetNextSibling(NGBox*);
  void SetFirstChild(NGBox*);

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(algorithm_);
    visitor->trace(fragment_);
    visitor->trace(next_sibling_);
    visitor->trace(first_child_);
  }

 private:
  // This is necessary for interop between old and new trees -- after our parent
  // positions us, it calls this function so we can store the position on the
  // underlying LayoutBox.
  void PositionUpdated();

  bool CanUseNewLayout();

  // We can either wrap a layout_box_ or a style_/next_sibling_/first_child_
  // combination.
  LayoutBox* layout_box_;
  RefPtr<ComputedStyle> style_;
  Member<NGBox> next_sibling_;
  Member<NGBox> first_child_;
  Member<NGBlockLayoutAlgorithm> algorithm_;
  Member<NGPhysicalFragment> fragment_;
};

}  // namespace blink

#endif  // NGBox_h
