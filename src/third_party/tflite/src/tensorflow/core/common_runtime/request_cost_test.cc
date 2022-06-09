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

#include "tensorflow/core/common_runtime/request_cost.h"

#include "absl/time/time.h"
#include "tensorflow/core/platform/test.h"

namespace tensorflow {
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(RequestCostTest, Basic) {
  RequestCost request_cost;

  request_cost.RecordCost(
      {{"tpu_v1", absl::Milliseconds(1)}, {"tpu_v2", absl::Milliseconds(2)}});
  request_cost.RecordCost({{"tpu_v1", absl::Milliseconds(10)},
                           {"tpu_v2", absl::Milliseconds(20)},
                           {"cpu_v1", absl::Milliseconds(30)},
                           {"cpu_v2", absl::Milliseconds(40)}});
  EXPECT_THAT(request_cost.GetCosts(),
              UnorderedElementsAre(Pair("tpu_v1", absl::Milliseconds(11)),
                                   Pair("tpu_v2", absl::Milliseconds(22)),
                                   Pair("cpu_v1", absl::Milliseconds(30)),
                                   Pair("cpu_v2", absl::Milliseconds(40))));

  request_cost.RecordCost(
      {{"cpu_v1", absl::Milliseconds(3)}, {"cpu_v2", absl::Milliseconds(4)}});
  EXPECT_THAT(request_cost.GetCosts(),
              UnorderedElementsAre(Pair("tpu_v1", absl::Milliseconds(11)),
                                   Pair("tpu_v2", absl::Milliseconds(22)),
                                   Pair("cpu_v1", absl::Milliseconds(33)),
                                   Pair("cpu_v2", absl::Milliseconds(44))));
}

}  // namespace
}  // namespace tensorflow
