/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include "platform/animation/TimingFunction.h"

#include <sstream>
#include <string>
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/wtf/text/WTFString.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Macro is only used to allow the use of streams.
// Can be removed if a pretty failure message isn't needed.
#define EXPECT_NE_WITH_MESSAGE(a, b)                                        \
  EXPECT_NE(*a.second, *b.second)                                           \
      << a.first << " (" << a.second->ToString().Latin1().data() << ")"     \
      << " ==  " << b.first << " (" << b.second->ToString().Latin1().data() \
      << ")"                                                                \
      << "\n";

namespace blink {

namespace {

class TimingFunctionTest : public ::testing::Test,
                           private ScopedFramesTimingFunctionForTest {
 public:
  TimingFunctionTest() : ScopedFramesTimingFunctionForTest(true) {}

  void NotEqualHelperLoop(
      Vector<std::pair<std::string, RefPtr<TimingFunction>>>& v) {
    for (size_t i = 0; i < v.size(); ++i) {
      for (size_t j = 0; j < v.size(); ++j) {
        if (i == j)
          continue;
        EXPECT_NE_WITH_MESSAGE(v[i], v[j]);
      }
    }
  }
};

TEST_F(TimingFunctionTest, LinearToString) {
  RefPtr<TimingFunction> linear_timing = LinearTimingFunction::Shared();
  EXPECT_EQ(linear_timing->ToString(), "linear");
}

TEST_F(TimingFunctionTest, CubicToString) {
  RefPtr<TimingFunction> cubic_ease_timing = CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE);
  EXPECT_EQ("ease", cubic_ease_timing->ToString());
  RefPtr<TimingFunction> cubic_ease_in_timing =
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_IN);
  EXPECT_EQ("ease-in", cubic_ease_in_timing->ToString());
  RefPtr<TimingFunction> cubic_ease_out_timing =
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_OUT);
  EXPECT_EQ("ease-out", cubic_ease_out_timing->ToString());
  RefPtr<TimingFunction> cubic_ease_in_out_timing =
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_IN_OUT);
  EXPECT_EQ("ease-in-out", cubic_ease_in_out_timing->ToString());

  RefPtr<TimingFunction> cubic_custom_timing =
      CubicBezierTimingFunction::Create(0.17, 0.67, 1, -1.73);
  EXPECT_EQ("cubic-bezier(0.17, 0.67, 1, -1.73)",
            cubic_custom_timing->ToString());
}

TEST_F(TimingFunctionTest, StepToString) {
  RefPtr<TimingFunction> step_timing_start =
      StepsTimingFunction::Preset(StepsTimingFunction::StepPosition::START);
  EXPECT_EQ("steps(1, start)", step_timing_start->ToString());

  RefPtr<TimingFunction> step_timing_middle =
      StepsTimingFunction::Preset(StepsTimingFunction::StepPosition::MIDDLE);
  EXPECT_EQ("steps(1, middle)", step_timing_middle->ToString());

  RefPtr<TimingFunction> step_timing_end =
      StepsTimingFunction::Preset(StepsTimingFunction::StepPosition::END);
  EXPECT_EQ("steps(1)", step_timing_end->ToString());

  RefPtr<TimingFunction> step_timing_custom_start =
      StepsTimingFunction::Create(3, StepsTimingFunction::StepPosition::START);
  EXPECT_EQ("steps(3, start)", step_timing_custom_start->ToString());

  RefPtr<TimingFunction> step_timing_custom_middle =
      StepsTimingFunction::Create(4, StepsTimingFunction::StepPosition::MIDDLE);
  EXPECT_EQ("steps(4, middle)", step_timing_custom_middle->ToString());

  RefPtr<TimingFunction> step_timing_custom_end =
      StepsTimingFunction::Create(5, StepsTimingFunction::StepPosition::END);
  EXPECT_EQ("steps(5)", step_timing_custom_end->ToString());
}

TEST_F(TimingFunctionTest, FrameToString) {
  RefPtr<TimingFunction> frame_timing = FramesTimingFunction::Create(3);
  EXPECT_EQ("frames(3)", frame_timing->ToString());
}

TEST_F(TimingFunctionTest, BaseOperatorEq) {
  RefPtr<TimingFunction> linear_timing = LinearTimingFunction::Shared();
  RefPtr<TimingFunction> cubic_timing1 = CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE_IN);
  RefPtr<TimingFunction> cubic_timing2 =
      CubicBezierTimingFunction::Create(0.17, 0.67, 1, -1.73);
  RefPtr<TimingFunction> steps_timing1 =
      StepsTimingFunction::Preset(StepsTimingFunction::StepPosition::END);
  RefPtr<TimingFunction> steps_timing2 =
      StepsTimingFunction::Create(5, StepsTimingFunction::StepPosition::START);
  RefPtr<TimingFunction> frames_timing = FramesTimingFunction::Create(5);

  Vector<std::pair<std::string, RefPtr<TimingFunction>>> v;
  v.push_back(std::make_pair("linearTiming", linear_timing));
  v.push_back(std::make_pair("cubicTiming1", cubic_timing1));
  v.push_back(std::make_pair("cubicTiming2", cubic_timing2));
  v.push_back(std::make_pair("stepsTiming1", steps_timing1));
  v.push_back(std::make_pair("stepsTiming2", steps_timing2));
  v.push_back(std::make_pair("framesTiming", frames_timing));
  NotEqualHelperLoop(v);
}

TEST_F(TimingFunctionTest, LinearOperatorEq) {
  RefPtr<TimingFunction> linear_timing1 = LinearTimingFunction::Shared();
  RefPtr<TimingFunction> linear_timing2 = LinearTimingFunction::Shared();
  EXPECT_EQ(*linear_timing1, *linear_timing1);
  EXPECT_EQ(*linear_timing1, *linear_timing2);
}

TEST_F(TimingFunctionTest, CubicOperatorEq) {
  RefPtr<TimingFunction> cubic_ease_in_timing1 =
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_IN);
  RefPtr<TimingFunction> cubic_ease_in_timing2 =
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_IN);
  EXPECT_EQ(*cubic_ease_in_timing1, *cubic_ease_in_timing1);
  EXPECT_EQ(*cubic_ease_in_timing1, *cubic_ease_in_timing2);

  RefPtr<TimingFunction> cubic_ease_out_timing1 =
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_OUT);
  RefPtr<TimingFunction> cubic_ease_out_timing2 =
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_OUT);
  EXPECT_EQ(*cubic_ease_out_timing1, *cubic_ease_out_timing1);
  EXPECT_EQ(*cubic_ease_out_timing1, *cubic_ease_out_timing2);

  RefPtr<TimingFunction> cubic_ease_in_out_timing1 =
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_IN_OUT);
  RefPtr<TimingFunction> cubic_ease_in_out_timing2 =
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_IN_OUT);
  EXPECT_EQ(*cubic_ease_in_out_timing1, *cubic_ease_in_out_timing1);
  EXPECT_EQ(*cubic_ease_in_out_timing1, *cubic_ease_in_out_timing2);

  RefPtr<TimingFunction> cubic_custom_timing1 =
      CubicBezierTimingFunction::Create(0.17, 0.67, 1, -1.73);
  RefPtr<TimingFunction> cubic_custom_timing2 =
      CubicBezierTimingFunction::Create(0.17, 0.67, 1, -1.73);
  EXPECT_EQ(*cubic_custom_timing1, *cubic_custom_timing1);
  EXPECT_EQ(*cubic_custom_timing1, *cubic_custom_timing2);

  Vector<std::pair<std::string, RefPtr<TimingFunction>>> v;
  v.push_back(std::make_pair("cubicEaseInTiming1", cubic_ease_in_timing1));
  v.push_back(std::make_pair("cubicEaseOutTiming1", cubic_ease_out_timing1));
  v.push_back(
      std::make_pair("cubicEaseInOutTiming1", cubic_ease_in_out_timing1));
  v.push_back(std::make_pair("cubicCustomTiming1", cubic_custom_timing1));
  NotEqualHelperLoop(v);
}

TEST_F(TimingFunctionTest, CubicOperatorEqReflectivity) {
  RefPtr<TimingFunction> cubic_a = CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE_IN);
  RefPtr<TimingFunction> cubic_b =
      CubicBezierTimingFunction::Create(0.42, 0.0, 1.0, 1.0);
  EXPECT_NE(*cubic_a, *cubic_b);
  EXPECT_NE(*cubic_b, *cubic_a);
}

TEST_F(TimingFunctionTest, StepsOperatorEq) {
  RefPtr<TimingFunction> steps_timing_start1 =
      StepsTimingFunction::Preset(StepsTimingFunction::StepPosition::START);
  RefPtr<TimingFunction> steps_timing_start2 =
      StepsTimingFunction::Preset(StepsTimingFunction::StepPosition::START);
  EXPECT_EQ(*steps_timing_start1, *steps_timing_start1);
  EXPECT_EQ(*steps_timing_start1, *steps_timing_start2);

  RefPtr<TimingFunction> steps_timing_end1 =
      StepsTimingFunction::Preset(StepsTimingFunction::StepPosition::END);
  RefPtr<TimingFunction> steps_timing_end2 =
      StepsTimingFunction::Preset(StepsTimingFunction::StepPosition::END);
  EXPECT_EQ(*steps_timing_end1, *steps_timing_end1);
  EXPECT_EQ(*steps_timing_end1, *steps_timing_end2);

  RefPtr<TimingFunction> steps_timing_custom1 =
      StepsTimingFunction::Create(5, StepsTimingFunction::StepPosition::START);
  RefPtr<TimingFunction> steps_timing_custom2 =
      StepsTimingFunction::Create(5, StepsTimingFunction::StepPosition::END);
  RefPtr<TimingFunction> steps_timing_custom3 =
      StepsTimingFunction::Create(7, StepsTimingFunction::StepPosition::START);
  RefPtr<TimingFunction> steps_timing_custom4 =
      StepsTimingFunction::Create(7, StepsTimingFunction::StepPosition::END);

  EXPECT_EQ(
      *StepsTimingFunction::Create(5, StepsTimingFunction::StepPosition::START),
      *steps_timing_custom1);
  EXPECT_EQ(
      *StepsTimingFunction::Create(5, StepsTimingFunction::StepPosition::END),
      *steps_timing_custom2);
  EXPECT_EQ(
      *StepsTimingFunction::Create(7, StepsTimingFunction::StepPosition::START),
      *steps_timing_custom3);
  EXPECT_EQ(
      *StepsTimingFunction::Create(7, StepsTimingFunction::StepPosition::END),
      *steps_timing_custom4);

  Vector<std::pair<std::string, RefPtr<TimingFunction>>> v;
  v.push_back(std::make_pair("stepsTimingStart1", steps_timing_start1));
  v.push_back(std::make_pair("stepsTimingEnd1", steps_timing_end1));
  v.push_back(std::make_pair("stepsTimingCustom1", steps_timing_custom1));
  v.push_back(std::make_pair("stepsTimingCustom2", steps_timing_custom2));
  v.push_back(std::make_pair("stepsTimingCustom3", steps_timing_custom3));
  v.push_back(std::make_pair("stepsTimingCustom4", steps_timing_custom4));
  NotEqualHelperLoop(v);
}

TEST_F(TimingFunctionTest, StepsOperatorEqPreset) {
  RefPtr<TimingFunction> steps_a =
      StepsTimingFunction::Preset(StepsTimingFunction::StepPosition::START);
  RefPtr<TimingFunction> steps_b =
      StepsTimingFunction::Create(1, StepsTimingFunction::StepPosition::START);
  EXPECT_EQ(*steps_a, *steps_b);
  EXPECT_EQ(*steps_b, *steps_a);
}

TEST_F(TimingFunctionTest, FramesOperatorEq) {
  RefPtr<TimingFunction> frames_timing1 = FramesTimingFunction::Create(5);
  RefPtr<TimingFunction> frames_timing2 = FramesTimingFunction::Create(7);

  EXPECT_EQ(*FramesTimingFunction::Create(5), *frames_timing1);
  EXPECT_EQ(*FramesTimingFunction::Create(7), *frames_timing2);

  Vector<std::pair<std::string, RefPtr<TimingFunction>>> v;
  v.push_back(std::make_pair("framesTiming1", frames_timing1));
  v.push_back(std::make_pair("framesTiming2", frames_timing2));
  NotEqualHelperLoop(v);
}

TEST_F(TimingFunctionTest, LinearEvaluate) {
  RefPtr<TimingFunction> linear_timing = LinearTimingFunction::Shared();
  EXPECT_EQ(0.2, linear_timing->Evaluate(0.2, 0));
  EXPECT_EQ(0.6, linear_timing->Evaluate(0.6, 0));
  EXPECT_EQ(-0.2, linear_timing->Evaluate(-0.2, 0));
  EXPECT_EQ(1.6, linear_timing->Evaluate(1.6, 0));
}

TEST_F(TimingFunctionTest, LinearRange) {
  double start = 0;
  double end = 1;
  RefPtr<TimingFunction> linear_timing = LinearTimingFunction::Shared();
  linear_timing->Range(&start, &end);
  EXPECT_NEAR(0, start, 0.01);
  EXPECT_NEAR(1, end, 0.01);
  start = -1;
  end = 10;
  linear_timing->Range(&start, &end);
  EXPECT_NEAR(-1, start, 0.01);
  EXPECT_NEAR(10, end, 0.01);
}

TEST_F(TimingFunctionTest, StepRange) {
  double start = 0;
  double end = 1;
  RefPtr<TimingFunction> steps =
      StepsTimingFunction::Preset(StepsTimingFunction::StepPosition::START);
  steps->Range(&start, &end);
  EXPECT_NEAR(0, start, 0.01);
  EXPECT_NEAR(1, end, 0.01);

  start = -1;
  end = 10;
  steps->Range(&start, &end);
  EXPECT_NEAR(0, start, 0.01);
  EXPECT_NEAR(1, end, 0.01);
}

TEST_F(TimingFunctionTest, FrameRange) {
  double start = 0;
  double end = 1;
  RefPtr<TimingFunction> frames = FramesTimingFunction::Create(4);
  frames->Range(&start, &end);
  EXPECT_NEAR(0, start, 0.01);
  EXPECT_NEAR(1, end, 0.01);

  start = -1;
  end = 10;
  frames->Range(&start, &end);
  EXPECT_NEAR(0, start, 0.01);
  EXPECT_NEAR(1, end, 0.01);
}

TEST_F(TimingFunctionTest, CubicRange) {
  double start = 0;
  double end = 1;

  RefPtr<TimingFunction> cubic_ease_timing = CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE);
  start = 0;
  end = 1;
  cubic_ease_timing->Range(&start, &end);
  EXPECT_NEAR(0, start, 0.01);
  EXPECT_NEAR(1, end, 0.01);
  start = -1;
  end = 10;
  cubic_ease_timing->Range(&start, &end);
  EXPECT_NEAR(-0.4, start, 0.01);
  EXPECT_NEAR(1, end, 0.01);

  RefPtr<TimingFunction> cubic_ease_in_timing =
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_IN);
  start = 0;
  end = 1;
  cubic_ease_in_timing->Range(&start, &end);
  EXPECT_NEAR(0, start, 0.01);
  EXPECT_NEAR(1, end, 0.01);
  start = -1;
  end = 10;
  cubic_ease_in_timing->Range(&start, &end);
  EXPECT_NEAR(0.0, start, 0.01);
  EXPECT_NEAR(16.51, end, 0.01);

  RefPtr<TimingFunction> cubic_ease_out_timing =
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_OUT);
  start = 0;
  end = 1;
  cubic_ease_out_timing->Range(&start, &end);
  EXPECT_NEAR(0, start, 0.01);
  EXPECT_NEAR(1, end, 0.01);
  start = -1;
  end = 10;
  cubic_ease_out_timing->Range(&start, &end);
  EXPECT_NEAR(-1.72, start, 0.01);
  EXPECT_NEAR(1.0, end, 0.01);

  RefPtr<TimingFunction> cubic_ease_in_out_timing =
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_IN_OUT);
  start = 0;
  end = 1;
  cubic_ease_in_out_timing->Range(&start, &end);
  EXPECT_NEAR(0, start, 0.01);
  EXPECT_NEAR(1, end, 0.01);
  start = -1;
  end = 10;
  cubic_ease_in_out_timing->Range(&start, &end);
  EXPECT_NEAR(0.0, start, 0.01);
  EXPECT_NEAR(1.0, end, 0.01);

  RefPtr<TimingFunction> cubic_custom_timing =
      CubicBezierTimingFunction::Create(0.17, 0.67, 1.0, -1.73);
  start = 0;
  end = 1;
  cubic_custom_timing->Range(&start, &end);
  EXPECT_NEAR(-0.33, start, 0.01);
  EXPECT_NEAR(1.0, end, 0.01);

  start = -1;
  end = 10;
  cubic_custom_timing->Range(&start, &end);
  EXPECT_NEAR(-3.94, start, 0.01);
  EXPECT_NEAR(4.578, end, 0.01);
}

TEST_F(TimingFunctionTest, CubicEvaluate) {
  double tolerance = 0.01;
  RefPtr<TimingFunction> cubic_ease_timing = CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE);
  EXPECT_NEAR(0.418, cubic_ease_timing->Evaluate(0.25, tolerance), tolerance);
  EXPECT_NEAR(0.805, cubic_ease_timing->Evaluate(0.50, tolerance), tolerance);
  EXPECT_NEAR(0.960, cubic_ease_timing->Evaluate(0.75, tolerance), tolerance);

  RefPtr<TimingFunction> cubic_ease_in_timing =
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_IN);
  EXPECT_NEAR(0.093, cubic_ease_in_timing->Evaluate(0.25, tolerance),
              tolerance);
  EXPECT_NEAR(0.305, cubic_ease_in_timing->Evaluate(0.50, tolerance),
              tolerance);
  EXPECT_NEAR(0.620, cubic_ease_in_timing->Evaluate(0.75, tolerance),
              tolerance);

  RefPtr<TimingFunction> cubic_ease_out_timing =
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_OUT);
  EXPECT_NEAR(0.379, cubic_ease_out_timing->Evaluate(0.25, tolerance),
              tolerance);
  EXPECT_NEAR(0.694, cubic_ease_out_timing->Evaluate(0.50, tolerance),
              tolerance);
  EXPECT_NEAR(0.906, cubic_ease_out_timing->Evaluate(0.75, tolerance),
              tolerance);

  RefPtr<TimingFunction> cubic_ease_in_out_timing =
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_IN_OUT);
  EXPECT_NEAR(0.128, cubic_ease_in_out_timing->Evaluate(0.25, tolerance),
              tolerance);
  EXPECT_NEAR(0.500, cubic_ease_in_out_timing->Evaluate(0.50, tolerance),
              tolerance);
  EXPECT_NEAR(0.871, cubic_ease_in_out_timing->Evaluate(0.75, tolerance),
              tolerance);

  RefPtr<TimingFunction> cubic_custom_timing =
      CubicBezierTimingFunction::Create(0.17, 0.67, 1, -1.73);
  EXPECT_NEAR(0.034, cubic_custom_timing->Evaluate(0.25, tolerance), tolerance);
  EXPECT_NEAR(-0.217, cubic_custom_timing->Evaluate(0.50, tolerance),
              tolerance);
  EXPECT_NEAR(-0.335, cubic_custom_timing->Evaluate(0.75, tolerance),
              tolerance);
}

TEST_F(TimingFunctionTest, StepsEvaluate) {
  RefPtr<TimingFunction> steps_timing_start =
      StepsTimingFunction::Preset(StepsTimingFunction::StepPosition::START);
  EXPECT_EQ(-1.00, steps_timing_start->Evaluate(-1.10, 0));
  EXPECT_EQ(0.00, steps_timing_start->Evaluate(-0.10, 0));
  EXPECT_EQ(1.00, steps_timing_start->Evaluate(0.00, 0));
  EXPECT_EQ(1.00, steps_timing_start->Evaluate(0.20, 0));
  EXPECT_EQ(1.00, steps_timing_start->Evaluate(0.60, 0));
  EXPECT_EQ(1.00, steps_timing_start->Evaluate(1.00, 0));
  EXPECT_EQ(3.00, steps_timing_start->Evaluate(2.00, 0));

  RefPtr<TimingFunction> steps_timing_middle =
      StepsTimingFunction::Preset(StepsTimingFunction::StepPosition::MIDDLE);
  EXPECT_EQ(-2.00, steps_timing_middle->Evaluate(-2.50, 0));
  EXPECT_EQ(0.00, steps_timing_middle->Evaluate(0.00, 0));
  EXPECT_EQ(0.00, steps_timing_middle->Evaluate(0.49, 0));
  EXPECT_EQ(1.00, steps_timing_middle->Evaluate(0.50, 0));
  EXPECT_EQ(1.00, steps_timing_middle->Evaluate(1.00, 0));
  EXPECT_EQ(3.00, steps_timing_middle->Evaluate(2.50, 0));

  RefPtr<TimingFunction> steps_timing_end =
      StepsTimingFunction::Preset(StepsTimingFunction::StepPosition::END);
  EXPECT_EQ(-2.00, steps_timing_end->Evaluate(-2.00, 0));
  EXPECT_EQ(0.00, steps_timing_end->Evaluate(0.00, 0));
  EXPECT_EQ(0.00, steps_timing_end->Evaluate(0.20, 0));
  EXPECT_EQ(0.00, steps_timing_end->Evaluate(0.60, 0));
  EXPECT_EQ(1.00, steps_timing_end->Evaluate(1.00, 0));
  EXPECT_EQ(2.00, steps_timing_end->Evaluate(2.00, 0));

  RefPtr<TimingFunction> steps_timing_custom_start =
      StepsTimingFunction::Create(4, StepsTimingFunction::StepPosition::START);
  EXPECT_EQ(-0.25, steps_timing_custom_start->Evaluate(-0.50, 0));
  EXPECT_EQ(0.25, steps_timing_custom_start->Evaluate(0.00, 0));
  EXPECT_EQ(0.25, steps_timing_custom_start->Evaluate(0.24, 0));
  EXPECT_EQ(0.50, steps_timing_custom_start->Evaluate(0.25, 0));
  EXPECT_EQ(0.50, steps_timing_custom_start->Evaluate(0.49, 0));
  EXPECT_EQ(0.75, steps_timing_custom_start->Evaluate(0.50, 0));
  EXPECT_EQ(0.75, steps_timing_custom_start->Evaluate(0.74, 0));
  EXPECT_EQ(1.00, steps_timing_custom_start->Evaluate(0.75, 0));
  EXPECT_EQ(1.00, steps_timing_custom_start->Evaluate(1.00, 0));
  EXPECT_EQ(1.75, steps_timing_custom_start->Evaluate(1.50, 0));

  RefPtr<TimingFunction> steps_timing_custom_middle =
      StepsTimingFunction::Create(4, StepsTimingFunction::StepPosition::MIDDLE);
  EXPECT_EQ(-2.00, steps_timing_custom_middle->Evaluate(-2.00, 0));
  EXPECT_EQ(0.00, steps_timing_custom_middle->Evaluate(0.00, 0));
  EXPECT_EQ(0.00, steps_timing_custom_middle->Evaluate(0.12, 0));
  EXPECT_EQ(0.25, steps_timing_custom_middle->Evaluate(0.13, 0));
  EXPECT_EQ(0.25, steps_timing_custom_middle->Evaluate(0.37, 0));
  EXPECT_EQ(0.50, steps_timing_custom_middle->Evaluate(0.38, 0));
  EXPECT_EQ(0.50, steps_timing_custom_middle->Evaluate(0.62, 0));
  EXPECT_EQ(0.75, steps_timing_custom_middle->Evaluate(0.63, 0));
  EXPECT_EQ(0.75, steps_timing_custom_middle->Evaluate(0.87, 0));
  EXPECT_EQ(1.00, steps_timing_custom_middle->Evaluate(0.88, 0));
  EXPECT_EQ(1.00, steps_timing_custom_middle->Evaluate(1.00, 0));
  EXPECT_EQ(3.00, steps_timing_custom_middle->Evaluate(3.00, 0));

  RefPtr<TimingFunction> steps_timing_custom_end =
      StepsTimingFunction::Create(4, StepsTimingFunction::StepPosition::END);
  EXPECT_EQ(-2.00, steps_timing_custom_end->Evaluate(-2.00, 0));
  EXPECT_EQ(0.00, steps_timing_custom_end->Evaluate(0.00, 0));
  EXPECT_EQ(0.00, steps_timing_custom_end->Evaluate(0.24, 0));
  EXPECT_EQ(0.25, steps_timing_custom_end->Evaluate(0.25, 0));
  EXPECT_EQ(0.25, steps_timing_custom_end->Evaluate(0.49, 0));
  EXPECT_EQ(0.50, steps_timing_custom_end->Evaluate(0.50, 0));
  EXPECT_EQ(0.50, steps_timing_custom_end->Evaluate(0.74, 0));
  EXPECT_EQ(0.75, steps_timing_custom_end->Evaluate(0.75, 0));
  EXPECT_EQ(0.75, steps_timing_custom_end->Evaluate(0.99, 0));
  EXPECT_EQ(1.00, steps_timing_custom_end->Evaluate(1.00, 0));
  EXPECT_EQ(2.00, steps_timing_custom_end->Evaluate(2.00, 0));
}

TEST_F(TimingFunctionTest, FramesEvaluate) {
  RefPtr<TimingFunction> frames_timing = FramesTimingFunction::Create(5);
  EXPECT_EQ(-2.50, frames_timing->Evaluate(-2.00, 0));
  EXPECT_EQ(0.00, frames_timing->Evaluate(0.00, 0));
  EXPECT_EQ(0.00, frames_timing->Evaluate(0.19, 0));
  EXPECT_EQ(0.25, frames_timing->Evaluate(0.20, 0));
  EXPECT_EQ(0.25, frames_timing->Evaluate(0.39, 0));
  EXPECT_EQ(0.50, frames_timing->Evaluate(0.40, 0));
  EXPECT_EQ(0.50, frames_timing->Evaluate(0.59, 0));
  EXPECT_EQ(0.75, frames_timing->Evaluate(0.60, 0));
  EXPECT_EQ(0.75, frames_timing->Evaluate(0.79, 0));
  EXPECT_EQ(1.00, frames_timing->Evaluate(0.80, 0));
  EXPECT_EQ(1.00, frames_timing->Evaluate(1.00, 0));
  EXPECT_EQ(3.75, frames_timing->Evaluate(3.00, 0));
}

}  // namespace

}  // namespace blink
