// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItem_h
#define DisplayItem_h

#include "platform/PlatformExport.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/ContiguousContainer.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Noncopyable.h"

#ifndef NDEBUG
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/WTFString.h"
#endif

namespace blink {

class GraphicsContext;
class LayoutSize;
class WebDisplayItemList;

class PLATFORM_EXPORT DisplayItem {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  enum {
    // Must be kept in sync with core/paint/PaintPhase.h.
    kPaintPhaseMax = 11,
  };

  // A display item type uniquely identifies a display item of a client.
  // Some display item types can be categorized using the following directives:
  // - In enum Type:
  //   - enum value <Category>First;
  //   - enum values of the category, first of which should equal
  //     <Category>First (for ease of maintenance, the values should be in
  //     alphabetic order);
  //   - enum value <Category>Last which should be equal to the last of the enum
  //     values of the category
  // - DEFINE_CATEGORY_METHODS(<Category>) to define is<Category>Type(Type) and
  //   is<Category>() methods.
  //
  // A category or subset of a category can contain types each of which
  // corresponds to a PaintPhase:
  // - In enum Type:
  //   - enum value <Category>[<Subset>]PaintPhaseFirst;
  //   - enum value <Category>[<Subset>]PaintPhaseLast =
  //     <Category>[<Subset>]PaintPhaseFirst + PaintPhaseMax;
  // - DEFINE_PAINT_PHASE_CONVERSION_METHOD(<Category>[<Subset>]) to define
  //   paintPhaseTo<Category>[<Subset>]Type(PaintPhase) method.
  //
  // A category can be derived from another category, containing types each of
  // which corresponds to a value of the latter category:
  // - In enum Type:
  //   - enum value <Category>First;
  //   - enum value <Category>Last =
  //     <Category>First + <BaseCategory>Last - <BaseCategory>First;
  // - DEFINE_CONVERSION_METHODS(<Category>,
  //                             <category>,
  //                             <BaseCategory>,
  //                             <baseCategory>)
  //   to define methods to convert types between the categories.
  enum Type {
    kDrawingFirst,
    kDrawingPaintPhaseFirst = kDrawingFirst,
    kDrawingPaintPhaseLast = kDrawingFirst + kPaintPhaseMax,
    kBoxDecorationBackground,
    kCaret,
    kColumnRules,
    kDebugDrawing,
    kDocumentBackground,
    kDragImage,
    kDragCaret,
    kSVGImage,
    kLinkHighlight,
    kImageAreaFocusRing,
    kPageOverlay,
    kPageWidgetDelegateBackgroundFallback,
    kPopupContainerBorder,
    kPopupListBoxBackground,
    kPopupListBoxRow,
    kPrintedContentDestinationLocations,
    kPrintedContentPDFURLRect,
    kResizer,
    kSVGClip,
    kSVGFilter,
    kSVGMask,
    kScrollbarBackButtonEnd,
    kScrollbarBackButtonStart,
    kScrollbarBackground,
    kScrollbarBackTrack,
    kScrollbarCorner,
    kScrollbarForwardButtonEnd,
    kScrollbarForwardButtonStart,
    kScrollbarForwardTrack,
    kScrollbarThumb,
    kScrollbarTickmarks,
    kScrollbarTrackBackground,
    kScrollbarCompositedScrollbar,
    kSelectionTint,
    kTableCollapsedBorders,
    kVideoBitmap,
    kWebPlugin,
    kWebFont,
    kReflectionMask,
    kDrawingLast = kReflectionMask,

    kForeignLayerFirst,
    kForeignLayerCanvas = kForeignLayerFirst,
    kForeignLayerPlugin,
    kForeignLayerVideo,
    kForeignLayerLast = kForeignLayerVideo,

    kClipFirst,
    kClipBoxPaintPhaseFirst = kClipFirst,
    kClipBoxPaintPhaseLast = kClipBoxPaintPhaseFirst + kPaintPhaseMax,
    kClipColumnBoundsPaintPhaseFirst,
    kClipColumnBoundsPaintPhaseLast =
        kClipColumnBoundsPaintPhaseFirst + kPaintPhaseMax,
    kClipLayerFragmentPaintPhaseFirst,
    kClipLayerFragmentPaintPhaseLast =
        kClipLayerFragmentPaintPhaseFirst + kPaintPhaseMax,
    kClipFileUploadControlRect,
    kClipFrameToVisibleContentRect,
    kClipFrameScrollbars,
    kClipLayerBackground,
    kClipLayerColumnBounds,
    kClipLayerFilter,
    kClipLayerForeground,
    kClipLayerParent,
    kClipLayerOverflowControls,
    kClipPopupListBoxFrame,
    kClipScrollbarsToBoxBounds,
    kClipSelectionImage,
    kPageWidgetDelegateClip,
    kClipLast = kPageWidgetDelegateClip,

    kEndClipFirst,
    kEndClipLast = kEndClipFirst + kClipLast - kClipFirst,

    kFloatClipFirst,
    kFloatClipPaintPhaseFirst = kFloatClipFirst,
    kFloatClipPaintPhaseLast = kFloatClipFirst + kPaintPhaseMax,
    kFloatClipLast = kFloatClipPaintPhaseLast,
    kEndFloatClipFirst,
    kEndFloatClipLast = kEndFloatClipFirst + kFloatClipLast - kFloatClipFirst,

    kScrollFirst,
    kScrollPaintPhaseFirst = kScrollFirst,
    kScrollPaintPhaseLast = kScrollPaintPhaseFirst + kPaintPhaseMax,
    kScrollOverflowControls,
    kScrollLast = kScrollOverflowControls,
    kEndScrollFirst,
    kEndScrollLast = kEndScrollFirst + kScrollLast - kScrollFirst,

    kTransform3DFirst,
    kTransform3DElementTransform = kTransform3DFirst,
    kTransform3DLast = kTransform3DElementTransform,
    kEndTransform3DFirst,
    kEndTransform3DLast =
        kEndTransform3DFirst + kTransform3DLast - kTransform3DFirst,

    kBeginFilter,
    kEndFilter,
    kBeginCompositing,
    kEndCompositing,
    kBeginTransform,
    kEndTransform,
    kBeginClipPath,
    kEndClipPath,
    kUninitializedType,
    kTypeLast = kUninitializedType
  };

  DisplayItem(const DisplayItemClient& client, Type type, size_t derived_size)
      : client_(&client),
        visual_rect_(client.VisualRect()),
        outset_for_raster_effects_(client.VisualRectOutsetForRasterEffects()),
        type_(type),
        derived_size_(derived_size),
        skipped_cache_(false)
#ifndef NDEBUG
        ,
        client_debug_string_(client.DebugName())
#endif
  {
    // derivedSize must fit in m_derivedSize.
    // If it doesn't, enlarge m_derivedSize and fix this assert.
    SECURITY_DCHECK(derived_size < (1 << 8));
    SECURITY_DCHECK(derived_size >= sizeof(*this));
  }

  virtual ~DisplayItem() {}

  // Ids are for matching new DisplayItems with existing DisplayItems.
  struct Id {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    Id(const DisplayItemClient& client, const Type type)
        : client(client), type(type) {}

    const DisplayItemClient& client;
    const Type type;
  };

  Id GetId() const { return Id(*client_, type_); }

  virtual void Replay(GraphicsContext&) const {}

  const DisplayItemClient& Client() const {
    DCHECK(client_);
    return *client_;
  }

  // This equals to Client().VisualRect() as long as the client is alive and is
  // not invalidated. Otherwise it saves the previous visual rect of the client.
  // See DisplayItemClient::VisualRect() about its coordinate space.
  const LayoutRect& VisualRect() const { return visual_rect_; }
  LayoutUnit OutsetForRasterEffects() const {
    return outset_for_raster_effects_;
  }

  // Visual rect can change without needing invalidation of the client, e.g.
  // when ancestor clip changes. This is called from PaintController::
  // UseCachedDrawingIfPossible() to update the visual rect of a cached display
  // item.
  void UpdateVisualRect() { visual_rect_ = client_->VisualRect(); }

  Type GetType() const { return type_; }

  // Size of this object in memory, used to move it with memcpy.
  // This is not sizeof(*this), because it needs to account for the size of
  // the derived class (i.e. runtime type). Derived classes are expected to
  // supply this to the DisplayItem constructor.
  size_t DerivedSize() const { return derived_size_; }

  // For PaintController only. Painters should use DisplayItemCacheSkipper
  // instead.
  void SetSkippedCache() { skipped_cache_ = true; }
  bool SkippedCache() const { return skipped_cache_; }

  // Appends this display item to the WebDisplayItemList, if applicable.
  // |visual_rect_offset| is the offset between the space of the GraphicsLayer
  // which owns the display item and the coordinate space of VisualRect().
  // TODO(wangxianzhu): Remove the parameter for slimming paint v2.
  virtual void AppendToWebDisplayItemList(const LayoutSize& visual_rect_offset,
                                          WebDisplayItemList*) const {}

// See comments of enum Type for usage of the following macros.
#define DEFINE_CATEGORY_METHODS(Category)                           \
  static bool Is##Category##Type(Type type) {                       \
    return type >= k##Category##First && type <= k##Category##Last; \
  }                                                                 \
  bool Is##Category() const { return Is##Category##Type(GetType()); }

#define DEFINE_CONVERSION_METHODS(Category1, category1, Category2, category2) \
  static Type Category1##TypeTo##Category2##Type(Type type) {                 \
    static_assert(k##Category1##Last - k##Category1##First ==                 \
                      k##Category2##Last - k##Category2##First,               \
                  "Categories " #Category1 " and " #Category2                 \
                  " should have same number of enum values. See comments of " \
                  "DisplayItem::Type");                                       \
    DCHECK(Is##Category1##Type(type));                                        \
    return static_cast<Type>(type - k##Category1##First +                     \
                             k##Category2##First);                            \
  }                                                                           \
  static Type category2##TypeTo##Category1##Type(Type type) {                 \
    DCHECK(Is##Category2##Type(type));                                        \
    return static_cast<Type>(type - k##Category2##First +                     \
                             k##Category1##First);                            \
  }

#define DEFINE_PAIRED_CATEGORY_METHODS(Category, category) \
  DEFINE_CATEGORY_METHODS(Category)                        \
  DEFINE_CATEGORY_METHODS(End##Category)                   \
  DEFINE_CONVERSION_METHODS(Category, category, End##Category, end##Category)

#define DEFINE_PAINT_PHASE_CONVERSION_METHOD(Category)                   \
  static Type PaintPhaseTo##Category##Type(int paintPhase) {             \
    static_assert(                                                       \
        k##Category##PaintPhaseLast - k##Category##PaintPhaseFirst ==    \
            k##PaintPhaseMax,                                            \
        "Invalid paint-phase-based category " #Category                  \
        ". See comments of DisplayItem::Type");                          \
    return static_cast<Type>(paintPhase + k##Category##PaintPhaseFirst); \
  }

  DEFINE_CATEGORY_METHODS(Drawing)
  DEFINE_PAINT_PHASE_CONVERSION_METHOD(Drawing)

  DEFINE_CATEGORY_METHODS(ForeignLayer)

  DEFINE_PAIRED_CATEGORY_METHODS(Clip, clip)
  DEFINE_PAINT_PHASE_CONVERSION_METHOD(ClipLayerFragment)
  DEFINE_PAINT_PHASE_CONVERSION_METHOD(ClipBox)
  DEFINE_PAINT_PHASE_CONVERSION_METHOD(ClipColumnBounds)

  DEFINE_PAIRED_CATEGORY_METHODS(FloatClip, floatClip)
  DEFINE_PAINT_PHASE_CONVERSION_METHOD(FloatClip)

  DEFINE_PAIRED_CATEGORY_METHODS(Scroll, scroll)
  DEFINE_PAINT_PHASE_CONVERSION_METHOD(Scroll)

  DEFINE_PAIRED_CATEGORY_METHODS(Transform3D, transform3D)

  static bool IsCacheableType(Type type) { return IsDrawingType(type); }
  bool IsCacheable() const { return !SkippedCache() && IsCacheableType(type_); }

  virtual bool IsBegin() const { return false; }
  virtual bool IsEnd() const { return false; }

#if DCHECK_IS_ON()
  virtual bool IsEndAndPairedWith(DisplayItem::Type other_type) const {
    return false;
  }
#endif

  virtual bool Equals(const DisplayItem& other) const {
    return client_ == other.client_ && type_ == other.type_ &&
           derived_size_ == other.derived_size_ &&
           skipped_cache_ == other.skipped_cache_;
  }

  // True if the client is non-null. Because m_client is const, this should
  // never be false except when we explicitly create a tombstone/"dead display
  // item" as part of moving an item from one list to another (see:
  // DisplayItemList::appendByMoving).
  bool HasValidClient() const { return client_; }

  virtual bool DrawsContent() const { return false; }

#ifndef NDEBUG
  static WTF::String TypeAsDebugString(DisplayItem::Type);
  const WTF::String ClientDebugString() const { return client_debug_string_; }
  void SetClientDebugString(const WTF::String& s) { client_debug_string_ = s; }
  WTF::String AsDebugString() const;
  virtual void DumpPropertiesAsDebugString(WTF::StringBuilder&) const;
#endif

 private:
  // The default DisplayItem constructor is only used by
  // ContiguousContainer::appendByMoving where an invalid DisplaItem is
  // constructed at the source location.
  template <typename T, unsigned alignment>
  friend class ContiguousContainer;
  friend class DisplayItemList;

  DisplayItem()
      : client_(nullptr),
        type_(kUninitializedType),
        derived_size_(sizeof(*this)),
        skipped_cache_(false) {}

  const DisplayItemClient* client_;
  LayoutRect visual_rect_;
  LayoutUnit outset_for_raster_effects_;

  static_assert(kTypeLast < (1 << 16),
                "DisplayItem::Type should fit in 16 bits");
  const Type type_ : 16;
  const unsigned derived_size_ : 8;  // size of the actual derived class
  unsigned skipped_cache_ : 1;

#ifndef NDEBUG
  WTF::String client_debug_string_;
#endif
};

inline bool operator==(const DisplayItem::Id& a, const DisplayItem::Id& b) {
  return a.client == b.client && a.type == b.type;
}

inline bool operator!=(const DisplayItem::Id& a, const DisplayItem::Id& b) {
  return !(a == b);
}

class PLATFORM_EXPORT PairedBeginDisplayItem : public DisplayItem {
 protected:
  PairedBeginDisplayItem(const DisplayItemClient& client,
                         Type type,
                         size_t derived_size)
      : DisplayItem(client, type, derived_size) {
    DCHECK(!RuntimeEnabledFeatures::SlimmingPaintV2Enabled());
  }

 private:
  bool IsBegin() const final { return true; }
};

class PLATFORM_EXPORT PairedEndDisplayItem : public DisplayItem {
 protected:
  PairedEndDisplayItem(const DisplayItemClient& client,
                       Type type,
                       size_t derived_size)
      : DisplayItem(client, type, derived_size) {
    DCHECK(!RuntimeEnabledFeatures::SlimmingPaintV2Enabled());
  }

#if DCHECK_IS_ON()
  bool IsEndAndPairedWith(DisplayItem::Type other_type) const override = 0;
#endif

 private:
  bool IsEnd() const final { return true; }
};

}  // namespace blink

#endif  // DisplayItem_h
