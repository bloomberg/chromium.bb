// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_

#include "third_party/blink/public/platform/web_layer_tree_view.h"
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
  DOMNodeId node_id = kInvalidDOMNodeId;
  uint64_t first_size = 0;
  // This is treated as unset.
  base::TimeTicks first_paint_time = base::TimeTicks();
#ifndef NDEBUG
  String text = "";
#endif
};

// TextPaintTimingDetector contains Largest Text Paint.
//
// Largest Text Paint timing measures when the largest text element gets painted
// within viewport. Specifically, it:
// 1. Tracks all texts' first invalidation, recording their visual size, paint
// time.
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
      WTF::CrossThreadFunction<void(WebLayerTreeView::SwapResult,
                                    base::TimeTicks)>;
  using TextRecordSetComparator = bool (*)(const base::WeakPtr<TextRecord>&,
                                           const base::WeakPtr<TextRecord>&);
  using TextRecordSet =
      std::set<base::WeakPtr<TextRecord>, TextRecordSetComparator>;
  friend class TextPaintTimingDetectorTest;

 public:
  TextPaintTimingDetector(LocalFrameView* frame_view);
  void RecordText(const LayoutObject& object, const PropertyTreeState&);
  TextRecord* FindLargestPaintCandidate();
  void OnPaintFinished();
  void NotifyNodeRemoved(DOMNodeId);
  void Dispose() { timer_.Stop(); }
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

  void ReportSwapTime(WebLayerTreeView::SwapResult result,
                      base::TimeTicks timestamp);
  void RegisterNotifySwapTime(ReportTimeCallback callback);
  void OnLargestTextDetected(const TextRecord&);
  TextRecord* FindCandidate(const TextRecordSet& ordered_set);

  HashMap<DOMNodeId, std::unique_ptr<TextRecord>> id_record_map_;
  HashSet<DOMNodeId> size_zero_node_ids_;
  HashSet<DOMNodeId> detached_ids_;
  TextRecordSet size_ordered_set_;
  std::queue<DOMNodeId> texts_to_record_swap_time_;

  // Make sure that at most one swap promise is ongoing.
  bool awaiting_swap_promise_ = false;
  unsigned largest_text_candidate_index_max_ = 0;
  bool is_recording_ = true;

  base::TimeTicks largest_text_paint_;
  uint64_t largest_text_paint_size_ = 0;
  TaskRunnerTimer<TextPaintTimingDetector> timer_;
  Member<LocalFrameView> frame_view_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_
