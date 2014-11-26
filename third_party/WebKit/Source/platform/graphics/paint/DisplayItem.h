// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItem_h
#define DisplayItem_h

#include "platform/PlatformExport.h"

#ifndef NDEBUG
#include "wtf/text/WTFString.h"
#endif

namespace blink {

class GraphicsContext;

typedef void* DisplayItemClient;

class PLATFORM_EXPORT DisplayItem {
public:
    enum Type {
        // DisplayItem types must be kept in sync with PaintPhase.
        DrawingPaintPhaseBlockBackground = 0,
        DrawingPaintPhaseChildBlockBackground = 1,
        DrawingPaintPhaseChildBlockBackgrounds = 2,
        DrawingPaintPhaseFloat = 3,
        DrawingPaintPhaseForeground = 4,
        DrawingPaintPhaseOutline = 5,
        DrawingPaintPhaseChildOutlines = 6,
        DrawingPaintPhaseSelfOutline = 7,
        DrawingPaintPhaseSelection = 8,
        DrawingPaintPhaseCollapsedTableBorders = 9,
        DrawingPaintPhaseTextClip = 10,
        DrawingPaintPhaseMask = 11,
        DrawingPaintPhaseClippingMask = 12,
        ClipLayerOverflowControls,
        ClipLayerBackground,
        ClipLayerParent,
        ClipLayerFilter,
        ClipLayerForeground,
        ClipLayerFragmentFloat,
        ClipLayerFragmentForeground,
        ClipLayerFragmentChildOutline,
        ClipLayerFragmentOutline,
        ClipLayerFragmentMask,
        ClipLayerFragmentClippingMask,
        ClipLayerFragmentParent,
        ClipLayerFragmentSelection,
        ClipLayerFragmentChildBlockBackgrounds,
        EndClip,
        BeginFilter,
        EndFilter,
        TransparencyClip,
        BeginTransparency,
        EndTransparency,
        ClipBoxChildBlockBackgrounds,
        ClipBoxFloat,
        ClipBoxForeground,
        ClipBoxChildOutlines,
        ClipBoxSelection,
        ClipBoxCollapsedTableBorders,
        ClipBoxTextClip,
        ClipBoxClippingMask,
        BeginTransform,
        EndTransform
    };

    virtual ~DisplayItem() { }

    virtual void replay(GraphicsContext*) = 0;

    DisplayItemClient client() const { return m_id.client; }
    Type type() const { return m_id.type; }
    bool idsEqual(const DisplayItem& other) const { return m_id.client == other.m_id.client && m_id.type == other.m_id.type; }

#ifndef NDEBUG
    static WTF::String typeAsDebugString(DisplayItem::Type);

    void setClientDebugString(const WTF::String& clientDebugString) { m_clientDebugString = clientDebugString; }
    const WTF::String& clientDebugString() const { return m_clientDebugString; }

    virtual WTF::String asDebugString() const;
#endif

protected:
    DisplayItem(DisplayItemClient client, Type type)
        : m_id(client, type)
    { }

private:
    struct Id {
        Id(DisplayItemClient c, Type t)
            : client(c)
            , type(t)
        { }

        const DisplayItemClient client;
        const Type type;
    } m_id;
#ifndef NDEBUG
    WTF::String m_clientDebugString;
#endif
};

}

#endif // DisplayItem_h
