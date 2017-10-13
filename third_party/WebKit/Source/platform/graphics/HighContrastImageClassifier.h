// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HighContrastImageClassifier_h
#define HighContrastImageClassifier_h

#include <vector>

#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/Image.h"

namespace blink {

class PLATFORM_EXPORT HighContrastImageClassifier {
 public:
  HighContrastImageClassifier() = default;
  ~HighContrastImageClassifier() = default;

  // Decides if a high contrast filter should be applied to the image or not.
  bool ShouldApplyHighContrastFilterToImage(Image&);

  bool ComputeImageFeaturesForTesting(Image& image,
                                      std::vector<float>* features) {
    return ComputeImageFeatures(image, features);
  }

 private:
  // Computes the features vector for a given image.
  bool ComputeImageFeatures(Image&, std::vector<float>*);

  // Converts image to SkBitmap and returns true if successful.
  bool GetBitmap(Image&, SkBitmap*);

  // Extracts sample pixels and transparency ratio from the given bitmap.
  void GetSamples(const SkBitmap&, std::vector<SkColor>*, float*);

  // Computes the features, given sampled pixels and transparency ratio.
  void GetFeatures(const std::vector<SkColor>&,
                   const float,
                   std::vector<float>*);

  // Makes a decision about image given its features.
  HighContrastClassification ClassifyImage(const std::vector<float>&);

  // Receives sampled pixels, and returns the number of color buckets.
  int CountColorBuckets(const std::vector<SkColor>&);
};

}  // namespace blink

#endif  // HighContrastImageClassifier_h
