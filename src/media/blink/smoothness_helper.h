// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_SMOOTHNESS_HELPER_H_
#define MEDIA_BLINK_SMOOTHNESS_HELPER_H_

#include <memory>

#include "media/base/buffering_state.h"
#include "media/base/pipeline_status.h"
#include "media/blink/media_blink_export.h"
#include "media/learning/common/labelled_example.h"

namespace media {

namespace learning {
class LearningTaskController;
}

// Helper class to construct learning observations about the smoothness of a
// video playback.  Currently measures the worst-case frame drop ratio observed
// among fixed-length segments.
class MEDIA_BLINK_EXPORT SmoothnessHelper {
 public:
  // Callback that provides the number of dropped / decoded frames since some
  // point in the past.  We assume that these values are comparable during
  // playback, so that we can compute deltas.
  class Client {
   public:
    virtual ~Client() {}

    virtual unsigned DecodedFrameCount() const = 0;
    virtual unsigned DroppedFrameCount() const = 0;
  };

  virtual ~SmoothnessHelper();

  // Return the features that we were constructed with.
  const learning::FeatureVector& features() const { return features_; }

  // Notify us that an NNR has occurred.
  virtual void NotifyNNR() = 0;

  // |features| are the features that we'll use for any labelled examples that
  // we create.  They should be features that could be captured at the time a
  // prediction would be needed.
  static std::unique_ptr<SmoothnessHelper> Create(
      std::unique_ptr<learning::LearningTaskController> bad_controller,
      std::unique_ptr<learning::LearningTaskController> nnr_controller,
      const learning::FeatureVector& features,
      Client* player);

  // We split playbacks up into |kSegmentSize| units, and record the worst
  // dropped frame ratio over all segments of a playback.  A playback is not
  // recorded if it doesn't contain at least one full segment.
  static base::TimeDelta SegmentSizeForTesting();

 protected:
  SmoothnessHelper(const learning::FeatureVector& features);

  learning::FeatureVector features_;
};

}  // namespace media

#endif  // MEDIA_BLINK_SMOOTHNESS_HELPER_H_
