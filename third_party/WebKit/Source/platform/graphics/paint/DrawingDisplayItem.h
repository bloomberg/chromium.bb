// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DrawingDisplayItem_h
#define DrawingDisplayItem_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

class PLATFORM_EXPORT DrawingDisplayItem : public DisplayItem {
public:
    DrawingDisplayItem(DisplayItemClient client, Type type, PassRefPtr<SkPicture> picture, const FloatPoint& location)
        : DisplayItem(client, type), m_picture(picture), m_location(location) { }

    PassRefPtr<SkPicture> picture() const { return m_picture; }
    const FloatPoint& location() const { return m_location; }

private:
    virtual void replay(GraphicsContext*);

    RefPtr<SkPicture> m_picture;
    const FloatPoint m_location;
#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif
};

} // namespace blink

#endif // DrawingDisplayItem_h
