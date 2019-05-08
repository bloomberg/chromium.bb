// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"

#include <memory>

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/layout/layout_file_upload_control.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

// Calculate metrics candidate every 1 second after the first text pre-paint.
static constexpr TimeDelta kTimerDelay = TimeDelta::FromSeconds(1);
constexpr size_t kTextNodeNumberLimit = 5000;
constexpr size_t kVisitedTextObjectsLimit = 10000;

static bool LargeTextFirst(const base::WeakPtr<TextRecord>& a,
                           const base::WeakPtr<TextRecord>& b) {
  DCHECK(a);
  DCHECK(b);
  if (a->first_size != b->first_size)
    return a->first_size > b->first_size;
  // This make sure that two different nodes with the same |first_size| wouldn't
  // be merged in the set.
  return a->node_id > b->node_id;
}

TextPaintTimingDetector::TextPaintTimingDetector(LocalFrameView* frame_view)
    : records_manager_(),
      timer_(frame_view->GetFrame().GetTaskRunner(TaskType::kInternalDefault),
             this,
             &TextPaintTimingDetector::TimerFired),
      frame_view_(frame_view) {}

void TextPaintTimingDetector::PopulateTraceValue(
    TracedValue& value,
    const TextRecord& first_text_paint,
    unsigned candidate_index) const {
  value.SetInteger("DOMNodeId", static_cast<int>(first_text_paint.node_id));
#ifndef NDEBUG
  value.SetString("text", first_text_paint.text);
#endif
  value.SetInteger("size", static_cast<int>(first_text_paint.first_size));
  value.SetInteger("candidateIndex", candidate_index);
  value.SetBoolean("isMainFrame", frame_view_->GetFrame().IsMainFrame());
  value.SetBoolean("isOOPIF",
                   !frame_view_->GetFrame().LocalFrameRoot().IsMainFrame());
}

void TextPaintTimingDetector::OnLargestTextDetected(
    const TextRecord& largest_text_record) {
  largest_text_paint_ = largest_text_record.paint_time;
  largest_text_paint_size_ = largest_text_record.first_size;
  auto value = std::make_unique<TracedValue>();
  PopulateTraceValue(*value, largest_text_record, count_candidates_++);
  TRACE_EVENT_MARK_WITH_TIMESTAMP2(
      "loading", "LargestTextPaint::Candidate", largest_text_paint_, "data",
      std::move(value), "frame", ToTraceValue(&frame_view_->GetFrame()));
}

void TextPaintTimingDetector::TimerFired(TimerBase* time) {
  // Wrap |UpdateCandidate| method in TimerFired so that we can drop |time| for
  // |UpdateCandidate| in testing.
  UpdateCandidate();
}

void TextPaintTimingDetector::UpdateCandidate() {
  TextRecord* candidate = records_manager_.FindLargestPaintCandidate();
  if (!candidate) {
    largest_text_paint_ = base::TimeTicks();
    largest_text_paint_size_ = 0;
    frame_view_->GetPaintTimingDetector().DidChangePerformanceTiming();
  } else if (candidate->paint_time != largest_text_paint_) {
    OnLargestTextDetected(*candidate);
    frame_view_->GetPaintTimingDetector().DidChangePerformanceTiming();
  }
  frame_view_->GetPaintTimingDetector().NotifyLargestText(
      largest_text_paint_, largest_text_paint_size_);
}

void TextPaintTimingDetector::OnPaintFinished() {
  if (need_update_timing_at_frame_end_) {
    need_update_timing_at_frame_end_ = false;
    UpdateCandidate();
  }
  if (records_manager_.NeedMeausuringPaintTime()) {
    // Start repeating timer only once at the first text paint.
    if (!timer_.IsActive())
      timer_.StartRepeating(kTimerDelay, FROM_HERE);
    if (!awaiting_swap_promise_) {
      RegisterNotifySwapTime(
          CrossThreadBind(&TextPaintTimingDetector::ReportSwapTime,
                          WrapCrossThreadWeakPersistent(this)));
    }
  }
}

void TextPaintTimingDetector::NotifyNodeRemoved(DOMNodeId node_id) {
  if (!is_recording_)
    return;
  if (!records_manager_.IsKnownVisibleNode(node_id))
    return;
  records_manager_.SetNodeDetachedIfNeeded(node_id);
  need_update_timing_at_frame_end_ = true;
}

void TextPaintTimingDetector::RegisterNotifySwapTime(
    ReportTimeCallback callback) {
  // ReportSwapTime on layerTreeView will queue a swap-promise, the callback is
  // called when the swap for current render frame completes or fails to happen.
  LocalFrame& frame = frame_view_->GetFrame();
  if (!frame.GetPage())
    return;
  frame.GetPage()->GetChromeClient().NotifySwapTime(
      frame, ConvertToBaseCallback(std::move(callback)));
  awaiting_swap_promise_ = true;
}

void TextPaintTimingDetector::ReportSwapTime(WebWidgetClient::SwapResult result,
                                             base::TimeTicks timestamp) {
  records_manager_.AssignPaintTimeToQueuedNodes(timestamp);
  awaiting_swap_promise_ = false;
}

void TextPaintTimingDetector::AggregateText(
    const LayoutObject& object,
    const PropertyTreeState& current_paint_chunk_properties) {
  if (!is_recording_)
    return;
  // Text nodes are aggregated with the lowest of either block-level block
  // element or self-painting inline-level element.
  DCHECK_GT(walking_block_stack_.size(), 0u);
  AggregateTextToClosestBlock(object, current_paint_chunk_properties);
}

void TextPaintTimingDetector::AggregateTextToClosestBlock(
    const LayoutObject& text_object,
    const PropertyTreeState& current_paint_chunk_properties) {
  DCHECK_GT(walking_block_stack_.size(), 0u);

  if (visited_text_objects_.Contains(&text_object) ||
      visited_text_objects_.size() >= kVisitedTextObjectsLimit)
    return;

  IntRect visual_rect = text_object.FragmentsVisualRectBoundingBox();
  if (visual_rect.IsEmpty())
    return;

  visited_text_objects_.insert(&text_object);
  FloatRect mapped_visual_rect =
      frame_view_->GetPaintTimingDetector().CalculateVisualRect(
          visual_rect, current_paint_chunk_properties);
  if (mapped_visual_rect.Size().Area() == 0)
    return;
  walking_block_stack_.back().Aggregate(mapped_visual_rect);
}

void TextPaintTimingDetector::RecordAggregatedText(
    const LayoutObject& text_aggregating_object,
    uint64_t aggregated_size) {
  if (!is_recording_)
    return;
  DCHECK(!records_manager_.HasTooManyNodes());
  // TODO(crbug.com/933479): Use LayoutObject::GeneratingNode() to include
  // anonymous objects' rect.
  Node* node = text_aggregating_object.GetNode();
  if (!node)
    return;
  DOMNodeId node_id = DOMNodeIds::IdForNode(node);
  DCHECK_NE(node_id, kInvalidDOMNodeId);

  records_manager_.MarkNodeReattachedIfNeeded(node_id);

  // This metric defines the size of a text block by its first size. So it
  // early-returns if the text block has been recorded.
  if (records_manager_.HasRecorded(node_id))
    return;

  if (aggregated_size == 0) {
    records_manager_.RecordInvisibleNode(node_id);
  } else {
    records_manager_.RecordVisibleNode(node_id, aggregated_size,
                                       text_aggregating_object);
  }

  if (records_manager_.HasTooManyNodes()) {
    TRACE_EVENT_INSTANT2("loading", "TextPaintTimingDetector::OverNodeLimit",
                         TRACE_EVENT_SCOPE_THREAD, "count_size_non_zero_nodes",
                         records_manager_.CountVisibleNodes(),
                         "count_size_zero_nodes",
                         records_manager_.CountInvisibleNodes());
    StopRecordEntries();
  }
}

TextRecord* TextPaintTimingDetector::FindLargestPaintCandidate() {
  return records_manager_.FindLargestPaintCandidate();
}

void TextPaintTimingDetector::StopRecordEntries() {
  timer_.Stop();
  is_recording_ = false;
}

void TextPaintTimingDetector::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_view_);
}

TextRecordsManager::TextRecordsManager() : size_ordered_set_(&LargeTextFirst) {}

bool TextRecordsManager::AreAllVisibleNodesDetached() const {
  return visible_node_map_.size() == detached_ids_.size();
}

void TextRecordsManager::SetNodeDetachedIfNeeded(const DOMNodeId& node_id) {
  if (!visible_node_map_.Contains(node_id))
    return;
  if (detached_ids_.Contains(node_id))
    return;
  detached_ids_.insert(node_id);
  is_result_invalidated_ = true;
}

void TextRecordsManager::AssignPaintTimeToQueuedNodes(
    const base::TimeTicks& timestamp) {
  // If texts_queued_for_paint_time_.size == 0, it means the array has been
  // consumed in a callback earlier than this one. That violates the assumption
  // that only one or zero callback will be called after one OnPaintFinished.
  DCHECK_GT(texts_queued_for_paint_time_.size(), 0UL);
  while (!texts_queued_for_paint_time_.IsEmpty()) {
    base::WeakPtr<TextRecord>& record = texts_queued_for_paint_time_.front();
    DCHECK(visible_node_map_.Contains(record->node_id));
    DCHECK_EQ(record->paint_time, base::TimeTicks());
    record->paint_time = timestamp;

    texts_queued_for_paint_time_.pop_front();
    is_result_invalidated_ = true;
  }
}

void TextRecordsManager::MarkNodeReattachedIfNeeded(const DOMNodeId& node_id) {
  if (!detached_ids_.Contains(node_id))
    return;
  DCHECK(visible_node_map_.Contains(node_id) ||
         invisible_node_ids_.Contains(node_id));
  detached_ids_.erase(node_id);
  is_result_invalidated_ = true;
}

bool TextRecordsManager::HasRecorded(const DOMNodeId& node_id) const {
  return visible_node_map_.Contains(node_id) ||
         invisible_node_ids_.Contains(node_id);
}

void TextRecordsManager::RecordInvisibleNode(const DOMNodeId& node_id) {
  DCHECK(!HasTooManyNodes());
  invisible_node_ids_.insert(node_id);
}

void TextRecordsManager::RecordVisibleNode(const DOMNodeId& node_id,
                                           const uint64_t& visual_size,
                                           const LayoutObject& text_object) {
  DCHECK(!HasTooManyNodes());
  DCHECK_GT(visual_size, 0u);
  std::unique_ptr<TextRecord> record =
      std::make_unique<TextRecord>(node_id, visual_size);
#ifndef NDEBUG
  String text;
  if (text_object.IsText()) {
    text = ToLayoutText(&text_object)->GetText();
  } else if (text_object.IsFileUploadControl()) {
    text = ToLayoutFileUploadControl(&text_object)->FileTextValue();
  } else {
    text = String("NON-TEXT-OBJECT");
  }
  record->text = text;
#endif
  size_ordered_set_.emplace(record->AsWeakPtr());
  QueueToMeasurePaintTime(record->AsWeakPtr());
  visible_node_map_.insert(node_id, std::move(record));
  is_result_invalidated_ = true;
}

void TextRecordsManager::QueueToMeasurePaintTime(
    base::WeakPtr<TextRecord> record) {
  texts_queued_for_paint_time_.push_back(std::move(record));
}

bool TextRecordsManager::HasTooManyNodes() const {
  return visible_node_map_.size() + invisible_node_ids_.size() >=
         kTextNodeNumberLimit;
}

TextRecord* TextRecordsManager::FindLargestPaintCandidate() {
  DCHECK_EQ(visible_node_map_.size(), size_ordered_set_.size());
  if (!is_result_invalidated_)
    return cached_largest_paint_candidate_;
  TextRecord* new_largest_paint_candidate = nullptr;
  for (auto it = size_ordered_set_.begin(); it != size_ordered_set_.end();
       ++it) {
    // WeakPtr::IsValid() is expensive. We use raw pointer to reduce the checks.
    TextRecord* text_record = (*it).get();
    DCHECK(text_record);
    if (detached_ids_.Contains(text_record->node_id) ||
        text_record->paint_time.is_null())
      continue;
    DCHECK(visible_node_map_.Contains(text_record->node_id));
    new_largest_paint_candidate = text_record;
    break;
  }
  cached_largest_paint_candidate_ = new_largest_paint_candidate;
  is_result_invalidated_ = false;
  return new_largest_paint_candidate;
}

void TextPaintTimingDetector::WillWalkTextAggregatingNode() {
  walking_block_stack_.push_back(BlockInfo());
}

void TextPaintTimingDetector::DidWalkTextAggregatingNode(
    const LayoutBoxModelObject& text_aggregating_block) {
  uint64_t aggregated_size = walking_block_stack_.back().AggregatedTextSize();
  if (aggregated_size > 0)
    RecordAggregatedText(text_aggregating_block, aggregated_size);
  walking_block_stack_.pop_back();
}
}  // namespace blink
