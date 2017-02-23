// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PropertyTreeState_h
#define PropertyTreeState_h

#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "wtf/HashFunctions.h"
#include "wtf/HashTraits.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

// A complete set of paint properties including those that are inherited from
// other objects.  RefPtrs are used to guard against use-after-free bugs and
// DCHECKs ensure PropertyTreeState never retains the last reference to a
// property tree node.
class PLATFORM_EXPORT PropertyTreeState {
 public:
  PropertyTreeState(const TransformPaintPropertyNode* transform,
                    const ClipPaintPropertyNode* clip,
                    const EffectPaintPropertyNode* effect)
      : m_transform(transform), m_clip(clip), m_effect(effect) {
    DCHECK(!m_transform || !m_transform->hasOneRef());
    DCHECK(!m_clip || !m_clip->hasOneRef());
    DCHECK(!m_effect || !m_effect->hasOneRef());
  }

  bool hasDirectCompositingReasons() const;

  const TransformPaintPropertyNode* transform() const {
    DCHECK(!m_transform || !m_transform->hasOneRef());
    return m_transform.get();
  }
  void setTransform(RefPtr<const TransformPaintPropertyNode> node) {
    m_transform = std::move(node);
  }

  const ClipPaintPropertyNode* clip() const {
    DCHECK(!m_clip || !m_clip->hasOneRef());
    return m_clip.get();
  }
  void setClip(RefPtr<const ClipPaintPropertyNode> node) {
    m_clip = std::move(node);
  }

  const EffectPaintPropertyNode* effect() const {
    DCHECK(!m_effect || !m_effect->hasOneRef());
    return m_effect.get();
  }
  void setEffect(RefPtr<const EffectPaintPropertyNode> node) {
    m_effect = std::move(node);
  }

  // Returns the compositor element id, if any, for this property state. If
  // neither the effect nor transform nodes have a compositor element id then a
  // default instance is returned.
  const CompositorElementId compositorElementId() const;

  enum InnermostNode {
    None,  // None means that all nodes are their root values
    Transform,
    Clip,
    Effect,
  };

  // There is always a well-defined order in which the transform, clip
  // and effects of a PropertyTreeState apply. This method returns which
  // of them applies first to content drawn with this PropertyTreeState.
  // Note that it may be the case that multiple nodes from the same tree apply
  // before any from another tree. This can happen, for example, if multiple
  // effects or clips apply to a descendant transform state from the transform
  // node.
  //
  // This method is meant to be used in concert with
  // |PropertyTreeStateIterator|, which allows one to iterate over the nodes in
  // the order in which they apply.
  //
  // Example:
  //
  //  Transform tree      Clip tree      Effect tree
  //  ~~~~~~~~~~~~~~      ~~~~~~~~~      ~~~~~~~~~~~
  //       Root              Root            Root
  //        |                 |               |
  //        T                 C               E
  //
  // Suppose that PropertyTreeState(T, C, E).innerMostNode() is E, and
  // PropertytreeState(T, C, Root).innermostNode() is C. Then a PaintChunk
  // that has propertyTreeState = PropertyTreeState(T, C, E) can be painted
  // with the following display list structure:
  //
  // [BeginTransform] [BeginClip] [BeginEffect] PaintChunk drawings
  //    [EndEffect] [EndClip] [EndTransform]
  //
  // The PropertyTreeStateIterator will behave like this:
  //
  // PropertyTreeStateIterator iterator(PropertyTreeState(T, C, E));
  // DCHECK(iterator.innermostNode() == Effect);
  // DCHECK(iterator.next()->innermostNode() == Clip);
  // DCHECK(iterator.next()->innermostNode() == Transform);
  // DCHECK(iterator.next()->innermostNode() == None);
  InnermostNode innermostNode() const;

#if DCHECK_IS_ON()
  // Dumps the tree from this state up to the root as a string.
  String toTreeString() const;
#endif

 private:
  RefPtr<const TransformPaintPropertyNode> m_transform;
  RefPtr<const ClipPaintPropertyNode> m_clip;
  RefPtr<const EffectPaintPropertyNode> m_effect;
};

inline bool operator==(const PropertyTreeState& a, const PropertyTreeState& b) {
  return a.transform() == b.transform() && a.clip() == b.clip() &&
         a.effect() == b.effect();
}

// Iterates over the sequence transforms, clips and effects for a
// PropertyTreeState between that state and the "root" state (all nodes equal
// to *::Root()), in the order that they apply.
//
// See also PropertyTreeState::innermostNode for a more detailed example.
class PLATFORM_EXPORT PropertyTreeStateIterator {
 public:
  PropertyTreeStateIterator(const PropertyTreeState& properties)
      : m_properties(properties) {}
  const PropertyTreeState* next();

 private:
  PropertyTreeState m_properties;
};

#if DCHECK_IS_ON()

template <typename PropertyTreeNode>
class PropertyTreeStatePrinter {
 public:
  String pathAsString(const PropertyTreeNode* lastNode) {
    const PropertyTreeNode* node = lastNode;
    while (!node->isRoot()) {
      addPropertyNode(node, "");
      node = node->parent();
    }
    addPropertyNode(node, "root");

    StringBuilder stringBuilder;
    addAllPropertyNodes(stringBuilder, node);
    return stringBuilder.toString();
  }

  void addPropertyNode(const PropertyTreeNode* node, String debugInfo) {
    m_nodeToDebugString.set(node, debugInfo);
  }

  void addAllPropertyNodes(StringBuilder& stringBuilder,
                           const PropertyTreeNode* node,
                           unsigned indent = 0) {
    DCHECK(node);
    for (unsigned i = 0; i < indent; i++)
      stringBuilder.append(' ');
    if (m_nodeToDebugString.contains(node))
      stringBuilder.append(m_nodeToDebugString.at(node));
    stringBuilder.append(String::format(" %p ", node));
    stringBuilder.append(node->toString());
    stringBuilder.append("\n");

    for (const auto* childNode : m_nodeToDebugString.keys()) {
      if (childNode->parent() == node)
        addAllPropertyNodes(stringBuilder, childNode, indent + 2);
    }
  }

  HashMap<const PropertyTreeNode*, String> m_nodeToDebugString;
};

#endif

}  // namespace blink

#endif  // PropertyTreeState_h
