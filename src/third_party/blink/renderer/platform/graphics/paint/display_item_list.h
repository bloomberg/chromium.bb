// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DISPLAY_ITEM_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DISPLAY_ITEM_LIST_H_

#include "third_party/blink/renderer/platform/graphics/contiguous_container.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/scrollbar_display_item.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

class JSONArray;

// kDisplayItemAlignment must be a multiple of alignof(derived display item) for
// each derived display item; the ideal value is the least common multiple.
// The validity of kDisplayItemAlignment and kMaximumDisplayItemSize are checked
// in PaintController::CreateAndAppend().
static constexpr wtf_size_t kDisplayItemAlignment =
    alignof(ScrollbarDisplayItem);
static constexpr wtf_size_t kMaximumDisplayItemSize =
    sizeof(ScrollbarDisplayItem);

// A container for a list of display items.
class PLATFORM_EXPORT DisplayItemList
    : public ContiguousContainer<DisplayItem, kDisplayItemAlignment> {
 public:
  static constexpr wtf_size_t kDefaultCapacityInBytes = 512;

  // Using 0 as the default value to make 0 also fall back to
  // kDefaultCapacityInBytes.
  explicit DisplayItemList(wtf_size_t initial_capacity_in_bytes = 0)
      : ContiguousContainer(kMaximumDisplayItemSize,
                            initial_capacity_in_bytes
                                ? initial_capacity_in_bytes
                                : kDefaultCapacityInBytes) {}

  DisplayItem& AppendByMoving(DisplayItem& item) {
    SECURITY_CHECK(!item.IsTombstone());
    DisplayItem& result =
        ContiguousContainer::AppendByMoving(item, item.DerivedSize());
    SetupTombstone(item, result);
    return result;
  }

  DisplayItem& ReplaceLastByMoving(DisplayItem& item) {
    SECURITY_CHECK(!item.IsTombstone());
    DCHECK_EQ(back().DerivedSize(), item.DerivedSize());
    DisplayItem& result =
        ContiguousContainer::ReplaceLastByMoving(item, item.DerivedSize());
    SetupTombstone(item, result);
    return result;
  }

  // Useful for iterating with a range-based for loop.
  template <typename Iterator>
  class Range {
   public:
    Range(const Iterator& begin, const Iterator& end)
        : begin_(begin), end_(end) {}
    Iterator begin() const { return begin_; }
    Iterator end() const { return end_; }
    wtf_size_t size() const { return end_ - begin_; }

    // To meet the requirement of gmock ElementsAre().
    using value_type = DisplayItem;
    using const_iterator = DisplayItemList::const_iterator;

   private:
    Iterator begin_;
    Iterator end_;
  };

  // In most cases, we should use PaintChunkSubset::Iterator::DisplayItems()
  // instead of these.
  Range<iterator> ItemsInRange(wtf_size_t begin_index, wtf_size_t end_index) {
    return Range<iterator>(begin() + begin_index, begin() + end_index);
  }
  Range<const_iterator> ItemsInRange(wtf_size_t begin_index,
                                     wtf_size_t end_index) const {
    return Range<const_iterator>(begin() + begin_index, begin() + end_index);
  }

#if DCHECK_IS_ON()
  enum JsonOptions {
    kDefault = 0,
    kClientKnownToBeAlive = 1,
    // Only show a compact representation of the display item list. This flag
    // cannot be used with additional flags such as kShowPaintRecords.
    kCompact = 1 << 1,
    kShowPaintRecords = 1 << 2,
    kShowOnlyDisplayItemTypes = 1 << 3
  };
  typedef unsigned JsonFlags;

  static std::unique_ptr<JSONArray> DisplayItemsAsJSON(
      wtf_size_t first_item_index,
      const Range<const_iterator>& display_items,
      JsonFlags);
#endif  // DCHECK_IS_ON()

 private:
  // Called by AppendByMoving() and ReplaceLastByMoving() which created a
  // tombstone/"dead display item" that can be safely destructed but should
  // never be used except for debugging and raster invalidation.
  void SetupTombstone(DisplayItem& item, const DisplayItem& new_item) {
    DCHECK(item.IsTombstone());
    // We need |visual_rect_| and |outset_for_raster_effects_| of the old
    // display item for raster invalidation. Also, the fields that make up the
    // ID (|client_|, |type_| and |fragment_|) need to match. As their values
    // were either initialized to default values or were left uninitialized by
    // DisplayItem's default constructor, now copy their original values back
    // from |result|.
    item.client_ = new_item.client_;
    item.type_ = new_item.type_;
    item.fragment_ = new_item.fragment_;
    DCHECK_EQ(item.GetId(), new_item.GetId());
    item.visual_rect_ = new_item.visual_rect_;
    item.raster_effect_outset_ = new_item.raster_effect_outset_;
  }
};

using DisplayItemIterator = DisplayItemList::const_iterator;
using DisplayItemRange = DisplayItemList::Range<DisplayItemIterator>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DISPLAY_ITEM_LIST_H_
