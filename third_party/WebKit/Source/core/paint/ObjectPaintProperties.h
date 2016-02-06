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

    static PassOwnPtr<ObjectPaintProperties> create(
        PassRefPtr<TransformPaintPropertyNode> paintOffsetTranslation,
        PassRefPtr<TransformPaintPropertyNode> transform,
        PassRefPtr<EffectPaintPropertyNode> effect,
        PassRefPtr<ClipPaintPropertyNode> overflowClip,
        PassRefPtr<TransformPaintPropertyNode> perspective,
        PassRefPtr<TransformPaintPropertyNode> scrollTranslation,
        PassOwnPtr<LocalBorderBoxProperties> localBorderBoxProperties)
    {
        return adoptPtr(new ObjectPaintProperties(paintOffsetTranslation, transform, effect, overflowClip, perspective, scrollTranslation, localBorderBoxProperties));
    }

    // The hierarchy of transform subtree created by a LayoutObject.
    // [ paintOffsetTranslation ]           Normally paint offset is accumulated without creating a node
    // |                                    until we see, for example, transform or position:fixed.
    // +---[ transform ]                    The space created by CSS transform.
    //     |                                This is the local border box space, see: LocalBorderBoxProperties below.
    //     +---[ perspective ]              The space created by CSS perspective.
    //         +---[ scrollTranslation ]    The space created by overflow clip.
    TransformPaintPropertyNode* paintOffsetTranslation() const { return m_paintOffsetTranslation.get(); }
    TransformPaintPropertyNode* transform() const { return m_transform.get(); }
    TransformPaintPropertyNode* perspective() const { return m_perspective.get(); }
    TransformPaintPropertyNode* scrollTranslation() const { return m_scrollTranslation.get(); }

    EffectPaintPropertyNode* effect() const { return m_effect.get(); }

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
        PaintChunkProperties properties;
    };
    LocalBorderBoxProperties* localBorderBoxProperties() const { return m_localBorderBoxProperties.get(); }

private:
    ObjectPaintProperties(
        PassRefPtr<TransformPaintPropertyNode> paintOffsetTranslation,
        PassRefPtr<TransformPaintPropertyNode> transform,
        PassRefPtr<EffectPaintPropertyNode> effect,
        PassRefPtr<ClipPaintPropertyNode> overflowClip,
        PassRefPtr<TransformPaintPropertyNode> perspective,
        PassRefPtr<TransformPaintPropertyNode> scrollTranslation,
        PassOwnPtr<LocalBorderBoxProperties> localBorderBoxProperties)
        : m_paintOffsetTranslation(paintOffsetTranslation)
        , m_transform(transform)
        , m_effect(effect)
        , m_overflowClip(overflowClip)
        , m_perspective(perspective)
        , m_scrollTranslation(scrollTranslation)
        , m_localBorderBoxProperties(localBorderBoxProperties) { }

    RefPtr<TransformPaintPropertyNode> m_paintOffsetTranslation;
    RefPtr<TransformPaintPropertyNode> m_transform;
    RefPtr<EffectPaintPropertyNode> m_effect;
    RefPtr<ClipPaintPropertyNode> m_overflowClip;
    RefPtr<TransformPaintPropertyNode> m_perspective;
    RefPtr<TransformPaintPropertyNode> m_scrollTranslation;

    OwnPtr<LocalBorderBoxProperties> m_localBorderBoxProperties;
};

} // namespace blink

#endif // ObjectPaintProperties_h
