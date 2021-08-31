/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/lite/micro/testing/micro_test.h"
#include "tensorflow/lite/micro/testing/test_utils.h"

TF_LITE_MICRO_TESTS_BEGIN

TF_LITE_MICRO_TEST(ArgumentsExecutedOnlyOnce) {
  float count = 0.;
  // Make sure either argument is executed once after macro expansion.
  TF_LITE_MICRO_EXPECT_NEAR(0, count++, 0.1);
  TF_LITE_MICRO_EXPECT_NEAR(1, count++, 0.1);
  TF_LITE_MICRO_EXPECT_NEAR(count++, 2, 0.1);
  TF_LITE_MICRO_EXPECT_NEAR(count++, 3, 0.1);
}

TF_LITE_MICRO_TESTS_END
