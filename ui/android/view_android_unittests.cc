// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/event_forwarder.h"
#include "ui/android/view_android.h"
#include "ui/android/view_client.h"
#include "ui/events/android/motion_event_android.h"

namespace ui {

using base::android::JavaParamRef;

class TestViewClient : public ViewClient {
 public:
  TestViewClient() : handle_event_(true), called_(false) {}

  void SetHandleEvent(bool handle_event) { handle_event_ = handle_event; }
  bool OnTouchEvent(const MotionEventAndroid& event,
                    bool for_touch_handle) override {
    called_ = true;
    return handle_event_;
  }

  bool EventHandled() { return called_ && handle_event_; }
  bool EventCalled() { return called_; }
  void Reset() { called_ = false; }

 private:
  bool handle_event_;  // Marks as event was consumed. True by default.
  bool called_;
};

class ViewAndroidBoundsTest : public testing::Test {
 public:
  ViewAndroidBoundsTest()
      : root_(nullptr),
        view1_(&client1_),
        view2_(&client2_),
        view3_(&client3_) {
    root_.GetEventForwarder();
    root_.SetLayout(ViewAndroid::LayoutParams::MatchParent());
  }

  void Reset() {
    client1_.Reset();
    client2_.Reset();
    client3_.Reset();
  }

  void GenerateTouchEventAt(float x, float y) {
    ui::MotionEventAndroid::Pointer pointer0(0, x, y, 0, 0, 0, 0, 0);
    ui::MotionEventAndroid::Pointer pointer1(0, 0, 0, 0, 0, 0, 0, 0);
    ui::MotionEventAndroid event(nullptr, JavaParamRef<jobject>(nullptr), 1.f,
                                 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
                                 &pointer0, &pointer1);
    root_.OnTouchEvent(event, false);
  }

  void ExpectHit(const TestViewClient& hitClient) {
    TestViewClient* clients[3] = {&client1_, &client2_, &client3_};
    for (auto* client : clients) {
      if (&hitClient == client)
        EXPECT_TRUE(client->EventHandled());
      else
        EXPECT_FALSE(client->EventHandled());
    }
    Reset();
  }

  ViewAndroid root_;
  ViewAndroid view1_;
  ViewAndroid view2_;
  ViewAndroid view3_;
  TestViewClient client1_;
  TestViewClient client2_;
  TestViewClient client3_;
};

TEST_F(ViewAndroidBoundsTest, MatchesViewInFront) {
  view1_.SetLayout(ViewAndroid::LayoutParams::Normal(50, 50, 400, 600));
  view2_.SetLayout(ViewAndroid::LayoutParams::Normal(50, 50, 400, 600));
  root_.AddChild(&view2_);
  root_.AddChild(&view1_);

  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(client1_);

  // View 2 moves up to the top, and events should hit it from now.
  root_.MoveToFront(&view2_);
  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(client2_);
}

TEST_F(ViewAndroidBoundsTest, MatchesViewArea) {
  view1_.SetLayout(ViewAndroid::LayoutParams::Normal(50, 50, 200, 200));
  view2_.SetLayout(ViewAndroid::LayoutParams::Normal(20, 20, 400, 600));

  root_.AddChild(&view2_);
  root_.AddChild(&view1_);

  // Falls within |view1_|'s bounds
  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(client1_);

  // Falls within |view2_|'s bounds
  GenerateTouchEventAt(300.f, 400.f);
  ExpectHit(client2_);
}

TEST_F(ViewAndroidBoundsTest, MatchesViewAfterMove) {
  view1_.SetLayout(ViewAndroid::LayoutParams::Normal(50, 50, 200, 200));
  view2_.SetLayout(ViewAndroid::LayoutParams::Normal(20, 20, 400, 600));
  root_.AddChild(&view2_);
  root_.AddChild(&view1_);

  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(client1_);

  view1_.SetLayout(ViewAndroid::LayoutParams::Normal(150, 150, 200, 200));
  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(client2_);
}

TEST_F(ViewAndroidBoundsTest, MatchesViewSizeOfkMatchParent) {
  view1_.SetLayout(ViewAndroid::LayoutParams::Normal(20, 20, 400, 600));
  view3_.SetLayout(ViewAndroid::LayoutParams::MatchParent());
  view2_.SetLayout(ViewAndroid::LayoutParams::Normal(50, 50, 200, 200));

  root_.AddChild(&view1_);
  root_.AddChild(&view2_);
  view1_.AddChild(&view3_);

  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(client2_);

  GenerateTouchEventAt(300.f, 400.f);
  ExpectHit(client1_);

  client1_.SetHandleEvent(false);
  GenerateTouchEventAt(300.f, 400.f);
  EXPECT_TRUE(client1_.EventCalled());
  ExpectHit(client3_);
}

TEST_F(ViewAndroidBoundsTest, MatchesViewsWithOffset) {
  view1_.SetLayout(ViewAndroid::LayoutParams::Normal(10, 20, 150, 100));
  view2_.SetLayout(ViewAndroid::LayoutParams::Normal(20, 30, 40, 30));
  view3_.SetLayout(ViewAndroid::LayoutParams::Normal(70, 30, 40, 30));

  root_.AddChild(&view1_);
  view1_.AddChild(&view2_);
  view1_.AddChild(&view3_);

  GenerateTouchEventAt(70, 30);
  ExpectHit(client1_);

  client1_.SetHandleEvent(false);
  GenerateTouchEventAt(40, 60);
  EXPECT_TRUE(client1_.EventCalled());
  ExpectHit(client2_);

  GenerateTouchEventAt(100, 70);
  EXPECT_TRUE(client1_.EventCalled());
  ExpectHit(client3_);
}

TEST(ViewAndroidTest, ChecksMultipleEventForwarders) {
  ViewAndroid parent;
  ViewAndroid child;
  parent.GetEventForwarder();
  child.GetEventForwarder();
  EXPECT_DCHECK_DEATH(parent.AddChild(&child));

  ViewAndroid parent2;
  ViewAndroid child2;
  parent2.GetEventForwarder();
  parent2.AddChild(&child2);
  EXPECT_DCHECK_DEATH(child2.GetEventForwarder());

  ViewAndroid window;
  ViewAndroid wcv1, wcv2;
  ViewAndroid rwhv1a, rwhv1b, rwhv2;
  wcv1.GetEventForwarder();
  wcv2.GetEventForwarder();

  window.AddChild(&wcv1);
  wcv1.AddChild(&rwhv1a);
  wcv1.AddChild(&rwhv1b);

  wcv2.AddChild(&rwhv2);

  // window should be able to add wcv2 since there's only one event forwarder
  // in the path window - wcv2* - rwvh2
  window.AddChild(&wcv2);

  // Additional event forwarder will cause failure.
  EXPECT_DCHECK_DEATH(rwhv2.GetEventForwarder());
}

}  // namespace ui
