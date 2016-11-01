// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EffectPaintPropertyNode_h
#define EffectPaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "platform/graphics/CompositorFilterOperations.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

#include <iosfwd>

namespace blink {

// Effect nodes are abstraction of isolated groups, along with optional effects
// that can be applied to the composited output of the group.
//
// The effect tree is rooted at a node with no parent. This root node should
// not be modified.
class PLATFORM_EXPORT EffectPaintPropertyNode
    : public RefCounted<EffectPaintPropertyNode> {
 public:
  static EffectPaintPropertyNode* root();

  static PassRefPtr<EffectPaintPropertyNode> create(
      PassRefPtr<const EffectPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> localTransformSpace,
      PassRefPtr<const ClipPaintPropertyNode> outputClip,
      CompositorFilterOperations filter,
      float opacity) {
    return adoptRef(new EffectPaintPropertyNode(
        std::move(parent), std::move(localTransformSpace),
        std::move(outputClip), std::move(filter), opacity));
  }

  void update(PassRefPtr<const EffectPaintPropertyNode> parent,
              PassRefPtr<const TransformPaintPropertyNode> localTransformSpace,
              PassRefPtr<const ClipPaintPropertyNode> outputClip,
              CompositorFilterOperations filter,
              float opacity) {
    DCHECK(!isRoot());
    DCHECK(parent != this);
    m_parent = parent;
    m_localTransformSpace = localTransformSpace;
    m_outputClip = outputClip;
    m_filter = std::move(filter);
    m_opacity = opacity;
  }

  const TransformPaintPropertyNode* localTransformSpace() const {
    return m_localTransformSpace.get();
  }
  const ClipPaintPropertyNode* outputClip() const { return m_outputClip.get(); }

  float opacity() const { return m_opacity; }
  const CompositorFilterOperations& filter() const { return m_filter; }

  // Parent effect or nullptr if this is the root effect.
  const EffectPaintPropertyNode* parent() const { return m_parent.get(); }
  bool isRoot() const { return !m_parent; }

 private:
  EffectPaintPropertyNode(
      PassRefPtr<const EffectPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> localTransformSpace,
      PassRefPtr<const ClipPaintPropertyNode> outputClip,
      CompositorFilterOperations filter,
      float opacity)
      : m_parent(parent),
        m_localTransformSpace(localTransformSpace),
        m_outputClip(outputClip),
        m_filter(std::move(filter)),
        m_opacity(opacity) {}

  RefPtr<const EffectPaintPropertyNode> m_parent;
  // The local transform space serves two purposes:
  // 1. Assign a depth mapping for 3D depth sorting against other paint chunks
  //    and effects under the same parent.
  // 2. Some effects are spatial (namely blur filter and reflection), the
  //    effect parameters will be specified in the local space.
  RefPtr<const TransformPaintPropertyNode> m_localTransformSpace;
  // The output of the effect can be optionally clipped when composited onto
  // the current backdrop.
  RefPtr<const ClipPaintPropertyNode> m_outputClip;

  // Optionally a number of effects can be applied to the composited output.
  // The chain of effects will be applied in the following order:
  // === Begin of effects ===
  CompositorFilterOperations m_filter;
  float m_opacity;
  // === End of effects ===
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const EffectPaintPropertyNode&, std::ostream*);

}  // namespace blink

#endif  // EffectPaintPropertyNode_h
