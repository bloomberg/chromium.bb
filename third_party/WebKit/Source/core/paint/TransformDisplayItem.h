// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransformDisplayItem_h
#define TransformDisplayItem_h

#include "core/paint/ViewDisplayList.h"

namespace blink {

class TransformationMatrix;

class BeginTransformDisplayItem : public DisplayItem {
public:
    BeginTransformDisplayItem(const RenderObject* renderer, const TransformationMatrix&);
    virtual void replay(GraphicsContext*) override;

#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif
    const TransformationMatrix& m_transform;
};

class EndTransformDisplayItem : public DisplayItem {
public:
    EndTransformDisplayItem(const RenderObject* renderer)
        : DisplayItem(renderer, EndTransform) { }
    virtual void replay(GraphicsContext*) override;

#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif
};

} // namespace blink

#endif // TransformDisplayItem_h
