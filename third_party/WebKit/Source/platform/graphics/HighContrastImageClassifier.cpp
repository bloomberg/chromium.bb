// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/HighContrastImageClassifier.h"

#include "third_party/skia/include/utils/SkNullCanvas.h"

namespace {

bool IsColorGray(const SkColor& color) {
  return abs(static_cast<int>(SkColorGetR(color)) -
             static_cast<int>(SkColorGetG(color))) +
             abs(static_cast<int>(SkColorGetG(color)) -
                 static_cast<int>(SkColorGetB(color))) <=
         8;
}

enum class ColorMode { kColor = 0, kGrayscale = 1 };

const int kOneDimensionalPixelsToSample = 34;

}  // namespace

namespace blink {

bool HighContrastImageClassifier::ShouldApplyHighContrastFilterToImage(
    Image& image) {
  HighContrastClassification result = image.GetHighContrastClassification();
  if (result != HighContrastClassification::kNotClassified)
    return result == HighContrastClassification::kApplyHighContrastFilter;

  std::vector<float> features;
  if (!ComputeImageFeatures(image, &features))
    result = HighContrastClassification::kDoNotApplyHighContrastFilter;
  else
    result = ClassifyImage(features);

  image.SetHighContrastClassification(result);
  return result == HighContrastClassification::kApplyHighContrastFilter;
}

// This function computes a single feature vector based on a sample set of image
// pixels. Please refer to |GetSamples| function for description of the sampling
// method, and |GetFeatures| function for description of the features.
bool HighContrastImageClassifier::ComputeImageFeatures(
    Image& image,
    std::vector<float>* features) {
  SkBitmap bitmap;
  if (!GetBitmap(image, &bitmap))
    return false;

  std::vector<SkColor> sampled_pixels;
  float transparency_ratio;
  GetSamples(bitmap, &sampled_pixels, &transparency_ratio);

  GetFeatures(sampled_pixels, transparency_ratio, features);
  return true;
}

bool HighContrastImageClassifier::GetBitmap(Image& image, SkBitmap* bitmap) {
  if (!image.IsBitmapImage() || !image.width() || !image.height())
    return false;

  bitmap->allocPixels(
      SkImageInfo::MakeN32(image.width(), image.height(), kPremul_SkAlphaType));
  SkCanvas canvas(*bitmap);
  canvas.clear(SK_ColorTRANSPARENT);
  canvas.drawImageRect(image.PaintImageForCurrentFrame().GetSkImage(),
                       SkRect::MakeIWH(image.width(), image.height()), nullptr);
  return true;
}

// This function selects a set of sample pixels from an image, and returns them
// along with ratio of transparent pixels to all pixels.
// Sampling is done uniformly along the width and height of the image.
void HighContrastImageClassifier::GetSamples(
    const SkBitmap& bitmap,
    std::vector<SkColor>* sampled_pixels,
    float* transparency_ratio) {
  int cx = static_cast<int>(
      ceil(bitmap.width() / static_cast<float>(kOneDimensionalPixelsToSample)));
  int cy = static_cast<int>(ceil(
      bitmap.height() / static_cast<float>(kOneDimensionalPixelsToSample)));

  sampled_pixels->clear();
  int transparent_pixels = 0;
  for (int y = 0; y < bitmap.height(); y += cy) {
    for (int x = 0; x < bitmap.width(); x += cx) {
      SkColor new_sample = bitmap.getColor(x, y);
      if (SkColorGetA(new_sample) < 128)
        transparent_pixels++;
      else
        sampled_pixels->push_back(new_sample);
    }
  }

  *transparency_ratio = (transparent_pixels * 1.0) /
                        (transparent_pixels + sampled_pixels->size());
}

// This function computes a single feature vector from a sample set of image
// pixels. The current features are:
// 0: 1 if color, 0 if grayscale.
// 1: Ratio of the number of bucketed colors used in the image to all
//    possiblities. Color buckets are represented with 4 bits per color channel.
void HighContrastImageClassifier::GetFeatures(
    const std::vector<SkColor>& sampled_pixels,
    const float transparency_ratio,
    std::vector<float>* features) {
  int samples_count = static_cast<int>(sampled_pixels.size());

  // Is image grayscale.
  int color_pixels = 0;
  for (const SkColor& sample : sampled_pixels) {
    if (!IsColorGray(sample))
      color_pixels++;
  }
  ColorMode color_mode = (color_pixels > samples_count / 100)
                             ? ColorMode::kColor
                             : ColorMode::kGrayscale;

  features->resize(2);

  // Feature 0: Is Colorful?
  (*features)[0] = color_mode == ColorMode::kColor;

  // Feature 1: Color Buckets Ratio
  // Using 4 bit per channel representation of each color bucket, there would be
  // 2^4 buckets for grayscale images and 2^12 for color images.
  const float max_buckets[] = {16, 4096};
  (*features)[1] = CountColorBuckets(sampled_pixels) /
                   max_buckets[color_mode == ColorMode::kColor];
}

int HighContrastImageClassifier::CountColorBuckets(
    const std::vector<SkColor>& sampled_pixels) {
  std::set<unsigned> buckets;
  for (const SkColor& sample : sampled_pixels) {
    unsigned bucket = ((SkColorGetR(sample) >> 4) << 8) +
                      ((SkColorGetG(sample) >> 4) << 4) +
                      ((SkColorGetB(sample) >> 4));
    buckets.insert(bucket);
  }

  return static_cast<int>(buckets.size());
}

HighContrastClassification HighContrastImageClassifier::ClassifyImage(
    const std::vector<float>& features) {
  bool result = false;

  // Shallow decision tree trained by C4.5.
  if (features.size() == 2) {
    if (features[1] < 0.016)
      result = true;
    else
      result = (features[0] == 0);
  }

  return result ? HighContrastClassification::kApplyHighContrastFilter
                : HighContrastClassification::kDoNotApplyHighContrastFilter;
}

}  // namespace blink
