// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AD_METRICS_FRAME_DATA_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AD_METRICS_FRAME_DATA_H_

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/common/page_load_metrics/page_load_metrics.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/origin.h"

// Store information received for a frame on the page. FrameData is meant
// to represent a frame along with it's entire subtree.
class FrameData {
 public:
  // The origin of the ad relative to the main frame's origin.
  // Note: Logged to UMA, keep in sync with CrossOriginAdStatus in enums.xml.
  //   Add new entries to the end, and do not renumber.
  enum class OriginStatus {
    kUnknown = 0,
    kSame = 1,
    kCross = 2,
    kMaxValue = kCross,
  };

  // Whether or not the ad frame has a display: none styling.
  enum FrameVisibility {
    kNonVisible = 0,
    kVisible = 1,
    kAnyVisibility = 2,
    kMaxValue = kAnyVisibility,
  };

  // Whether or not the frame size intervention would have triggered on
  // this frame.  These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  enum class FrameSizeInterventionStatus {
    kNone = 0,
    kTriggered = 1,
    kMaxValue = kTriggered,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. For any additions, also update the
  // corresponding PageEndReason enum in enums.xml.
  enum class UserActivationStatus {
    kNoActivation = 0,
    kReceivedActivation = 1,
    kMaxValue = kReceivedActivation,
  };

  // High level categories of mime types for resources loaded by the frame.
  enum class ResourceMimeType {
    kJavascript = 0,
    kVideo = 1,
    kImage = 2,
    kCss = 3,
    kHtml = 4,
    kOther = 5,
    kMaxValue = kOther,
  };

  // Whether or not media has been played in this frame. These values are
  // persisted to logs. Entries should not be renumbered and numeric values
  // should never be reused.
  enum class MediaStatus {
    kNotPlayed = 0,
    kPlayed = 1,
    kMaxValue = kPlayed,
  };

  // Maximum number of bytes allowed to be loaded by a frame.
  static const int kFrameSizeInterventionByteThreshold = 1050 * 1024;

  using FrameTreeNodeId =
      page_load_metrics::PageLoadMetricsObserver::FrameTreeNodeId;

  // Get the mime type of a resource. This only returns a subset of mime types,
  // grouped at a higher level. For example, all video mime types return the
  // same value.
  static ResourceMimeType GetResourceMimeType(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);

  explicit FrameData(FrameTreeNodeId frame_tree_node_id);
  ~FrameData();

  // Update the metadata of this frame if it is being navigated.
  void UpdateForNavigation(content::RenderFrameHost* render_frame_host,
                           bool frame_navigated);

  // Updates the number of bytes loaded in the frame given a resource load.
  void ProcessResourceLoadInFrame(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);

  // Adds additional bytes to the ad resource byte counts. This
  // is used to notify the frame that some bytes were tagged as ad bytes after
  // they were loaded.
  void AdjustAdBytes(int64_t unaccounted_ad_bytes, ResourceMimeType mime_type);

  // Sets the size of the frame and updates its visibility state.
  void SetFrameSize(gfx::Size frame_size_);

  // Sets the display state of the frame and updates its visibility state.
  void SetDisplayState(bool is_display_none);

  // Records that the sticky user activation bit has been set on the frame. This
  // cannot be unset.
  void set_received_user_activation() {
    user_activation_status_ = UserActivationStatus::kReceivedActivation;
  }

  FrameTreeNodeId frame_tree_node_id() const { return frame_tree_node_id_; }

  OriginStatus origin_status() const { return origin_status_; }

  size_t bytes() const { return bytes_; }

  size_t network_bytes() const { return network_bytes_; }

  size_t same_origin_bytes() const { return same_origin_bytes_; }

  size_t ad_bytes() const { return ad_bytes_; }

  size_t ad_network_bytes() const { return ad_network_bytes_; }

  size_t ad_video_network_bytes() const { return ad_video_network_bytes_; }

  size_t ad_javascript_network_bytes() const {
    return ad_javascript_network_bytes_;
  }

  UserActivationStatus user_activation_status() const {
    return user_activation_status_;
  }

  bool frame_navigated() const { return frame_navigated_; }

  FrameVisibility visibility() const { return visibility_; }

  gfx::Size frame_size() const { return frame_size_; }

  FrameSizeInterventionStatus size_intervention_status() const {
    return size_intervention_status_;
  }

  MediaStatus media_status() const { return media_status_; }

  void set_media_status(MediaStatus media_status) {
    media_status_ = media_status;
  }

 private:
  // Updates whether or not this frame meets the criteria for visibility.
  void UpdateFrameVisibility();

  // Total bytes used to load resources in the frame, including headers.
  size_t bytes_;
  size_t network_bytes_;

  // Tallies for bytes and counts observed in resource data updates for the
  // frame.
  size_t ad_javascript_network_bytes_ = 0u;
  size_t ad_video_network_bytes_ = 0u;

  // Tracks the number of bytes that were used to load resources which were
  // detected to be ads inside of this frame. For ad frames, these counts should
  // match |frame_bytes| and |frame_network_bytes|.
  size_t ad_bytes_ = 0u;
  size_t ad_network_bytes_ = 0u;

  // The number of bytes that are same origin to the root ad frame.
  size_t same_origin_bytes_;
  const FrameTreeNodeId frame_tree_node_id_;
  OriginStatus origin_status_;
  bool frame_navigated_;
  UserActivationStatus user_activation_status_;
  bool is_display_none_;
  FrameVisibility visibility_;
  gfx::Size frame_size_;
  url::Origin origin_;
  MediaStatus media_status_ = MediaStatus::kNotPlayed;

  // Indicates whether or not this frame would have triggered a size
  // intervention.
  FrameSizeInterventionStatus size_intervention_status_;

  DISALLOW_COPY_AND_ASSIGN(FrameData);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AD_METRICS_FRAME_DATA_H_
