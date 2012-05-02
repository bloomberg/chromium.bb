// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_analysis.h"

#include <algorithm>
#include <vector>

#include "ui/gfx/codec/png_codec.h"

namespace {

// RGBA KMean Constants
const uint32_t kNumberOfClusters = 4;
const int kNumberOfIterations = 50;
const uint32_t kMaxBrightness = 600;
const uint32_t kMinDarkness = 100;

// Background Color Modification Constants
const SkColor kDefaultBgColor = SK_ColorWHITE;

// Support class to hold information about each cluster of pixel data in
// the KMean algorithm. While this class does not contain all of the points
// that exist in the cluster, it keeps track of the aggregate sum so it can
// compute the new center appropriately.
class KMeanCluster {
 public:
  KMeanCluster() {
    Reset();
  }

  void Reset() {
    centroid[0] = centroid[1] = centroid[2] = 0;
    aggregate[0] = aggregate[1] = aggregate[2] = 0;
    counter = 0;
    weight = 0;
  }

  inline void SetCentroid(uint8_t r, uint8_t g, uint8_t b) {
    centroid[0] = r;
    centroid[1] = g;
    centroid[2] = b;
  }

  inline void GetCentroid(uint8_t* r, uint8_t* g, uint8_t* b) {
    *r = centroid[0];
    *g = centroid[1];
    *b = centroid[2];
  }

  inline bool IsAtCentroid(uint8_t r, uint8_t g, uint8_t b) {
    return r == centroid[0] && g == centroid[1] && b == centroid[2];
  }

  // Recomputes the centroid of the cluster based on the aggregate data. The
  // number of points used to calculate this center is stored for weighting
  // purposes. The aggregate and counter are then cleared to be ready for the
  // next iteration.
  inline void RecomputeCentroid() {
    if (counter > 0) {
      centroid[0] = aggregate[0] / counter;
      centroid[1] = aggregate[1] / counter;
      centroid[2] = aggregate[2] / counter;

      aggregate[0] = aggregate[1] = aggregate[2] = 0;
      weight = counter;
      counter = 0;
    }
  }

  inline void AddPoint(uint8_t r, uint8_t g, uint8_t b) {
    aggregate[0] += r;
    aggregate[1] += g;
    aggregate[2] += b;
    ++counter;
  }

  // Just returns the distance^2. Since we are comparing relative distances
  // there is no need to perform the expensive sqrt() operation.
  inline uint32_t GetDistanceSqr(uint8_t r, uint8_t g, uint8_t b) {
    return (r - centroid[0]) * (r - centroid[0]) +
           (g - centroid[1]) * (g - centroid[1]) +
           (b - centroid[2]) * (b - centroid[2]);
  }

  // In order to determine if we have hit convergence or not we need to see
  // if the centroid of the cluster has moved. This determines whether or
  // not the centroid is the same as the aggregate sum of points that will be
  // used to generate the next centroid.
  inline bool CompareCentroidWithAggregate() {
    if (counter == 0)
      return false;

    return aggregate[0] / counter == centroid[0] &&
           aggregate[1] / counter == centroid[1] &&
           aggregate[2] / counter == centroid[2];
  }

  // Returns the previous counter, which is used to determine the weight
  // of the cluster for sorting.
  inline uint32_t GetWeight() const {
    return weight;
  }

  static bool SortKMeanClusterByWeight(const KMeanCluster& a,
                                       const KMeanCluster& b) {
    return a.GetWeight() > b.GetWeight();
  }

 private:
  uint8_t centroid[3];

  // Holds the sum of all the points that make up this cluster. Used to
  // generate the next centroid as well as to check for convergence.
  uint32_t aggregate[3];
  uint32_t counter;

  // The weight of the cluster, determined by how many points were used
  // to generate the previous centroid.
  uint32_t weight;
};

} // namespace

namespace color_utils {

KMeanImageSampler::KMeanImageSampler() {
}

KMeanImageSampler::~KMeanImageSampler() {
}

RandomSampler::RandomSampler() {
}

RandomSampler::~RandomSampler() {
}

int RandomSampler::GetSample(int width, int height) {
  return rand();
}

GridSampler::GridSampler() : calls_(0) {
}

GridSampler::~GridSampler() {
}

int GridSampler::GetSample(int width, int height) {
  calls_++;
  // We may keep getting called after we've gone of the edge of the grid; in
  // this case we offset future return values by the number of times we've gone
  // off the grid.
  return (width * height * calls_ / kNumberOfClusters) % (width * height) +
      calls_ / kNumberOfClusters;
}

SkColor CalculateRecommendedBgColorForPNG(
    scoped_refptr<base::RefCountedMemory> png) {
  RandomSampler sampler;
  return CalculateRecommendedBgColorForPNG(png, sampler);
}

SkColor CalculateKMeanColorOfPNG(scoped_refptr<base::RefCountedMemory> png,
                                 uint32_t darkness_limit,
                                 uint32_t brightness_limit) {
  RandomSampler sampler;
  return CalculateKMeanColorOfPNG(png, darkness_limit, brightness_limit,
                                  sampler);
}

SkColor CalculateRecommendedBgColorForPNG(
    scoped_refptr<base::RefCountedMemory> png,
    KMeanImageSampler& sampler) {
  return CalculateKMeanColorOfPNG(png,
                                  kMinDarkness,
                                  kMaxBrightness,
                                  sampler);
}

SkColor CalculateKMeanColorOfPNG(scoped_refptr<base::RefCountedMemory> png,
                                 uint32_t darkness_limit,
                                 uint32_t brightness_limit,
                                 KMeanImageSampler& sampler) {
  int img_width, img_height;
  std::vector<uint8_t> decoded_data;
  SkColor color = kDefaultBgColor;

  if (png.get() &&
      png->size() &&
      gfx::PNGCodec::Decode(png->front(),
                            png->size(),
                            gfx::PNGCodec::FORMAT_BGRA,
                            &decoded_data,
                            &img_width,
                            &img_height)) {
    std::vector<KMeanCluster> clusters;
    clusters.resize(kNumberOfClusters, KMeanCluster());

    // Pick a starting point for each cluster
    std::vector<KMeanCluster>::iterator cluster = clusters.begin();
    while (cluster != clusters.end()) {
      // Try up to 10 times to find a unique color. If no unique color can be
      // found, destroy this cluster.
      bool color_unique = false;
      for (int i = 0; i < 10; ++i) {
        int pixel_pos = sampler.GetSample(img_width, img_height) %
            (img_width * img_height);

        uint8_t b = decoded_data[pixel_pos * 4];
        uint8_t g = decoded_data[pixel_pos * 4 + 1];
        uint8_t r = decoded_data[pixel_pos * 4 + 2];

        // Loop through the previous clusters and check to see if we have seen
        // this color before.
        color_unique = true;
        for (std::vector<KMeanCluster>::iterator
            cluster_check = clusters.begin();
            cluster_check != cluster; ++cluster_check) {
          if (cluster_check->IsAtCentroid(r, g, b)) {
            color_unique = false;
            break;
          }
        }

        // If we have a unique color set the center of the cluster to
        // that color.
        if (color_unique) {
          cluster->SetCentroid(r, g, b);
          break;
        }
      }

      // If we don't have a unique color erase this cluster.
      if (!color_unique) {
        cluster = clusters.erase(cluster);
      } else {
        // Have to increment the iterator here, otherwise the increment in the
        // for loop will skip a cluster due to the erase if the color wasn't
        // unique.
        ++cluster;
      }
    }

    bool convergence = false;
    for (int iteration = 0;
        iteration < kNumberOfIterations && !convergence && !clusters.empty();
        ++iteration) {

      // Loop through each pixel so we can place it in the appropriate cluster.
      std::vector<uint8_t>::iterator pixel = decoded_data.begin();
      while (pixel != decoded_data.end()) {
        uint8_t b = *(pixel++);
        if (pixel == decoded_data.end())
          continue;
        uint8_t g = *(pixel++);
        if (pixel == decoded_data.end())
          continue;
        uint8_t r = *(pixel++);
        if (pixel == decoded_data.end())
          continue;
        ++pixel; // Ignore the alpha channel.

        uint32_t distance_sqr_to_closest_cluster = UINT_MAX;
        std::vector<KMeanCluster>::iterator closest_cluster = clusters.begin();

        // Figure out which cluster this color is closest to in RGB space.
        for (std::vector<KMeanCluster>::iterator cluster = clusters.begin();
            cluster != clusters.end(); ++cluster) {
          uint32_t distance_sqr = cluster->GetDistanceSqr(r, g, b);

          if (distance_sqr < distance_sqr_to_closest_cluster) {
            distance_sqr_to_closest_cluster = distance_sqr;
            closest_cluster = cluster;
          }
        }

        closest_cluster->AddPoint(r, g, b);
      }

      // Calculate the new cluster centers and see if we've converged or not.
      convergence = true;
      for (std::vector<KMeanCluster>::iterator cluster = clusters.begin();
          cluster != clusters.end(); ++cluster) {
        convergence &= cluster->CompareCentroidWithAggregate();

        cluster->RecomputeCentroid();
      }
    }

    // Sort the clusters by population so we can tell what the most popular
    // color is.
    std::sort(clusters.begin(), clusters.end(),
              KMeanCluster::SortKMeanClusterByWeight);

    // Loop through the clusters to figure out which cluster has an appropriate
    // color. Skip any that are too bright/dark and go in order of weight.
    for (std::vector<KMeanCluster>::iterator cluster = clusters.begin();
        cluster != clusters.end(); ++cluster) {
      uint8_t r, g, b;
      cluster->GetCentroid(&r, &g, &b);
      // Sum the RGB components to determine if the color is too bright or too
      // dark.
      // TODO (dtrainor): Look into using HSV here instead. This approximation
      // might be fine though.
      uint32_t summed_color = r + g + b;

      if (summed_color < brightness_limit && summed_color > darkness_limit) {
        // If we found a valid color just set it and break. We don't want to
        // check the other ones.
        color = SkColorSetARGB(0xFF, r, g, b);
        break;
      } else if (cluster == clusters.begin()) {
        // We haven't found a valid color, but we are at the first color so
        // set the color anyway to make sure we at least have a value here.
        color = SkColorSetARGB(0xFF, r, g, b);
      }
    }
  }

  return color;
}

}  // color_utils
