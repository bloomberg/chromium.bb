// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ImageQualityController.h"

#include <memory>
#include "core/layout/LayoutImage.h"
#include "core/layout/LayoutTestHelper.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/scheduler/test/fake_web_task_runner.h"
#include "platform/wtf/PtrUtil.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ImageQualityControllerTest : public RenderingTest {
 protected:
  ImageQualityController* Controller() { return controller_; }

 private:
  void SetUp() override {
    controller_ = ImageQualityController::GetImageQualityController();
    RenderingTest::SetUp();
  }

  ImageQualityController* controller_;
};

TEST_F(ImageQualityControllerTest, RegularImage) {
  SetBodyInnerHTML("<img src='myimage'></img>");
  LayoutObject* obj = GetDocument().body()->FirstChild()->GetLayoutObject();

  EXPECT_EQ(kInterpolationDefault, Controller()->ChooseInterpolationQuality(
                                       *obj, nullptr, nullptr, LayoutSize()));
}

TEST_F(ImageQualityControllerTest, ImageRenderingPixelated) {
  SetBodyInnerHTML(
      "<img src='myimage' style='image-rendering: pixelated'></img>");
  LayoutObject* obj = GetDocument().body()->FirstChild()->GetLayoutObject();

  EXPECT_EQ(kInterpolationNone, Controller()->ChooseInterpolationQuality(
                                    *obj, nullptr, nullptr, LayoutSize()));
}

#if !USE(LOW_QUALITY_IMAGE_INTERPOLATION)

class TestImageAnimated : public Image {
 public:
  bool MaybeAnimated() override { return true; }
  bool CurrentFrameKnownToBeOpaque(
      MetadataMode = kUseCurrentMetadata) override {
    return false;
  }
  IntSize Size() const override { return IntSize(); }
  void DestroyDecodedData() override {}
  void Draw(PaintCanvas*,
            const PaintFlags&,
            const FloatRect& dst_rect,
            const FloatRect& src_rect,
            RespectImageOrientationEnum,
            ImageClampingMode) override {}
  sk_sp<SkImage> ImageForCurrentFrame() override { return nullptr; }
};

TEST_F(ImageQualityControllerTest, ImageMaybeAnimated) {
  SetBodyInnerHTML("<img src='myimage'></img>");
  LayoutImage* img =
      ToLayoutImage(GetDocument().body()->FirstChild()->GetLayoutObject());

  RefPtr<TestImageAnimated> test_image = AdoptRef(new TestImageAnimated);
  EXPECT_EQ(kInterpolationMedium,
            Controller()->ChooseInterpolationQuality(*img, test_image.Get(),
                                                     nullptr, LayoutSize()));
}

class TestImageWithContrast : public Image {
 public:
  bool MaybeAnimated() override { return true; }
  bool CurrentFrameKnownToBeOpaque(
      MetadataMode = kUseCurrentMetadata) override {
    return false;
  }
  IntSize Size() const override { return IntSize(); }
  void DestroyDecodedData() override {}
  void Draw(PaintCanvas*,
            const PaintFlags&,
            const FloatRect& dst_rect,
            const FloatRect& src_rect,
            RespectImageOrientationEnum,
            ImageClampingMode) override {}

  bool IsBitmapImage() const override { return true; }
  sk_sp<SkImage> ImageForCurrentFrame() override { return nullptr; }
};

TEST_F(ImageQualityControllerTest, LowQualityFilterForContrast) {
  SetBodyInnerHTML(
      "<img src='myimage' style='image-rendering: "
      "-webkit-optimize-contrast'></img>");
  LayoutImage* img =
      ToLayoutImage(GetDocument().body()->FirstChild()->GetLayoutObject());

  RefPtr<TestImageWithContrast> test_image =
      AdoptRef(new TestImageWithContrast);
  EXPECT_EQ(kInterpolationLow,
            Controller()->ChooseInterpolationQuality(
                *img, test_image.Get(), test_image.Get(), LayoutSize()));
}

class TestImageLowQuality : public Image {
 public:
  bool MaybeAnimated() override { return true; }
  bool CurrentFrameKnownToBeOpaque(
      MetadataMode = kUseCurrentMetadata) override {
    return false;
  }
  IntSize Size() const override { return IntSize(1, 1); }
  void DestroyDecodedData() override {}
  void Draw(PaintCanvas*,
            const PaintFlags&,
            const FloatRect& dst_rect,
            const FloatRect& src_rect,
            RespectImageOrientationEnum,
            ImageClampingMode) override {}

  bool IsBitmapImage() const override { return true; }
  sk_sp<SkImage> ImageForCurrentFrame() override { return nullptr; }
};

TEST_F(ImageQualityControllerTest, MediumQualityFilterForUnscaledImage) {
  SetBodyInnerHTML("<img src='myimage'></img>");
  LayoutImage* img =
      ToLayoutImage(GetDocument().body()->FirstChild()->GetLayoutObject());

  RefPtr<TestImageLowQuality> test_image = AdoptRef(new TestImageLowQuality);
  EXPECT_EQ(kInterpolationMedium,
            Controller()->ChooseInterpolationQuality(
                *img, test_image.Get(), test_image.Get(), LayoutSize(1, 1)));
}

// TODO(alexclarke): Remove this when possible.
class MockTimer : public TaskRunnerTimer<ImageQualityController> {
 public:
  using TimerFiredFunction =
      typename TaskRunnerTimer<ImageQualityController>::TimerFiredFunction;

  static std::unique_ptr<MockTimer> Create(ImageQualityController* o,
                                           TimerFiredFunction f) {
    auto task_runner = AdoptRef(new scheduler::FakeWebTaskRunner);
    return WTF::WrapUnique(new MockTimer(std::move(task_runner), o, f));
  }

  void Fire() {
    Fired();
    Stop();
  }

  void SetTime(double new_time) { task_runner_->SetTime(new_time); }

 private:
  MockTimer(RefPtr<scheduler::FakeWebTaskRunner> task_runner,
            ImageQualityController* o,
            TimerFiredFunction f)
      : TaskRunnerTimer(task_runner, o, f),
        task_runner_(std::move(task_runner)) {}

  RefPtr<scheduler::FakeWebTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockTimer);
};

TEST_F(ImageQualityControllerTest, LowQualityFilterForResizingImage) {
  MockTimer* mock_timer =
      MockTimer::Create(Controller(),
                        &ImageQualityController::HighQualityRepaintTimerFired)
          .release();
  Controller()->SetTimer(WTF::WrapUnique(mock_timer));
  SetBodyInnerHTML("<img src='myimage'></img>");
  LayoutImage* img =
      ToLayoutImage(GetDocument().body()->FirstChild()->GetLayoutObject());

  RefPtr<TestImageLowQuality> test_image = AdoptRef(new TestImageLowQuality);
  std::unique_ptr<PaintController> paint_controller = PaintController::Create();
  GraphicsContext context(*paint_controller);

  // Paint once. This will kick off a timer to see if we resize it during that
  // timer's execution.
  EXPECT_EQ(kInterpolationMedium,
            Controller()->ChooseInterpolationQuality(
                *img, test_image.Get(), test_image.Get(), LayoutSize(2, 2)));

  // Go into low-quality mode now that the size changed.
  EXPECT_EQ(kInterpolationLow,
            Controller()->ChooseInterpolationQuality(
                *img, test_image.Get(), test_image.Get(), LayoutSize(3, 3)));

  // Stay in low-quality mode since the size changed again.
  EXPECT_EQ(kInterpolationLow,
            Controller()->ChooseInterpolationQuality(
                *img, test_image.Get(), test_image.Get(), LayoutSize(4, 4)));

  mock_timer->Fire();
  // The timer fired before painting at another size, so this doesn't count as
  // animation. Therefore not painting at low quality.
  EXPECT_EQ(kInterpolationMedium,
            Controller()->ChooseInterpolationQuality(
                *img, test_image.Get(), test_image.Get(), LayoutSize(4, 4)));
}

TEST_F(ImageQualityControllerTest,
       MediumQualityFilterForNotAnimatedWhileAnotherAnimates) {
  MockTimer* mock_timer =
      MockTimer::Create(Controller(),
                        &ImageQualityController::HighQualityRepaintTimerFired)
          .release();
  Controller()->SetTimer(WTF::WrapUnique(mock_timer));
  SetBodyInnerHTML(
      "<img id='myAnimatingImage' src='myimage'></img> <img "
      "id='myNonAnimatingImage' src='myimage2'></img>");
  LayoutImage* animating_image = ToLayoutImage(
      GetDocument().GetElementById("myAnimatingImage")->GetLayoutObject());
  LayoutImage* non_animating_image = ToLayoutImage(
      GetDocument().GetElementById("myNonAnimatingImage")->GetLayoutObject());

  RefPtr<TestImageLowQuality> test_image = AdoptRef(new TestImageLowQuality);
  std::unique_ptr<PaintController> paint_controller = PaintController::Create();
  GraphicsContext context(*paint_controller);

  // Paint once. This will kick off a timer to see if we resize it during that
  // timer's execution.
  EXPECT_EQ(kInterpolationMedium, Controller()->ChooseInterpolationQuality(
                                      *animating_image, test_image.Get(),
                                      test_image.Get(), LayoutSize(2, 2)));

  // Go into low-quality mode now that the size changed.
  EXPECT_EQ(kInterpolationLow, Controller()->ChooseInterpolationQuality(
                                   *animating_image, test_image.Get(),
                                   test_image.Get(), LayoutSize(3, 3)));

  // The non-animating image receives a medium-quality filter, even though the
  // other one is animating.
  EXPECT_EQ(kInterpolationMedium, Controller()->ChooseInterpolationQuality(
                                      *non_animating_image, test_image.Get(),
                                      test_image.Get(), LayoutSize(4, 4)));

  // Now the second image has animated, so it also gets painted with a
  // low-quality filter.
  EXPECT_EQ(kInterpolationLow, Controller()->ChooseInterpolationQuality(
                                   *non_animating_image, test_image.Get(),
                                   test_image.Get(), LayoutSize(3, 3)));

  mock_timer->Fire();
  // The timer fired before painting at another size, so this doesn't count as
  // animation. Therefore not painting at low quality for any image.
  EXPECT_EQ(kInterpolationMedium, Controller()->ChooseInterpolationQuality(
                                      *animating_image, test_image.Get(),
                                      test_image.Get(), LayoutSize(4, 4)));
  EXPECT_EQ(kInterpolationMedium, Controller()->ChooseInterpolationQuality(
                                      *non_animating_image, test_image.Get(),
                                      test_image.Get(), LayoutSize(4, 4)));
}

TEST_F(ImageQualityControllerTest,
       DontKickTheAnimationTimerWhenPaintingAtTheSameSize) {
  MockTimer* mock_timer =
      MockTimer::Create(Controller(),
                        &ImageQualityController::HighQualityRepaintTimerFired)
          .release();
  Controller()->SetTimer(WTF::WrapUnique(mock_timer));
  SetBodyInnerHTML("<img src='myimage'></img>");
  LayoutImage* img =
      ToLayoutImage(GetDocument().body()->FirstChild()->GetLayoutObject());

  RefPtr<TestImageLowQuality> test_image = AdoptRef(new TestImageLowQuality);

  // Paint once. This will kick off a timer to see if we resize it during that
  // timer's execution.
  EXPECT_EQ(kInterpolationMedium,
            Controller()->ChooseInterpolationQuality(
                *img, test_image.Get(), test_image.Get(), LayoutSize(2, 2)));

  // Go into low-quality mode now that the size changed.
  EXPECT_EQ(kInterpolationLow,
            Controller()->ChooseInterpolationQuality(
                *img, test_image.Get(), test_image.Get(), LayoutSize(3, 3)));

  // Stay in low-quality mode since the size changed again.
  EXPECT_EQ(kInterpolationLow,
            Controller()->ChooseInterpolationQuality(
                *img, test_image.Get(), test_image.Get(), LayoutSize(4, 4)));

  mock_timer->Stop();
  EXPECT_FALSE(mock_timer->IsActive());
  // Painted at the same size, so even though timer is still executing, don't go
  // to low quality.
  EXPECT_EQ(kInterpolationLow,
            Controller()->ChooseInterpolationQuality(
                *img, test_image.Get(), test_image.Get(), LayoutSize(4, 4)));
  // Check that the timer was not kicked. It should not have been, since the
  // image was painted at the same size as last time.
  EXPECT_FALSE(mock_timer->IsActive());
}

TEST_F(ImageQualityControllerTest, DontRestartTimerUnlessAdvanced) {
  MockTimer* mock_timer =
      MockTimer::Create(Controller(),
                        &ImageQualityController::HighQualityRepaintTimerFired)
          .release();
  Controller()->SetTimer(WTF::WrapUnique(mock_timer));
  SetBodyInnerHTML("<img src='myimage'></img>");
  LayoutImage* img =
      ToLayoutImage(GetDocument().body()->FirstChild()->GetLayoutObject());

  RefPtr<TestImageLowQuality> test_image = AdoptRef(new TestImageLowQuality);

  // Paint once. This will kick off a timer to see if we resize it during that
  // timer's execution.
  mock_timer->SetTime(0.1);
  EXPECT_FALSE(Controller()->ShouldPaintAtLowQuality(
      *img, test_image.Get(), test_image.Get(), LayoutSize(2, 2), 0.1));
  EXPECT_EQ(ImageQualityController::kCLowQualityTimeThreshold,
            mock_timer->NextFireInterval());

  // Go into low-quality mode now that the size changed.
  double next_time =
      0.1 + ImageQualityController::kCTimerRestartThreshold / 2.0;
  mock_timer->SetTime(next_time);
  EXPECT_EQ(true, Controller()->ShouldPaintAtLowQuality(
                      *img, test_image.Get(), test_image.Get(),
                      LayoutSize(3, 3), next_time));
  // The fire interval has decreased, because we have not restarted the timer.
  EXPECT_EQ(ImageQualityController::kCLowQualityTimeThreshold -
                ImageQualityController::kCTimerRestartThreshold / 2.0,
            mock_timer->NextFireInterval());

  // This animation is far enough in the future to make the timer restart, since
  // it is half over.
  next_time = 0.1 + ImageQualityController::kCTimerRestartThreshold + 0.01;
  EXPECT_EQ(true, Controller()->ShouldPaintAtLowQuality(
                      *img, test_image.Get(), test_image.Get(),
                      LayoutSize(4, 4), next_time));
  // Now the timer has restarted, leading to a larger fire interval.
  EXPECT_EQ(ImageQualityController::kCLowQualityTimeThreshold,
            mock_timer->NextFireInterval());
}

#endif

}  // namespace blink
