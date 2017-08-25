// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGUnpositionedFloat_h
#define NGUnpositionedFloat_h

#include "core/layout/ng/geometry/ng_box_strut.h"
#include "core/layout/ng/geometry/ng_logical_size.h"
#include "core/layout/ng/ng_block_break_token.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

// Struct that keeps all information needed to position floats in LayoutNG.
struct CORE_EXPORT NGUnpositionedFloat
    : public RefCounted<NGUnpositionedFloat> {
 public:
  static RefPtr<NGUnpositionedFloat> Create(NGLogicalSize available_size,
                                            NGLogicalSize percentage_size,
                                            LayoutUnit origin_bfc_line_offset,
                                            LayoutUnit bfc_line_offset,
                                            NGBoxStrut margins,
                                            NGBlockNode node,
                                            NGBlockBreakToken* token) {
    return AdoptRef(new NGUnpositionedFloat(
        margins, available_size, percentage_size, origin_bfc_line_offset,
        bfc_line_offset, node, token));
  }

  NGBlockNode node;
  RefPtr<NGBlockBreakToken> token;

  // Available size of the constraint space that will be used by
  // NGLayoutOpportunityIterator to position this floating object.
  NGLogicalSize available_size;
  NGLogicalSize percentage_size;

  // This is the BFC inline-offset for where we begin searching for layout
  // opportunities for this float.
  LayoutUnit origin_bfc_line_offset;

  // This is the BFC inline-offset for the float's parent. This is used for
  // calculating the offset between the float and its parent.
  LayoutUnit bfc_line_offset;

  // The margins are relative to the writing mode of the block formatting
  // context. They are stored for convinence and could be recomputed with other
  // data on this object.
  NGBoxStrut margins;

  // The layout result for this unpositioned float. This is only present if
  // it's in a different writing mode than the BFC.
  RefPtr<NGLayoutResult> layout_result;

  bool IsLeft() const;
  bool IsRight() const;
  EClear ClearType() const;

 private:
  NGUnpositionedFloat(const NGBoxStrut& margins,
                      const NGLogicalSize& available_size,
                      const NGLogicalSize& percentage_size,
                      LayoutUnit origin_bfc_line_offset,
                      LayoutUnit bfc_line_offset,
                      NGBlockNode node,
                      NGBlockBreakToken* token)
      : node(node),
        token(token),
        available_size(available_size),
        percentage_size(percentage_size),
        origin_bfc_line_offset(origin_bfc_line_offset),
        bfc_line_offset(bfc_line_offset),
        margins(margins) {}
};

}  // namespace blink

#endif  // NGUnpositionedFloat_h
