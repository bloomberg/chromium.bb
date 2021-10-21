// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_processor.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_command_line.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/processed_study.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/study_filtering.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace variations {
namespace {

class TestOverrideStringCallback {
 public:
  typedef std::map<uint32_t, std::u16string> OverrideMap;

  TestOverrideStringCallback()
      : callback_(base::BindRepeating(&TestOverrideStringCallback::Override,
                                      base::Unretained(this))) {}
  TestOverrideStringCallback(const TestOverrideStringCallback&) = delete;
  TestOverrideStringCallback& operator=(const TestOverrideStringCallback&) =
      delete;

  virtual ~TestOverrideStringCallback() {}

  const VariationsSeedProcessor::UIStringOverrideCallback& callback() const {
    return callback_;
  }

  const OverrideMap& overrides() const { return overrides_; }

 private:
  void Override(uint32_t hash, const std::u16string& string) {
    overrides_[hash] = string;
  }

  VariationsSeedProcessor::UIStringOverrideCallback callback_;
  OverrideMap overrides_;
};

struct Environment {
  Environment() { base::CommandLine::Init(0, nullptr); }

  base::AtExitManager at_exit_manager;
};

}  // namespace

void CreateTrialFromStudyFuzzer(const Study& study) {
  base::FieldTrialList field_trial_list(
      std::make_unique<SHA1EntropyProvider>("client_id"));
  base::FeatureList feature_list;

  TestOverrideStringCallback override_callback;
  base::MockEntropyProvider mock_low_entropy_provider(0.9);

  ProcessedStudy processed_study;
  const bool is_expired = internal::IsStudyExpired(study, base::Time::Now());
  if (processed_study.Init(&study, is_expired)) {
    VariationsSeedProcessor().CreateTrialFromStudy(
        processed_study, override_callback.callback(),
        &mock_low_entropy_provider, &feature_list);
  }
}

DEFINE_PROTO_FUZZER(const Study& study) {
  static Environment env;
  CreateTrialFromStudyFuzzer(study);
}

}  // namespace variations
