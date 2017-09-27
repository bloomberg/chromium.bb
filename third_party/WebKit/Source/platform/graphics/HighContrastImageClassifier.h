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

 private:
  // Converts image to SkBitmap and returns true if successful.
  bool GetBitmap(Image&, SkBitmap*);

  // Extracts sample pixels and transparency ratio from the given bitmap.
  void GetSamples(const SkBitmap&, std::vector<SkColor>*, float*);

  // Computes the features given sample pixels and transparency ratio.
  void GetFeatures(const std::vector<SkColor>&,
                   const float,
                   std::vector<float>*);

  // Makes a decision about image given its features.
  HighContrastClassification ClassifyImage(const std::vector<float>&);
};

}  // namespace blink

#endif  // HighContrastImageClassifier_h
