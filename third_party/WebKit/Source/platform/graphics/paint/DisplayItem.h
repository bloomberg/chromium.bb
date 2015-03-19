// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItem_h
#define DisplayItem_h

#include "platform/PlatformExport.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "wtf/Assertions.h"
#include "wtf/PassOwnPtr.h"

#ifndef NDEBUG
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"
#endif


namespace blink {

class GraphicsContext;
class WebDisplayItemList;

class PLATFORM_EXPORT DisplayItem {
public:
    enum {
        // Must be kept in sync with core/layout/PaintPhase.h.
        PaintPhaseMax = 12,
    };

    // A display item type uniquely identifies a display item of a client.
    // Some display item types can be categorized using the following directives:
    // - In enum Type:
    //   - enum value <Category>First;
    //   - enum values of the category, first of which should equal <Category>First;
    //     (for ease of maintenance, the values should be in alphabetic order)
    //   - enum value <Category>Last which should be equal to the last of the enum values of the category
    // - DEFINE_CATEGORY_METHODS(<Category>) to define is<Category>Type(Type) and is<Category>() methods.
    //
    // A category or subset of a category can contain types each of which corresponds to a PaintPhase:
    // - In enum Type:
    //   - enum value <Category>[<Subset>]PaintPhaseFirst;
    //   - enum value <Category>[<Subset>]PaintPhaseLast = <Category>[<Subset>]PaintPhaseFirst + PaintPhaseMax;
    // - DEFINE_PAINT_PHASE_CONVERSION_METHOD(<Category>[<Subset>]) to define
    //   paintPhaseTo<Category>[<Subset>]Type(PaintPhase) method.
    //
    // A category can be derived from another category, containing types each of which corresponds to a
    // value of the latter category:
    // - In enum Type:
    //   - enum value <Category>First;
    //   - enum value <Category>Last = <Category>First + <BaseCategory>Last - <BaseCategory>First;
    // - DEFINE_CONVERSION_METHODS(<Category>, <category>, <BaseCategory>, <baseCategory>) to define methods to
    //   convert types between the categories;
    enum Type {
        DrawingFirst,
        DrawingPaintPhaseFirst = DrawingFirst,
        DrawingPaintPhaseLast = DrawingFirst + PaintPhaseMax,
        BoxDecorationBackground,
        Caret,
        ColumnRules,
        DebugRedFill,
        DragImage,
        LinkHighlight,
        PageOverlay,
        PageWidgetDelegateBackgroundFallback,
        PopupContainerBorder,
        PopupListBoxBackground,
        PopupListBoxRow,
        Resizer,
        SVGClip,
        SVGFilter,
        SVGMask,
        ScrollbarCorner,
        ScrollbarHorizontal,
        ScrollbarTickMark,
        ScrollbarVertical,
        SelectionGap,
        SelectionTint,
        VideoBitmap,
        ViewBackground,
        WebPlugin,
        DrawingLast = WebPlugin,

        CachedFirst,
        CachedLast = CachedFirst + DrawingLast - DrawingFirst,

        ClipFirst,
        ClipBoxPaintPhaseFirst = ClipFirst,
        ClipBoxPaintPhaseLast = ClipBoxPaintPhaseFirst + PaintPhaseMax,
        ClipColumnBoundsPaintPhaseFirst,
        ClipColumnBoundsPaintPhaseLast = ClipColumnBoundsPaintPhaseFirst + PaintPhaseMax,
        ClipLayerFragmentPaintPhaseFirst,
        ClipLayerFragmentPaintPhaseLast = ClipLayerFragmentPaintPhaseFirst + PaintPhaseMax,
        ClipFileUploadControlRect,
        ClipFrameToVisibleContentRect,
        ClipFrameScrollbars,
        ClipLayerBackground,
        ClipLayerColumnBounds,
        ClipLayerFilter,
        ClipLayerForeground,
        ClipLayerParent,
        ClipLayerOverflowControls,
        ClipNodeImage,
        ClipPopupListBoxFrame,
        ClipSelectionImage,
        PageWidgetDelegateClip,
        TransparencyClip,
        ClipLast = TransparencyClip,

        EndClipFirst,
        EndClipLast = EndClipFirst + ClipLast - ClipFirst,

        FloatClipFirst,
        FloatClipPaintPhaseFirst = FloatClipFirst,
        FloatClipPaintPhaseLast = FloatClipFirst + PaintPhaseMax,
        FloatClipLast = FloatClipPaintPhaseLast,
        EndFloatClipFirst,
        EndFloatClipLast = EndFloatClipFirst + FloatClipLast - FloatClipFirst,

        ScrollFirst,
        ScrollPaintPhaseFirst = ScrollFirst,
        ScrollPaintPhaseLast = ScrollPaintPhaseFirst + PaintPhaseMax,
        ScrollLast = ScrollPaintPhaseLast,
        EndScrollFirst,
        EndScrollLast = EndScrollFirst + ScrollLast - ScrollFirst,

        Transform3DFirst,
        Transform3DElementTransform = Transform3DFirst,
        Transform3DLast = Transform3DElementTransform,
        EndTransform3DFirst,
        EndTransform3DLast = EndTransform3DFirst + Transform3DLast - Transform3DFirst,

        BeginFilter,
        EndFilter,
        BeginCompositing,
        EndCompositing,
        BeginTransform,
        EndTransform,
        BeginClipPath,
        EndClipPath,

        SubtreeCachedFirst,
        SubtreeCachedPaintPhaseFirst = SubtreeCachedFirst,
        SubtreeCachedPaintPhaseLast = SubtreeCachedPaintPhaseFirst + PaintPhaseMax,
        SubtreeCachedLast = SubtreeCachedPaintPhaseLast,

        BeginSubtreeFirst,
        BeginSubtreePaintPhaseFirst = BeginSubtreeFirst,
        BeginSubtreePaintPhaseLast = BeginSubtreePaintPhaseFirst + PaintPhaseMax,
        BeginSubtreeLast = BeginSubtreePaintPhaseLast,

        EndSubtreeFirst,
        EndSubtreePaintPhaseFirst = EndSubtreeFirst,
        EndSubtreePaintPhaseLast = EndSubtreePaintPhaseFirst + PaintPhaseMax,
        EndSubtreeLast = EndSubtreePaintPhaseLast,

        TypeLast = EndSubtreeLast
    };

    virtual ~DisplayItem() { }

    virtual void replay(GraphicsContext*) { }

    DisplayItemClient client() const { return m_id.client; }
    Type type() const { return m_id.type; }
    bool idsEqual(const DisplayItem& other, Type overrideType) const
    {
        return m_id.client == other.m_id.client
            && m_id.type == overrideType
            && m_id.scopeContainer == other.m_id.scopeContainer
            && m_id.scopeId == other.m_id.scopeId;
    }

    void setScope(DisplayItemClient scopeContainer, int scopeId)
    {
        m_id.scopeContainer = scopeContainer;
        m_id.scopeId = scopeId;
    }

    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const { }

    // See comments of enum Type for usage of the following macros.
#define DEFINE_CATEGORY_METHODS(Category) \
    static bool is##Category##Type(Type type) { return type >= Category##First && type <= Category##Last; } \
    bool is##Category() const { return is##Category##Type(type()); }

#define DEFINE_CONVERSION_METHODS(Category1, category1, Category2, category2) \
    static Type category1##TypeTo##Category2##Type(Type type) \
    { \
        static_assert(Category1##Last - Category1##First == Category2##Last - Category2##First, \
            "Categories " #Category1 " and " #Category2 " should have same number of enum values. See comments of DisplayItem::Type"); \
        ASSERT(is##Category1##Type(type)); \
        return static_cast<Type>(type - Category1##First + Category2##First); \
    } \
    static Type category2##TypeTo##Category1##Type(Type type) \
    { \
        ASSERT(is##Category2##Type(type)); \
        return static_cast<Type>(type - Category2##First + Category1##First); \
    }

#define DEFINE_PAIRED_CATEGORY_METHODS(Category, category) \
    DEFINE_CATEGORY_METHODS(Category) \
    DEFINE_CATEGORY_METHODS(End##Category) \
    DEFINE_CONVERSION_METHODS(Category, category, End##Category, end##Category)

#define DEFINE_PAINT_PHASE_CONVERSION_METHOD(Category) \
    static Type paintPhaseTo##Category##Type(int paintPhase) \
    { \
        static_assert(Category##PaintPhaseLast - Category##PaintPhaseFirst == PaintPhaseMax, \
            "Invalid paint-phase-based category " #Category ". See comments of DisplayItem::Type"); \
        return static_cast<Type>(paintPhase + Category##PaintPhaseFirst); \
    }

    DEFINE_CATEGORY_METHODS(Drawing)
    DEFINE_PAINT_PHASE_CONVERSION_METHOD(Drawing)
    DEFINE_CATEGORY_METHODS(Cached)
    DEFINE_CONVERSION_METHODS(Drawing, drawing, Cached, cached)

    DEFINE_PAIRED_CATEGORY_METHODS(Clip, clip)
    DEFINE_PAINT_PHASE_CONVERSION_METHOD(ClipLayerFragment)
    DEFINE_PAINT_PHASE_CONVERSION_METHOD(ClipBox)
    DEFINE_PAINT_PHASE_CONVERSION_METHOD(ClipColumnBounds)

    DEFINE_PAIRED_CATEGORY_METHODS(FloatClip, floatClip)
    DEFINE_PAINT_PHASE_CONVERSION_METHOD(FloatClip)

    DEFINE_PAIRED_CATEGORY_METHODS(Scroll, scroll)
    DEFINE_PAINT_PHASE_CONVERSION_METHOD(Scroll)

    DEFINE_PAIRED_CATEGORY_METHODS(Transform3D, transform3D);

    DEFINE_CATEGORY_METHODS(SubtreeCached)
    DEFINE_PAINT_PHASE_CONVERSION_METHOD(SubtreeCached)
    DEFINE_CATEGORY_METHODS(BeginSubtree)
    DEFINE_PAINT_PHASE_CONVERSION_METHOD(BeginSubtree)
    DEFINE_CATEGORY_METHODS(EndSubtree)
    DEFINE_PAINT_PHASE_CONVERSION_METHOD(EndSubtree)
    DEFINE_CONVERSION_METHODS(SubtreeCached, subtreeCached, BeginSubtree, beginSubtree)
    DEFINE_CONVERSION_METHODS(SubtreeCached, subtreeCached, EndSubtree, endSubtree)
    DEFINE_CONVERSION_METHODS(BeginSubtree, beginSubtree, EndSubtree, endSubtree)

    virtual bool isBegin() const { return false; }
    virtual bool isEnd() const { return false; }

#if ENABLE(ASSERT)
    virtual bool isEndAndPairedWith(const DisplayItem& other) const { return false; }
#endif

    virtual bool drawsContent() const { return false; }

#ifndef NDEBUG
    static WTF::String typeAsDebugString(DisplayItem::Type);

    void setClientDebugString(const WTF::String& clientDebugString) { m_clientDebugString = clientDebugString; }
    const WTF::String& clientDebugString() const { return m_clientDebugString; }

    WTF::String asDebugString() const;
    virtual void dumpPropertiesAsDebugString(WTF::StringBuilder&) const;
#endif

protected:
    DisplayItem(DisplayItemClient client, Type type)
        : m_id(client, type)
    {
        ASSERT(client);
    }

private:
    struct Id {
        Id(DisplayItemClient c, Type t) : client(c), type(t), scopeContainer(nullptr), scopeId(0) { }

        const DisplayItemClient client;
        const Type type;
        DisplayItemClient scopeContainer;
        int scopeId;
    } m_id;

#ifndef NDEBUG
    WTF::String m_clientDebugString;
#endif
};

class PLATFORM_EXPORT PairedBeginDisplayItem : public DisplayItem {
protected:
    PairedBeginDisplayItem(DisplayItemClient client, Type type) : DisplayItem(client, type) { }

private:
    virtual bool isBegin() const override final { return true; }
};

class PLATFORM_EXPORT PairedEndDisplayItem : public DisplayItem {
protected:
    PairedEndDisplayItem(DisplayItemClient client, Type type) : DisplayItem(client, type) { }

#if ENABLE(ASSERT)
    virtual bool isEndAndPairedWith(const DisplayItem& other) const override = 0;
#endif

private:
    virtual bool isEnd() const override final { return true; }
};

} // namespace blink

#endif // DisplayItem_h
