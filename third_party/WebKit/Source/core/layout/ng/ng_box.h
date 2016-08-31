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
class CORE_EXPORT NGBox final : public GarbageCollected<NGBox> {
 public:
  explicit NGBox(LayoutObject*);

  // Returns true when done; when this function returns false, it has to be
  // called again. The out parameter will only be set when this function
  // returns true. The same constraint space has to be passed each time.
  // TODO(layout-ng): Should we have a StartLayout function to avoid passing
  // the same space for each Layout iteration?
  bool Layout(const NGConstraintSpace*, NGFragment**);
  const ComputedStyle* Style() const;

  NGBox* NextSibling() const;

  NGBox* FirstChild() const;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(algorithm_);
    visitor->trace(fragment_);
  }

 private:
  // This is necessary for interop between old and new trees -- after our parent
  // positions us, it calls this function so we can store the position on the
  // underlying LayoutBox.
  void PositionUpdated();
  bool CanUseNewLayout();

  LayoutBox* layout_box_;
  Member<NGBlockLayoutAlgorithm> algorithm_;
  Member<NGPhysicalFragment> fragment_;
};

}  // namespace blink

#endif  // NGBox_h
