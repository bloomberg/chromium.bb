// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/element_area.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill_assistant/browser/mock_run_once_callback.h"
#include "components/autofill_assistant/browser/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Eq;

namespace autofill_assistant {

namespace {

class ElementAreaTest : public testing::Test {
 protected:
  ElementAreaTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        element_area_(&mock_web_controller_) {
    ON_CALL(mock_web_controller_, OnGetElementPosition(_, _))
        .WillByDefault(RunOnceCallback<1>(false, RectF()));
  }

  // scoped_task_environment_ must be first to guarantee other field
  // creation run in that environment.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  MockWebController mock_web_controller_;
  ElementArea element_area_;
};

TEST_F(ElementAreaTest, Empty) {
  EXPECT_TRUE(element_area_.IsEmpty());
  EXPECT_FALSE(element_area_.Contains(0.5f, 0.5f));
}

TEST_F(ElementAreaTest, ElementNotFound) {
  element_area_.SetElements({Selector({"#not_found"})});
  EXPECT_TRUE(element_area_.IsEmpty());
  EXPECT_FALSE(element_area_.Contains(0.5f, 0.5f));
}

TEST_F(ElementAreaTest, OneElement) {
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#found"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.25f, 0.25f, 0.75f, 0.75f)));

  element_area_.SetElements({Selector({"#found"})});
  EXPECT_FALSE(element_area_.IsEmpty());
  EXPECT_TRUE(element_area_.Contains(0.5f, 0.5f));
  EXPECT_FALSE(element_area_.Contains(0.1f, 0.5f));
  EXPECT_FALSE(element_area_.Contains(0.9f, 0.5f));
  EXPECT_FALSE(element_area_.Contains(0.5f, 0.1f));
  EXPECT_FALSE(element_area_.Contains(0.5f, 0.9f));
}

TEST_F(ElementAreaTest, TwoElements) {
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#top_left"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.0f, 0.0f, 0.25f, 0.25f)));
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#bottom_right"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.25f, 0.25f, 1.0f, 1.0f)));

  element_area_.SetElements(
      {Selector({"#top_left"}), Selector({"#bottom_right"})});
  EXPECT_FALSE(element_area_.IsEmpty());
  EXPECT_TRUE(element_area_.Contains(0.1f, 0.1f));
  EXPECT_TRUE(element_area_.Contains(0.9f, 0.9f));
  EXPECT_FALSE(element_area_.Contains(0.1f, 0.9f));
  EXPECT_FALSE(element_area_.Contains(0.9f, 0.1f));
}

TEST_F(ElementAreaTest, ElementMovesAfterUpdate) {
  testing::InSequence seq;
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#element"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.0f, 0.25f, 1.0f, 0.5f)))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.0f, 0.5f, 1.0f, 0.75f)));

  element_area_.SetElements({Selector({"#element"})});

  EXPECT_FALSE(element_area_.Contains(0.5f, 0.1f));
  EXPECT_TRUE(element_area_.Contains(0.5f, 0.4f));
  EXPECT_FALSE(element_area_.Contains(0.5f, 0.6f));
  EXPECT_FALSE(element_area_.Contains(0.5f, 0.8f));

  element_area_.UpdatePositions();

  EXPECT_FALSE(element_area_.Contains(0.5f, 0.1f));
  EXPECT_FALSE(element_area_.Contains(0.5f, 0.4f));
  EXPECT_TRUE(element_area_.Contains(0.5f, 0.6f));
  EXPECT_FALSE(element_area_.Contains(0.5f, 0.8f));
}

TEST_F(ElementAreaTest, ElementMovesWithTime) {
  testing::InSequence seq;
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#element"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.0f, 0.25f, 1.0f, 0.5f)))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.0f, 0.5f, 1.0f, 0.75f)));

  element_area_.SetElements({Selector({"#element"})});

  EXPECT_FALSE(element_area_.Contains(0.5f, 0.1f));
  EXPECT_TRUE(element_area_.Contains(0.5f, 0.4f));
  EXPECT_FALSE(element_area_.Contains(0.5f, 0.6f));
  EXPECT_FALSE(element_area_.Contains(0.5f, 0.8f));

  scoped_task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(100));

  EXPECT_FALSE(element_area_.Contains(0.5f, 0.1f));
  EXPECT_FALSE(element_area_.Contains(0.5f, 0.4f));
  EXPECT_TRUE(element_area_.Contains(0.5f, 0.6f));
  EXPECT_FALSE(element_area_.Contains(0.5f, 0.8f));
}

}  // namespace
}  // namespace autofill_assistant
