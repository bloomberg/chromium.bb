// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGExclusionSpace_h
#define NGExclusionSpace_h

#include "core/CoreExport.h"
#include "core/layout/ng/exclusions/ng_layout_opportunity.h"
#include "core/layout/ng/geometry/ng_bfc_offset.h"
#include "core/layout/ng/geometry/ng_bfc_rect.h"
#include "core/layout/ng/ng_exclusion.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/LayoutUnit.h"
#include "platform/wtf/Vector.h"

namespace blink {

// The exclusion space represents all of the exclusions within a block
// formatting context.
//
// The space is mutated simply by adding exclusions, and various information
// can be queried based on the exclusions.
class CORE_EXPORT NGExclusionSpace {
 public:
  NGExclusionSpace();
  ~NGExclusionSpace(){};

  void Add(scoped_refptr<const NGExclusion> exclusion);

  // Returns a layout opportunity, within the BFC.
  // The area to search for layout opportunities is defined by the given offset,
  // and available_inline_size. The layout opportunity must be greater than the
  // given minimum_size.
  NGLayoutOpportunity FindLayoutOpportunity(
      const NGBfcOffset& offset,
      const LayoutUnit available_inline_size,
      const NGLogicalSize& minimum_size) const;

  Vector<NGLayoutOpportunity> AllLayoutOpportunities(
      const NGBfcOffset& offset,
      const LayoutUnit available_inline_size) const;

  // Returns the clearance offset based on the provided {@code clear_type}.
  LayoutUnit ClearanceOffset(EClear clear_type) const;

  // Returns the block start offset of the last float added.
  LayoutUnit LastFloatBlockStart() const { return last_float_block_start_; }

  bool HasLeftFloat() const { return has_left_float_; }
  bool HasRightFloat() const { return has_right_float_; }

  bool operator==(const NGExclusionSpace& other) const;
  bool operator!=(const NGExclusionSpace& other) const {
    return !(*this == other);
  }

  // The shelf is an internal data-structure representing the bottom of a
  // float. A shelf has a inline-size which is defined by the line_left and
  // line_right members. E.g.
  //
  //    0 1 2 3 4 5 6 7 8
  // 0  +---++--+    +---+
  //    |xxx||xx|    |xxx|
  // 10 |xxx|X-------Xxxx|
  //    +---+        +---+
  // 20
  //
  // In the above diagram the shelf is at the block-end edge of the smallest
  // float. It would have the internal values of:
  // {
  //   block_offset: 10,
  //   line_left: 20,
  //   line_right: 65,
  //   line_left_edges: [{0, 15}],
  //   line_right_edges: [{0, 15}],
  // }
  // The line_left_edges and line_right_edges are all the floats which are
  // "against" the shelf at the line_left and line_right offset respectively.
  //
  // An opportunity has a "solid" edge if there is at least one float adjacent
  // to the line-left or line-right edge. If an opportunity has no adjacent
  // floats it is invalid.
  //
  // These are used for:
  //  - When we create an opportunity, making sure it has "solid" edges.
  //  - The opportunity also holds onto a list of these edges to support
  //    css-shapes.
  struct NGShelf {
    explicit NGShelf(LayoutUnit block_offset)
        : block_offset(block_offset),
          line_left(LayoutUnit::Min()),
          line_right(LayoutUnit::Max()) {}

    LayoutUnit block_offset;
    LayoutUnit line_left;
    LayoutUnit line_right;

    Vector<scoped_refptr<const NGExclusion>, 1> line_left_edges;
    Vector<scoped_refptr<const NGExclusion>, 1> line_right_edges;
  };

 private:
  Vector<scoped_refptr<const NGExclusion>> exclusions_;

  // See NGShelf for a broad description of what shelves are. We always begin
  // with one, which has the internal value of:
  // {
  //   block_offset: LayoutUnit::Min(),
  //   line_left: LayoutUnit::Min(),
  //   line_right: LayoutUnit::Max(),
  // }
  //
  // The list of opportunities represent "closed-off" areas. E.g.
  //
  //    0 1 2 3 4 5 6 7 8
  // 0  +---+.      .+---+
  //    |xxx|.      .|xxx|
  // 10 |xxx|.      .|xxx|
  //    +---+.      .+---+
  // 20      ........
  //      +---+
  // 30   |xxx|
  //      |xxx|
  // 40   +---+
  //
  // In the above example the opportunity is represented with the dotted line.
  // It has the internal values of:
  // {
  //   start_offset: {20, LayoutUnit::Min()},
  //   end_offset: {65, 25},
  // }
  // Once an opportunity has been created, it can never been changed due to the
  // property that floats always align their block-start edges.
  //
  // We exploit this property by keeping this list of "closed-off" areas, and
  // removing shelves to make insertion faster.
  Vector<NGShelf> shelves_;
  Vector<NGLayoutOpportunity> opportunities_;

  // This member is used for implementing the "top edge alignment rule" for
  // floats. Floats can be positioned at negative offsets, hence is initialized
  // the minimum value.
  LayoutUnit last_float_block_start_;

  // These members are used for keeping track of the "lowest" offset for each
  // type of float. This is used for implementing float clearance.
  LayoutUnit left_float_clear_offset_;
  LayoutUnit right_float_clear_offset_;

  unsigned has_left_float_ : 1;
  unsigned has_right_float_ : 1;
};

}  // namespace blink

#endif  // NGExclusionSpace_h
