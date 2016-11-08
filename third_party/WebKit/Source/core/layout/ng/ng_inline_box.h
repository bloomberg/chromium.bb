// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineBox_h
#define NGInlineBox_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class LayoutBox;
class LayoutObject;
class NGConstraintSpace;
class NGFragment;
class NGLayoutAlgorithm;
class NGPhysicalFragment;
struct MinAndMaxContentSizes;

// Represents an inline node to be laid out.
// TODO(layout-dev): Make this and NGBox inherit from a common class.
class CORE_EXPORT NGInlineBox final
    : public GarbageCollectedFinalized<NGInlineBox> {
 public:
  explicit NGInlineBox(LayoutObject* start_inline);

  // Returns true when done; when this function returns false, it has to be
  // called again. The out parameter will only be set when this function
  // returns true. The same constraint space has to be passed each time.
  // TODO(layout-ng): Should we have a StartLayout function to avoid passing
  // the same space for each Layout iteration?
  bool Layout(const NGConstraintSpace*, NGFragment**);

  NGInlineBox* NextSibling();

  DECLARE_VIRTUAL_TRACE();

 private:
  LayoutObject* start_inline_;
  LayoutObject* last_inline_;

  Member<NGInlineBox> next_sibling_;
  Member<NGLayoutAlgorithm> layout_algorithm_;
};

}  // namespace blink

#endif  // NGInlineBox_h
