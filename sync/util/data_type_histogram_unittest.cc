// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/util/data_type_histogram.h"

#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class DataTypeHistogramTest : public testing::Test {
};

// Create a histogram of type HISTOGRAM_COUNTS for each model type. Nothing
// should break.
TEST(DataTypeHistogramTest, BasicCount) {
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i <= syncable::LAST_REAL_MODEL_TYPE; ++i) {
    syncable::ModelType type = syncable::ModelTypeFromInt(i);
#define PER_DATA_TYPE_MACRO(type_str) \
    HISTOGRAM_COUNTS("Prefix" type_str "Suffix", 1);
    SYNC_DATA_TYPE_HISTOGRAM(type);
#undef PER_DATA_TYPE_MACRO
  }
}

// Create a histogram of type SYNC_FREQ_HISTOGRAM for each model type. Nothing
// should break.
TEST(DataTypeHistogramTest, BasicFreq) {
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i <= syncable::LAST_REAL_MODEL_TYPE; ++i) {
    syncable::ModelType type = syncable::ModelTypeFromInt(i);
#define PER_DATA_TYPE_MACRO(type_str) \
    SYNC_FREQ_HISTOGRAM("Prefix" type_str "Suffix", \
                        base::TimeDelta::FromSeconds(1));
    SYNC_DATA_TYPE_HISTOGRAM(type);
#undef PER_DATA_TYPE_MACRO
  }
}

// Create a histogram of type UMA_HISTOGRAM_ENUMERATION for each model type.
// Nothing should break.
TEST(DataTypeHistogramTest, BasicEnum) {
  enum HistTypes {
    TYPE_1,
    TYPE_2,
    TYPE_COUNT,
  };
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i <= syncable::LAST_REAL_MODEL_TYPE; ++i) {
    syncable::ModelType type = syncable::ModelTypeFromInt(i);
#define PER_DATA_TYPE_MACRO(type_str) \
    UMA_HISTOGRAM_ENUMERATION("Prefix" type_str "Suffix", \
                              (i % 2 ? TYPE_1 : TYPE_2), TYPE_COUNT);
    SYNC_DATA_TYPE_HISTOGRAM(type);
#undef PER_DATA_TYPE_MACRO
  }
}

}  // namespace
}  // namespace syncer
