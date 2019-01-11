// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
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

static bool LargeTextOnTop(const std::unique_ptr<TextRecord>& a,
                           const std::unique_ptr<TextRecord>& b) {
  return a->first_size < b->first_size;
}

static bool LateTextOnTop(const std::unique_ptr<TextRecord>& a,
                          const std::unique_ptr<TextRecord>& b) {
  return a->first_paint_time < b->first_paint_time;
}

TextPaintTimingDetector::TextPaintTimingDetector(LocalFrameView* frame_view)
    : largest_text_heap_(&LargeTextOnTop),
      latest_text_heap_(&LateTextOnTop),
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
  value.SetString("frame",
                  IdentifiersFactory::FrameId(&frame_view_->GetFrame()));
}

void TextPaintTimingDetector::OnLargestTextDetected(
    const TextRecord& largest_text_record) {
  largest_text_paint_ = largest_text_record.first_paint_time;
  largest_text_paint_size_ = largest_text_record.first_size;
  std::unique_ptr<TracedValue> value = TracedValue::Create();
  PopulateTraceValue(*value, largest_text_record,
                     largest_text_candidate_index_max_++);
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
      "loading", "LargestTextPaint::Candidate", TRACE_EVENT_SCOPE_THREAD,
      largest_text_paint_, "data", std::move(value));
}

void TextPaintTimingDetector::OnLastTextDetected(
    const TextRecord& last_text_record) {
  last_text_paint_ = last_text_record.first_paint_time;
  last_text_paint_size_ = last_text_record.first_size;

  std::unique_ptr<TracedValue> value = TracedValue::Create();
  PopulateTraceValue(*value, last_text_record,
                     last_text_candidate_index_max_++);
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
      "loading", "LastTextPaint::Candidate", TRACE_EVENT_SCOPE_THREAD,
      last_text_paint_, "data", std::move(value));
}

void TextPaintTimingDetector::TimerFired(TimerBase* time) {
  // Wrap Analyze method in TimerFired so that we can drop |time| for Analyze
  // in testing.
  Analyze();
}

void TextPaintTimingDetector::Analyze() {
  TextRecord* largest_text_first_paint = FindLargestPaintCandidate();
  bool new_candidate_detected = false;
  DCHECK(!largest_text_first_paint ||
         !largest_text_first_paint->first_paint_time.is_null());
  if (largest_text_first_paint &&
      largest_text_first_paint->first_paint_time != largest_text_paint_) {
    OnLargestTextDetected(*largest_text_first_paint);
    new_candidate_detected = true;
  }
  TextRecord* last_text_first_paint = FindLastPaintCandidate();
  DCHECK(!last_text_first_paint ||
         !last_text_first_paint->first_paint_time.is_null());
  if (last_text_first_paint &&
      last_text_first_paint->first_paint_time != last_text_paint_) {
    OnLastTextDetected(*last_text_first_paint);
    new_candidate_detected = true;
  }
  if (new_candidate_detected) {
    frame_view_->GetPaintTimingDetector().DidChangePerformanceTiming();
  }
}

void TextPaintTimingDetector::OnPrePaintFinished() {
  if (texts_to_record_swap_time_.size() > 0) {
    // Start repeating timer only once after the first text prepaint.
    if (!timer_.IsActive()) {
      timer_.StartRepeating(kTimerDelay, FROM_HERE);
    }
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
  for (TextRecord& record : texts_to_record_swap_time_) {
    if (record.node_id == node_id)
      record.node_id = kInvalidDOMNodeId;
  }
  if (recorded_text_node_ids_.find(node_id) == recorded_text_node_ids_.end())
    return;
  // We assume that removed nodes' id would not be recycled, and it's expensive
  // to remove records from largest_text_heap_ and latest_text_heap_, so we
  // intentionally keep the records of removed nodes in largest_text_heap_ and
  // latest_text_heap_.
  recorded_text_node_ids_.erase(node_id);
  if (recorded_text_node_ids_.size() == 0) {
    const bool largest_text_paint_invalidated =
        largest_text_paint_ != base::TimeTicks();
    const bool last_text_paint_invalidated =
        last_text_paint_ != base::TimeTicks();
    if (largest_text_paint_invalidated)
      largest_text_paint_ = base::TimeTicks();
    if (last_text_paint_invalidated)
      last_text_paint_ = base::TimeTicks();
    if (largest_text_paint_invalidated || last_text_paint_invalidated) {
      frame_view_->GetPaintTimingDetector().DidChangePerformanceTiming();
    }
  }
}

void TextPaintTimingDetector::RegisterNotifySwapTime(
    ReportTimeCallback callback) {
  // ReportSwapTime on layerTreeView will queue a swap-promise, the callback is
  // called when the swap for current render frame completes or fails to happen.
  LocalFrame& frame = frame_view_->GetFrame();
  if (!frame.GetPage())
    return;
  if (WebLayerTreeView* layerTreeView =
          frame.GetPage()->GetChromeClient().GetWebLayerTreeView(&frame)) {
    layerTreeView->NotifySwapTime(ConvertToBaseCallback(std::move(callback)));
    awaiting_swap_promise_ = true;
  }
}

void TextPaintTimingDetector::ReportSwapTime(
    WebLayerTreeView::SwapResult result,
    base::TimeTicks timestamp) {
  // If texts_to_record_swap_time_.size == 0, it means the array has been
  // consumed in a callback earlier than this one. That violates the assumption
  // that only one or zero callback will be called after one OnPrePaintFinished.
  DCHECK_GT(texts_to_record_swap_time_.size(), 0UL);
  for (TextRecord& record : texts_to_record_swap_time_) {
    if (record.node_id == kInvalidDOMNodeId)
      continue;
    record.first_paint_time = timestamp;
    recorded_text_node_ids_.insert(record.node_id);
    largest_text_heap_.push(std::make_unique<TextRecord>(record));
    latest_text_heap_.push(std::make_unique<TextRecord>(record));
  }
  texts_to_record_swap_time_.clear();
  awaiting_swap_promise_ = false;
}

void TextPaintTimingDetector::RecordText(const LayoutObject& object,
                                         const PaintLayer& painting_layer) {
  if (!is_recording_)
    return;
  Node* node = object.GetNode();
  if (!node)
    return;
  DOMNodeId node_id = DOMNodeIds::IdForNode(node);

  // This metric defines the size of a text by its first size. So it
  // early-returns if the text has been recorded.
  if (size_zero_node_ids_.find(node_id) != size_zero_node_ids_.end())
    return;
  if (recorded_text_node_ids_.find(node_id) != recorded_text_node_ids_.end())
    return;
  // When node_id is not found in recorded_text_node_ids_, this invalidation is
  // the text's first invalidation.

  uint64_t rect_size = 0;
  LayoutRect invalidated_rect = object.FirstFragment().VisualRect();
  if (!invalidated_rect.IsEmpty()) {
    rect_size = frame_view_->GetPaintTimingDetector().CalculateVisualSize(
        invalidated_rect, painting_layer);
  }

  // When rect_size == 0, it either means the text size is 0 or the text is out
  // of viewport. In either case, we don't record their time for efficiency.
  if (rect_size == 0) {
    size_zero_node_ids_.insert(node_id);
  } else {
    // Non-trivial text is found.
    TextRecord record;
    record.node_id = node_id;
    record.first_size = rect_size;
#ifndef NDEBUG
    record.text = ToLayoutText(&object)->GetText();
#endif
    texts_to_record_swap_time_.push_back(record);
  }

  if (recorded_text_node_ids_.size() + size_zero_node_ids_.size() +
          texts_to_record_swap_time_.size() >=
      kTextNodeNumberLimit) {
    TRACE_EVENT_INSTANT2("loading", "TextPaintTimingDetector::OverNodeLimit",
                         TRACE_EVENT_SCOPE_THREAD, "recorded_node_count",
                         recorded_text_node_ids_.size(), "size_zero_node_count",
                         size_zero_node_ids_.size());
    StopRecordEntries();
  }
}

void TextPaintTimingDetector::StopRecordEntries() {
  timer_.Stop();
  is_recording_ = false;
}

TextRecord* TextPaintTimingDetector::FindLargestPaintCandidate() {
  while (!largest_text_heap_.empty() &&
         !recorded_text_node_ids_.Contains(largest_text_heap_.top()->node_id)) {
    // If recorded_text_node_ids_ doesn't have record.node_id, the node has
    // been deleted. We discard the records of deleted node.
    largest_text_heap_.pop();
  }
  if (!largest_text_heap_.empty())
    return largest_text_heap_.top().get();
  return nullptr;
}

TextRecord* TextPaintTimingDetector::FindLastPaintCandidate() {
  while (!latest_text_heap_.empty() &&
         !recorded_text_node_ids_.Contains(latest_text_heap_.top()->node_id)) {
    // If recorded_text_node_ids_ doesn't have record.node_id, the node has
    // been deleted. We discard the records of deleted node.
    latest_text_heap_.pop();
  }
  if (!latest_text_heap_.empty())
    return latest_text_heap_.top().get();
  return nullptr;
}

void TextPaintTimingDetector::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_view_);
}
}  // namespace blink
