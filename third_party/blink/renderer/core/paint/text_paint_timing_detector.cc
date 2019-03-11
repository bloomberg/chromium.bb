// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"
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

static bool LargeTextFirst(const base::WeakPtr<TextRecord>& a,
                           const base::WeakPtr<TextRecord>& b) {
  return a->first_size > b->first_size;
}

TextPaintTimingDetector::TextPaintTimingDetector(LocalFrameView* frame_view)
    : size_ordered_set_(&LargeTextFirst),
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
  value.SetBoolean("isMainFrame", frame_view_->GetFrame().IsMainFrame());
  value.SetBoolean("isOOPIF",
                   !frame_view_->GetFrame().LocalFrameRoot().IsMainFrame());
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

void TextPaintTimingDetector::TimerFired(TimerBase* time) {
  // Wrap Analyze method in TimerFired so that we can drop |time| for Analyze
  // in testing.
  Analyze();
}

void TextPaintTimingDetector::Analyze() {
  TextRecord* largest_text_first_paint = FindLargestPaintCandidate();
  DCHECK(!largest_text_first_paint ||
         !largest_text_first_paint->first_paint_time.is_null());
  if (largest_text_first_paint &&
      largest_text_first_paint->first_paint_time != largest_text_paint_) {
    OnLargestTextDetected(*largest_text_first_paint);
    frame_view_->GetPaintTimingDetector().DidChangePerformanceTiming();
  }
}

void TextPaintTimingDetector::OnPaintFinished() {
  if (!texts_to_record_swap_time_.empty()) {
    // Start repeating timer only once after the first text paint.
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
  if (id_record_map_.find(node_id) == id_record_map_.end())
    return;
  detached_ids_.insert(node_id);
  if (id_record_map_.size() == detached_ids_.size() &&
      largest_text_paint_ != base::TimeTicks()) {
    largest_text_paint_ = base::TimeTicks();
    frame_view_->GetPaintTimingDetector().DidChangePerformanceTiming();
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
  // that only one or zero callback will be called after one OnPaintFinished.
  DCHECK_GT(texts_to_record_swap_time_.size(), 0UL);
  while (!texts_to_record_swap_time_.empty()) {
    DOMNodeId node_id = texts_to_record_swap_time_.front();
    DCHECK(id_record_map_.Contains(node_id));
    TextRecord* record = id_record_map_.at(node_id);
    record->first_paint_time = timestamp;
    size_ordered_set_.insert(record->AsWeakPtr());

    texts_to_record_swap_time_.pop();
  }
  awaiting_swap_promise_ = false;
}

void TextPaintTimingDetector::RecordText(
    const LayoutObject& object,
    const PropertyTreeState& current_paint_chunk_properties) {
  if (!is_recording_)
    return;
  // TODO(crbug.com/933479): Use LayoutObject::GeneratingNode() to include
  // anonymous objects' rect.
  Node* node = object.GetNode();
  if (!node)
    return;
  DOMNodeId node_id = DOMNodeIds::IdForNode(node);
  DCHECK_NE(node_id, kInvalidDOMNodeId);

  // This metric defines the size of a text by its first size. So it
  // early-returns if the text has been recorded.
  if (size_zero_node_ids_.Contains(node_id))
    return;
  // The node is reattached.
  if (id_record_map_.Contains(node_id) && detached_ids_.Contains(node_id))
    detached_ids_.erase(node_id);
  if (id_record_map_.Contains(node_id))
    return;
  // When node_id is not found in id_record_map_, this invalidation is
  // the text's first invalidation.

  uint64_t rect_size = 0;
  // Compared to object.FirstFragment().VisualRect(), this will include other
  // fragments of the object.
  LayoutRect visual_rect = object.FragmentsVisualRectBoundingBox();
  if (!visual_rect.IsEmpty()) {
    rect_size = frame_view_->GetPaintTimingDetector().CalculateVisualSize(
        visual_rect, current_paint_chunk_properties);
  }
  DVLOG(2) << "Node id (" << node_id << "): size=" << rect_size
           << ", type=" << object.DebugName();

  // When rect_size == 0, it either means the text size is 0 or the text is out
  // of viewport. In either case, we don't record their time for efficiency.
  if (rect_size == 0) {
    size_zero_node_ids_.insert(node_id);
  } else {
    // Non-trivial text is found.
    std::unique_ptr<TextRecord> record = std::make_unique<TextRecord>();
    record->node_id = node_id;
    record->first_size = rect_size;
#ifndef NDEBUG
    if (object.IsText()) {
      record->text = ToLayoutText(&object)->GetText();
    } else if (object.IsFileUploadControl()) {
      record->text = ToLayoutFileUploadControl(&object)->FileTextValue();
    } else {
      record->text = String("NON-TEXT_OBJECT");
    }
#endif
    id_record_map_.insert(node_id, std::move(record));
    texts_to_record_swap_time_.push(node_id);
  }

  if (id_record_map_.size() + size_zero_node_ids_.size() >=
      kTextNodeNumberLimit) {
    TRACE_EVENT_INSTANT2("loading", "TextPaintTimingDetector::OverNodeLimit",
                         TRACE_EVENT_SCOPE_THREAD, "recorded_node_count",
                         id_record_map_.size(), "size_zero_node_count",
                         size_zero_node_ids_.size());
    StopRecordEntries();
  }
}

void TextPaintTimingDetector::StopRecordEntries() {
  timer_.Stop();
  is_recording_ = false;
}

TextRecord* TextPaintTimingDetector::FindLargestPaintCandidate() {
  return FindCandidate(size_ordered_set_);
}

TextRecord* TextPaintTimingDetector::FindCandidate(
    const TextRecordSet& ordered_set) {
  for (auto it = ordered_set.begin(); it != ordered_set.end(); ++it) {
    if (detached_ids_.Contains((*it)->node_id) ||
        (*it)->first_paint_time.is_null())
      continue;
    DCHECK(id_record_map_.Contains((*it)->node_id));
    return (*it).get();
  }
  return nullptr;
}

void TextPaintTimingDetector::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_view_);
}
}  // namespace blink
