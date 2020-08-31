// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_FEATURES_PARSER_H_
#define COMPONENTS_ARC_ARC_FEATURES_PARSER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"

namespace arc {

// This struct contains an ARC available feature map, unavailable feature set
// and ARC build property map.
struct ArcFeatures {
  // Key is the feature name. Value is the feature version.
  using FeatureVersionMapping = std::map<std::string, int>;

  // Each item in the vector is the feature name.
  using FeatureList = std::vector<std::string>;

  // Key is the property key, such as "ro.build.version.sdk". Value is the
  // corresponding property value.
  using BuildPropsMapping = std::map<std::string, std::string>;

  ArcFeatures();
  ArcFeatures(ArcFeatures&& other);
  ~ArcFeatures();

  ArcFeatures& operator=(ArcFeatures&& other);

  // This map contains all ARC system available features. For each feature, it
  // has the name and version. Unavailable features have been filtered out from
  // this map.
  FeatureVersionMapping feature_map;

  // This list contains all ARC unavailable feature names.
  FeatureList unavailable_features;

  // This map contains all ARC build properties.
  BuildPropsMapping build_props;

  std::string play_store_version;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcFeatures);
};

// Parses JSON files for Android system available features and build properties.
//
// A feature JSON file looks like this:
// {
//   "features": [
//     {
//       "name": "android.hardware.location",
//       "version: 2
//     },
//     {
//       "name": "android.hardware.location.network",
//       "version": 0
//     }
//   ],
//   "unavailable_features": [
//     "android.hardware.usb.accessory",
//     "android.software.live_tv"
//   ],
//   "properties": {
//     "ro.product.cpu.abilist": "x86_64,x86,armeabi-v7a,armeabi",
//     "ro.build.version.sdk": "25"
//   },
//   "play_store_version": "81010860"
// }
class ArcFeaturesParser {
 public:
  // Get ARC system available features.
  static void GetArcFeatures(
      base::OnceCallback<void(base::Optional<ArcFeatures>)> callback);

  // Given an input feature JSON, return ARC features. This method is for
  // testing only.
  static base::Optional<ArcFeatures> ParseFeaturesJsonForTesting(
      base::StringPiece input_json);

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcFeaturesParser);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_FEATURES_PARSER_H_
