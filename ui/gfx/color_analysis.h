// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_ANALYSIS_H_
#define UI_GFX_COLOR_ANALYSIS_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_export.h"

namespace color_utils {

// This class exposes the sampling method to the caller, which allows
// stubbing out for things like unit tests. Might be useful to pass more
// arguments into the GetSample method in the future (such as which
// cluster is being worked on, etc.).
class UI_EXPORT KMeanImageSampler {
 public:
  virtual int GetSample(int width, int height) = 0;

 protected:
  KMeanImageSampler();
  virtual ~KMeanImageSampler();
};

// This sampler will pick a random pixel as a sample centroid.
class RandomSampler : public KMeanImageSampler {
  public:
   RandomSampler();
   virtual ~RandomSampler();

   virtual int GetSample(int width, int height) OVERRIDE;
};

// This sampler will pick pixels from an evenly spaced grid.
class UI_EXPORT GridSampler : public KMeanImageSampler {
  public:
   GridSampler();
   virtual ~GridSampler();

   virtual int GetSample(int width, int height) OVERRIDE;

  private:
   // The number of times GetSample has been called.
   int calls_;
};

// Returns a recommended background color for a PNG image. This color might
// not even exist in the image, but it is typically representative of the
// image and tinted towards the white end of the spectrum.
// The function CalculateKMeanColorOfPNG is used to grab the KMean calculated
// color of the image, then the color is moved to HSV space so the saturation
// and luminance can be modified to move the color towards the white end of
// the spectrum. The color is then moved back to RGB space and returned.
SkColor CalculateRecommendedBgColorForPNG(scoped_refptr<RefCountedMemory> png);

SkColor CalculateRecommendedBgColorForPNG(scoped_refptr<RefCountedMemory> png,
                                          KMeanImageSampler& sampler);

// Returns an SkColor that represents the calculated dominant color in the png.
// This uses a KMean clustering algorithm to find clusters of pixel colors in
// RGB space.
// |png| represents the data of a png encoded image.
// |darkness_limit| represents the minimum sum of the RGB components that is
// acceptable as a color choice. This can be from 0 to 765.
// |brightness_limit| represents the maximum sum of the RGB components that is
// acceptable as a color choice. This can be from 0 to 765.
//
// RGB KMean Algorithm (N clusters, M iterations):
// TODO (dtrainor): Try moving most/some of this to HSV space?  Better for
// color comparisons/averages?
// 1.Pick N starting colors by randomly sampling the pixels. If you see a
//   color you already saw keep sampling. After a certain number of tries
//   just remove the cluster and continue with N = N-1 clusters (for an image
//   with just one color this should devolve to N=1). These colors are the
//   centers of your N clusters.
//   TODO (dtrainor): Check to ignore colors with an alpha of 0?
// 2.For each pixel in the image find the cluster that it is closest to in RGB
//   space. Add that pixel's color to that cluster (we keep a sum and a count
//   of all of the pixels added to the space, so just add it to the sum and
//   increment count).
// 3.Calculate the new cluster centroids by getting the average color of all of
//   the pixels in each cluster (dividing the sum by the count).
// 4.See if the new centroids are the same as the old centroids.
//     a) If this is the case for all N clusters than we have converged and
//        can move on.
//     b) If any centroid moved, repeat step 2 with the new centroids for up
//        to M iterations.
// 5.Once the clusters have converged or M iterations have been tried, sort
//   the clusters by weight (where weight is the number of pixels that make up
//   this cluster).
// 6.Going through the sorted list of clusters, pick the first cluster with the
//   largest weight that's centroid fulfills the equation
//   |darkness_limit| < SUM(R, G, B) < |brightness_limit|. Return that color.
//   If no color fulfills that requirement return the color with the largest
//   weight regardless of whether or not it fulfills the equation above.
SkColor CalculateKMeanColorOfPNG(scoped_refptr<RefCountedMemory> png,
                                 uint32_t darkness_limit,
                                 uint32_t brightness_limit);

UI_EXPORT SkColor CalculateKMeanColorOfPNG(scoped_refptr<RefCountedMemory> png,
                                           uint32_t darkness_limit,
                                           uint32_t brightness_limit,
                                           KMeanImageSampler& sampler);

}  // namespace color_utils

#endif  // UI_GFX_COLOR_ANALYSIS_H_
