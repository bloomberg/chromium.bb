// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_

#include <memory>
#include <queue>
#include <set>

#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {
class LayoutObject;
class TracedValue;
class LocalFrameView;
class PropertyTreeState;

class TextRecord : public base::SupportsWeakPtr<TextRecord> {
 public:
  TextRecord(DOMNodeId new_node_id, uint64_t new_first_size)
      : node_id(new_node_id), first_size(new_first_size) {}

  DOMNodeId node_id = kInvalidDOMNodeId;
  uint64_t first_size = 0;
  // The time of the first paint after fully loaded.
  base::TimeTicks paint_time = base::TimeTicks();
#ifndef NDEBUG
  String text = "";
#endif
  DISALLOW_COPY_AND_ASSIGN(TextRecord);
};

class TextRecordsManager {
  using TextRecordSetComparator = bool (*)(const base::WeakPtr<TextRecord>&,
                                           const base::WeakPtr<TextRecord>&);
  using TextRecordSet =
      std::set<base::WeakPtr<TextRecord>, TextRecordSetComparator>;
  friend class TextPaintTimingDetectorTest;

 public:
  TextRecordsManager();
  TextRecord* FindLargestPaintCandidate();

  bool AreAllVisibleNodesDetached() const;
  void SetNodeDetachedIfNeeded(const DOMNodeId&);
  void MarkNodeReattachedIfNeeded(const DOMNodeId&);

  void RecordInvisibleNode(const DOMNodeId&);
  void RecordVisibleNode(const DOMNodeId&,
                         const uint64_t& visual_size,
                         const LayoutObject&);
  bool NeedMeausuringPaintTime() const {
    return !texts_queued_for_paint_time_.empty();
  }
  void AssignPaintTimeToQueuedNodes(const base::TimeTicks&);

  bool HasTooManyNodes() const;
  bool HasRecorded(const DOMNodeId&) const;

  size_t CountVisibleNodes() const { return visible_node_map_.size(); }
  size_t CountInvisibleNodes() const { return invisible_node_ids_.size(); }

  bool IsKnownVisibleNode(const DOMNodeId& node_id) const {
    return visible_node_map_.Contains(node_id);
  }

 private:
  void QueueToMeasurePaintTime(base::WeakPtr<TextRecord>);
  // This is used to cache the largest text paint result for better efficiency.
  // The result will be invalidated whenever any change is done to the variables
  // used in |FindLargestPaintCandidate|.
  bool is_result_invalidated_ = false;
  HashMap<DOMNodeId, std::unique_ptr<TextRecord>> visible_node_map_;
  HashSet<DOMNodeId> invisible_node_ids_;
  HashSet<DOMNodeId> detached_ids_;
  // This is used to order the nodes in |visible_node_map_| so that we can find
  // the largest node efficiently. Note that the entries in |size_ordered_set_|
  // and |visible_node_map_| should always be added/deleted together.
  TextRecordSet size_ordered_set_;
  std::queue<base::WeakPtr<TextRecord>> texts_queued_for_paint_time_;
  TextRecord* cached_largest_paint_candidate_;

  DISALLOW_COPY_AND_ASSIGN(TextRecordsManager);
};

// TextPaintTimingDetector contains Largest Text Paint.
//
// Largest Text Paint timing measures when the largest text element gets painted
// within viewport. Specifically, it:
// 1. Tracks all texts' first paints, recording their visual size, paint time.
// 2. Every 1 second after the first text pre-paint, the algorithm starts an
// analysis. In the analysis:
// 2.1 Largest Text Paint finds the text with the  largest first visual size,
// reports its first paint time as a candidate result.
//
// For all these candidate results, Telemetry picks the lastly reported
// Largest Text Paint candidate as the final result.
//
// See also:
// https://docs.google.com/document/d/1DRVd4a2VU8-yyWftgOparZF-sf16daf0vfbsHuz2rws/edit#heading=h.lvno2v283uls
class CORE_EXPORT TextPaintTimingDetector final
    : public GarbageCollectedFinalized<TextPaintTimingDetector> {
  using ReportTimeCallback =
      WTF::CrossThreadFunction<void(WebWidgetClient::SwapResult,
                                    base::TimeTicks)>;
  friend class TextPaintTimingDetectorTest;

 public:
  TextPaintTimingDetector(LocalFrameView* frame_view);
  void RecordText(const LayoutObject& object, const PropertyTreeState&);
  void OnPaintFinished();
  void NotifyNodeRemoved(DOMNodeId);
  void Dispose() { timer_.Stop(); }
  TextRecord* FindLargestPaintCandidate();
  base::TimeTicks LargestTextPaint() const { return largest_text_paint_; }
  uint64_t LargestTextPaintSize() const { return largest_text_paint_size_; }
  void StopRecordEntries();
  bool IsRecording() const { return is_recording_; }
  void Trace(blink::Visitor*);

 private:
  void PopulateTraceValue(TracedValue& value,
                          const TextRecord& first_text_paint,
                          unsigned candidate_index) const;
  void TimerFired(TimerBase*);
  void Analyze();

  void ReportSwapTime(WebWidgetClient::SwapResult result,
                      base::TimeTicks timestamp);
  void RegisterNotifySwapTime(ReportTimeCallback callback);
  void OnLargestTextDetected(const TextRecord&);

  TextRecordsManager records_manager_;

  // Make sure that at most one swap promise is ongoing.
  bool awaiting_swap_promise_ = false;
  unsigned count_candidates_ = 0;
  bool is_recording_ = true;

  bool has_records_changed_ = true;

  base::TimeTicks largest_text_paint_;
  uint64_t largest_text_paint_size_ = 0;
  TaskRunnerTimer<TextPaintTimingDetector> timer_;
  Member<LocalFrameView> frame_view_;

  DISALLOW_COPY_AND_ASSIGN(TextPaintTimingDetector);
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_
