// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutInputNode_h
#define NGLayoutInputNode_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class ComputedStyle;
class LayoutObject;
class LayoutBox;
class NGBreakToken;
class NGConstraintSpace;
class NGLayoutResult;
struct MinMaxContentSize;

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

  bool CreatesNewFormattingContext() const;

  // Performs layout on this input node, will return the layout result.
  RefPtr<NGLayoutResult> Layout(NGConstraintSpace*, NGBreakToken*);

  MinMaxContentSize ComputeMinMaxContentSize();

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
