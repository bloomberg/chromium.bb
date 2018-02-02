// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintPropertyNode_h
#define PaintPropertyNode_h

#include "base/memory/scoped_refptr.h"
#include "platform/PlatformExport.h"
#include "platform/json/JSONValues.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/text/WTFString.h"

#if DCHECK_IS_ON()
#include "platform/wtf/ListHashSet.h"
#include "platform/wtf/text/StringBuilder.h"
#endif

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
  const NodeType* Parent() const { return parent_.get(); }
  bool IsRoot() const { return !parent_; }

  bool IsAncestorOf(const NodeType& other) const {
    for (const NodeType* node = &other; node != this; node = node->Parent()) {
      if (!node)
        return false;
    }
    return true;
  }

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

  String ToString() const {
    auto s = static_cast<const NodeType*>(this)->ToJSON()->ToJSONString();
#if DCHECK_IS_ON()
    return debug_name_ + String::Format(" %p ", this) + s;
#else
    return s;
#endif
  }

#if DCHECK_IS_ON()
  String ToTreeString() const;

  String DebugName() const { return debug_name_; }
  void SetDebugName(const String& name) { debug_name_ = name; }
#endif

 protected:
  PaintPropertyNode(scoped_refptr<const NodeType> parent)
      : parent_(std::move(parent)), changed_(false) {}

  bool Update(scoped_refptr<const NodeType> parent) {
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
  scoped_refptr<const NodeType> parent_;
  mutable bool changed_;

#if DCHECK_IS_ON()
  String debug_name_;
#endif
};

#if DCHECK_IS_ON()

template <typename NodeType>
class PropertyTreePrinter {
 public:
  void AddNode(const NodeType* node) {
    if (node)
      nodes_.insert(node);
  }

  String NodesAsTreeString() {
    if (nodes_.IsEmpty())
      return "";
    StringBuilder string_builder;
    BuildTreeString(string_builder, RootNode(), 0);
    return string_builder.ToString();
  }

  String PathAsString(const NodeType* last_node) {
    for (const auto* n = last_node; !n->IsRoot(); n = n->Parent())
      AddNode(n);
    return NodesAsTreeString();
  }

 private:
  void BuildTreeString(StringBuilder& string_builder,
                       const NodeType* node,
                       unsigned indent) {
    DCHECK(node);
    for (unsigned i = 0; i < indent; i++)
      string_builder.Append(' ');
    string_builder.Append(node->ToString());
    string_builder.Append("\n");

    for (const auto* child_node : nodes_) {
      if (child_node->Parent() == node)
        BuildTreeString(string_builder, child_node, indent + 2);
    }
  }

  const NodeType* RootNode() {
    const auto* node = nodes_.back();
    while (!node->IsRoot())
      node = node->Parent();
    if (node->DebugName().IsEmpty())
      const_cast<NodeType*>(node)->SetDebugName("root");
    nodes_.insert(node);
    return node;
  }

  ListHashSet<const NodeType*> nodes_;
};

template <typename NodeType>
String PaintPropertyNode<NodeType>::ToTreeString() const {
  return PropertyTreePrinter<NodeType>().PathAsString(
      static_cast<const NodeType*>(this));
}

#endif  // DCHECK_IS_ON()

}  // namespace blink

#endif  // PaintPropertyNode_h
