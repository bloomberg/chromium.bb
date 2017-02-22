// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EffectPaintPropertyNode_h
#define EffectPaintPropertyNode_h

#include "cc/layers/layer.h"
#include "platform/PlatformExport.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/graphics/CompositorFilterOperations.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

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
  // This node is really a sentinel, and does not represent a real effect.
  static EffectPaintPropertyNode* root();

  static PassRefPtr<EffectPaintPropertyNode> create(
      PassRefPtr<const EffectPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> localTransformSpace,
      PassRefPtr<const ClipPaintPropertyNode> outputClip,
      ColorFilter colorFilter,
      CompositorFilterOperations filter,
      float opacity,
      SkBlendMode blendMode,
      CompositingReasons directCompositingReasons = CompositingReasonNone,
      const CompositorElementId& compositorElementId = CompositorElementId(),
      const FloatPoint& paintOffset = FloatPoint()) {
    return adoptRef(new EffectPaintPropertyNode(
        std::move(parent), std::move(localTransformSpace),
        std::move(outputClip), colorFilter, std::move(filter), opacity,
        blendMode, directCompositingReasons, compositorElementId, paintOffset));
  }

  void update(
      PassRefPtr<const EffectPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> localTransformSpace,
      PassRefPtr<const ClipPaintPropertyNode> outputClip,
      ColorFilter colorFilter,
      CompositorFilterOperations filter,
      float opacity,
      SkBlendMode blendMode,
      CompositingReasons directCompositingReasons = CompositingReasonNone,
      const CompositorElementId& compositorElementId = CompositorElementId(),
      const FloatPoint& paintOffset = FloatPoint()) {
    DCHECK(!isRoot());
    DCHECK(parent != this);
    m_parent = parent;
    m_localTransformSpace = localTransformSpace;
    m_outputClip = outputClip;
    m_colorFilter = colorFilter;
    m_filter = std::move(filter);
    m_opacity = opacity;
    m_blendMode = blendMode;
    m_directCompositingReasons = directCompositingReasons;
    m_compositorElementId = compositorElementId;
    m_paintOffset = paintOffset;
  }

  const TransformPaintPropertyNode* localTransformSpace() const {
    return m_localTransformSpace.get();
  }
  const ClipPaintPropertyNode* outputClip() const { return m_outputClip.get(); }

  SkBlendMode blendMode() const { return m_blendMode; }
  float opacity() const { return m_opacity; }
  const CompositorFilterOperations& filter() const { return m_filter; }
  ColorFilter colorFilter() const { return m_colorFilter; }

  // Parent effect or nullptr if this is the root effect.
  const EffectPaintPropertyNode* parent() const { return m_parent.get(); }
  bool isRoot() const { return !m_parent; }

  bool hasFilterThatMovesPixels() const {
    return m_filter.hasFilterThatMovesPixels();
  }

  // Returns a rect covering the pixels that can be affected by pixels in
  // |inputRect|. The rects are in the space of localTransformSpace.
  FloatRect mapRect(const FloatRect& inputRect) const;

  cc::Layer* ensureDummyLayer() const;

#if DCHECK_IS_ON()
  // The clone function is used by FindPropertiesNeedingUpdate.h for recording
  // an effect node before it has been updated, to later detect changes.
  PassRefPtr<EffectPaintPropertyNode> clone() const {
    return adoptRef(new EffectPaintPropertyNode(
        m_parent, m_localTransformSpace, m_outputClip, m_colorFilter, m_filter,
        m_opacity, m_blendMode, m_directCompositingReasons,
        m_compositorElementId, m_paintOffset));
  }

  // The equality operator is used by FindPropertiesNeedingUpdate.h for checking
  // if an effect node has changed. It ignores changes of reference filters
  // because SkImageFilter doesn't have an equality operator.
  bool operator==(const EffectPaintPropertyNode& o) const {
    return m_parent == o.m_parent &&
           m_localTransformSpace == o.m_localTransformSpace &&
           m_outputClip == o.m_outputClip && m_colorFilter == o.m_colorFilter &&
           m_filter.equalsIgnoringReferenceFilters(o.m_filter) &&
           m_opacity == o.m_opacity && m_blendMode == o.m_blendMode &&
           m_directCompositingReasons == o.m_directCompositingReasons &&
           m_compositorElementId == o.m_compositorElementId &&
           m_paintOffset == o.m_paintOffset;
  }

  String toTreeString() const;
#endif

  String toString() const;

  bool hasDirectCompositingReasons() const {
    return m_directCompositingReasons != CompositingReasonNone;
  }

  bool requiresCompositingForAnimation() const {
    return m_directCompositingReasons & CompositingReasonActiveAnimation;
  }

  const CompositorElementId& compositorElementId() const {
    return m_compositorElementId;
  }

 private:
  EffectPaintPropertyNode(
      PassRefPtr<const EffectPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> localTransformSpace,
      PassRefPtr<const ClipPaintPropertyNode> outputClip,
      ColorFilter colorFilter,
      CompositorFilterOperations filter,
      float opacity,
      SkBlendMode blendMode,
      CompositingReasons directCompositingReasons,
      CompositorElementId compositorElementId,
      const FloatPoint& paintOffset)
      : m_parent(parent),
        m_localTransformSpace(localTransformSpace),
        m_outputClip(outputClip),
        m_colorFilter(colorFilter),
        m_filter(std::move(filter)),
        m_opacity(opacity),
        m_blendMode(blendMode),
        m_directCompositingReasons(directCompositingReasons),
        m_compositorElementId(compositorElementId),
        m_paintOffset(paintOffset) {}

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
  ColorFilter m_colorFilter;
  CompositorFilterOperations m_filter;
  float m_opacity;
  SkBlendMode m_blendMode;
  // === End of effects ===

  // TODO(trchen): Remove the dummy layer.
  // The main purpose of the dummy layer is to maintain a permanent identity
  // to associate with cc::RenderSurfaceImpl for damage tracking. This shall
  // be removed in favor of a stable ID once cc::LayerImpl no longer owns
  // RenderSurfaceImpl.
  mutable scoped_refptr<cc::Layer> m_dummyLayer;

  CompositingReasons m_directCompositingReasons;
  CompositorElementId m_compositorElementId;

  // The offset of the effect's local space in m_localTransformSpace. Some
  // effects e.g. reflection need this to apply geometry effects in the local
  // space.
  FloatPoint m_paintOffset;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const EffectPaintPropertyNode&, std::ostream*);

}  // namespace blink

#endif  // EffectPaintPropertyNode_h
