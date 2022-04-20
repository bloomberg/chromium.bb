// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_TRACK_COLLECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_TRACK_COLLECTION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

struct NGGridPlacementData;

// |NGGridTrackCollectionBase| provides an implementation for some shared
// functionality on grid collections, specifically binary search on the
// collection to get the range that contains a specific grid line.
class CORE_EXPORT NGGridTrackCollectionBase {
 public:
  explicit NGGridTrackCollectionBase(GridTrackSizingDirection track_direction)
      : track_direction_(track_direction) {}

  virtual ~NGGridTrackCollectionBase() = default;

  // Returns the number of track ranges in the collection.
  virtual wtf_size_t RangeCount() const = 0;
  // Returns the start line of a given track range.
  virtual wtf_size_t RangeStartLine(wtf_size_t range_index) const = 0;
  // Returns the number of tracks in a given track range.
  virtual wtf_size_t RangeTrackCount(wtf_size_t range_index) const = 0;

  // Returns the end line of a given track range.
  wtf_size_t RangeEndLine(wtf_size_t range_index) const;
  // Gets the index of the range that contains the given grid line.
  wtf_size_t RangeIndexFromGridLine(wtf_size_t grid_line) const;

  GridTrackSizingDirection Direction() const { return track_direction_; }

 private:
  GridTrackSizingDirection track_direction_;
};

class CORE_EXPORT TrackSpanProperties {
 public:
  enum PropertyId : unsigned {
    kNone = 0,
    kHasAutoMinimumTrack = 1 << 1,
    kHasFixedMaximumTrack = 1 << 2,
    kHasFixedMinimumTrack = 1 << 3,
    kHasFlexibleTrack = 1 << 4,
    kHasIntrinsicTrack = 1 << 5,
    kHasNonDefiniteTrack = 1 << 6,
    kIsCollapsed = 1 << 7,
    kIsDependentOnAvailableSize = 1 << 8,
    kIsImplicit = 1 << 9,
  };

  inline bool HasProperty(PropertyId id) const { return bitmask_ & id; }
  inline void SetProperty(PropertyId id) { bitmask_ |= id; }
  inline void Reset() { bitmask_ = kNone; }

 private:
  wtf_size_t bitmask_{kNone};
};

class CORE_EXPORT NGGridBlockTrackCollection
    : public NGGridTrackCollectionBase {
 public:
  struct CORE_EXPORT Range {
    bool IsCollapsed() const;
    bool IsImplicit() const;

    void SetIsCollapsed();
    void SetIsImplicit();

    wtf_size_t start_line;
    wtf_size_t track_count;
    wtf_size_t repeater_index;
    wtf_size_t repeater_offset;

    TrackSpanProperties properties;
  };

  // This structure represents the grid line boundaries of a repeater or item
  // placed on the implicit grid. For the latter, a pointer to its respective
  // range start/end index is provided to cache its value during
  // |FinalizeRanges|.
  struct TrackBoundaryToRangePair {
    explicit TrackBoundaryToRangePair(
        wtf_size_t grid_line,
        wtf_size_t* grid_item_range_index_to_cache = nullptr)
        : grid_line(grid_line),
          grid_item_range_index_to_cache(grid_item_range_index_to_cache) {}
    wtf_size_t grid_line;
    wtf_size_t* grid_item_range_index_to_cache;
  };

  NGGridBlockTrackCollection(const ComputedStyle& grid_style,
                             const NGGridPlacementData& placement_data,
                             GridTrackSizingDirection track_direction);

  // NGGridTrackCollectionBase overrides.
  wtf_size_t RangeCount() const override { return ranges_.size(); }
  wtf_size_t RangeStartLine(wtf_size_t range_index) const override;
  wtf_size_t RangeTrackCount(wtf_size_t range_index) const override;

  // Ensures that after FinalizeRanges is called, a range will start at the
  // |start_line|, a range will end at |start_line| + |span_length|.
  // |grid_item_start_range_index| and |grid_item_end_range_index| will be
  // written to during |FinalizeRanges|.
  void EnsureTrackCoverage(wtf_size_t start_line,
                           wtf_size_t span_length,
                           wtf_size_t* grid_item_start_range_index,
                           wtf_size_t* grid_item_end_range_index);
  // Build the collection of ranges based on information provided through the
  // specified tracks and |EnsureTrackCoverage|.
  void FinalizeRanges();

  const NGGridTrackList& ExplicitTracks() const { return explicit_tracks_; }
  const NGGridTrackList& ImplicitTracks() const { return implicit_tracks_; }
  const Vector<Range>& Ranges() const { return ranges_; }

 private:
  friend class NGGridTrackCollectionTest;

  // This constructor is used exclusively in testing.
  NGGridBlockTrackCollection(const NGGridTrackList& explicit_tracks,
                             const NGGridTrackList& implicit_tracks,
                             wtf_size_t auto_repetitions);

  wtf_size_t auto_repetitions_;
  wtf_size_t start_offset_;

  bool track_indices_need_sort_ : 1;

  // Stores the grid's explicit and implicit tracks.
  const NGGridTrackList& explicit_tracks_;
  const NGGridTrackList& implicit_tracks_;

  // Starting and ending tracks mark where ranges will start and end.
  // The corresponding range_index will be written to during |FinalizeRanges|.
  // Once the ranges have been built in FinalizeRanges, these are cleared.
  Vector<TrackBoundaryToRangePair> start_lines_;
  Vector<TrackBoundaryToRangePair> end_lines_;

  Vector<Range> ranges_;
};

class CORE_EXPORT NGGridLayoutTrackCollection
    : public NGGridTrackCollectionBase {
  USING_FAST_MALLOC(NGGridLayoutTrackCollection);

 public:
  struct CORE_EXPORT Range {
    bool IsCollapsed() const;

    wtf_size_t set_count;
    wtf_size_t start_line;
    wtf_size_t track_count;
    wtf_size_t begin_set_index;

    TrackSpanProperties properties;
  };

  struct SetGeometry {
    SetGeometry(LayoutUnit offset, wtf_size_t track_count)
        : offset(offset), track_count(track_count) {}

    LayoutUnit offset;
    wtf_size_t track_count;
  };

  explicit NGGridLayoutTrackCollection(GridTrackSizingDirection track_direction)
      : NGGridTrackCollectionBase(track_direction) {}

  bool operator==(const NGGridLayoutTrackCollection& other) const;

  // NGGridTrackCollectionBase overrides.
  wtf_size_t RangeCount() const override { return ranges_.size(); }
  wtf_size_t RangeStartLine(wtf_size_t range_index) const override;
  wtf_size_t RangeTrackCount(wtf_size_t range_index) const override;

  // Returns the number of sets spanned by a given track range.
  wtf_size_t RangeSetCount(wtf_size_t range_index) const;
  // Return the index of the first set spanned by a given track range.
  wtf_size_t RangeBeginSetIndex(wtf_size_t range_index) const;

  // Returns true if the specified property has been set in the track span
  // properties bitmask of the range at position |range_index|.
  bool RangeHasTrackSpanProperty(
      wtf_size_t range_index,
      TrackSpanProperties::PropertyId property_id) const;

  wtf_size_t EndLineOfImplicitGrid() const;
  // Returns true if |grid_line| is contained within the implicit grid.
  bool IsGridLineWithinImplicitGrid(wtf_size_t grid_line) const;

  wtf_size_t GetSetCount() const;
  LayoutUnit GetSetOffset(wtf_size_t set_index) const;
  wtf_size_t GetSetTrackCount(wtf_size_t set_index) const;

  LayoutUnit MajorBaseline(wtf_size_t set_index) const;
  LayoutUnit MinorBaseline(wtf_size_t set_index) const;

  // Increase by |delta| the offset of every set with index > |set_index|.
  void AdjustSetOffsets(wtf_size_t set_index, LayoutUnit delta);

  // Returns the total size of all sets in the collection.
  LayoutUnit ComputeSetSpanSize() const;
  // Returns the total size of all sets with index in the range [begin, end).
  LayoutUnit ComputeSetSpanSize(wtf_size_t begin_set_index,
                                wtf_size_t end_set_index) const;

  // Creates a track collection containing every |Range| with index in the range
  // [begin, end], including their respective |SetGeometry| and baselines.
  NGGridLayoutTrackCollection CreateSubgridCollection(
      wtf_size_t begin_range_index,
      wtf_size_t end_range_index,
      GridTrackSizingDirection subgrid_track_direction) const;

  LayoutUnit GutterSize() const { return gutter_size_; }
  const Vector<Range>& Ranges() const { return ranges_; }

  // Don't allow this class to be used for grid sizing.
  virtual bool IsForSizing() const { return false; }
  // Indefinite indices are only used while measuring grid item contributions.
  virtual bool IsSpanningIndefiniteSet(wtf_size_t begin_set_index,
                                       wtf_size_t end_set_index) const {
    return false;
  }

 protected:
  LayoutUnit gutter_size_;

  Vector<Range> ranges_;
  Vector<LayoutUnit> major_baselines_;
  Vector<LayoutUnit> minor_baselines_;
  Vector<SetGeometry> sets_geometry_;
};

// |NGGridBlockTrackCollection::EnsureTrackCoverage| may introduce a range start
// and/or end at the middle of any repeater from the block collection. This will
// affect how some repeated tracks within the same repeater group resolve their
// track sizes; e.g. consider the track list 'repeat(10, auto)' with a grid item
// spanning from the 3rd to the 7th track in the repeater, every track within
// the item's range will grow to fit the content of that item first.
//
// For the track sizing algorithm we want to have separate data (e.g. base size,
// growth limit, etc.) between tracks in different ranges; instead of trivially
// expanding the repeaters, which will limit our implementation to support
// relatively small track counts, we introduce the concept of a "set".
//
// A "set" is a collection of distinct track definitions that compose a range in
// |NGGridSizingTrackCollection|; each set element stores the number of tracks
// within the range that share its definition. The |NGGridSet| class represents
// a single element from a set.
//
// As an example, consider the following grid definition:
//   - 'grid-template-columns: repeat(4, 5px 1fr)'
//   - Grid item 1 with 'grid-column: 1 / span 5'
//   - Grid item 2 with 'grid-column: 2 / span 1'
//   - Grid item 3 with 'grid-column: 6 / span 8'
//
// Expanding the track definitions above we would look at the explicit grid:
//   | 5px | 1fr | 5px | 1fr | 5px | 1fr | 5px | 1fr |
//
// This example would produce the following ranges and their respective sets:
//   Range 1:  [1-1], Set 1: {  5px (1) }
//   Range 2:  [2-2], Set 2: {  1fr (1) }
//   Range 3:  [3-5], Set 3: {  5px (2) , 1fr (1) }
//   Range 4:  [6-8], Set 4: {  1fr (2) , 5px (1) }
//   Range 5: [9-13], Set 5: { auto (5) }
//
// Note that, since |NGGridBlockTrackCollection|'s ranges are assured to span a
// single repeater and to not cross any grid item's boundary in the respective
// dimension, tracks within a set are "commutative" and can be sized evenly.
struct CORE_EXPORT NGGridSet {
  explicit NGGridSet(wtf_size_t track_count);

  // |is_available_size_indefinite| is used to normalize percentage track
  // sizing functions; from https://drafts.csswg.org/css-grid-2/#track-sizes:
  //   "If the size of the grid container depends on the size of its tracks,
  //   then the <percentage> must be treated as 'auto'".
  NGGridSet(wtf_size_t track_count,
            const GridTrackSize& track_definition,
            bool is_available_size_indefinite);

  double FlexFactor() const;
  LayoutUnit BaseSize() const;
  LayoutUnit GrowthLimit() const;

  void InitBaseSize(LayoutUnit new_base_size);
  void IncreaseBaseSize(LayoutUnit new_base_size);
  void IncreaseGrowthLimit(LayoutUnit new_growth_limit);

  void EnsureGrowthLimitIsNotLessThanBaseSize();
  bool IsGrowthLimitLessThanBaseSize() const;

  wtf_size_t track_count;
  GridTrackSize track_size;

  // Fields used by the track sizing algorithm.
  LayoutUnit base_size;
  LayoutUnit growth_limit;
  LayoutUnit planned_increase;
  LayoutUnit fit_content_limit;
  LayoutUnit item_incurred_increase;

  bool is_infinitely_growable : 1;
};

class CORE_EXPORT NGGridSizingTrackCollection
    : public NGGridLayoutTrackCollection {
  USING_FAST_MALLOC(NGGridSizingTrackCollection);

 public:
  template <bool is_const>
  class CORE_EXPORT SetIteratorBase {
   public:
    using TrackCollectionPtr =
        typename std::conditional<is_const,
                                  const NGGridSizingTrackCollection*,
                                  NGGridSizingTrackCollection*>::type;
    using NGGridSetRef =
        typename std::conditional<is_const, const NGGridSet&, NGGridSet&>::type;

    SetIteratorBase(TrackCollectionPtr track_collection,
                    wtf_size_t begin_set_index,
                    wtf_size_t end_set_index)
        : track_collection_(track_collection),
          current_set_index_(begin_set_index),
          end_set_index_(end_set_index) {
      DCHECK(track_collection_);
      DCHECK_LE(current_set_index_, end_set_index_);
      DCHECK_LE(end_set_index_, track_collection_->GetSetCount());
    }

    bool IsAtEnd() const {
      DCHECK_LE(current_set_index_, end_set_index_);
      return current_set_index_ == end_set_index_;
    }

    bool MoveToNextSet() {
      current_set_index_ = std::min(current_set_index_ + 1, end_set_index_);
      return current_set_index_ < end_set_index_;
    }

    NGGridSetRef CurrentSet() const {
      DCHECK_LT(current_set_index_, end_set_index_);
      return track_collection_->GetSetAt(current_set_index_);
    }

   private:
    TrackCollectionPtr track_collection_;
    wtf_size_t current_set_index_;
    wtf_size_t end_set_index_;
  };

  typedef SetIteratorBase<false> SetIterator;
  typedef SetIteratorBase<true> ConstSetIterator;

  // |is_available_size_indefinite| is used to normalize percentage track
  // sizing functions (see the constructor for |NGGridSet|).
  NGGridSizingTrackCollection(
      const NGGridBlockTrackCollection& block_track_collection,
      bool is_available_size_indefinite);

  NGGridSizingTrackCollection(const NGGridSizingTrackCollection&) = delete;
  NGGridSizingTrackCollection& operator=(const NGGridSizingTrackCollection&) =
      delete;

  // Returns a reference to the set located at position |set_index|.
  NGGridSet& GetSetAt(wtf_size_t set_index);
  const NGGridSet& GetSetAt(wtf_size_t set_index) const;
  // Returns an iterator for all the sets contained in this collection.
  SetIterator GetSetIterator();
  ConstSetIterator GetConstSetIterator() const;
  // Returns an iterator for every set in this collection's |sets_| located at
  // an index in the interval [begin_set_index, end_set_index).
  SetIterator GetSetIterator(wtf_size_t begin_set_index,
                             wtf_size_t end_set_index);

  // This class should be specifically used for grid sizing.
  bool IsForSizing() const override { return true; }
  bool IsSpanningIndefiniteSet(wtf_size_t begin_set_index,
                               wtf_size_t end_set_index) const override;

  wtf_size_t NonCollapsedTrackCount() const {
    return non_collapsed_track_count_;
  }
  LayoutUnit TotalTrackSize() const;

  // Caches the initial geometry used to compute grid item contributions.
  void InitializeSetsGeometry(LayoutUnit first_set_offset,
                              LayoutUnit gutter_size);
  // Caches the final geometry required to place |NGGridSet| in the grid.
  void CacheSetsGeometry(LayoutUnit first_set_offset, LayoutUnit gutter_size);
  void SetIndefiniteGrowthLimitsToBaseSize();

  void ResetBaselines();

  void SetMajorBaseline(wtf_size_t set_index, LayoutUnit candidate_baseline);
  void SetMinorBaseline(wtf_size_t set_index, LayoutUnit candidate_baseline);

  void SetGutterSize(LayoutUnit gutter_size) { gutter_size_ = gutter_size; }

 private:
  void AppendTrackRange(
      const NGGridBlockTrackCollection::Range& block_track_range,
      const NGGridTrackList& specified_track_list,
      bool is_available_size_indefinite);

  wtf_size_t non_collapsed_track_count_;

  // Initially we only know some of the set sizes - others will be indefinite.
  // To represent this we store both the offset for the set, and a vector of all
  // last indefinite indices (or kNotFound if everything so far has been
  // definite). This allows us to get the appropriate size if a grid item spans
  // only fixed tracks, but will allow us to return an indefinite size if it
  // spans any indefinite set.
  //
  // As an example:
  //   grid-template-rows: auto auto 100px 100px auto 100px;
  //
  // Results in:
  //                  |  auto |  auto |   100   |   100   |   auto  |   100 |
  //   [{0, kNotFound}, {0, 0}, {0, 1}, {100, 1}, {200, 1}, {200, 4}, {300, 4}]
  //
  // Various queries (start/end refer to the grid lines):
  //  start: 0, end: 1 -> indefinite as:
  //    "start <= sets[end].last_indefinite_index"
  //  start: 1, end: 3 -> indefinite as:
  //    "start <= sets[end].last_indefinite_index"
  //  start: 2, end: 4 -> 200px
  //  start: 5, end: 6 -> 100px
  //  start: 3, end: 5 -> indefinite as:
  //    "start <= sets[end].last_indefinite_index"
  Vector<wtf_size_t> last_indefinite_indices_;

  // A vector of every set element that compose the entire collection's ranges;
  // track definitions from the same set are stored in consecutive positions,
  // preserving the order in which the definitions appear in their range.
  Vector<NGGridSet> sets_;
};

template <>
struct DowncastTraits<NGGridSizingTrackCollection> {
  static bool AllowFrom(const NGGridLayoutTrackCollection& layout_collection) {
    return layout_collection.IsForSizing();
  }
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGGridLayoutTrackCollection::Range)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGGridLayoutTrackCollection::SetGeometry)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_TRACK_COLLECTION_H_
