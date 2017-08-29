// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/css/CSSTransitionData.h"

#include "platform/animation/TimingFunction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSSTransitionData, TransitionsMatchForStyleRecalc_Initial) {
  std::unique_ptr<CSSTransitionData> transition1 = CSSTransitionData::Create();
  std::unique_ptr<CSSTransitionData> transition2 = CSSTransitionData::Create();
  EXPECT_TRUE(transition1->TransitionsMatchForStyleRecalc(*transition2));
}

TEST(CSSTransitionData, TransitionsMatchForStyleRecalc_CubicBezierSameObject) {
  std::unique_ptr<CSSTransitionData> transition1 = CSSTransitionData::Create();
  std::unique_ptr<CSSTransitionData> transition2 = CSSTransitionData::Create();
  RefPtr<TimingFunction> func =
      CubicBezierTimingFunction::Create(0.2f, 0.2f, 0.9f, 0.7f);
  transition1->TimingFunctionList().push_back(func);
  transition2->TimingFunctionList().push_back(func);
  EXPECT_TRUE(transition1->TransitionsMatchForStyleRecalc(*transition2));
}

TEST(CSSTransitionData,
     TransitionsMatchForStyleRecalc_CubicBezierDifferentObjects) {
  std::unique_ptr<CSSTransitionData> transition1 = CSSTransitionData::Create();
  std::unique_ptr<CSSTransitionData> transition2 = CSSTransitionData::Create();
  RefPtr<TimingFunction> func1 =
      CubicBezierTimingFunction::Create(0.2f, 0.2f, 0.9f, 0.7f);
  RefPtr<TimingFunction> func2 =
      CubicBezierTimingFunction::Create(0.2f, 0.2f, 0.9f, 0.7f);
  transition1->TimingFunctionList().push_back(func1);
  transition2->TimingFunctionList().push_back(func2);
  EXPECT_TRUE(transition1->TransitionsMatchForStyleRecalc(*transition2));
}

TEST(CSSTransitionData,
     TransitionsMatchForStyleRecalc_CubicBezierDifferentValues) {
  std::unique_ptr<CSSTransitionData> transition1 = CSSTransitionData::Create();
  std::unique_ptr<CSSTransitionData> transition2 = CSSTransitionData::Create();
  RefPtr<TimingFunction> func1 =
      CubicBezierTimingFunction::Create(0.1f, 0.25f, 0.9f, 0.57f);
  RefPtr<TimingFunction> func2 =
      CubicBezierTimingFunction::Create(0.2f, 0.2f, 0.9f, 0.7f);
  transition1->TimingFunctionList().push_back(func1);
  transition2->TimingFunctionList().push_back(func2);
  EXPECT_FALSE(transition1->TransitionsMatchForStyleRecalc(*transition2));
}

}  // namespace blink
