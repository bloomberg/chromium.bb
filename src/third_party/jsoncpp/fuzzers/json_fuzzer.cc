// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// JsonCpp fuzzing wrapper to help with automated fuzz testing.

#include <stdint.h>
#include <climits>
#include <cstdio>
#include <memory>
#include "third_party/jsoncpp/source/include/json/json.h"

// JsonCpp has a few different parsing options. The code below makes sure that
// the most intersting variants are tested.
enum {
  kFeatureSetAll = 0,
  kFeatureSetDefault = 1,
  kFeatureSetStrict = 2,
  kFeatureSetLast
};
const int kSizeOfFeatureSet = kFeatureSetLast;

namespace {
struct Common {
  Json::Features features[kSizeOfFeatureSet];
};
}  // namespace

static Common* Initialize() {
  static Common common;
  common.features[kFeatureSetAll].allowComments_ = true;
  common.features[kFeatureSetAll].strictRoot_ = false;

  common.features[kFeatureSetStrict] = Json::Features::strictMode();

  return &common;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Common* common = Initialize();

  for (int i = 0; i < kSizeOfFeatureSet; ++i) {
    // Parse Json.
    Json::Reader reader(common->features[i]);
    Json::Value root;
    bool res = reader.parse(reinterpret_cast<const char*>(data),
                            reinterpret_cast<const char*>(data + size), root,
                            /*collectComments=*/true);
    if (!res) {
      continue;
    }

    // Write and re-read json.
    Json::FastWriter writer;
    std::string output_json = writer.write(root);

    Json::Value root_again;
    res = reader.parse(output_json, root_again, /*collectComments=*/true);
    if (!res) {
      continue;
    }

    // Run equality test.
    // Note: This actually causes the Json::Value tree to be traversed and all
    // the values to be dereferenced (until two of them are found not equal),
    // which is great for detecting memory corruption bugs when compiled with
    // AddressSanitizer. The result of the comparison is ignored, as it is
    // expected that both the original and the re-read version will differ from
    // time to time (e.g. due to floating point accuracy loss).
    (void)(root == root_again);
  }

  return 0;
}
