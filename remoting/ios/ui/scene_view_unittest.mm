// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/ui/scene_view.h"

#import "base/compiler_specific.h"
#import "testing/gtest_mac.h"

namespace remoting {

namespace {
const int kClientWidth = 200;
const int kClientHeight = 100;
const webrtc::DesktopSize kClientSize(kClientWidth, kClientHeight);
// Smaller then ClientSize
const webrtc::DesktopSize kSmall(50, 75);
// Inverted - The vertical is closer to an edge than the horizontal
const webrtc::DesktopSize kSmallInversed(175, 50);
// Larger then ClientSize
const webrtc::DesktopSize kLarge(800, 125);
const webrtc::DesktopSize kLargeInversed(225, 400);
}  // namespace

class SceneViewTest : public ::testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    scene_ = [[SceneView alloc] init];
    [scene_
        setContentSize:CGSizeMake(kClientSize.width(), kClientSize.height())];
    [scene_ setFrameSize:kLarge];
  }

  void MakeLarge() { [scene_ setFrameSize:kLarge]; }

  SceneView* scene_;
};

TEST(SceneViewTest_Property, ContentSize) {
  SceneView* scene = [[SceneView alloc] init];

  [scene setContentSize:CGSizeMake(0, 0)];
  EXPECT_EQ(0, scene.contentSize.width());
  EXPECT_EQ(0, scene.contentSize.height());
  float zeros[16] = {1.0f / 0.0f, 0, 0, 0, 0,           1.0f / 0.0f, 0, 0,
                     0,           0, 1, 0, 0.0f / 0.0f, 0.0f / 0.0f, 0, 1};

  ASSERT_TRUE(memcmp(zeros, scene.projectionMatrix.m, 16 * sizeof(float)) == 0);

  [scene setContentSize:CGSizeMake(kClientSize.width(), kClientSize.height())];
  EXPECT_EQ(kClientSize.width(), scene.contentSize.width());
  EXPECT_EQ(kClientSize.height(), scene.contentSize.height());

  EXPECT_TRUE(memcmp(GLKMatrix4MakeOrtho(
                         0.0, kClientWidth, 0.0, kClientHeight, 1.0, -1.0).m,
                     scene.projectionMatrix.m,
                     16 * sizeof(float)) == 0);
}

TEST(SceneViewTest_Property, FrameSizeInit) {
  SceneView* scene = [[SceneView alloc] init];
  [scene setContentSize:CGSizeMake(kClientSize.width(), kClientSize.height())];

  [scene setFrameSize:webrtc::DesktopSize(1, 1)];
  EXPECT_EQ(1, scene.frameSize.width());
  EXPECT_EQ(1, scene.frameSize.height());

  EXPECT_EQ(0, scene.position.x);
  EXPECT_EQ(0, scene.position.y);
  EXPECT_EQ(1, scene.position.z);

  EXPECT_FALSE(scene.anchored.left);
  EXPECT_FALSE(scene.anchored.right);
  EXPECT_FALSE(scene.anchored.top);
  EXPECT_FALSE(scene.anchored.bottom);

  EXPECT_EQ(0, scene.mousePosition.x());
  EXPECT_EQ(0, scene.mousePosition.y());
}

TEST(SceneViewTest_Property, FrameSizeLarge) {
  SceneView* scene = [[SceneView alloc] init];
  [scene setContentSize:CGSizeMake(kClientSize.width(), kClientSize.height())];
  [scene setFrameSize:kLarge];
  EXPECT_EQ(kLarge.width(), scene.frameSize.width());
  EXPECT_EQ(kLarge.height(), scene.frameSize.height());

  // Screen is positioned in the lower,left corner, zoomed until the vertical
  // fits exactly, and then centered horizontally
  //                                          HOST
  //     CLIENT      ------------------------------------------------
  //  ------------  |                                                |
  // |            | |                                                |
  // |            | |                                                |
  // |            | |                                                |
  //  ------------   ------------------------------------------------
  // RESULT - ONSCREEN is completely covered, with some of the HOST off screen
  // (-.-) the mouse cursor
  //  -----------------------------------------
  // |  ONSCREEN  |        OFFSCREEN           |
  // |    -.-     |                            |
  // |            |                            |
  //  -----------------------------------------
  float scale = static_cast<float>(kClientSize.height()) /
                static_cast<float>(kLarge.height());
  // vertical fits exactly
  EXPECT_EQ(scale, scene.position.z);

  // sitting on both Axis
  EXPECT_EQ(0, scene.position.x);
  EXPECT_EQ(0, scene.position.y);

  // bound on 3 sides, not on the right
  EXPECT_TRUE(scene.anchored.left);
  EXPECT_FALSE(scene.anchored.right);
  EXPECT_TRUE(scene.anchored.top);
  EXPECT_TRUE(scene.anchored.bottom);

  // mouse is off center on the left horizontal
  EXPECT_EQ(kClientSize.width() / (scale * 2), scene.mousePosition.x());
  // mouse is centered vertical
  EXPECT_EQ(kLarge.height() / 2, scene.mousePosition.y());
}

TEST(SceneViewTest_Property, FrameSizeLargeInversed) {
  SceneView* scene = [[SceneView alloc] init];
  [scene setContentSize:CGSizeMake(kClientSize.width(), kClientSize.height())];
  [scene setFrameSize:kLargeInversed];
  EXPECT_EQ(kLargeInversed.width(), scene.frameSize.width());
  EXPECT_EQ(kLargeInversed.height(), scene.frameSize.height());

  // Screen is positioned in the lower,left corner, zoomed until the vertical
  // fits exactly, and then centered horizontally
  //                       HOST
  //                  ---------------
  //                 |               |
  //                 |               |
  //                 |               |
  //                 |               |
  //                 |               |
  //                 |               |
  //                 |               |
  // CLIENT          |               |
  //  -------------  |               |
  // |             | |               |
  // |             | |               |
  // |             | |               |
  //  -------------   ---------------
  // RESULT, entire HOST is on screen
  // (-.-) the mouse cursor, XX is black backdrop
  //  -------------
  // |XX|       |XX|
  // |XX|  -.-  |XX|
  // |XX|       |XX|
  //  -------------
  float scale = static_cast<float>(kClientSize.height()) /
                static_cast<float>(kLargeInversed.height());
  // Vertical fits exactly
  EXPECT_EQ(scale, scene.position.z);

  // centered
  EXPECT_EQ(
      (kClientSize.width() - static_cast<int>(scale * kLargeInversed.width())) /
          2,
      scene.position.x);
  // sits on Axis
  EXPECT_EQ(0, scene.position.y);

  // bound on all 4 sides
  EXPECT_TRUE(scene.anchored.left);
  EXPECT_TRUE(scene.anchored.right);
  EXPECT_TRUE(scene.anchored.top);
  EXPECT_TRUE(scene.anchored.bottom);

  // mouse is in centered both vertical and horizontal
  EXPECT_EQ(kLargeInversed.width() / 2, scene.mousePosition.x());
  EXPECT_EQ(kLargeInversed.height() / 2, scene.mousePosition.y());
}

TEST(SceneViewTest_Property, FrameSizeSmall) {
  SceneView* scene = [[SceneView alloc] init];
  [scene setContentSize:CGSizeMake(kClientSize.width(), kClientSize.height())];
  [scene setFrameSize:kSmall];
  EXPECT_EQ(kSmall.width(), scene.frameSize.width());
  EXPECT_EQ(kSmall.height(), scene.frameSize.height());

  // Screen is positioned in the lower,left corner, zoomed until the vertical
  // fits exactly, and then centered horizontally
  //             CLIENT
  //  ---------------------------
  // |                           |   HOST
  // |                           |  -------
  // |                           | |       |
  // |                           | |       |
  // |                           | |       |
  // |                           | |       |
  // |                           | |       |
  //  ---------------------------   -------
  // RESULT, entire HOST is on screen
  // (-.-) the mouse cursor, XX is black backdrop
  //  ---------------------------
  // |XXXXXXXXX|       |XXXXXXXXX|
  // |XXXXXXXXX|       |XXXXXXXXX|
  // |XXXXXXXXX|       |XXXXXXXXX|
  // |XXXXXXXXX|  -.-  |XXXXXXXXX|
  // |XXXXXXXXX|       |XXXXXXXXX|
  // |XXXXXXXXX|       |XXXXXXXXX|
  // |XXXXXXXXX|       |XXXXXXXXX|
  //  ---------------------------
  float scale = static_cast<float>(kClientSize.height()) /
                static_cast<float>(kSmall.height());
  // Vertical fits exactly
  EXPECT_EQ(scale, scene.position.z);

  // centered
  EXPECT_EQ(
      (kClientSize.width() - static_cast<int>(scale * kSmall.width())) / 2,
      scene.position.x);
  // sits on Axis
  EXPECT_EQ(0, scene.position.y);

  // bound on all 4 sides
  EXPECT_TRUE(scene.anchored.left);
  EXPECT_TRUE(scene.anchored.right);
  EXPECT_TRUE(scene.anchored.top);
  EXPECT_TRUE(scene.anchored.bottom);

  // mouse is in centered both vertical and horizontal
  EXPECT_EQ((kSmall.width() / 2) - 1,  // -1 for pixel rounding
            scene.mousePosition.x());
  EXPECT_EQ(kSmall.height() / 2, scene.mousePosition.y());
}

TEST(SceneViewTest_Property, FrameSizeSmallInversed) {
  SceneView* scene = [[SceneView alloc] init];
  [scene setContentSize:CGSizeMake(kClientSize.width(), kClientSize.height())];
  [scene setFrameSize:kSmallInversed];
  EXPECT_EQ(kSmallInversed.width(), scene.frameSize.width());
  EXPECT_EQ(kSmallInversed.height(), scene.frameSize.height());

  // Screen is positioned in the lower,left corner, zoomed until the vertical
  // fits exactly, and then centered horizontally
  //             CLIENT
  //  ---------------------------
  // |                           |
  // |                           |
  // |                           |           HOST
  // |                           |  ----------------------
  // |                           | |                      |
  // |                           | |                      |
  // |                           | |                      |
  //  ---------------------------   ----------------------
  // RESULT - ONSCREEN is completely covered, with some of the HOST off screen
  // (-.-) the mouse cursor
  //  --------------------------------------------
  // |          ONSCREEN         |   OFFSCREEN    |
  // |                           |                |
  // |                           |                |
  // |            -.-            |                |
  // |                           |                |
  // |                           |                |
  // |                           |                |
  //  --------------------------------------------
  float scale = static_cast<float>(kClientSize.height()) /
                static_cast<float>(kSmallInversed.height());
  // vertical fits exactly
  EXPECT_EQ(scale, scene.position.z);

  // sitting on both Axis
  EXPECT_EQ(0, scene.position.x);
  EXPECT_EQ(0, scene.position.y);

  // bound on 3 sides, not on the right
  EXPECT_TRUE(scene.anchored.left);
  EXPECT_FALSE(scene.anchored.right);
  EXPECT_TRUE(scene.anchored.top);
  EXPECT_TRUE(scene.anchored.bottom);

  // mouse is off center on the left horizontal
  EXPECT_EQ(kClientSize.width() / (scale * 2), scene.mousePosition.x());
  // mouse is centered vertical
  EXPECT_EQ(kSmallInversed.height() / 2, scene.mousePosition.y());
}

TEST_F(SceneViewTest, ContainsTouchPoint) {
  int midWidth = kClientWidth / 2;
  int midHeight = kClientHeight / 2;
  // left
  EXPECT_FALSE([scene_ containsTouchPoint:CGPointMake(-1, midHeight)]);
  EXPECT_TRUE([scene_ containsTouchPoint:CGPointMake(0, midHeight)]);
  // right
  EXPECT_FALSE(
      [scene_ containsTouchPoint:CGPointMake(kClientWidth, midHeight)]);
  EXPECT_TRUE(
      [scene_ containsTouchPoint:CGPointMake(kClientWidth - 1, midHeight)]);
  // top
  EXPECT_FALSE(
      [scene_ containsTouchPoint:CGPointMake(midWidth, kClientHeight)]);
  EXPECT_TRUE(
      [scene_ containsTouchPoint:CGPointMake(midWidth, kClientHeight - 1)]);
  // bottom
  EXPECT_FALSE([scene_ containsTouchPoint:CGPointMake(midWidth, -1)]);
  EXPECT_TRUE([scene_ containsTouchPoint:CGPointMake(midWidth, 0)]);

  [scene_ setMarginsFromLeft:10 right:10 top:10 bottom:10];

  // left
  EXPECT_FALSE([scene_ containsTouchPoint:CGPointMake(9, midHeight)]);
  EXPECT_TRUE([scene_ containsTouchPoint:CGPointMake(10, midHeight)]);
  // right
  EXPECT_FALSE(
      [scene_ containsTouchPoint:CGPointMake(kClientWidth - 10, midHeight)]);
  EXPECT_TRUE(
      [scene_ containsTouchPoint:CGPointMake(kClientWidth - 11, midHeight)]);
  // top
  EXPECT_FALSE(
      [scene_ containsTouchPoint:CGPointMake(midWidth, kClientHeight - 10)]);
  EXPECT_TRUE(
      [scene_ containsTouchPoint:CGPointMake(midWidth, kClientHeight - 11)]);
  // bottom
  EXPECT_FALSE([scene_ containsTouchPoint:CGPointMake(midWidth, 9)]);
  EXPECT_TRUE([scene_ containsTouchPoint:CGPointMake(midWidth, 10)]);
}

TEST_F(SceneViewTest,
       UpdateMousePositionAndAnchorsWithTranslationNoMovement) {

  webrtc::DesktopVector originalPosition = scene_.mousePosition;
  AnchorPosition originalAnchors = scene_.anchored;

  [scene_ updateMousePositionAndAnchorsWithTranslation:CGPointMake(0, 0)
                                                 scale:1];

  webrtc::DesktopVector newPosition = scene_.mousePosition;

  EXPECT_EQ(0, abs(originalPosition.x() - newPosition.x()));
  EXPECT_EQ(0, abs(originalPosition.y() - newPosition.y()));

  EXPECT_EQ(originalAnchors.right, scene_.anchored.right);
  EXPECT_EQ(originalAnchors.top, scene_.anchored.top);
  EXPECT_EQ(originalAnchors.left, scene_.anchored.left);
  EXPECT_EQ(originalAnchors.bottom, scene_.anchored.bottom);

  EXPECT_FALSE(scene_.tickPanVelocity);
}

TEST_F(SceneViewTest,
       UpdateMousePositionAndAnchorsWithTranslationTowardLeftAndTop) {
  // Translation is in a coordinate space where (0,0) is the bottom left of the
  // view.  Mouse position in in a coordinate space where (0,0) is the top left
  // of the view.  So |y| is moved in the negative direction.

  webrtc::DesktopVector originalPosition = scene_.mousePosition;

  [scene_ setPanVelocity:CGPointMake(1, 1)];
  [scene_ updateMousePositionAndAnchorsWithTranslation:CGPointMake(2, -1)
                                                 scale:1];

  webrtc::DesktopVector newPosition = scene_.mousePosition;

  // We could do these checks as a single test, for a positive vs negative
  // difference.  But this style has a clearer meaning that the position moved
  // toward or away from the origin.
  EXPECT_LT(newPosition.x(), originalPosition.x());
  EXPECT_LT(newPosition.y(), originalPosition.y());
  EXPECT_EQ(2, abs(originalPosition.x() - newPosition.x()));
  EXPECT_EQ(1, abs(originalPosition.y() - newPosition.y()));

  EXPECT_TRUE(scene_.anchored.left);
  EXPECT_TRUE(scene_.anchored.top);

  EXPECT_FALSE(scene_.anchored.right);
  EXPECT_FALSE(scene_.anchored.bottom);

  EXPECT_TRUE(scene_.tickPanVelocity);

  // move much further than the bounds allow
  [scene_ setPanVelocity:CGPointMake(1, 1)];
  [scene_
      updateMousePositionAndAnchorsWithTranslation:CGPointMake(10000, -10000)
                                             scale:1];

  newPosition = scene_.mousePosition;

  EXPECT_EQ(0, newPosition.x());
  EXPECT_EQ(0, newPosition.y());

  EXPECT_TRUE(scene_.anchored.left);
  EXPECT_TRUE(scene_.anchored.top);

  EXPECT_FALSE(scene_.anchored.right);
  EXPECT_FALSE(scene_.anchored.bottom);

  EXPECT_FALSE(scene_.tickPanVelocity);
}

TEST_F(SceneViewTest,
       UpdateMousePositionAndAnchorsWithTranslationTowardLeftAndBottom) {
  webrtc::DesktopVector originalPosition = scene_.mousePosition;

  // see notes for Test
  // UpdateMousePositionAndAnchorsWithTranslationTowardLeftAndTop
  [scene_ setPanVelocity:CGPointMake(1, 1)];
  [scene_ updateMousePositionAndAnchorsWithTranslation:CGPointMake(2, 1)
                                                 scale:1];
  webrtc::DesktopVector newPosition = scene_.mousePosition;

  EXPECT_LT(newPosition.x(), originalPosition.x());
  EXPECT_GT(newPosition.y(), originalPosition.y());
  EXPECT_EQ(2, abs(originalPosition.x() - newPosition.x()));
  EXPECT_EQ(1, abs(originalPosition.y() - newPosition.y()));

  EXPECT_TRUE(scene_.anchored.left);
  EXPECT_TRUE(scene_.anchored.bottom);

  EXPECT_FALSE(scene_.anchored.right);
  EXPECT_FALSE(scene_.anchored.top);

  EXPECT_TRUE(scene_.tickPanVelocity);

  [scene_ setPanVelocity:CGPointMake(1, 1)];
  [scene_ updateMousePositionAndAnchorsWithTranslation:CGPointMake(10000, 10000)
                                                 scale:1];
  newPosition = scene_.mousePosition;

  EXPECT_EQ(0, newPosition.x());
  EXPECT_EQ(scene_.frameSize.height() - 1, newPosition.y());

  EXPECT_TRUE(scene_.anchored.left);
  EXPECT_TRUE(scene_.anchored.bottom);

  EXPECT_FALSE(scene_.anchored.right);
  EXPECT_FALSE(scene_.anchored.top);

  EXPECT_FALSE(scene_.tickPanVelocity);
}

TEST_F(SceneViewTest,
       UpdateMousePositionAndAnchorsWithTranslationTowardRightAndTop) {
  webrtc::DesktopVector originalPosition = scene_.mousePosition;

  // see notes for Test
  // UpdateMousePositionAndAnchorsWithTranslationTowardLeftAndTop

  // When moving to the right the mouse remains centered since the horizontal
  // display space is larger than the view space
  [scene_ setPanVelocity:CGPointMake(1, 1)];
  [scene_ updateMousePositionAndAnchorsWithTranslation:CGPointMake(-2, -1)
                                                 scale:1];
  webrtc::DesktopVector newPosition = scene_.mousePosition;

  EXPECT_LT(newPosition.y(), originalPosition.y());
  EXPECT_EQ(0, abs(originalPosition.x() - newPosition.x()));
  EXPECT_EQ(1, abs(originalPosition.y() - newPosition.y()));

  EXPECT_TRUE(scene_.anchored.top);

  EXPECT_FALSE(scene_.anchored.left);
  EXPECT_FALSE(scene_.anchored.right);
  EXPECT_FALSE(scene_.anchored.bottom);

  EXPECT_TRUE(scene_.tickPanVelocity);

  [scene_ setPanVelocity:CGPointMake(1, 1)];
  [scene_
      updateMousePositionAndAnchorsWithTranslation:CGPointMake(-10000, -10000)
                                             scale:1];
  newPosition = scene_.mousePosition;

  EXPECT_EQ(scene_.frameSize.width() - 1, newPosition.x());
  EXPECT_EQ(0, newPosition.y());

  EXPECT_TRUE(scene_.anchored.right);
  EXPECT_TRUE(scene_.anchored.top);

  EXPECT_FALSE(scene_.anchored.left);
  EXPECT_FALSE(scene_.anchored.bottom);

  EXPECT_FALSE(scene_.tickPanVelocity);
}

TEST_F(SceneViewTest,
       UpdateMousePositionAndAnchorsWithTranslationTowardRightAndBottom) {
  webrtc::DesktopVector originalPosition = scene_.mousePosition;

  // see notes for Test
  // UpdateMousePositionAndAnchorsWithTranslationTowardLeftAndTop

  // When moving to the right the mouse remains centered since the horizontal
  // display space is larger than the view space
  [scene_ setPanVelocity:CGPointMake(1, 1)];
  [scene_ updateMousePositionAndAnchorsWithTranslation:CGPointMake(-2, 1)
                                                 scale:1];
  webrtc::DesktopVector newPosition = scene_.mousePosition;

  EXPECT_GT(newPosition.y(), originalPosition.y());
  EXPECT_EQ(0, abs(originalPosition.x() - newPosition.x()));
  EXPECT_EQ(1, abs(originalPosition.y() - newPosition.y()));

  EXPECT_TRUE(scene_.anchored.bottom);

  EXPECT_FALSE(scene_.anchored.left);
  EXPECT_FALSE(scene_.anchored.right);
  EXPECT_FALSE(scene_.anchored.top);

  EXPECT_TRUE(scene_.tickPanVelocity);

  [scene_ setPanVelocity:CGPointMake(1, 1)];
  [scene_
      updateMousePositionAndAnchorsWithTranslation:CGPointMake(-10000, 10000)
                                             scale:1];
  newPosition = scene_.mousePosition;

  EXPECT_EQ(scene_.frameSize.width() - 1, newPosition.x());
  EXPECT_EQ(scene_.frameSize.height() - 1, newPosition.y());

  EXPECT_TRUE(scene_.anchored.right);
  EXPECT_TRUE(scene_.anchored.bottom);

  EXPECT_FALSE(scene_.anchored.left);
  EXPECT_FALSE(scene_.anchored.top);

  EXPECT_FALSE(scene_.tickPanVelocity);
}

TEST(SceneViewTest_Static, PositionDeltaFromScaling) {

  // Legend:
  // * anchored point or end point
  // | unanchored endpoint
  // - onscreen
  // # offscreen

  // *---|
  // *-------|
  EXPECT_EQ(
      0,
      [SceneView positionDeltaFromScaling:2.0F position:0 length:100 anchor:0]);
  // *---|
  // *-|
  EXPECT_EQ(
      0,
      [SceneView positionDeltaFromScaling:0.5F position:0 length:100 anchor:0]);
  //     |---*
  // |-------*
  EXPECT_EQ(100,
            [SceneView positionDeltaFromScaling:2.0F
                                       position:0
                                         length:100
                                         anchor:100]);
  // |----*
  //   |--*
  EXPECT_EQ(-50,
            [SceneView positionDeltaFromScaling:0.5F
                                       position:0
                                         length:100
                                         anchor:100]);
  //  |*---|
  // |-*-------|
  EXPECT_EQ(25,
            [SceneView positionDeltaFromScaling:2.0F
                                       position:0
                                         length:100
                                         anchor:25]);
  // |-*--|
  //  |*-|
  EXPECT_EQ(-12.5,
            [SceneView positionDeltaFromScaling:0.5F
                                       position:0
                                         length:100
                                         anchor:25]);
  //    |---*|
  // |------*-|
  EXPECT_EQ(75,
            [SceneView positionDeltaFromScaling:2.0F
                                       position:0
                                         length:100
                                         anchor:75]);
  // |--*-|
  //  |-*|
  EXPECT_EQ(-37.5,
            [SceneView positionDeltaFromScaling:0.5F
                                       position:0
                                         length:100
                                         anchor:75]);
  //   |-*-|
  // |---*---|
  EXPECT_EQ(50,
            [SceneView positionDeltaFromScaling:2.0F
                                       position:0
                                         length:100
                                         anchor:50]);
  // |--*--|
  //   |*|
  EXPECT_EQ(-25,
            [SceneView positionDeltaFromScaling:0.5F
                                       position:0
                                         length:100
                                         anchor:50]);
  //////////////////////////////////
  //  Change position to 50, anchor is relatively the same
  //////////////////////////////////
  EXPECT_EQ(0,
            [SceneView positionDeltaFromScaling:2.0F
                                       position:50
                                         length:100
                                         anchor:50]);
  EXPECT_EQ(0,
            [SceneView positionDeltaFromScaling:0.5F
                                       position:50
                                         length:100
                                         anchor:50]);
  EXPECT_EQ(100,
            [SceneView positionDeltaFromScaling:2.0F
                                       position:50
                                         length:100
                                         anchor:150]);
  EXPECT_EQ(-50,
            [SceneView positionDeltaFromScaling:0.5F
                                       position:50
                                         length:100
                                         anchor:150]);
  EXPECT_EQ(25,
            [SceneView positionDeltaFromScaling:2.0F
                                       position:50
                                         length:100
                                         anchor:75]);
  EXPECT_EQ(-12.5,
            [SceneView positionDeltaFromScaling:0.5F
                                       position:50
                                         length:100
                                         anchor:75]);
  EXPECT_EQ(75,
            [SceneView positionDeltaFromScaling:2.0F
                                       position:50
                                         length:100
                                         anchor:125]);
  EXPECT_EQ(-37.5,
            [SceneView positionDeltaFromScaling:0.5F
                                       position:50
                                         length:100
                                         anchor:125]);
  EXPECT_EQ(50,
            [SceneView positionDeltaFromScaling:2.0F
                                       position:50
                                         length:100
                                         anchor:100]);
  EXPECT_EQ(-25,
            [SceneView positionDeltaFromScaling:0.5F
                                       position:50
                                         length:100
                                         anchor:100]);

  //////////////////////////////////
  // Change position to -50, length to 200, anchor is relatively the same
  //////////////////////////////////
  EXPECT_EQ(0,
            [SceneView positionDeltaFromScaling:2.0F
                position:-50
                length:200
                anchor:-50]);
  EXPECT_EQ(0,
            [SceneView positionDeltaFromScaling:0.5F
                position:-50
                length:200
                anchor:-50]);
  EXPECT_EQ(200,
            [SceneView positionDeltaFromScaling:2.0F
                                       position:-50
                                         length:200
                                         anchor:150]);
  EXPECT_EQ(-100,
            [SceneView positionDeltaFromScaling:0.5F
                                       position:-50
                                         length:200
                                         anchor:150]);
  EXPECT_EQ(50,
            [SceneView positionDeltaFromScaling:2.0F
                                       position:-50
                                         length:200
                                         anchor:0]);
  EXPECT_EQ(-25,
            [SceneView positionDeltaFromScaling:0.5F
                                       position:-50
                                         length:200
                                         anchor:0]);
  EXPECT_EQ(150,
            [SceneView positionDeltaFromScaling:2.0F
                                       position:-50
                                         length:200
                                         anchor:100]);
  EXPECT_EQ(-75,
            [SceneView positionDeltaFromScaling:0.5F
                                       position:-50
                                         length:200
                                         anchor:100]);
  EXPECT_EQ(100,
            [SceneView positionDeltaFromScaling:2.0F
                                       position:-50
                                         length:200
                                         anchor:50]);
  EXPECT_EQ(-50,
            [SceneView positionDeltaFromScaling:0.5F
                                       position:-50
                                         length:200
                                         anchor:50]);
}

TEST(SceneViewTest_Static, PositionDeltaFromTranslation) {
  // Anchored on both sides.  Center it by using 1/2 the free space, offset by
  // the current position
  EXPECT_EQ(50,
            [SceneView positionDeltaFromTranslation:0
                                           position:0
                                          freeSpace:100
                              scaleingPositionDelta:0
                                      isAnchoredLow:YES
                                     isAnchoredHigh:YES]);
  EXPECT_EQ(50,
            [SceneView positionDeltaFromTranslation:100
                                           position:0
                                          freeSpace:100
                              scaleingPositionDelta:0
                                      isAnchoredLow:YES
                                     isAnchoredHigh:YES]);
  EXPECT_EQ(-50,
            [SceneView positionDeltaFromTranslation:0
                                           position:100
                                          freeSpace:100
                              scaleingPositionDelta:0
                                      isAnchoredLow:YES
                                     isAnchoredHigh:YES]);
  EXPECT_EQ(50,
            [SceneView positionDeltaFromTranslation:0
                                           position:0
                                          freeSpace:100
                              scaleingPositionDelta:100
                                      isAnchoredLow:YES
                                     isAnchoredHigh:YES]);
  EXPECT_EQ(100,
            [SceneView positionDeltaFromTranslation:0
                                           position:0
                                          freeSpace:200
                              scaleingPositionDelta:0
                                      isAnchoredLow:YES
                                     isAnchoredHigh:YES]);

  // Anchored only on the left.  Don't move it
  EXPECT_EQ(0,
            [SceneView positionDeltaFromTranslation:0
                                           position:0
                                          freeSpace:100
                              scaleingPositionDelta:0
                                      isAnchoredLow:YES
                                     isAnchoredHigh:NO]);
  EXPECT_EQ(0,
            [SceneView positionDeltaFromTranslation:100
                                           position:0
                                          freeSpace:100
                              scaleingPositionDelta:0
                                      isAnchoredLow:YES
                                     isAnchoredHigh:NO]);
  EXPECT_EQ(0,
            [SceneView positionDeltaFromTranslation:0
                                           position:100
                                          freeSpace:100
                              scaleingPositionDelta:0
                                      isAnchoredLow:YES
                                     isAnchoredHigh:NO]);
  EXPECT_EQ(0,
            [SceneView positionDeltaFromTranslation:0
                                           position:0
                                          freeSpace:200
                              scaleingPositionDelta:100
                                      isAnchoredLow:YES
                                     isAnchoredHigh:NO]);
  EXPECT_EQ(0,
            [SceneView positionDeltaFromTranslation:0
                                           position:0
                                          freeSpace:200
                              scaleingPositionDelta:0
                                      isAnchoredLow:YES
                                     isAnchoredHigh:NO]);
  // Anchored only on the right. Move by the scaling delta
  EXPECT_EQ(25,
            [SceneView positionDeltaFromTranslation:0
                                           position:0
                                          freeSpace:100
                              scaleingPositionDelta:25
                                      isAnchoredLow:NO
                                     isAnchoredHigh:YES]);
  EXPECT_EQ(50,
            [SceneView positionDeltaFromTranslation:100
                                           position:0
                                          freeSpace:100
                              scaleingPositionDelta:50
                                      isAnchoredLow:NO
                                     isAnchoredHigh:YES]);
  EXPECT_EQ(75,
            [SceneView positionDeltaFromTranslation:0
                                           position:100
                                          freeSpace:100
                              scaleingPositionDelta:75
                                      isAnchoredLow:NO
                                     isAnchoredHigh:YES]);
  EXPECT_EQ(100,
            [SceneView positionDeltaFromTranslation:0
                                           position:0
                                          freeSpace:100
                              scaleingPositionDelta:100
                                      isAnchoredLow:NO
                                     isAnchoredHigh:YES]);
  EXPECT_EQ(125,
            [SceneView positionDeltaFromTranslation:0
                                           position:0
                                          freeSpace:200
                              scaleingPositionDelta:125
                                      isAnchoredLow:NO
                                     isAnchoredHigh:YES]);
  // Not anchored, translate and move by the scaling delta
  EXPECT_EQ(0,
            [SceneView positionDeltaFromTranslation:0
                                           position:0
                                          freeSpace:100
                              scaleingPositionDelta:0
                                      isAnchoredLow:NO
                                     isAnchoredHigh:NO]);
  EXPECT_EQ(25,
            [SceneView positionDeltaFromTranslation:25
                                           position:0
                                          freeSpace:100
                              scaleingPositionDelta:0
                                      isAnchoredLow:NO
                                     isAnchoredHigh:NO]);
  EXPECT_EQ(50,
            [SceneView positionDeltaFromTranslation:50
                                           position:100
                                          freeSpace:100
                              scaleingPositionDelta:0
                                      isAnchoredLow:NO
                                     isAnchoredHigh:NO]);
  EXPECT_EQ(175,
            [SceneView positionDeltaFromTranslation:75
                                           position:0
                                          freeSpace:100
                              scaleingPositionDelta:100
                                      isAnchoredLow:NO
                                     isAnchoredHigh:NO]);
  EXPECT_EQ(100,
            [SceneView positionDeltaFromTranslation:100
                                           position:0
                                          freeSpace:200
                              scaleingPositionDelta:0
                                      isAnchoredLow:NO
                                     isAnchoredHigh:NO]);
}

TEST(SceneViewTest_Static, BoundDeltaFromPosition) {
  // Entire entity fits in our view, lower bound is not less than the
  // upperBound. The delta is bounded to the lowerBound.
  EXPECT_EQ(200,
            [SceneView boundDeltaFromPosition:0
                                        delta:0
                                   lowerBound:200
                                   upperBound:100]);
  EXPECT_EQ(100,
            [SceneView boundDeltaFromPosition:100
                                        delta:0
                                   lowerBound:200
                                   upperBound:100]);
  EXPECT_EQ(200,
            [SceneView boundDeltaFromPosition:0
                                        delta:100
                                   lowerBound:200
                                   upperBound:100]);
  EXPECT_EQ(150,
            [SceneView boundDeltaFromPosition:50
                                        delta:100
                                   lowerBound:200
                                   upperBound:200]);
  // Entity does not fit in our view.  The result would be out of bounds on the
  // high bound. The delta is bounded to the upper bound and the delta from the
  // position is returned.
  EXPECT_EQ(100,
            [SceneView boundDeltaFromPosition:0
                                        delta:1000
                                   lowerBound:0
                                   upperBound:100]);
  EXPECT_EQ(99,
            [SceneView boundDeltaFromPosition:1
                                        delta:1000
                                   lowerBound:0
                                   upperBound:100]);
  EXPECT_EQ(-50,
            [SceneView boundDeltaFromPosition:150
                                        delta:1000
                                   lowerBound:50
                                   upperBound:100]);
  EXPECT_EQ(100,
            [SceneView boundDeltaFromPosition:100
                                        delta:1000
                                   lowerBound:0
                                   upperBound:200]);
  // Entity does not fit in our view.  The result would be out of bounds on the
  // low bound. The delta is bounded to the lower bound and the delta from the
  // position is returned.
  EXPECT_EQ(0,
            [SceneView boundDeltaFromPosition:0
                                        delta:-1000
                                   lowerBound:0
                                   upperBound:100]);
  EXPECT_EQ(-20,
            [SceneView boundDeltaFromPosition:20
                                        delta:-1000
                                   lowerBound:0
                                   upperBound:100]);
  EXPECT_EQ(21,
            [SceneView boundDeltaFromPosition:29
                                        delta:-1000
                                   lowerBound:50
                                   upperBound:100]);
  EXPECT_EQ(1,
            [SceneView boundDeltaFromPosition:-1
                delta:-1000
                lowerBound:0
                upperBound:200]);
  // Entity does not fit in our view.  The result is in bounds.  The delta is
  // returned unchanged.
  EXPECT_EQ(50,
            [SceneView boundDeltaFromPosition:0
                                        delta:50
                                   lowerBound:0
                                   upperBound:100]);
  EXPECT_EQ(-10,
            [SceneView boundDeltaFromPosition:20
                                        delta:-10
                                   lowerBound:0
                                   upperBound:100]);
  EXPECT_EQ(31,
            [SceneView boundDeltaFromPosition:29
                                        delta:31
                                   lowerBound:50
                                   upperBound:100]);
  EXPECT_EQ(50,
            [SceneView boundDeltaFromPosition:100
                                        delta:50
                                   lowerBound:0
                                   upperBound:200]);
}

TEST(SceneViewTest_Static, BoundMouseGivenNextPosition) {
  // Mouse would move off screen in the negative
  EXPECT_EQ(0,
            [SceneView boundMouseGivenNextPosition:-1
                                       maxPosition:50
                                    centerPosition:2
                                     isAnchoredLow:YES
                                    isAnchoredHigh:YES]);
  EXPECT_EQ(0,
            [SceneView boundMouseGivenNextPosition:-1
                                       maxPosition:25
                                    centerPosition:99
                                     isAnchoredLow:NO
                                    isAnchoredHigh:NO]);
  EXPECT_EQ(0,
            [SceneView boundMouseGivenNextPosition:-11
                maxPosition:0
                centerPosition:-52
                isAnchoredLow:YES
                isAnchoredHigh:YES]);
  EXPECT_EQ(0,
            [SceneView boundMouseGivenNextPosition:-11
                maxPosition:-100
                centerPosition:44
                isAnchoredLow:NO
                isAnchoredHigh:NO]);
  EXPECT_EQ(0,
            [SceneView boundMouseGivenNextPosition:-1
                maxPosition:50
                centerPosition:-20
                isAnchoredLow:YES
                isAnchoredHigh:YES]);

  // Mouse would move off screen in the positive
  EXPECT_EQ(49,
            [SceneView boundMouseGivenNextPosition:50
                                       maxPosition:50
                                    centerPosition:2
                                     isAnchoredLow:YES
                                    isAnchoredHigh:YES]);
  EXPECT_EQ(24,
            [SceneView boundMouseGivenNextPosition:26
                                       maxPosition:25
                                    centerPosition:99
                                     isAnchoredLow:NO
                                    isAnchoredHigh:NO]);
  EXPECT_EQ(-1,
            [SceneView boundMouseGivenNextPosition:1
                                       maxPosition:0
                                    centerPosition:-52
                                     isAnchoredLow:YES
                                    isAnchoredHigh:YES]);
  EXPECT_EQ(-101,
            [SceneView boundMouseGivenNextPosition:0
                                       maxPosition:-100
                                    centerPosition:44
                                     isAnchoredLow:NO
                                    isAnchoredHigh:NO]);
  EXPECT_EQ(49,
            [SceneView boundMouseGivenNextPosition:60
                                       maxPosition:50
                                    centerPosition:-20
                                     isAnchoredLow:YES
                                    isAnchoredHigh:YES]);

  // Mouse is not out of bounds, and not anchored.  The Center is returned.
  EXPECT_EQ(2,
            [SceneView boundMouseGivenNextPosition:0
                                       maxPosition:100
                                    centerPosition:2
                                     isAnchoredLow:NO
                                    isAnchoredHigh:NO]);
  EXPECT_EQ(99,
            [SceneView boundMouseGivenNextPosition:25
                                       maxPosition:100
                                    centerPosition:99
                                     isAnchoredLow:NO
                                    isAnchoredHigh:NO]);
  EXPECT_EQ(-52,
            [SceneView boundMouseGivenNextPosition:99
                                       maxPosition:100
                                    centerPosition:-52
                                     isAnchoredLow:NO
                                    isAnchoredHigh:NO]);
  EXPECT_EQ(44,
            [SceneView boundMouseGivenNextPosition:120
                                       maxPosition:200
                                    centerPosition:44
                                     isAnchoredLow:NO
                                    isAnchoredHigh:NO]);
  EXPECT_EQ(-20,
            [SceneView boundMouseGivenNextPosition:180
                                       maxPosition:200
                                    centerPosition:-20
                                     isAnchoredLow:NO
                                    isAnchoredHigh:NO]);

  // Mouse is not out of bounds, and anchored.  The position closest
  // to the anchor is returned.
  EXPECT_EQ(0,
            [SceneView boundMouseGivenNextPosition:0
                                       maxPosition:100
                                    centerPosition:2
                                     isAnchoredLow:YES
                                    isAnchoredHigh:NO]);
  EXPECT_EQ(25,
            [SceneView boundMouseGivenNextPosition:25
                                       maxPosition:100
                                    centerPosition:99
                                     isAnchoredLow:YES
                                    isAnchoredHigh:NO]);
  EXPECT_EQ(-52,
            [SceneView boundMouseGivenNextPosition:99
                                       maxPosition:100
                                    centerPosition:-52
                                     isAnchoredLow:YES
                                    isAnchoredHigh:NO]);
  EXPECT_EQ(44,
            [SceneView boundMouseGivenNextPosition:120
                                       maxPosition:200
                                    centerPosition:44
                                     isAnchoredLow:YES
                                    isAnchoredHigh:NO]);
  EXPECT_EQ(-20,
            [SceneView boundMouseGivenNextPosition:180
                                       maxPosition:200
                                    centerPosition:-20
                                     isAnchoredLow:YES
                                    isAnchoredHigh:NO]);
  EXPECT_EQ(2,
            [SceneView boundMouseGivenNextPosition:0
                                       maxPosition:100
                                    centerPosition:2
                                     isAnchoredLow:NO
                                    isAnchoredHigh:YES]);
  EXPECT_EQ(99,
            [SceneView boundMouseGivenNextPosition:25
                                       maxPosition:100
                                    centerPosition:99
                                     isAnchoredLow:NO
                                    isAnchoredHigh:YES]);
  EXPECT_EQ(99,
            [SceneView boundMouseGivenNextPosition:99
                                       maxPosition:100
                                    centerPosition:-52
                                     isAnchoredLow:NO
                                    isAnchoredHigh:YES]);
  EXPECT_EQ(120,
            [SceneView boundMouseGivenNextPosition:120
                                       maxPosition:200
                                    centerPosition:44
                                     isAnchoredLow:NO
                                    isAnchoredHigh:YES]);
  EXPECT_EQ(180,
            [SceneView boundMouseGivenNextPosition:180
                                       maxPosition:200
                                    centerPosition:-20
                                     isAnchoredLow:NO
                                    isAnchoredHigh:YES]);
  EXPECT_EQ(0,
            [SceneView boundMouseGivenNextPosition:0
                                       maxPosition:100
                                    centerPosition:2
                                     isAnchoredLow:YES
                                    isAnchoredHigh:YES]);
  EXPECT_EQ(25,
            [SceneView boundMouseGivenNextPosition:25
                                       maxPosition:100
                                    centerPosition:99
                                     isAnchoredLow:YES
                                    isAnchoredHigh:YES]);
  EXPECT_EQ(99,
            [SceneView boundMouseGivenNextPosition:99
                                       maxPosition:100
                                    centerPosition:-52
                                     isAnchoredLow:YES
                                    isAnchoredHigh:YES]);
  EXPECT_EQ(120,
            [SceneView boundMouseGivenNextPosition:120
                                       maxPosition:200
                                    centerPosition:44
                                     isAnchoredLow:YES
                                    isAnchoredHigh:YES]);
  EXPECT_EQ(180,
            [SceneView boundMouseGivenNextPosition:180
                                       maxPosition:200
                                    centerPosition:-20
                                     isAnchoredLow:YES
                                    isAnchoredHigh:YES]);
}

TEST(SceneViewTest_Static, BoundVelocity) {
  // Outside bounds of the axis
  EXPECT_EQ(0, [SceneView boundVelocity:5.0f axisLength:100 mousePosition:0]);
  EXPECT_EQ(0, [SceneView boundVelocity:5.0f axisLength:100 mousePosition:99]);
  EXPECT_EQ(0, [SceneView boundVelocity:5.0f axisLength:200 mousePosition:200]);
  // Not outside bounds of the axis
  EXPECT_EQ(5.0f,
            [SceneView boundVelocity:5.0f axisLength:100 mousePosition:1]);
  EXPECT_EQ(5.0f,
            [SceneView boundVelocity:5.0f axisLength:100 mousePosition:98]);
  EXPECT_EQ(5.0f,
            [SceneView boundVelocity:5.0f axisLength:200 mousePosition:100]);
}

TEST_F(SceneViewTest, TickPanVelocity) {
  // We are in the large frame, which can pan left and right but not up and
  // down.  Start by resizing it to allow panning up and down.

  [scene_ panAndZoom:CGPointMake(0, 0) scaleBy:2.0f];

  // Going up and right
  [scene_ setPanVelocity:CGPointMake(1000, 1000)];
  [scene_ tickPanVelocity];

  webrtc::DesktopVector pos = scene_.mousePosition;
  int loopLimit = 0;
  bool didMove = false;
  bool inMotion = true;

  while (inMotion && loopLimit < 100) {
    inMotion = [scene_ tickPanVelocity];
    if (inMotion) {
      ASSERT_TRUE(pos.x() <= scene_.mousePosition.x()) << " after " << loopLimit
                                                       << " iterations.";
      ASSERT_TRUE(pos.y() <= scene_.mousePosition.y()) << " after " << loopLimit
                                                       << " iterations.";
      didMove = true;
    }
    pos = scene_.mousePosition;
    loopLimit++;
  }

  EXPECT_LT(1, loopLimit);
  EXPECT_TRUE(!inMotion);
  EXPECT_TRUE(didMove);

  // Going down and left
  [scene_ setPanVelocity:CGPointMake(-1000, -1000)];
  [scene_ tickPanVelocity];

  pos = scene_.mousePosition;
  loopLimit = 0;
  didMove = false;
  inMotion = true;

  while (inMotion && loopLimit < 100) {
    inMotion = [scene_ tickPanVelocity];
    if (inMotion) {
      ASSERT_TRUE(pos.x() >= scene_.mousePosition.x()) << " after " << loopLimit
                                                       << " iterations.";
      ASSERT_TRUE(pos.y() >= scene_.mousePosition.y()) << " after " << loopLimit
                                                       << " iterations.";
      didMove = true;
    }
    pos = scene_.mousePosition;
    loopLimit++;
  }

  EXPECT_LT(1, loopLimit);
  EXPECT_TRUE(!inMotion);
  EXPECT_TRUE(didMove);
}

}  // namespace remoting