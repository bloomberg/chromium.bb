// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutInputNode_h
#define NGLayoutInputNode_h

#include "core/CoreExport.h"
#include "platform/LayoutUnit.h"
#include "platform/wtf/Optional.h"

namespace blink {

class ComputedStyle;
class LayoutObject;
class LayoutBox;
class NGBreakToken;
class NGConstraintSpace;
class NGLayoutResult;
struct MinMaxSize;
struct NGLogicalSize;

// Represents the input to a layout algorithm for a given node. The layout
// engine should use the style, node type to determine which type of layout
// algorithm to use to produce fragments for this node.
class CORE_EXPORT NGLayoutInputNode {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  enum NGLayoutInputNodeType {
    kBlock,
    kInline
    // When adding new values, ensure type_ below has enough bits.
  };

  NGLayoutInputNode(std::nullptr_t) : box_(nullptr), type_(kBlock) {}

  bool IsInline() const;
  bool IsBlock() const;
  bool IsFloating() const;
  bool IsOutOfFlowPositioned() const;
  bool IsReplaced() const;
  bool IsAbsoluteContainer() const;
  bool IsFixedContainer() const;
  bool ShouldBeConsideredAsReplaced() const;

  // If the node is a quirky container for margin collapsing, see:
  // https://html.spec.whatwg.org/#margin-collapsing-quirks
  // NOTE: The spec appears to only somewhat match reality.
  bool IsQuirkyContainer() const;

  bool CreatesNewFormattingContext() const;

  // Performs layout on this input node, will return the layout result.
  RefPtr<NGLayoutResult> Layout(const NGConstraintSpace&, NGBreakToken*);

  MinMaxSize ComputeMinMaxSize();

  // Returns intrinsic sizing information for replaced elements.
  // ComputeReplacedSize can use it to compute actual replaced size.
  // The function arguments return values from LegacyLayout intrinsic size
  // computations: LayoutReplaced::IntrinsicSizingInfo,
  // and LayoutReplaced::IntrinsicSize.
  void IntrinsicSize(NGLogicalSize* default_intrinsic_size,
                     Optional<LayoutUnit>* computed_inline_size,
                     Optional<LayoutUnit>* computed_block_size,
                     NGLogicalSize* aspect_ratio) const;

  // Returns the next sibling.
  NGLayoutInputNode NextSibling();

  // Returns the LayoutObject which is associated with this node.
  LayoutObject* GetLayoutObject() const;

  const ComputedStyle& Style() const;

  String ToString() const;

  explicit operator bool() { return box_ != nullptr; }

  bool operator==(const NGLayoutInputNode& other) const {
    return box_ == other.box_;
  }

  bool operator!=(const NGLayoutInputNode& other) const {
    return !(*this == other);
  }

#ifndef NDEBUG
  void ShowNodeTree() const;
#endif

 protected:
  NGLayoutInputNode(LayoutBox* box, NGLayoutInputNodeType type)
      : box_(box), type_(type) {}

  LayoutBox* box_;

  unsigned type_ : 1;  // NGLayoutInputNodeType
};

}  // namespace blink

#endif  // NGLayoutInputNode_h
