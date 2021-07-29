// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/ignore_paint_timing_scope.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_under_invalidation_checker.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

PaintController::CounterForTesting* PaintController::counter_for_testing_ =
    nullptr;

PaintController::PaintController(Usage usage)
    : usage_(usage),
      current_paint_artifact_(usage == kMultiplePaints
                                  ? base::MakeRefCounted<PaintArtifact>()
                                  : nullptr),
      new_paint_artifact_(base::MakeRefCounted<PaintArtifact>()),
      paint_chunker_(new_paint_artifact_->PaintChunks()) {
  // frame_first_paints_ should have one null frame since the beginning, so
  // that PaintController is robust even if it paints outside of BeginFrame
  // and EndFrame cycles. It will also enable us to combine the first paint
  // data in this PaintController into another PaintController on which we
  // replay the recorded results in the future.
  frame_first_paints_.push_back(FrameFirstPaint(nullptr));
}

PaintController::~PaintController() {
#if DCHECK_IS_ON()
  if (usage_ == kMultiplePaints) {
    // New display items should have been committed.
    DCHECK(new_paint_artifact_->IsEmpty());
    // And the committed_ flag should have been cleared by FinishCycle().
    DCHECK(!committed_);
  }
#endif
}

void PaintController::EnsureChunk() {
  if (paint_chunker_.EnsureChunk())
    CheckNewChunk();
}

void PaintController::RecordHitTestData(const DisplayItemClient& client,
                                        const IntRect& rect,
                                        TouchAction touch_action,
                                        bool blocking_wheel) {
  if (rect.IsEmpty())
    return;
  // In CompositeAfterPaint, we ensure a paint chunk for correct composited
  // hit testing. In pre-CompositeAfterPaint, this is unnecessary, except that
  // there is special touch action, and that we have a non-root effect so that
  // PaintChunksToCcLayer will emit paint operations for filters.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      touch_action == TouchAction::kAuto && !blocking_wheel &&
      CurrentPaintChunkProperties().Effect().IsRoot())
    return;

  PaintChunk::Id id(client, DisplayItem::kHitTest, current_fragment_);
  ValidateNewChunkId(id);
  if (paint_chunker_.AddHitTestDataToCurrentChunk(id, rect, touch_action,
                                                  blocking_wheel))
    CheckNewChunk();
}

void PaintController::RecordScrollHitTestData(
    const DisplayItemClient& client,
    DisplayItem::Type type,
    const TransformPaintPropertyNode* scroll_translation,
    const IntRect& rect) {
  PaintChunk::Id id(client, type, current_fragment_);
  ValidateNewChunkId(id);
  paint_chunker_.CreateScrollHitTestChunk(id, scroll_translation, rect);
  CheckNewChunk();
}

void PaintController::RecordSelection(
    absl::optional<PaintedSelectionBound> start,
    absl::optional<PaintedSelectionBound> end) {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  DCHECK(start.has_value() || end.has_value());
  paint_chunker_.AddSelectionToCurrentChunk(start, end);
}

void PaintController::SetPossibleBackgroundColor(
    const DisplayItemClient& client,
    Color color,
    uint64_t area) {
  PaintChunk::Id id(client, DisplayItem::kBoxDecorationBackground,
                    current_fragment_);
  ValidateNewChunkId(id);
  if (paint_chunker_.ProcessBackgroundColorCandidate(id, color, area))
    CheckNewChunk();
}

bool PaintController::UseCachedItemIfPossible(const DisplayItemClient& client,
                                              DisplayItem::Type type) {
  if (usage_ == kTransient)
    return false;

  if (ShouldInvalidateDisplayItemForBenchmark())
    return false;

  if (!ClientCacheIsValid(client))
    return false;

  if (IsCheckingUnderInvalidation()) {
    // We are checking under-invalidation of a subsequence enclosing this
    // display item. Let the client continue to actually paint the display item.
    return false;
  }

  auto cached_item =
      FindCachedItem(DisplayItem::Id(client, type, current_fragment_));
  if (cached_item == kNotFound) {
    // See FindOutOfOrderCachedItemForward() for explanation of the situation.
    return false;
  }

  ++num_cached_new_items_;
  next_item_to_match_ = cached_item + 1;
  // Items before |next_item_to_match_| have been copied so we don't need to
  // index them.
  if (next_item_to_match_ > next_item_to_index_)
    next_item_to_index_ = next_item_to_match_;

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    EnsureUnderInvalidationChecker().WouldUseCachedItem(cached_item);
    // Return false to let the painter actually paint. We will check if the new
    // painting is the same as the cached one.
    return false;
  }

  ProcessNewItem(new_paint_artifact_->GetDisplayItemList().AppendByMoving(
      current_paint_artifact_->GetDisplayItemList()[cached_item]));

  return true;
}

bool PaintController::UseCachedSubsequenceIfPossible(
    const DisplayItemClient& client) {
  if (usage_ == kTransient)
    return false;

  if (ShouldInvalidateSubsequenceForBenchmark())
    return false;

  if (!ClientCacheIsValid(client))
    return false;

  if (IsCheckingUnderInvalidation()) {
    // We are checking under-invalidation of an ancestor subsequence enclosing
    // this one. The ancestor subsequence is supposed to have already "copied",
    // so we should let the client continue to actually paint the descendant
    // subsequences without "copying".
    ++num_cached_new_subsequences_;
    return false;
  }

  wtf_size_t subsequence_index = GetSubsequenceIndex(client);
  if (subsequence_index == kNotFound)
    return false;

  const auto& markers = current_subsequences_.tree[subsequence_index];
  DCHECK_EQ(markers.client, &client);
  wtf_size_t start_item_index =
      current_paint_artifact_->PaintChunks()[markers.start_chunk_index]
          .begin_index;
  wtf_size_t end_item_index =
      current_paint_artifact_->PaintChunks()[markers.end_chunk_index - 1]
          .end_index;
  if (end_item_index > start_item_index &&
      current_paint_artifact_->GetDisplayItemList()[start_item_index]
          .IsTombstone()) {
    // The subsequence has already been copied, indicating that the same client
    // created multiple subsequences. If DCHECK_IS_ON(), then we should have
    // encountered the DCHECK at the end of EndSubsequence() during the previous
    // paint.
    NOTREACHED();
    return false;
  }

  if (next_item_to_match_ == start_item_index) {
    // We are matching new and cached display items sequentially. Skip the
    // subsequence for later sequential matching of individual display items.
    next_item_to_match_ = end_item_index;
    // Items before |next_item_to_match_| have been copied so we don't need to
    // index them.
    if (next_item_to_match_ > next_item_to_index_)
      next_item_to_index_ = next_item_to_match_;
  }

  num_cached_new_items_ += end_item_index - start_item_index;
  ++num_cached_new_subsequences_;

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    EnsureUnderInvalidationChecker().WouldUseCachedSubsequence(client);
    // Return false to let the painter actually paint. We will check if the new
    // painting is the same as the cached one.
    return false;
  }

  AppendSubsequenceByMoving(client, subsequence_index,
                            markers.start_chunk_index, markers.end_chunk_index);
  return true;
}

wtf_size_t PaintController::GetSubsequenceIndex(
    const DisplayItemClient& client) const {
  auto result = current_subsequences_.map.find(&client);
  if (result == current_subsequences_.map.end())
    return kNotFound;
  DCHECK_EQ(&client, current_subsequences_.tree[result->value].client);
  return result->value;
}

const PaintController::SubsequenceMarkers*
PaintController::GetSubsequenceMarkers(const DisplayItemClient& client) const {
  wtf_size_t index = GetSubsequenceIndex(client);
  if (index == kNotFound)
    return nullptr;
  return &current_subsequences_.tree[index];
}

wtf_size_t PaintController::BeginSubsequence(const DisplayItemClient& client) {
  // Force new paint chunk which is required for subsequence caching.
  SetWillForceNewChunk(true);
  new_subsequences_.tree.push_back(SubsequenceMarkers{&client, NumNewChunks()});
  return new_subsequences_.tree.size() - 1;
}

void PaintController::EndSubsequence(wtf_size_t subsequence_index) {
  auto& markers = new_subsequences_.tree[subsequence_index];

  if (IsCheckingUnderInvalidation()) {
    under_invalidation_checker_->WillEndSubsequence(*markers.client,
                                                    markers.start_chunk_index);
  }

  wtf_size_t end_chunk_index = NumNewChunks();
  if (markers.start_chunk_index == end_chunk_index) {
    // Omit the empty subsequence. The WillForceNewChunk flag set in
    // BeginSubsequence() still applies, but it's useful to reduce churns of
    // raster invalidation and compositing when the subsequence switches between
    // empty and non-empty.
    new_subsequences_.tree.pop_back();
    return;
  }

  // Force new paint chunk which is required for subsequence caching.
  SetWillForceNewChunk(true);

#if DCHECK_IS_ON()
  DCHECK(!new_subsequences_.map.Contains(markers.client))
      << "Multiple subsequences for client: " << markers.client->DebugName();

  // Check tree integrity.
  if (subsequence_index > 0) {
    DCHECK_GE(markers.start_chunk_index,
              new_subsequences_.tree[subsequence_index - 1].end_chunk_index);
  }
  for (auto i = subsequence_index + 1; i < new_subsequences_.tree.size(); i++) {
    auto& child_markers = new_subsequences_.tree[i];
    DCHECK_GE(child_markers.start_chunk_index, markers.start_chunk_index);
    DCHECK_LE(child_markers.end_chunk_index, end_chunk_index);
  }
#endif

  new_subsequences_.map.insert(markers.client, subsequence_index);
  markers.end_chunk_index = end_chunk_index;
}

void PaintController::CheckNewItem(DisplayItem& display_item) {
  if (usage_ == kTransient)
    return;

#if DCHECK_IS_ON()
  if (display_item.IsCacheable()) {
    auto& new_display_item_list = new_paint_artifact_->GetDisplayItemList();
    auto index = FindItemFromIdIndexMap(display_item.GetId(),
                                        new_display_item_id_index_map_,
                                        new_display_item_list);
    if (index != kNotFound) {
      ShowDebugData();
      NOTREACHED() << "DisplayItem " << display_item.AsDebugString().Utf8()
                   << " has duplicated id with previous "
                   << new_display_item_list[index].AsDebugString().Utf8()
                   << " (index=" << index << ")";
    }
    AddToIdIndexMap(display_item.GetId(), new_display_item_list.size() - 1,
                    new_display_item_id_index_map_);
  }
#endif

  if (IsCheckingUnderInvalidation())
    under_invalidation_checker_->CheckNewItem();
}

void PaintController::ProcessNewItem(DisplayItem& display_item) {
  if (IsSkippingCache() && usage_ == kMultiplePaints) {
    display_item.Client().Invalidate(PaintInvalidationReason::kUncacheable);
    display_item.SetUncacheable();
  }

  if (paint_chunker_.IncrementDisplayItemIndex(display_item))
    CheckNewChunk();

  if (!frame_first_paints_.back().first_painted && display_item.IsDrawing() &&
      // Here we ignore all document-background paintings because we don't
      // know if the background is default. ViewPainter should have called
      // setFirstPainted() if this display item is for non-default
      // background.
      display_item.GetType() != DisplayItem::kDocumentBackground &&
      display_item.DrawsContent()) {
    SetFirstPainted();
  }

  CheckNewItem(display_item);
}

void PaintController::CheckNewChunk() {
#if DCHECK_IS_ON()
  auto& chunks = new_paint_artifact_->PaintChunks();
  if (chunks.back().is_cacheable) {
    AddToIdIndexMap(chunks.back().id, chunks.size() - 1,
                    new_paint_chunk_id_index_map_);
  }
#endif

  if (IsCheckingUnderInvalidation())
    under_invalidation_checker_->CheckNewChunk();
}

void PaintController::InvalidateAllForTesting() {
  CheckNoNewPaint();
  current_paint_artifact_ = base::MakeRefCounted<PaintArtifact>();
  current_subsequences_.map.clear();
  current_subsequences_.tree.clear();
  cache_is_all_invalid_ = true;
}

void PaintController::UpdateCurrentPaintChunkProperties(
    const PaintChunk::Id* id,
    const PropertyTreeStateOrAlias& properties) {
  if (id) {
    PaintChunk::Id id_with_fragment(*id, current_fragment_);
    ValidateNewChunkId(id_with_fragment);
    paint_chunker_.UpdateCurrentPaintChunkProperties(&id_with_fragment,
                                                     properties);
  } else {
    paint_chunker_.UpdateCurrentPaintChunkProperties(nullptr, properties);
  }
}

bool PaintController::ClientCacheIsValid(
    const DisplayItemClient& client) const {
#if DCHECK_IS_ON()
  DCHECK(client.IsAlive());
#endif
  if (IsSkippingCache() || cache_is_all_invalid_)
    return false;
  return client.IsValid();
}

wtf_size_t PaintController::FindItemFromIdIndexMap(
    const DisplayItem::Id& id,
    const IdIndexMap& display_item_id_index_map,
    const DisplayItemList& list) {
  auto it = display_item_id_index_map.find(id.AsHashKey());
  if (it == display_item_id_index_map.end())
    return kNotFound;

  wtf_size_t index = it->value;
  const DisplayItem& existing_item = list[index];
  if (existing_item.IsTombstone())
    return kNotFound;
  DCHECK_EQ(existing_item.GetId(), id);
  return index;
}

void PaintController::AddToIdIndexMap(const DisplayItem::Id& id,
                                      wtf_size_t index,
                                      IdIndexMap& map) {
  DCHECK(!map.Contains(id.AsHashKey()));
  map.insert(id.AsHashKey(), index);
}

wtf_size_t PaintController::FindCachedItem(const DisplayItem::Id& id) {
  DCHECK(ClientCacheIsValid(id.client));

  if (next_item_to_match_ <
      current_paint_artifact_->GetDisplayItemList().size()) {
    // If current_list[next_item_to_match_] matches the new item, we don't need
    // to update and lookup the index, which is fast. This is the common case
    // that the current list and the new list are in the same order around the
    // new item.
    const DisplayItem& item =
        current_paint_artifact_->GetDisplayItemList()[next_item_to_match_];
    // We encounter an item that has already been copied which indicates we
    // can't do sequential matching.
    if (!item.IsTombstone() && id == item.GetId()) {
#if DCHECK_IS_ON()
      ++num_sequential_matches_;
#endif
      return next_item_to_match_;
    }
  }

  wtf_size_t found_index =
      FindItemFromIdIndexMap(id, out_of_order_item_id_index_map_,
                             current_paint_artifact_->GetDisplayItemList());
  if (found_index != kNotFound) {
#if DCHECK_IS_ON()
    ++num_out_of_order_matches_;
#endif
    return found_index;
  }

  return FindOutOfOrderCachedItemForward(id);
}

// Find forward for the item and index all skipped indexable items.
wtf_size_t PaintController::FindOutOfOrderCachedItemForward(
    const DisplayItem::Id& id) {
  for (auto i = next_item_to_index_;
       i < current_paint_artifact_->GetDisplayItemList().size(); ++i) {
    const DisplayItem& item = current_paint_artifact_->GetDisplayItemList()[i];
    if (item.IsTombstone())
      continue;
    if (id == item.GetId()) {
#if DCHECK_IS_ON()
      ++num_sequential_matches_;
#endif
      return i;
    }
    if (item.IsCacheable()) {
#if DCHECK_IS_ON()
      ++num_indexed_items_;
#endif
      AddToIdIndexMap(item.GetId(), i, out_of_order_item_id_index_map_);
      next_item_to_index_ = i + 1;
    }
  }

#if DCHECK_IS_ON()
  // The display item newly appears while the client is not invalidated. The
  // situation alone (without other kinds of under-invalidations) won't corrupt
  // rendering, but causes AddItemToIndexIfNeeded() for all remaining display
  // item, which is not the best for performance. In this case, the caller
  // should fall back to repaint the display item.
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    // Ensure our paint invalidation tests don't trigger the less performant
    // situation which should be rare.
    DLOG(WARNING) << "Can't find cached display item: " << id;
    ShowDebugData();
  }
#endif
  return kNotFound;
}

void PaintController::AppendSubsequenceByMoving(const DisplayItemClient& client,
                                                wtf_size_t subsequence_index,
                                                wtf_size_t start_chunk_index,
                                                wtf_size_t end_chunk_index) {
#if DCHECK_IS_ON()
  DCHECK(!RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled());
  DCHECK_EQ(&client, current_subsequences_.tree[subsequence_index].client);
  DCHECK_GT(end_chunk_index, start_chunk_index);
  auto properties_before_subsequence = CurrentPaintChunkProperties();
#endif

  auto new_start_chunk_index = NumNewChunks();
  auto new_subsequence_index = BeginSubsequence(client);

  auto& current_chunks = current_paint_artifact_->PaintChunks();
  for (auto chunk_index = start_chunk_index; chunk_index < end_chunk_index;
       ++chunk_index) {
    auto& cached_chunk = current_chunks[chunk_index];
    ValidateNewChunkId(cached_chunk.id);
    paint_chunker_.AppendByMoving(std::move(cached_chunk));
    CheckNewChunk();
  }

  auto& new_display_item_list = new_paint_artifact_->GetDisplayItemList();
#if DCHECK_IS_ON()
  wtf_size_t new_item_start_index = new_display_item_list.size();
#endif
  new_display_item_list.AppendSubsequenceByMoving(
      current_paint_artifact_->GetDisplayItemList(),
      current_chunks[start_chunk_index].begin_index,
      current_chunks[end_chunk_index - 1].end_index);

#if DCHECK_IS_ON()
  for (auto& item : new_display_item_list.ItemsInRange(
           new_item_start_index, new_display_item_list.size())) {
    DCHECK(!item.IsTombstone());
    DCHECK(!item.IsCacheable() || ClientCacheIsValid(item.Client()));
    CheckNewItem(item);
  }
#endif

  // Keep descendant subsequence entries.
  for (wtf_size_t i = subsequence_index + 1;
       i < current_subsequences_.tree.size(); i++) {
    auto& markers = current_subsequences_.tree[i];
    if (markers.start_chunk_index >= end_chunk_index)
      break;
    DCHECK(!new_subsequences_.map.Contains(markers.client))
        << "Multiple subsequences for client: " << markers.client->DebugName();
    new_subsequences_.map.insert(markers.client, new_subsequences_.tree.size());
    new_subsequences_.tree.push_back(SubsequenceMarkers{
        markers.client,
        markers.start_chunk_index + new_start_chunk_index - start_chunk_index,
        markers.end_chunk_index + new_start_chunk_index - start_chunk_index,
        /*is_moved_from_cached_subsequence*/ true});
    ++num_cached_new_subsequences_;
  }

  EndSubsequence(new_subsequence_index);
  new_subsequences_.tree[new_subsequence_index]
      .is_moved_from_cached_subsequence = true;

#if DCHECK_IS_ON()
  DCHECK_EQ(properties_before_subsequence, CurrentPaintChunkProperties());
#endif
}

DISABLE_CFI_PERF
void PaintController::CommitNewDisplayItems() {
  TRACE_EVENT2(
      "blink,benchmark", "PaintController::commitNewDisplayItems",
      "current_display_list_size",
      current_paint_artifact_
          ? current_paint_artifact_->GetDisplayItemList().size()
          : 0,
      "num_non_cached_new_items",
      new_paint_artifact_->GetDisplayItemList().size() - num_cached_new_items_);

  if (counter_for_testing_) {
    counter_for_testing_->num_cached_items += num_cached_new_items_;
    counter_for_testing_->num_cached_subsequences +=
        num_cached_new_subsequences_;
  }

  num_cached_new_items_ = 0;
  num_cached_new_subsequences_ = 0;
#if DCHECK_IS_ON()
  new_display_item_id_index_map_.clear();
  new_paint_chunk_id_index_map_.clear();
#endif

  cache_is_all_invalid_ = false;
  committed_ = true;

  under_invalidation_checker_.reset();

  DCHECK_EQ(new_subsequences_.map.size(), new_subsequences_.tree.size());
  current_subsequences_.map.clear();
  current_subsequences_.tree.clear();
  std::swap(current_subsequences_, new_subsequences_);

  current_paint_artifact_ = std::move(new_paint_artifact_);
  if (usage_ == kMultiplePaints) {
    new_paint_artifact_ = base::MakeRefCounted<PaintArtifact>(
        current_paint_artifact_->GetDisplayItemList().size(),
        current_paint_artifact_->PaintChunks().size());
    paint_chunker_.ResetChunks(&new_paint_artifact_->PaintChunks());
  } else {
    new_paint_artifact_ = nullptr;
    paint_chunker_.ResetChunks(nullptr);
  }

  next_item_to_match_ = 0;
  next_item_to_index_ = 0;
  out_of_order_item_id_index_map_.clear();

#if DCHECK_IS_ON()
  num_indexed_items_ = 0;
  num_sequential_matches_ = 0;
  num_out_of_order_matches_ = 0;
#endif
}

void PaintController::FinishCycle() {
  if (usage_ == kTransient || !committed_)
    return;

  CheckNoNewPaint();
  committed_ = false;

  // Validate display item clients that have validly cached subsequence or
  // display items in this PaintController.
  for (auto& item : current_subsequences_.tree) {
    if (item.is_moved_from_cached_subsequence) {
      // We don't need to validate the client of a cached subsequence, because
      // it should be already valid. See http://crbug.com/1050090 for more
      // details.
      DCHECK(!item.client->IsCacheable() || ClientCacheIsValid(*item.client));
      continue;
    }
    if (item.client->IsCacheable())
      item.client->Validate();
  }
  for (wtf_size_t i = 0; i < current_paint_artifact_->PaintChunks().size();
       i++) {
    auto& chunk = current_paint_artifact_->PaintChunks()[i];
    chunk.client_is_just_created = false;
    const auto& client = chunk.id.client;
    if (chunk.is_moved_from_cached_subsequence) {
      // We don't need to validate the clients of paint chunks and display
      // items that are moved from a cached subsequence, because they should be
      // already valid. See http://crbug.com/1050090 for more details.
#if DCHECK_IS_ON()
      DCHECK(!chunk.is_cacheable || ClientCacheIsValid(client));
      for (const auto& item : current_paint_artifact_->DisplayItemsInChunk(i))
        DCHECK(!item.IsCacheable() || ClientCacheIsValid(item.Client()));
#endif
      continue;
    }
    if (client.IsCacheable())
      client.Validate();

    for (const auto& item : current_paint_artifact_->DisplayItemsInChunk(i)) {
      if (item.Client().IsCacheable())
        item.Client().Validate();
    }
  }

#if DCHECK_IS_ON()
  if (VLOG_IS_ON(1)) {
    VLOG(1) << "PaintController::FinishCycle() completed";
    if (VLOG_IS_ON(3))
      ShowDebugDataWithPaintRecords();
    else if (VLOG_IS_ON(2))
      ShowDebugData();
    else if (VLOG_IS_ON(1))
      ShowCompactDebugData();
  }
#endif
}

size_t PaintController::ApproximateUnsharedMemoryUsage() const {
  CheckNoNewPaint();

  size_t memory_usage = sizeof(*this);

  // Memory outside this class due to paint artifacts.
  if (current_paint_artifact_)
    memory_usage += current_paint_artifact_->ApproximateUnsharedMemoryUsage();
  if (new_paint_artifact_)
    memory_usage += new_paint_artifact_->ApproximateUnsharedMemoryUsage();

  // External objects, shared with the embedder, such as PaintRecord, should be
  // excluded to avoid double counting. It is the embedder's responsibility to
  // count such objects.

  // Memory outside this class due to current_subsequences_ and
  // new_subsequences_.
  memory_usage += current_subsequences_.map.Capacity() *
                  sizeof(decltype(current_subsequences_.map)::value_type);
  memory_usage += current_subsequences_.tree.CapacityInBytes();
  DCHECK(new_subsequences_.map.IsEmpty());
  memory_usage += new_subsequences_.map.Capacity() *
                  sizeof(decltype(new_subsequences_.map)::value_type);
  memory_usage += new_subsequences_.tree.CapacityInBytes();

  return memory_usage;
}

PaintUnderInvalidationChecker&
PaintController::EnsureUnderInvalidationChecker() {
  DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled());
  if (!under_invalidation_checker_) {
    under_invalidation_checker_ =
        std::make_unique<PaintUnderInvalidationChecker>(*this);
  }
  return *under_invalidation_checker_;
}

bool PaintController::IsCheckingUnderInvalidation() const {
  if (under_invalidation_checker_) {
    DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled());
    return under_invalidation_checker_->IsChecking();
  }
  return false;
}

void PaintController::SetFirstPainted() {
  if (!IgnorePaintTimingScope::IgnoreDepth())
    frame_first_paints_.back().first_painted = true;
}

void PaintController::SetTextPainted() {
  if (!IgnorePaintTimingScope::IgnoreDepth())
    frame_first_paints_.back().text_painted = true;
}

void PaintController::SetImagePainted() {
  if (!IgnorePaintTimingScope::IgnoreDepth())
    frame_first_paints_.back().image_painted = true;
}

void PaintController::BeginFrame(const void* frame) {
  frame_first_paints_.push_back(FrameFirstPaint(frame));
}

FrameFirstPaint PaintController::EndFrame(const void* frame) {
  FrameFirstPaint result = frame_first_paints_.back();
  DCHECK(result.frame == frame);
  frame_first_paints_.pop_back();
  return result;
}

void PaintController::ValidateNewChunkId(const PaintChunk::Id& id) {
  if (IsSkippingCache()) {
    if (usage_ == kMultiplePaints)
      id.client.Invalidate(PaintInvalidationReason::kUncacheable);
    return;
  }

#if DCHECK_IS_ON()
  if (DisplayItem::IsForeignLayerType(id.type))
    return;

  auto it = new_paint_chunk_id_index_map_.find(id.AsHashKey());
  if (it != new_paint_chunk_id_index_map_.end()) {
    ShowDebugData();
    NOTREACHED() << "New paint chunk id " << id
                 << " has duplicated id with previous chuck "
                 << new_paint_artifact_->PaintChunks()[it->value];
  }
#endif
}

bool PaintController::ShouldInvalidateDisplayItemForBenchmark() {
  if (benchmark_mode_ == PaintBenchmarkMode::kCachingDisabled)
    return true;

  // For kPartialInvalidation, invalidate one out of every
  // |kInvalidateDisplayItemInterval| display items for the micro benchmark of
  // record time with partial invalidation.
  constexpr int kInvalidateDisplayItemInterval = 8;
  return benchmark_mode_ == PaintBenchmarkMode::kPartialInvalidation &&
         !(partial_invalidation_display_item_count_++ %
           kInvalidateDisplayItemInterval);
}

bool PaintController::ShouldInvalidateSubsequenceForBenchmark() {
  if (benchmark_mode_ == PaintBenchmarkMode::kCachingDisabled ||
      benchmark_mode_ == PaintBenchmarkMode::kSubsequenceCachingDisabled)
    return true;

  // Similar to the ShouldInvalidateDisplayItemsForBenchmark(), but for
  // subsequences.
  constexpr int kInvalidateSubsequenceInterval = 2;
  return benchmark_mode_ == PaintBenchmarkMode::kPartialInvalidation &&
         !(partial_invalidation_subsequence_count_++ %
           kInvalidateSubsequenceInterval);
}

void PaintController::SetBenchmarkMode(PaintBenchmarkMode mode) {
  DCHECK(new_paint_artifact_->IsEmpty());
  benchmark_mode_ = mode;
  if (mode == PaintBenchmarkMode::kPartialInvalidation) {
    partial_invalidation_display_item_count_ = 0;
    partial_invalidation_subsequence_count_ = 0;
  } else if (mode == PaintBenchmarkMode::kSmallInvalidation) {
    auto& subsequences = current_subsequences_.tree;
    if (subsequences.size()) {
      // Invalidate the clients of the middle subsequence and its ancestors.
      wtf_size_t middle_index = subsequences.size() / 2;
      const auto& middle_markers = subsequences[middle_index];
      for (wtf_size_t i = 0; i <= middle_index; i++) {
        const auto& markers = subsequences[i];
        DCHECK_LE(markers.start_chunk_index, middle_markers.start_chunk_index);
        if (markers.end_chunk_index >= middle_markers.end_chunk_index)
          markers.client->Invalidate();
      }
    }
  }
}

}  // namespace blink
