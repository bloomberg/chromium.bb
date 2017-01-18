// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PropertyTreeState_h
#define PropertyTreeState_h

#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "wtf/HashFunctions.h"
#include "wtf/HashTraits.h"

namespace blink {

// A complete set of paint properties including those that are inherited from
// other objects.  RefPtrs are used to guard against use-after-free bugs and
// DCHECKs ensure PropertyTreeState never retains the last reference to a
// property tree node.
class PLATFORM_EXPORT PropertyTreeState {
 public:
  PropertyTreeState(const TransformPaintPropertyNode* transform,
                    const ClipPaintPropertyNode* clip,
                    const EffectPaintPropertyNode* effect,
                    const ScrollPaintPropertyNode* scroll)
      : m_transform(transform),
        m_clip(clip),
        m_effect(effect),
        m_scroll(scroll) {
    DCHECK(!m_transform || !m_transform->hasOneRef());
    DCHECK(!m_clip || !m_clip->hasOneRef());
    DCHECK(!m_effect || !m_effect->hasOneRef());
    DCHECK(!m_scroll || !m_scroll->hasOneRef());
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

  const ScrollPaintPropertyNode* scroll() const {
    DCHECK(!m_scroll || !m_scroll->hasOneRef());
    return m_scroll.get();
  }
  void setScroll(RefPtr<const ScrollPaintPropertyNode> node) {
    m_scroll = std::move(node);
  }

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

 private:
  RefPtr<const TransformPaintPropertyNode> m_transform;
  RefPtr<const ClipPaintPropertyNode> m_clip;
  RefPtr<const EffectPaintPropertyNode> m_effect;
  RefPtr<const ScrollPaintPropertyNode> m_scroll;
};

inline bool operator==(const PropertyTreeState& a, const PropertyTreeState& b) {
  return a.transform() == b.transform() && a.clip() == b.clip() &&
         a.effect() == b.effect() && a.scroll() == b.scroll();
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

}  // namespace blink

#endif  // PropertyTreeState_h
