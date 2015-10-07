// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ObjectPaintProperties_h
#define ObjectPaintProperties_h

#include "platform/graphics/paint/TransformPaintPropertyNode.h"
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
    WTF_MAKE_FAST_ALLOCATED(ObjectPaintProperties);
public:
    static PassOwnPtr<ObjectPaintProperties> create()
    {
        return adoptPtr(new ObjectPaintProperties());
    }

    bool hasTransform() const { return m_transform; }
    void setTransform(PassRefPtr<TransformPaintPropertyNode> transform) { m_transform = transform; }
    const TransformPaintPropertyNode* transform() const { return m_transform.get(); }

    bool hasPerspective() const { return m_perspective; }
    void setPerspective(PassRefPtr<TransformPaintPropertyNode> perspective) { m_perspective = perspective; }
    const TransformPaintPropertyNode* perspective() const { return m_perspective.get(); }

private:
    ObjectPaintProperties() { }

    RefPtr<TransformPaintPropertyNode> m_transform;
    RefPtr<TransformPaintPropertyNode> m_perspective;
};

} // namespace blink

#endif // ObjectPaintProperties_h
