// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_PAINT_TIMING_DETECTOR_H_

#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class LayoutObject;
class LocalFrameView;
class PropertyTreeState;
class TracedValue;
class Image;

class ImageRecord : public base::SupportsWeakPtr<ImageRecord> {
 public:
  DOMNodeId node_id = kInvalidDOMNodeId;
  uint64_t first_size = 0;
  unsigned frame_index = 0;
  // The time of the first paint after fully loaded.
  base::TimeTicks paint_time = base::TimeTicks();
  bool loaded = false;
#ifndef NDEBUG
  String image_url = "";
#endif
};

// ImagePaintTimingDetector contains Largest Image Paint.
//
// Largest Image Paint timing measures when the largest image element within
// viewport finishes painting. Specifically, it:
// 1. Tracks all images' first invalidation, recording their visual size, if
// this image is within viewport.
// 2. When an image finishes loading, record its paint time.
// 3. At the end of each frame, if new images are added and loaded, the
// algorithm will start an analysis.
//
// In the analysis:
// 3.1 Largest Image Paint finds the largest image by the first visual size. If
// it has finished loading, reports a candidate result as its first paint time
// since loaded.
//
// For all these candidate results, Telemetry picks the lastly reported
// Largest Image Paint candidate as its final result.
//
// See also:
// https://docs.google.com/document/d/1DRVd4a2VU8-yyWftgOparZF-sf16daf0vfbsHuz2rws/edit#heading=h.1k2rnrs6mdmt
class CORE_EXPORT ImagePaintTimingDetector final
    : public GarbageCollectedFinalized<ImagePaintTimingDetector> {
  friend class ImagePaintTimingDetectorTest;
  using NodesQueueComparator = bool (*)(const base::WeakPtr<ImageRecord>&,
                                        const base::WeakPtr<ImageRecord>&);
  using ImageRecordSet =
      std::set<base::WeakPtr<ImageRecord>, NodesQueueComparator>;

 public:
  ImagePaintTimingDetector(LocalFrameView*);
  void RecordImage(const LayoutObject&,
                   Image*,
                   const PropertyTreeState& current_paint_chunk_properties);
  static bool IsBackgroundImageContentful(const LayoutObject&, const Image&);
  static bool HasBackgroundImage(const LayoutObject& object);
  void OnPaintFinished();
  ImageRecord* FindLargestPaintCandidate();
  void NotifyNodeRemoved(DOMNodeId);
  base::TimeTicks LargestImagePaint() const {
    return !largest_image_paint_ ? base::TimeTicks()
                                 : largest_image_paint_->paint_time;
  }
  uint64_t LargestImagePaintSize() const {
    return !largest_image_paint_ ? 0 : largest_image_paint_->first_size;
  }
  // After the method being called, the detector stops to record new entries and
  // node removal. But it still observe the loading status. In other words, if
  // an image is recorded before stopping recording, and finish loading after
  // stopping recording, the detector can still observe the loading being
  // finished.
  void StopRecordEntries();
  bool IsRecording() const { return is_recording_; }
  void Trace(blink::Visitor*);

 private:
  void PopulateTraceValue(TracedValue&,
                          const ImageRecord& first_image_paint,
                          unsigned report_count) const;
  // This is provided for unit test to force invoking swap promise callback.
  void ReportSwapTime(unsigned last_queued_frame_index,
                      WebLayerTreeView::SwapResult,
                      base::TimeTicks);
  void RegisterNotifySwapTime();
  void OnLargestImagePaintDetected(ImageRecord*);
  void Deactivate();

  void Analyze();

  base::RepeatingCallback<void(WebLayerTreeView::ReportTimeCallback)>
      notify_swap_time_override_for_testing_;

  HashSet<DOMNodeId> invisible_node_ids_;
  // We will never destroy the pointers within |visible_node_map_|. Once created
  // they will exist for the whole life cycle of |visible_node_map_|.
  HashMap<DOMNodeId, std::unique_ptr<ImageRecord>> visible_node_map_;
  // This stores the image records, which are ordered by size.
  ImageRecordSet size_ordered_set_;
  // Use |DOMNodeId| instead of |ImageRecord|* for the efficiency of inserting
  // and erasing.
  HashSet<DOMNodeId> detached_ids_;

  // Node-ids of records pending swap time are stored in this queue until they
  // get a swap time.
  std::queue<base::WeakPtr<ImageRecord>> images_queued_for_paint_time_;

  // Used to report the last candidates of each metric.
  unsigned largest_image_candidate_index_max_ = 0;

  // Used to decide which frame a record belongs to.
  unsigned frame_index_ = 1;

  unsigned last_queued_frame_index_ = 0;
  // Used to control if we record new image entries and image removal, but has
  // no effect on recording the loading status.
  bool is_recording_ = true;

  ImageRecord* largest_image_paint_ = nullptr;
  Member<LocalFrameView> frame_view_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_PAINT_TIMING_DETECTOR_H_
