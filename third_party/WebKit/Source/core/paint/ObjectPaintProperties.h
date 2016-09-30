// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ObjectPaintProperties_h
#define ObjectPaintProperties_h

#include "core/CoreExport.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/GeometryPropertyTreeState.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

// A complete set of paint properties including those that are inherited from other objects.
// RefPtrs are used to guard against use-after-free bugs and DCHECKs ensure PropertyTreeState never
// retains the last reference to a property tree node.
class PropertyTreeState {
public:
    PropertyTreeState(const TransformPaintPropertyNode* transform,
        const ClipPaintPropertyNode* clip,
        const EffectPaintPropertyNode* effect,
        const ScrollPaintPropertyNode* scroll)
        : m_transform(transform), m_clip(clip), m_effect(effect), m_scroll(scroll)
    {
        DCHECK(!m_transform->hasOneRef() && !m_clip->hasOneRef() && !m_effect->hasOneRef() && !m_scroll->hasOneRef());
    }

    const TransformPaintPropertyNode* transform() const { DCHECK(!m_transform->hasOneRef()); return m_transform.get(); }
    const ClipPaintPropertyNode* clip() const { DCHECK(!m_clip->hasOneRef()); return m_clip.get(); }
    const EffectPaintPropertyNode* effect() const { DCHECK(!m_effect->hasOneRef()); return m_effect.get(); }
    const ScrollPaintPropertyNode* scroll() const { DCHECK(!m_scroll->hasOneRef()); return m_scroll.get(); }

private:
    RefPtr<const TransformPaintPropertyNode> m_transform;
    RefPtr<const ClipPaintPropertyNode> m_clip;
    RefPtr<const EffectPaintPropertyNode> m_effect;
    RefPtr<const ScrollPaintPropertyNode> m_scroll;
};

// This class stores property tree related information associated with a LayoutObject.
// Currently there are two groups of information:
// 1. The set of property nodes created locally by this LayoutObject.
// 2. [Optional] A suite of property nodes (PaintChunkProperties) and paint offset
//    that can be used to paint the border box of this LayoutObject.
class CORE_EXPORT ObjectPaintProperties {
    WTF_MAKE_NONCOPYABLE(ObjectPaintProperties);
    USING_FAST_MALLOC(ObjectPaintProperties);
public:
    struct LocalBorderBoxProperties;

    static std::unique_ptr<ObjectPaintProperties> create()
    {
        return wrapUnique(new ObjectPaintProperties());
    }

    // The hierarchy of the transform subtree created by a LayoutObject is as follows:
    // [ paintOffsetTranslation ]           Normally paint offset is accumulated without creating a node
    // |                                    until we see, for example, transform or position:fixed.
    // +---[ transform ]                    The space created by CSS transform.
    //     |                                This is the local border box space, see: LocalBorderBoxProperties below.
    //     +---[ perspective ]              The space created by CSS perspective.
    //     |   +---[ svgLocalToBorderBoxTransform ] Additional transform for children of the outermost root SVG.
    //     |              OR                (SVG does not support scrolling.)
    //     |   +---[ scrollTranslation ]    The space created by overflow clip.
    //     +---[ scrollbarPaintOffset ]     TODO(trchen): Remove this once we bake the paint offset into frameRect.
    //                                      This is equivalent to the local border box space above,
    //                                      with pixel snapped paint offset baked in. It is really redundant,
    //                                      but it is a pain to teach scrollbars to paint with an offset.
    const TransformPaintPropertyNode* paintOffsetTranslation() const { return m_paintOffsetTranslation.get(); }
    const TransformPaintPropertyNode* transform() const { return m_transform.get(); }
    const TransformPaintPropertyNode* perspective() const { return m_perspective.get(); }
    const TransformPaintPropertyNode* svgLocalToBorderBoxTransform() const { return m_svgLocalToBorderBoxTransform.get(); }
    const TransformPaintPropertyNode* scrollTranslation() const { return m_scrollTranslation.get(); }
    const TransformPaintPropertyNode* scrollbarPaintOffset() const { return m_scrollbarPaintOffset.get(); }

    // Auxiliary scrolling information. Includes information such as the hierarchy of scrollable
    // areas, the extent that can be scrolled, etc. The actual scroll offset is stored in the
    // transform tree (m_scrollTranslation).
    const ScrollPaintPropertyNode* scroll() const { return m_scroll.get(); }

    const EffectPaintPropertyNode* effect() const { return m_effect.get(); }

    // The hierarchy of the clip subtree created by a LayoutObject is as follows:
    // [ css clip ]
    // [ css clip fixed position]
    // |
    // +--- [ overflow clip ]
    const ClipPaintPropertyNode* cssClip() const { return m_cssClip.get(); }
    const ClipPaintPropertyNode* cssClipFixedPosition() const { return m_cssClipFixedPosition.get(); }
    const ClipPaintPropertyNode* overflowClip() const { return m_overflowClip.get(); }

    // This is a complete set of property nodes that should be used as a starting point to paint
    // this layout object. It is needed becauase some property inherits from the containing block,
    // not painting parent, thus can't be derived in O(1) during paint walk.
    // Note: If this layout object has transform or stacking-context effects, those are already
    // baked into in the context here. However for properties that affects only children,
    // for example, perspective and overflow clip, those should be applied by the painter
    // at the right painting step.
    // TODO(pdr): Refactor this to use PropertyTreeState.
    struct LocalBorderBoxProperties {
        LayoutPoint paintOffset;
        GeometryPropertyTreeState geometryPropertyTreeState;
        const ScrollPaintPropertyNode* scroll;
    };
    const LocalBorderBoxProperties* localBorderBoxProperties() const { return m_localBorderBoxProperties.get(); }
    // ContentsPropertyTreeState is the GeometryPropertyTreeState that is the same as in
    // localBorderBoxProperties, except that it is inside any clips and scrolls caused by this
    // object. This GeometryPropertyTreeState is suitable as the destination for paint invalidation.
    // |paintOffsetFromState| is the offset from the GeometryPropertyTreeState to this object's
    // contents space.
    void getContentsPropertyTreeState(GeometryPropertyTreeState&, LayoutPoint& paintOffsetFromState) const;

    void clearPaintOffsetTranslation() { m_paintOffsetTranslation = nullptr; }
    void clearTransform() { m_transform = nullptr; }
    void clearEffect() { m_effect = nullptr; }
    void clearCssClip() { m_cssClip = nullptr; }
    void clearCssClipFixedPosition() { m_cssClipFixedPosition = nullptr; }
    void clearOverflowClip() { m_overflowClip = nullptr; }
    void clearPerspective() { m_perspective = nullptr; }
    void clearSvgLocalToBorderBoxTransform() { m_svgLocalToBorderBoxTransform = nullptr; }
    void clearScrollTranslation() { m_scrollTranslation = nullptr; }
    void clearScrollbarPaintOffset() { m_scrollbarPaintOffset = nullptr; }
    void clearScroll() { m_scroll = nullptr; }

    template <typename... Args> TransformPaintPropertyNode* createOrUpdatePaintOffsetTranslation(Args&&... args) { return createOrUpdateProperty(m_paintOffsetTranslation, std::forward<Args>(args)...); }
    template <typename... Args> TransformPaintPropertyNode* createOrUpdateTransform(Args&&... args) { return createOrUpdateProperty(m_transform, std::forward<Args>(args)...); }
    template <typename... Args> TransformPaintPropertyNode* createOrUpdatePerspective(Args&&... args) { return createOrUpdateProperty(m_perspective, std::forward<Args>(args)...); }
    template <typename... Args> TransformPaintPropertyNode* createOrUpdateSvgLocalToBorderBoxTransform(Args&&... args)
    {
        DCHECK(!scrollTranslation()) << "SVG elements cannot scroll so there should never be both a scroll translation and an SVG local to border box transform.";
        return createOrUpdateProperty(m_svgLocalToBorderBoxTransform, std::forward<Args>(args)...);
    }
    template <typename... Args> TransformPaintPropertyNode* createOrUpdateScrollTranslation(Args&&... args)
    {
        DCHECK(!svgLocalToBorderBoxTransform()) << "SVG elements cannot scroll so there should never be both a scroll translation and an SVG local to border box transform.";
        return createOrUpdateProperty(m_scrollTranslation, std::forward<Args>(args)...);
    }
    template <typename... Args> TransformPaintPropertyNode* createOrUpdateScrollbarPaintOffset(Args&&... args) { return createOrUpdateProperty(m_scrollbarPaintOffset, std::forward<Args>(args)...); }
    template <typename... Args> ScrollPaintPropertyNode* createOrUpdateScroll(Args&&... args) { return createOrUpdateProperty(m_scroll, std::forward<Args>(args)...); }
    template <typename... Args> EffectPaintPropertyNode* createOrUpdateEffect(Args&&... args) { return createOrUpdateProperty(m_effect, std::forward<Args>(args)...); }
    template <typename... Args> ClipPaintPropertyNode* createOrUpdateCssClip(Args&&... args) { return createOrUpdateProperty(m_cssClip, std::forward<Args>(args)...); }
    template <typename... Args> ClipPaintPropertyNode* createOrUpdateCssClipFixedPosition(Args&&... args) { return createOrUpdateProperty(m_cssClipFixedPosition, std::forward<Args>(args)...); }
    template <typename... Args> ClipPaintPropertyNode* createOrUpdateOverflowClip(Args&&... args) { return createOrUpdateProperty(m_overflowClip, std::forward<Args>(args)...); }

    void setLocalBorderBoxProperties(std::unique_ptr<LocalBorderBoxProperties> properties) { m_localBorderBoxProperties = std::move(properties); }

private:
    ObjectPaintProperties() { }

    template <typename PaintPropertyNode, typename... Args>
    PaintPropertyNode* createOrUpdateProperty(RefPtr<PaintPropertyNode>& field, Args&&... args)
    {
        if (field)
            field->update(std::forward<Args>(args)...);
        else
            field = PaintPropertyNode::create(std::forward<Args>(args)...);
        return field.get();
    }

    RefPtr<TransformPaintPropertyNode> m_paintOffsetTranslation;
    RefPtr<TransformPaintPropertyNode> m_transform;
    RefPtr<EffectPaintPropertyNode> m_effect;
    RefPtr<ClipPaintPropertyNode> m_cssClip;
    RefPtr<ClipPaintPropertyNode> m_cssClipFixedPosition;
    RefPtr<ClipPaintPropertyNode> m_overflowClip;
    RefPtr<TransformPaintPropertyNode> m_perspective;
    // TODO(pdr): Only LayoutSVGRoot needs this and it should be moved there.
    RefPtr<TransformPaintPropertyNode> m_svgLocalToBorderBoxTransform;
    RefPtr<TransformPaintPropertyNode> m_scrollTranslation;
    RefPtr<TransformPaintPropertyNode> m_scrollbarPaintOffset;
    RefPtr<ScrollPaintPropertyNode> m_scroll;

    std::unique_ptr<LocalBorderBoxProperties> m_localBorderBoxProperties;
};

} // namespace blink

#endif // ObjectPaintProperties_h
