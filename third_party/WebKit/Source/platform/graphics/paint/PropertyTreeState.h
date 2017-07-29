// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PropertyTreeState_h
#define PropertyTreeState_h

#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/wtf/HashFunctions.h"
#include "platform/wtf/HashTraits.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

// A complete set of paint properties including those that are inherited from
// other objects.  RefPtrs are used to guard against use-after-free bugs.
class PLATFORM_EXPORT PropertyTreeState {
  USING_FAST_MALLOC(PropertyTreeState);

 public:
  PropertyTreeState(const TransformPaintPropertyNode* transform,
                    const ClipPaintPropertyNode* clip,
                    const EffectPaintPropertyNode* effect)
      : transform_(transform), clip_(clip), effect_(effect) {}

  bool HasDirectCompositingReasons() const;

  const TransformPaintPropertyNode* Transform() const {
    return transform_.Get();
  }
  void SetTransform(RefPtr<const TransformPaintPropertyNode> node) {
    transform_ = std::move(node);
  }

  const ClipPaintPropertyNode* Clip() const {
    return clip_.Get();
  }
  void SetClip(RefPtr<const ClipPaintPropertyNode> node) {
    clip_ = std::move(node);
  }

  const EffectPaintPropertyNode* Effect() const {
    return effect_.Get();
  }
  void SetEffect(RefPtr<const EffectPaintPropertyNode> node) {
    effect_ = std::move(node);
  }

  static const PropertyTreeState& Root();

  // Returns the compositor element id, if any, for this property state. If
  // neither the effect nor transform nodes have a compositor element id then a
  // default instance is returned.
  const CompositorElementId GetCompositorElementId(
      const CompositorElementIdSet& element_ids) const;

  // See PaintPropertyNode::Changed().
  bool Changed(const PropertyTreeState& relative_to_state) const {
    return Transform()->Changed(*relative_to_state.Transform()) ||
           Clip()->Changed(*relative_to_state.Clip()) ||
           Effect()->Changed(*relative_to_state.Effect());
  }

  void ClearChangedToRoot() const {
    Transform()->ClearChangedToRoot();
    Clip()->ClearChangedToRoot();
    Effect()->ClearChangedToRoot();
  }

#if DCHECK_IS_ON()
  // Dumps the tree from this state up to the root as a string.
  String ToTreeString() const;
#endif

 private:
  RefPtr<const TransformPaintPropertyNode> transform_;
  RefPtr<const ClipPaintPropertyNode> clip_;
  RefPtr<const EffectPaintPropertyNode> effect_;
};

inline bool operator==(const PropertyTreeState& a, const PropertyTreeState& b) {
  return a.Transform() == b.Transform() && a.Clip() == b.Clip() &&
         a.Effect() == b.Effect();
}

#if DCHECK_IS_ON()

template <typename PropertyTreeNode>
class PropertyTreeStatePrinter {
 public:
  String PathAsString(const PropertyTreeNode* last_node) {
    const PropertyTreeNode* node = last_node;
    while (!node->IsRoot()) {
      AddPropertyNode(node, "");
      node = node->Parent();
    }
    AddPropertyNode(node, "root");

    StringBuilder string_builder;
    AddAllPropertyNodes(string_builder, node);
    return string_builder.ToString();
  }

  void AddPropertyNode(const PropertyTreeNode* node, String debug_info) {
    node_to_debug_string_.Set(node, debug_info);
  }

  void AddAllPropertyNodes(StringBuilder& string_builder,
                           const PropertyTreeNode* node,
                           unsigned indent = 0) {
    DCHECK(node);
    for (unsigned i = 0; i < indent; i++)
      string_builder.Append(' ');
    if (node_to_debug_string_.Contains(node))
      string_builder.Append(node_to_debug_string_.at(node));
    string_builder.Append(String::Format(" %p ", node));
    string_builder.Append(node->ToString());
    string_builder.Append("\n");

    for (const auto* child_node : node_to_debug_string_.Keys()) {
      if (child_node->Parent() == node)
        AddAllPropertyNodes(string_builder, child_node, indent + 2);
    }
  }

  HashMap<const PropertyTreeNode*, String> node_to_debug_string_;
};

#endif

}  // namespace blink

#endif  // PropertyTreeState_h
