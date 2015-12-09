// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ObjectPaintProperties_h
#define ObjectPaintProperties_h

#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

// The minimal set of paint properties created by a |LayoutObject|. These
// properties encode a hierachy of transforms, clips, effects, etc, both between
// LayoutObjects (each property has a parent) and among the properties of a
// single LayoutObject (e.g., transform and perspective with the correct parent
// relationship to represent ordering).
//
// This differs from |PaintChunkProperties| because it can store multiple
// properties of the same type (e.g., transform and perspective which are both
// transforms).
class ObjectPaintProperties {
    WTF_MAKE_NONCOPYABLE(ObjectPaintProperties);
    USING_FAST_MALLOC(ObjectPaintProperties);
public:
    static PassOwnPtr<ObjectPaintProperties> create(
        PassRefPtr<TransformPaintPropertyNode> paintOffsetTranslation,
        PassRefPtr<TransformPaintPropertyNode> transform,
        PassRefPtr<EffectPaintPropertyNode> effect,
        PassRefPtr<ClipPaintPropertyNode> overflowClip,
        PassRefPtr<TransformPaintPropertyNode> perspective,
        PassRefPtr<TransformPaintPropertyNode> scrollTranslation)
    {
        return adoptPtr(new ObjectPaintProperties(paintOffsetTranslation, transform, effect, overflowClip, perspective, scrollTranslation));
    }

    // The hierarchy of transform subtree created by a LayoutObject.
    // [ paintOffsetTranslation ]           Normally paint offset is accumulated without creating a node
    // |                                    until we see, for example, transform or position:fixed.
    // +---[ transform ]                    The space created by CSS transform.
    //     +---[ perspective ]              The space created by CSS perspective.
    //         +---[ scrollTranslation ]    The space created by overflow clip.
    TransformPaintPropertyNode* paintOffsetTranslation() const { return m_paintOffsetTranslation.get(); }
    TransformPaintPropertyNode* transform() const { return m_transform.get(); }
    TransformPaintPropertyNode* perspective() const { return m_perspective.get(); }
    TransformPaintPropertyNode* scrollTranslation() const { return m_scrollTranslation.get(); }

    // Transform that applies to layer contents, or nullptr if this object
    // doesn't define one.
    TransformPaintPropertyNode* transformForLayerContents() const;

    EffectPaintPropertyNode* effect() const { return m_effect.get(); }

    ClipPaintPropertyNode* overflowClip() const { return m_overflowClip.get(); }

private:
    ObjectPaintProperties(
        PassRefPtr<TransformPaintPropertyNode> paintOffsetTranslation,
        PassRefPtr<TransformPaintPropertyNode> transform,
        PassRefPtr<EffectPaintPropertyNode> effect,
        PassRefPtr<ClipPaintPropertyNode> overflowClip,
        PassRefPtr<TransformPaintPropertyNode> perspective,
        PassRefPtr<TransformPaintPropertyNode> scrollTranslation)
        : m_paintOffsetTranslation(paintOffsetTranslation)
        , m_transform(transform)
        , m_effect(effect)
        , m_overflowClip(overflowClip)
        , m_perspective(perspective)
        , m_scrollTranslation(scrollTranslation) { }

    RefPtr<TransformPaintPropertyNode> m_paintOffsetTranslation;
    RefPtr<TransformPaintPropertyNode> m_transform;
    RefPtr<EffectPaintPropertyNode> m_effect;
    RefPtr<ClipPaintPropertyNode> m_overflowClip;
    RefPtr<TransformPaintPropertyNode> m_perspective;
    RefPtr<TransformPaintPropertyNode> m_scrollTranslation;
};

} // namespace blink

#endif // ObjectPaintProperties_h
