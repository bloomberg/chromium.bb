// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/view_root.h"

namespace ui {

using base::android::JavaParamRef;

class TestViewRoot : public ViewRoot {
 public:
  TestViewRoot() : ViewRoot(0L) {}
  float GetDipScale() override { return 1.f; }
};

class TestViewClient : public ViewClient {
 public:
  TestViewClient() : handle_event_(true),
                     called_(false) {}

  void SetHandleEvent(bool handle_event) { handle_event_ = handle_event; }
  bool OnTouchEvent(const MotionEventData& event) override {
    called_ = true;
    return handle_event_;
  }

  bool EventHandled() { return called_ && handle_event_; }
  void Reset() { called_ = false; }

 private:
  bool handle_event_;  // Marks as event was consumed. True by default.
  bool called_;
};

class ViewAndroidBoundsTest : public testing::Test {
 public:
  ViewAndroidBoundsTest() : view1_(&client1_),
                            view2_(&client2_),
                            view3_(&client3_) {}
  void Reset() {
    client1_.Reset();
    client2_.Reset();
    client3_.Reset();
  }

  void GenerateTouchEventAt(float x, float y) {
    root_.OnTouchEvent(nullptr,
                       JavaParamRef<jobject>(nullptr),
                       JavaParamRef<jobject>(nullptr),
                       0L, // time
                       0, 1, 0, 0,
                       x, y, 0.f, 0.f, // pos
                       0, 0,  // pointer_id
                       0.f, 0.f, 0.f, 0.f,  // touch
                       0.f, 0.f, 0.f, 0.f,
                       0.f, 0.f,
                       0, 0, 0, 0,
                       false);
  }

  void ExpectHit(const TestViewClient& hitClient) {
    TestViewClient* clients[3] = { &client1_, &client2_, &client3_ };
    for (auto& client : clients) {
      if (&hitClient == client)
        EXPECT_TRUE(client->EventHandled());
      else
        EXPECT_FALSE(client->EventHandled());
    }
    Reset();
  }

  TestViewRoot root_;
  TestViewClient client1_;
  TestViewClient client2_;
  TestViewClient client3_;
  ViewAndroid view1_;
  ViewAndroid view2_;
  ViewAndroid view3_;
};

TEST_F(ViewAndroidBoundsTest, MatchesViewInFront) {
  view1_.SetLayout(50, 50, 400, 600, false);
  view2_.SetLayout(50, 50, 400, 600, false);
  root_.AddChild(&view2_);
  root_.AddChild(&view1_);

  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(client1_);

  // View 2 moves up to the top, and events should hit it from now.
  root_.MoveToTop(&view2_);
  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(client2_);
}

TEST_F(ViewAndroidBoundsTest, MatchesViewArea) {
  view1_.SetLayout(50, 50, 200, 200, false);
  view2_.SetLayout(20, 20, 400, 600, false);

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
  view1_.SetLayout(50, 50, 200, 200, false);
  view2_.SetLayout(20, 20, 400, 600, false);
  root_.AddChild(&view2_);
  root_.AddChild(&view1_);

  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(client1_);

  view1_.SetLayout(150, 150, 200, 200, false);
  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(client2_);
}

TEST_F(ViewAndroidBoundsTest, MatchesViewSizeOfkMatchParent) {
  view1_.SetLayout(20, 20, 400, 600, false);
  view3_.SetLayout(0, 0, 0, 0, true);  // match parent
  view2_.SetLayout(50, 50, 200, 200, false);

  root_.AddChild(&view1_);
  root_.AddChild(&view2_);
  view1_.AddChild(&view3_);

  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(client2_);

  GenerateTouchEventAt(300.f, 400.f);
  ExpectHit(client3_);

  client3_.SetHandleEvent(false);
  GenerateTouchEventAt(300.f, 400.f);
  ExpectHit(client1_);
}

TEST_F(ViewAndroidBoundsTest, MatchesViewsWithOffset) {
  view1_.SetLayout(10, 20, 150, 100, false);
  view2_.SetLayout(20, 30, 40, 30, false);
  view3_.SetLayout(70, 30, 40, 30, false);

  root_.AddChild(&view1_);
  view1_.AddChild(&view2_);
  view1_.AddChild(&view3_);

  GenerateTouchEventAt(70, 30);
  ExpectHit(client1_);

  GenerateTouchEventAt(40, 60);
  ExpectHit(client2_);

  GenerateTouchEventAt(100, 70);
  ExpectHit(client3_);
}

}  // namespace ui
