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

#include "tensorflow/compiler/xla/permutation_util.h"

#include "tensorflow/compiler/xla/test.h"

namespace xla {
namespace {

TEST(PermutationUtilTest, IsPermutation) {
  EXPECT_TRUE(IsPermutation({}));
  EXPECT_TRUE(IsPermutation({0}));
  EXPECT_FALSE(IsPermutation({-3}));
  EXPECT_TRUE(IsPermutation({0, 1}));
  EXPECT_FALSE(IsPermutation({1, 1}));
  EXPECT_TRUE(IsPermutation({1, 0}));
  EXPECT_TRUE(IsPermutation({3, 1, 0, 2}));
  EXPECT_FALSE(IsPermutation({3, 0, 2}));
}

}  // namespace
}  // namespace xla
