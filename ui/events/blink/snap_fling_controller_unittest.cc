// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/snap_fling_controller.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "ui/events/blink/snap_fling_curve.h"

namespace ui {
namespace test {
namespace {

class MockSnapFlingClient : public SnapFlingClient {
 public:
  MOCK_CONST_METHOD3(GetSnapFlingInfo,
                     bool(const gfx::Vector2dF& natural_displacement,
                          gfx::Vector2dF* initial_offset,
                          gfx::Vector2dF* target_offset));
  MOCK_METHOD0(ScrollEndForSnapFling, void());
  MOCK_METHOD0(RequestAnimationForSnapFling, void());
  MOCK_METHOD1(ScrollByForSnapFling, gfx::Vector2dF(const gfx::Vector2dF&));
};

class MockSnapFlingCurve : public SnapFlingCurve {
 public:
  MockSnapFlingCurve()
      : SnapFlingCurve(gfx::Vector2dF(),
                       gfx::Vector2dF(0, 100),
                       base::TimeTicks()) {}
  MOCK_CONST_METHOD0(IsFinished, bool());
  MOCK_METHOD1(GetScrollDelta, gfx::Vector2dF(base::TimeTicks));
};

}  // namespace

class SnapFlingControllerTest : public testing::Test {
 public:
  SnapFlingControllerTest() {
    controller_ = std::make_unique<SnapFlingController>(&mock_client_);
  }
  void SetCurve(std::unique_ptr<SnapFlingCurve> curve) {
    controller_->SetCurveForTest(std::move(curve));
  }
  void SetActiveState() { controller_->SetActiveStateForTest(); }

 protected:
  testing::StrictMock<MockSnapFlingClient> mock_client_;
  std::unique_ptr<SnapFlingController> controller_;
};

TEST_F(SnapFlingControllerTest, DoesNotFilterGSBWhenIdle) {
  blink::WebGestureEvent event(blink::WebInputEvent::kGestureScrollBegin, 0, 0);
  EXPECT_FALSE(controller_->FilterEventForSnap(event));
}

TEST_F(SnapFlingControllerTest, FiltersGSUAndGSEDependingOnState) {
  blink::WebGestureEvent scroll_update(
      blink::WebInputEvent::kGestureScrollUpdate, 0, 0);
  blink::WebGestureEvent scroll_end(blink::WebInputEvent::kGestureScrollEnd, 0,
                                    0);
  // Should not filter GSU and GSE if the fling is not active.
  EXPECT_FALSE(controller_->FilterEventForSnap(scroll_update));
  EXPECT_FALSE(controller_->FilterEventForSnap(scroll_end));

  // Should filter GSU and GSE if the fling is active.
  SetActiveState();
  EXPECT_TRUE(controller_->FilterEventForSnap(scroll_update));
  EXPECT_TRUE(controller_->FilterEventForSnap(scroll_end));
}

TEST_F(SnapFlingControllerTest, CreatesAndAnimatesCurveOnFirstInertialGSU) {
  blink::WebGestureEvent event(blink::WebInputEvent::kGestureScrollUpdate, 0,
                               0);
  event.data.scroll_update.delta_x = 0;
  event.data.scroll_update.delta_y = -10;
  event.data.scroll_update.inertial_phase =
      blink::WebGestureEvent::kMomentumPhase;

  EXPECT_CALL(mock_client_,
              GetSnapFlingInfo(testing::_, testing::_, testing::_))
      .WillOnce(DoAll(testing::SetArgPointee<1>(gfx::Vector2dF(0, 0)),
                      testing::SetArgPointee<2>(gfx::Vector2dF(0, 100)),
                      testing::Return(true)));
  EXPECT_CALL(mock_client_, RequestAnimationForSnapFling()).Times(1);
  EXPECT_CALL(mock_client_, ScrollByForSnapFling(testing::_)).Times(1);
  EXPECT_TRUE(controller_->HandleGestureScrollUpdate(event));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
}

TEST_F(SnapFlingControllerTest, DoesNotHandleNonInertialGSU) {
  blink::WebGestureEvent event(blink::WebInputEvent::kGestureScrollUpdate, 0,
                               0);
  event.data.scroll_update.delta_x = 0;
  event.data.scroll_update.delta_y = -10;
  event.data.scroll_update.inertial_phase =
      blink::WebGestureEvent::kNonMomentumPhase;

  EXPECT_CALL(mock_client_,
              GetSnapFlingInfo(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_client_, RequestAnimationForSnapFling()).Times(0);
  EXPECT_CALL(mock_client_, ScrollByForSnapFling(testing::_)).Times(0);
  EXPECT_FALSE(controller_->HandleGestureScrollUpdate(event));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
}

TEST_F(SnapFlingControllerTest, AnimatesTheCurve) {
  std::unique_ptr<MockSnapFlingCurve> mock_curve =
      std::make_unique<MockSnapFlingCurve>();
  MockSnapFlingCurve* curve = mock_curve.get();
  SetCurve(std::move(mock_curve));

  EXPECT_CALL(*curve, IsFinished()).WillOnce(testing::Return(false));
  EXPECT_CALL(*curve, GetScrollDelta(testing::_))
      .WillOnce(testing::Return(gfx::Vector2dF(100, 100)));
  EXPECT_CALL(mock_client_, RequestAnimationForSnapFling()).Times(1);
  EXPECT_CALL(mock_client_, ScrollByForSnapFling(gfx::Vector2dF(100, 100)));
  controller_->Animate(base::TimeTicks() + base::TimeDelta::FromSeconds(1));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
  testing::Mock::VerifyAndClearExpectations(curve);
}

TEST_F(SnapFlingControllerTest, FinishesTheCurve) {
  std::unique_ptr<MockSnapFlingCurve> mock_curve =
      std::make_unique<MockSnapFlingCurve>();
  MockSnapFlingCurve* curve = mock_curve.get();
  SetCurve(std::move(mock_curve));
  EXPECT_CALL(*curve, IsFinished()).WillOnce(testing::Return(true));
  EXPECT_CALL(*curve, GetScrollDelta(testing::_)).Times(0);
  EXPECT_CALL(mock_client_, RequestAnimationForSnapFling()).Times(0);
  EXPECT_CALL(mock_client_, ScrollByForSnapFling(testing::_)).Times(0);
  EXPECT_CALL(mock_client_, ScrollEndForSnapFling()).Times(1);
  controller_->Animate(base::TimeTicks() + base::TimeDelta::FromSeconds(1));
  testing::Mock::VerifyAndClearExpectations(curve);
  testing::Mock::VerifyAndClearExpectations(&mock_client_);

  EXPECT_CALL(*curve, IsFinished()).Times(0);
  EXPECT_CALL(mock_client_, RequestAnimationForSnapFling()).Times(0);
  EXPECT_CALL(mock_client_, ScrollByForSnapFling(testing::_)).Times(0);
  EXPECT_CALL(mock_client_, ScrollEndForSnapFling()).Times(0);
  controller_->Animate(base::TimeTicks() + base::TimeDelta::FromSeconds(2));
  testing::Mock::VerifyAndClearExpectations(curve);
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
}

TEST_F(SnapFlingControllerTest, GSBNotFilteredAndResetsStateWhenActive) {
  SetActiveState();
  blink::WebGestureEvent update_event(
      blink::WebInputEvent::kGestureScrollUpdate, 0, 0);
  update_event.data.scroll_update.inertial_phase =
      blink::WebGestureEvent::kMomentumPhase;
  EXPECT_TRUE(controller_->FilterEventForSnap(update_event));

  EXPECT_CALL(mock_client_, ScrollEndForSnapFling()).Times(1);
  blink::WebGestureEvent begin_event(blink::WebInputEvent::kGestureScrollBegin,
                                     0, 0);
  EXPECT_FALSE(controller_->FilterEventForSnap(begin_event));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);

  EXPECT_FALSE(controller_->FilterEventForSnap(update_event));
}

}  // namespace test
}  // namespace ui
