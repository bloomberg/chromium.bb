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
class PropertyTreeState {
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

}  // namespace blink

#endif  // PropertyTreeState_h
