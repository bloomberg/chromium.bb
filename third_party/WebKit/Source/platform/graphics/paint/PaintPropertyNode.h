// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintPropertyNode_h
#define PaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class ClipPaintPropertyNode;
class EffectPaintPropertyNode;
class ScrollPaintPropertyNode;
class TransformPaintPropertyNode;

// Returns the lowest common ancestor in the paint property tree.
PLATFORM_EXPORT const ClipPaintPropertyNode& LowestCommonAncestor(
    const ClipPaintPropertyNode&,
    const ClipPaintPropertyNode&);
PLATFORM_EXPORT const EffectPaintPropertyNode& LowestCommonAncestor(
    const EffectPaintPropertyNode&,
    const EffectPaintPropertyNode&);
PLATFORM_EXPORT const ScrollPaintPropertyNode& LowestCommonAncestor(
    const ScrollPaintPropertyNode&,
    const ScrollPaintPropertyNode&);
PLATFORM_EXPORT const TransformPaintPropertyNode& LowestCommonAncestor(
    const TransformPaintPropertyNode&,
    const TransformPaintPropertyNode&);

template <typename NodeType>
class PaintPropertyNode : public RefCounted<NodeType> {
 public:
  // Parent property node, or nullptr if this is the root node.
  const NodeType* Parent() const { return parent_.Get(); }
  bool IsRoot() const { return !parent_; }

  // TODO(wangxianzhu): Changed() and ClearChangedToRoot() are inefficient
  // due to the tree walks. Optimize this if this affects overall performance.

  // Returns true if any node (excluding the lowest common ancestor of
  // |relative_to_node| and |this|) is marked changed along the shortest path
  // from |this| to |relative_to_node|.
  bool Changed(const NodeType& relative_to_node) const {
    if (this == &relative_to_node)
      return false;

    bool changed = false;
    for (const auto* n = this; n; n = n->Parent()) {
      if (n == &relative_to_node)
        return changed;
      if (n->changed_)
        changed = true;
    }

    // We reach here if |relative_to_node| is not an ancestor of |this|.
    const auto& lca = LowestCommonAncestor(static_cast<const NodeType&>(*this),
                                           relative_to_node);
    return Changed(lca) || relative_to_node.Changed(lca);
  }

  void ClearChangedToRoot() const {
    for (auto* n = this; n; n = n->Parent())
      n->changed_ = false;
  }

 protected:
  PaintPropertyNode(PassRefPtr<const NodeType> parent)
      : parent_(std::move(parent)), changed_(false) {}

  bool Update(PassRefPtr<const NodeType> parent) {
    DCHECK(!IsRoot());
    DCHECK(parent != this);
    if (parent == parent_)
      return false;

    SetChanged();
    parent_ = std::move(parent);
    return true;
  }

  void SetChanged() { changed_ = true; }

 private:
  RefPtr<const NodeType> parent_;
  mutable bool changed_;
};

}  // namespace blink

#endif  // PaintPropertyNode_h
