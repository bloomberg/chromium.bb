// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_ranker/tab_score_predictor.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/resource_coordinator/tab_ranker/native_inference.h"
#include "chrome/browser/resource_coordinator/tab_ranker/tab_features.h"
#include "chrome/grit/browser_resources.h"
#include "components/assist_ranker/example_preprocessing.h"
#include "components/assist_ranker/proto/example_preprocessor.pb.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"
#include "ui/base/resource/resource_bundle.h"

namespace tab_ranker {
namespace {

// Loads the preprocessor config protobuf, which lists each feature, their
// types, bucket configurations, etc.
// Returns true if the protobuf was successfully populated.
std::unique_ptr<assist_ranker::ExamplePreprocessorConfig>
LoadExamplePreprocessorConfig() {
  auto config = std::make_unique<assist_ranker::ExamplePreprocessorConfig>();

  scoped_refptr<base::RefCountedMemory> raw_config =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
          IDR_TAB_RANKER_EXAMPLE_PREPROCESSOR_CONFIG_PB);
  if (!raw_config || !raw_config->front()) {
    LOG(ERROR) << "Failed to load TabRanker example preprocessor config.";
    return nullptr;
  }

  if (!config->ParseFromArray(raw_config->front(), raw_config->size())) {
    LOG(ERROR) << "Failed to parse TabRanker example preprocessor config.";
    return nullptr;
  }

  return config;
}

}  // namespace

TabScorePredictor::TabScorePredictor() = default;
TabScorePredictor::~TabScorePredictor() = default;

TabRankerResult TabScorePredictor::ScoreTab(const TabFeatures& tab,
                                            float* score) {
  DCHECK(score);

  // No error is expected, but something could conceivably be misconfigured.
  TabRankerResult result = TabRankerResult::kSuccess;

  // Lazy-load the preprocessor config.
  LazyInitialize();
  if (preprocessor_config_) {
    // Build the RankerExample using the tab's features.
    assist_ranker::RankerExample example;
    PopulateTabFeaturesToRankerExample(tab, &example);

    // Process the RankerExample with the tab ranker config to vectorize the
    // feature list for inference.
    int preprocessor_error = preprocessor_->Process(&example);
    if (preprocessor_error) {
      // kNoFeatureIndexFound can occur normally (e.g., when the domain name
      // isn't known to the model or a rarely seen enum value is used).
      DCHECK_EQ(assist_ranker::ExamplePreprocessor::kNoFeatureIndexFound,
                preprocessor_error);
    }

    // This vector will be provided to the inference function.
    const auto& vectorized_features =
        example.features()
            .at(assist_ranker::ExamplePreprocessor::
                    kVectorizedFeatureDefaultName)
            .float_list()
            .float_value();
    CHECK_EQ(vectorized_features.size(), tfnative_model::FEATURES_SIZE);

    // Fixed amount of memory the inference function will use.
    if (!model_alloc_)
      model_alloc_ = std::make_unique<tfnative_model::FixedAllocations>();
    tfnative_model::Inference(vectorized_features.data(), score,
                              model_alloc_.get());

    if (preprocessor_error &&
        preprocessor_error !=
            assist_ranker::ExamplePreprocessor::kNoFeatureIndexFound) {
      // May indicate something is wrong with how we create the RankerExample.
      result = TabRankerResult::kPreprocessorOtherError;
    }
  } else {
    result = TabRankerResult::kPreprocessorInitializationFailed;
  }

  return result;
}

void TabScorePredictor::LazyInitialize() {
  if (preprocessor_config_)
    return;

  preprocessor_config_ = LoadExamplePreprocessorConfig();
  if (preprocessor_config_) {
    preprocessor_ = std::make_unique<assist_ranker::ExamplePreprocessor>(
        *preprocessor_config_);
  }
}

}  // namespace tab_ranker
