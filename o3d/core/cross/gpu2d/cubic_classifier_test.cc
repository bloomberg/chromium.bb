/*
 * Copyright 2010, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Tests for the cubic classifier.

#include "core/cross/gpu2d/cubic_classifier.h"
#include "gtest/gtest.h"

namespace o3d {
namespace gpu2d {

// The data points in the following tests were determined by hand in
// an interactive program.

TEST(CubicClassifierTest, TestSerpentine) {
  CubicClassifier::Result result =
      CubicClassifier::Classify(100, 100,
                                125, 175,
                                200, 100,
                                200, 200);
  EXPECT_EQ(CubicClassifier::kSerpentine, result.curve_type());
}

TEST(CubicClassifierTest, TestCusp) {
  CubicClassifier::Result result =
      CubicClassifier::Classify(100, 400,
                                200, 200,
                                300, 300,
                                400, 400);
  EXPECT_EQ(CubicClassifier::kCusp, result.curve_type());
}

TEST(CubicClassifierTest, TestLoop) {
  CubicClassifier::Result result =
      CubicClassifier::Classify(200, 200,
                                400, 100,
                                100, 100,
                                300, 200);
  EXPECT_EQ(CubicClassifier::kLoop, result.curve_type());
}

TEST(CubicClassifierTest, TestQuadratic) {
  CubicClassifier::Result result =
      CubicClassifier::Classify(100, 100,
                                166.66667f, 166.66667f,
                                233.33333f, 166.66667f,
                                300, 100);
  EXPECT_EQ(CubicClassifier::kQuadratic, result.curve_type());
}

// Can't figure out how to get the classifier to return kLine.

TEST(CubicClassifierTest, TestPoint) {
  CubicClassifier::Result result =
      CubicClassifier::Classify(100, 100,
                                100, 100,
                                100, 100,
                                100, 100);
  EXPECT_EQ(CubicClassifier::kPoint, result.curve_type());
}


}  // namespace gpu2d
}  // namespace o3d

