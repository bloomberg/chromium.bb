// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutInputNode_h
#define NGLayoutInputNode_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class LayoutObject;
class NGBreakToken;
class NGConstraintSpace;
class NGLayoutResult;

// Represents the input to a layout algorithm for a given node. The layout
// engine should use the style, node type to determine which type of layout
// algorithm to use to produce fragments for this node.
class CORE_EXPORT NGLayoutInputNode
    : public GarbageCollectedFinalized<NGLayoutInputNode> {
 public:
  enum NGLayoutInputNodeType { kLegacyBlock = 0, kLegacyInline = 1 };

  virtual ~NGLayoutInputNode(){};

  // Performs layout on this input node, will return the layout result.
  virtual RefPtr<NGLayoutResult> Layout(NGConstraintSpace*, NGBreakToken*) = 0;

  // Returns the next sibling.
  virtual NGLayoutInputNode* NextSibling() = 0;

  // Returns the LayoutObject which is associated with this node.
  virtual LayoutObject* GetLayoutObject() = 0;

  NGLayoutInputNodeType Type() const {
    return static_cast<NGLayoutInputNodeType>(type_);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 protected:
  explicit NGLayoutInputNode(NGLayoutInputNodeType type) : type_(type) {}

 private:
  unsigned type_ : 1;
};

}  // namespace blink

#endif  // NGLayoutInputNode_h
