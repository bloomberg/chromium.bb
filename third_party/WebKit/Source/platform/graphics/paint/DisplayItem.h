// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItem_h
#define DisplayItem_h

#include "platform/PlatformExport.h"
#include "wtf/Assertions.h"
#include "wtf/PassOwnPtr.h"

#ifndef NDEBUG
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"
#endif


namespace blink {

class GraphicsContext;
class WebDisplayItemList;

class DisplayItemClientInternalVoid;
typedef DisplayItemClientInternalVoid* DisplayItemClient;

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
        DrawingPaintPhaseCaret = 13,
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
        EndTransform,
        ScrollbarCorner,
        Scrollbar,
        Resizer,
        ColumnRules,
        ClipNodeImage,
        ClipFrameToVisibleContentRect,
        ClipFrameScrollbars,
        FloatClipForeground,
        FloatClipSelection,
        FloatClipSelfOutline,
        EndFloatClip
    };

    // Create a dummy display item which just holds the id but has no display operation.
    // It helps a CachedDisplayItem to match the corresponding original empty display item.
    static PassOwnPtr<DisplayItem> create(DisplayItemClient client, Type type) { return adoptPtr(new DisplayItem(client, type)); }

    virtual ~DisplayItem() { }

    virtual void replay(GraphicsContext*) { }

    DisplayItemClient client() const { return m_id.client; }
    Type type() const { return m_id.type; }
    bool idsEqual(const DisplayItem& other) const { return m_id.client == other.m_id.client && m_id.type == other.m_id.type; }

    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const { }

#ifndef NDEBUG
    static WTF::String typeAsDebugString(DisplayItem::Type);

    void setClientDebugString(const WTF::String& clientDebugString) { m_clientDebugString = clientDebugString; }
    const WTF::String& clientDebugString() const { return m_clientDebugString; }

    WTF::String asDebugString() const;
    virtual const char* name() const { return "Dummy"; }
    virtual void dumpPropertiesAsDebugString(WTF::StringBuilder&) const;
#endif

    virtual bool isCached() const { return false; }

protected:
    DisplayItem(DisplayItemClient client, Type type)
        : m_id(client, type)
    {
        ASSERT(client);
    }

private:
    struct Id {
        Id(DisplayItemClient c, Type t)
            : client(c)
            , type(t)
        {
            ASSERT(client);
        }

        const DisplayItemClient client;
        const Type type;
    } m_id;
#ifndef NDEBUG
    WTF::String m_clientDebugString;
#endif
};

}

#endif // DisplayItem_h
