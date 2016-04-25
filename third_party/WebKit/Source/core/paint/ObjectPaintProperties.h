// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ObjectPaintProperties_h
#define ObjectPaintProperties_h

#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

// This class stores property tree related information associated with a LayoutObject.
// Currently there are two groups of information:
// 1. The set of property nodes created locally by this LayoutObject.
// 2. [Optional] A suite of property nodes (PaintChunkProperties) and paint offset
//    that can be used to paint the border box of this LayoutObject.
class ObjectPaintProperties {
    WTF_MAKE_NONCOPYABLE(ObjectPaintProperties);
    USING_FAST_MALLOC(ObjectPaintProperties);
public:
    struct LocalBorderBoxProperties;

    static PassOwnPtr<ObjectPaintProperties> create()
    {
        return adoptPtr(new ObjectPaintProperties());
    }

    // The hierarchy of transform subtree created by a LayoutObject.
    // [ paintOffsetTranslation ]           Normally paint offset is accumulated without creating a node
    // |                                    until we see, for example, transform or position:fixed.
    // +---[ transform ]                    The space created by CSS transform.
    //     |                                This is the local border box space, see: LocalBorderBoxProperties below.
    //     +---[ perspective ]              The space created by CSS perspective.
    //     |   +---[ svgLocalTransform ]    The transform for an SVG element.
    //     |              OR                (SVG does not support scrolling.)
    //     |   +---[ scrollTranslation ]    The space created by overflow clip.
    //     +---[ scrollbarPaintOffset ]     TODO(trchen): Remove this once we bake the paint offset into frameRect.
    //                                      This is equivalent to the local border box space above,
    //                                      with pixel snapped paint offset baked in. It is really redundant,
    //                                      but it is a pain to teach scrollbars to paint with an offset.
    TransformPaintPropertyNode* paintOffsetTranslation() const { return m_paintOffsetTranslation.get(); }
    TransformPaintPropertyNode* transform() const { return m_transform.get(); }
    TransformPaintPropertyNode* perspective() const { return m_perspective.get(); }
    TransformPaintPropertyNode* svgLocalTransform() const { return m_svgLocalTransform.get(); }
    TransformPaintPropertyNode* scrollTranslation() const { return m_scrollTranslation.get(); }
    TransformPaintPropertyNode* scrollbarPaintOffset() const { return m_scrollbarPaintOffset.get(); }

    EffectPaintPropertyNode* effect() const { return m_effect.get(); }

    ClipPaintPropertyNode* cssClip() const { return m_cssClip.get(); }
    ClipPaintPropertyNode* cssClipFixedPosition() const { return m_cssClipFixedPosition.get(); }
    ClipPaintPropertyNode* overflowClip() const { return m_overflowClip.get(); }

    // This is a complete set of property nodes that should be used as a starting point to paint
    // this layout object. It is needed becauase some property inherits from the containing block,
    // not painting parent, thus can't be derived in O(1) during paint walk.
    // Note: If this layout object has transform or stacking-context effects, those are already
    // baked into in the context here. However for properties that affects only children,
    // for example, perspective and overflow clip, those should be applied by the painter
    // at the right painting step.
    struct LocalBorderBoxProperties {
        LayoutPoint paintOffset;
        RefPtr<TransformPaintPropertyNode> transform;
        RefPtr<ClipPaintPropertyNode> clip;
        RefPtr<EffectPaintPropertyNode> effect;
    };
    LocalBorderBoxProperties* localBorderBoxProperties() const { return m_localBorderBoxProperties.get(); }

private:
    ObjectPaintProperties() { }

    friend class PaintPropertyTreeBuilder;
    // These setters should only be used by PaintPropertyTreeBuilder.
    void setPaintOffsetTranslation(PassRefPtr<TransformPaintPropertyNode> paintOffset) { m_paintOffsetTranslation = paintOffset; }
    void setTransform(PassRefPtr<TransformPaintPropertyNode> transform) { m_transform = transform; }
    void setEffect(PassRefPtr<EffectPaintPropertyNode> effect) { m_effect = effect; }
    void setCssClip(PassRefPtr<ClipPaintPropertyNode> clip) { m_cssClip = clip; }
    void setCssClipFixedPosition(PassRefPtr<ClipPaintPropertyNode> clip) { m_cssClipFixedPosition = clip; }
    void setOverflowClip(PassRefPtr<ClipPaintPropertyNode> clip) { m_overflowClip = clip; }
    void setPerspective(PassRefPtr<TransformPaintPropertyNode> perspective) { m_perspective = perspective; }
    void setSvgLocalTransform(PassRefPtr<TransformPaintPropertyNode> transform)
    {
        DCHECK(!scrollTranslation()) << "SVG elements cannot scroll so there should never be both a scroll translation and an SVG local transform.";
        m_svgLocalTransform = transform;
    }
    void setScrollTranslation(PassRefPtr<TransformPaintPropertyNode> translation)
    {
        DCHECK(!svgLocalTransform()) << "SVG elements cannot scroll so there should never be both a scroll translation and an SVG local transform.";
        m_scrollTranslation = translation;
    }
    void setScrollbarPaintOffset(PassRefPtr<TransformPaintPropertyNode> paintOffset) { m_scrollbarPaintOffset = paintOffset; }
    void setLocalBorderBoxProperties(PassOwnPtr<LocalBorderBoxProperties> properties) { m_localBorderBoxProperties = std::move(properties); }

    RefPtr<TransformPaintPropertyNode> m_paintOffsetTranslation;
    RefPtr<TransformPaintPropertyNode> m_transform;
    RefPtr<EffectPaintPropertyNode> m_effect;
    RefPtr<ClipPaintPropertyNode> m_cssClip;
    RefPtr<ClipPaintPropertyNode> m_cssClipFixedPosition;
    RefPtr<ClipPaintPropertyNode> m_overflowClip;
    RefPtr<TransformPaintPropertyNode> m_perspective;
    RefPtr<TransformPaintPropertyNode> m_svgLocalTransform;
    RefPtr<TransformPaintPropertyNode> m_scrollTranslation;
    RefPtr<TransformPaintPropertyNode> m_scrollbarPaintOffset;

    OwnPtr<LocalBorderBoxProperties> m_localBorderBoxProperties;
};

} // namespace blink

#endif // ObjectPaintProperties_h
