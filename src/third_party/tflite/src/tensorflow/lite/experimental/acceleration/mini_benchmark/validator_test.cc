/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/lite/experimental/acceleration/mini_benchmark/validator.h"

#include <string>

#include <gtest/gtest.h>
#include "flatbuffers/flatbuffers.h"  // from @flatbuffers
#include "tensorflow/lite/experimental/acceleration/configuration/configuration_generated.h"
#include "tensorflow/lite/experimental/acceleration/mini_benchmark/embedded_mobilenet_model.h"
#include "tensorflow/lite/experimental/acceleration/mini_benchmark/embedded_mobilenet_validation_model.h"
#include "tensorflow/lite/experimental/acceleration/mini_benchmark/mini_benchmark_test_helper.h"
#include "tensorflow/lite/experimental/acceleration/mini_benchmark/status_codes.h"

// Note that these tests are not meant to be completely exhaustive, but to test
// error propagation.

namespace tflite {
namespace acceleration {
namespace {

class ValidatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    validation_model_path_ = MiniBenchmarkTestHelper::DumpToTempFile(
        "mobilenet_quant_with_validation.tflite",
        g_tflite_acceleration_embedded_mobilenet_validation_model,
        g_tflite_acceleration_embedded_mobilenet_validation_model_len);
    ASSERT_TRUE(!validation_model_path_.empty());

    plain_model_path_ = MiniBenchmarkTestHelper::DumpToTempFile(
        "mobilenet_quant.tflite",
        g_tflite_acceleration_embedded_mobilenet_model,
        g_tflite_acceleration_embedded_mobilenet_model_len);
    ASSERT_TRUE(!plain_model_path_.empty());
  }

  std::string validation_model_path_;
  std::string plain_model_path_;
};

TEST_F(ValidatorTest, HappyPath) {
  flatbuffers::FlatBufferBuilder fbb;
  fbb.Finish(CreateComputeSettings(fbb));
  const ComputeSettings* settings =
      flatbuffers::GetRoot<ComputeSettings>(fbb.GetBufferPointer());

  Validator validator(validation_model_path_, settings);
  Validator::Results results;
  EXPECT_EQ(validator.RunValidation(&results), kMinibenchmarkSuccess);
  EXPECT_TRUE(results.ok);
  EXPECT_EQ(results.delegate_error, 0);
}

TEST_F(ValidatorTest, NoValidationSubgraph) {
  flatbuffers::FlatBufferBuilder fbb;
  fbb.Finish(CreateComputeSettings(fbb));
  const ComputeSettings* settings =
      flatbuffers::GetRoot<ComputeSettings>(fbb.GetBufferPointer());

  Validator validator(plain_model_path_, settings);
  EXPECT_EQ(validator.CheckModel(), kMinibenchmarkValidationSubgraphNotFound);
  Validator::Results results;
  EXPECT_EQ(validator.RunValidation(&results),
            kMinibenchmarkValidationSubgraphNotFound);
}

TEST_F(ValidatorTest, InvalidModel) {
  // Drop 12k to introduce a truncated model. Last ~11k are associated files.
  const std::string dump_path = MiniBenchmarkTestHelper::DumpToTempFile(
      "foo.tflite", g_tflite_acceleration_embedded_mobilenet_validation_model,
      g_tflite_acceleration_embedded_mobilenet_validation_model_len - 12000);
  ASSERT_TRUE(!dump_path.empty());
  flatbuffers::FlatBufferBuilder fbb;
  fbb.Finish(CreateComputeSettings(fbb));
  const ComputeSettings* settings =
      flatbuffers::GetRoot<ComputeSettings>(fbb.GetBufferPointer());

  Validator validator(dump_path, settings);
  EXPECT_EQ(validator.CheckModel(), kMinibenchmarkModelBuildFailed);
  Validator::Results results;
  EXPECT_EQ(validator.RunValidation(&results), kMinibenchmarkModelBuildFailed);
}

}  // namespace
}  // namespace acceleration
}  // namespace tflite
