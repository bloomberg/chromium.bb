// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemList_h
#define DisplayItemList_h

#include "platform/graphics/ContiguousContainer.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/Transform3DDisplayItem.h"
#include "platform/wtf/Alignment.h"
#include "platform/wtf/Assertions.h"

namespace blink {

class JSONArray;
struct PaintChunk;

// kDisplayItemAlignment must be a multiple of alignof(derived display item) for
// each derived display item; the ideal value is the least common multiple.
// Currently the limiting factor is TransformationMatrix (in
// BeginTransform3DDisplayItem), which requests 16-byte alignment.
static const size_t kDisplayItemAlignment =
    WTF_ALIGN_OF(BeginTransform3DDisplayItem);
static const size_t kMaximumDisplayItemSize =
    sizeof(BeginTransform3DDisplayItem);

// A container for a list of display items.
class PLATFORM_EXPORT DisplayItemList
    : public ContiguousContainer<DisplayItem, kDisplayItemAlignment> {
 public:
  DisplayItemList(size_t initial_size_bytes)
      : ContiguousContainer(kMaximumDisplayItemSize, initial_size_bytes) {}
  DisplayItemList(DisplayItemList&& source)
      : ContiguousContainer(std::move(source)) {}

  DisplayItemList& operator=(DisplayItemList&& source) {
    ContiguousContainer::operator=(std::move(source));
    return *this;
  }

  DisplayItem& AppendByMoving(DisplayItem& item) {
#ifndef NDEBUG
    String original_debug_string = item.AsDebugString();
#endif
    DCHECK(item.HasValidClient());
    DisplayItem& result =
        ContiguousContainer::AppendByMoving(item, item.DerivedSize());
    // ContiguousContainer::AppendByMoving() calls an in-place constructor
    // on item which replaces it with a tombstone/"dead display item" that
    // can be safely destructed but should never be used.
    DCHECK(!item.HasValidClient());
#ifndef NDEBUG
    // Save original debug string in the old item to help debugging.
    item.SetClientDebugString(original_debug_string);
#endif
    item.visual_rect_ = result.visual_rect_;
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

   private:
    Iterator begin_;
    Iterator end_;
  };
  Range<iterator> ItemsInPaintChunk(const PaintChunk&);
  Range<const_iterator> ItemsInPaintChunk(const PaintChunk&) const;

  enum JsonOptions {
    kDefault = 0,
    kShowPaintRecords = 1,
    kSkipNonDrawings = 1 << 1,
    kShowClientDebugName = 1 << 2,
    kShownOnlyDisplayItemTypes = 1 << 3
  };
  typedef unsigned JsonFlags;

  std::unique_ptr<JSONArray> SubsequenceAsJSON(size_t begin_index,
                                               size_t end_index,
                                               JsonFlags) const;
  void AppendSubsequenceAsJSON(size_t begin_index,
                               size_t end_index,
                               JsonFlags,
                               JSONArray&) const;
};

}  // namespace blink

#endif  // DisplayItemList_h
