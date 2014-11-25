// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransformDisplayItem_h
#define TransformDisplayItem_h

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/transforms/TransformationMatrix.h"

namespace blink {

class TransformationMatrix;

class PLATFORM_EXPORT BeginTransformDisplayItem : public DisplayItem {
public:
    BeginTransformDisplayItem(DisplayItemClient, const TransformationMatrix&);
    virtual void replay(GraphicsContext*) override;

#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif
    const TransformationMatrix m_transform;
};

class PLATFORM_EXPORT EndTransformDisplayItem : public DisplayItem {
public:
    EndTransformDisplayItem(DisplayItemClient client)
        : DisplayItem(client, EndTransform) { }
    virtual void replay(GraphicsContext*) override;

#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif
};

} // namespace blink

#endif // TransformDisplayItem_h
