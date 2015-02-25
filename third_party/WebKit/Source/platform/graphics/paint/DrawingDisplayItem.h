// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DrawingDisplayItem_h
#define DrawingDisplayItem_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class PLATFORM_EXPORT DrawingDisplayItem : public DisplayItem {
public:
    static PassOwnPtr<DrawingDisplayItem> create(DisplayItemClient client, Type type, PassRefPtr<const SkPicture> picture)
    {
        return adoptPtr(new DrawingDisplayItem(client, type, picture));
    }

    virtual void replay(GraphicsContext*);
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;
    virtual bool drawsContent() const override;

    PassRefPtr<const SkPicture> picture() const { return m_picture; }

    DrawingDisplayItem(DisplayItemClient client, Type type, PassRefPtr<const SkPicture> picture)
        : DisplayItem(client, type), m_picture(picture)
    {
        ASSERT(m_picture);
        ASSERT(isDrawingType(type));
    }

private:
#ifndef NDEBUG
    virtual void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif

    RefPtr<const SkPicture> m_picture;
};

} // namespace blink

#endif // DrawingDisplayItem_h
