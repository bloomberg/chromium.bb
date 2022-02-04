// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DUMMY_SEGMENTATION_PLATFORM_SERVICE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DUMMY_SEGMENTATION_PLATFORM_SERVICE_H_

#include <string>

#include "components/segmentation_platform/public/segmentation_platform_service.h"

namespace segmentation_platform {

// A dummy implementation of the SegmentationPlatformService that can be
// returned whenever the feature is not enabled.
class DummySegmentationPlatformService : public SegmentationPlatformService {
 public:
  DummySegmentationPlatformService();
  ~DummySegmentationPlatformService() override;

  // Disallow copy/assign.
  DummySegmentationPlatformService(const DummySegmentationPlatformService&) =
      delete;
  DummySegmentationPlatformService& operator=(
      const DummySegmentationPlatformService&) = delete;

  // SegmentationPlatformService overrides.
  void GetSelectedSegment(const std::string& segmentation_key,
                          SegmentSelectionCallback callback) override;
  SegmentSelectionResult GetCachedSegmentResult(
      const std::string& segmentation_key) override;
  void EnableMetrics(bool signal_collection_allowed) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DUMMY_SEGMENTATION_PLATFORM_SERVICE_H_
