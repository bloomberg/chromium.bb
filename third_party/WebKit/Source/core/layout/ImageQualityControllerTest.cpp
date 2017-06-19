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

double FrameTime(const LayoutObject* obj) {
  return obj->GetFrameView()
      ->GetPage()
      ->GetChromeClient()
      .LastFrameTimeMonotonic();
}

const Settings* GetSettings(const LayoutObject* obj) {
  return obj->GetFrame() ? obj->GetFrame()->GetSettings() : nullptr;
}

TEST_F(ImageQualityControllerTest, RegularImage) {
  SetBodyInnerHTML("<img src='myimage'></img>");
  LayoutObject* obj = GetDocument().body()->firstChild()->GetLayoutObject();

  EXPECT_EQ(kInterpolationDefault,
            Controller()->ChooseInterpolationQuality(
                *obj, obj->StyleRef(), GetSettings(obj), nullptr, nullptr,
                LayoutSize(), FrameTime(obj)));
}

TEST_F(ImageQualityControllerTest, ImageRenderingPixelated) {
  SetBodyInnerHTML(
      "<img src='myimage' style='image-rendering: pixelated'></img>");
  LayoutObject* obj = GetDocument().body()->firstChild()->GetLayoutObject();

  EXPECT_EQ(kInterpolationNone,
            Controller()->ChooseInterpolationQuality(
                *obj, obj->StyleRef(), GetSettings(obj), nullptr, nullptr,
                LayoutSize(), FrameTime(obj)));
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
      ToLayoutImage(GetDocument().body()->firstChild()->GetLayoutObject());

  RefPtr<TestImageAnimated> test_image = AdoptRef(new TestImageAnimated);
  EXPECT_EQ(kInterpolationMedium,
            Controller()->ChooseInterpolationQuality(
                *img, img->StyleRef(), GetSettings(img), test_image.Get(),
                nullptr, LayoutSize(), FrameTime(img)));
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
      ToLayoutImage(GetDocument().body()->firstChild()->GetLayoutObject());

  RefPtr<TestImageWithContrast> test_image =
      AdoptRef(new TestImageWithContrast);
  EXPECT_EQ(kInterpolationLow,
            Controller()->ChooseInterpolationQuality(
                *img, img->StyleRef(), GetSettings(img), test_image.Get(),
                test_image.Get(), LayoutSize(), FrameTime(img)));
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
      ToLayoutImage(GetDocument().body()->firstChild()->GetLayoutObject());

  RefPtr<TestImageLowQuality> test_image = AdoptRef(new TestImageLowQuality);
  EXPECT_EQ(kInterpolationMedium,
            Controller()->ChooseInterpolationQuality(
                *img, img->StyleRef(), GetSettings(img), test_image.Get(),
                test_image.Get(), LayoutSize(1, 1), FrameTime(img)));
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
      ToLayoutImage(GetDocument().body()->firstChild()->GetLayoutObject());

  RefPtr<TestImageLowQuality> test_image = AdoptRef(new TestImageLowQuality);
  std::unique_ptr<PaintController> paint_controller = PaintController::Create();
  GraphicsContext context(*paint_controller);

  // Paint once. This will kick off a timer to see if we resize it during that
  // timer's execution.
  EXPECT_EQ(kInterpolationMedium,
            Controller()->ChooseInterpolationQuality(
                *img, img->StyleRef(), GetSettings(img), test_image.Get(),
                test_image.Get(), LayoutSize(2, 2), FrameTime(img)));

  // Go into low-quality mode now that the size changed.
  EXPECT_EQ(kInterpolationLow,
            Controller()->ChooseInterpolationQuality(
                *img, img->StyleRef(), GetSettings(img), test_image.Get(),
                test_image.Get(), LayoutSize(3, 3), FrameTime(img)));

  // Stay in low-quality mode since the size changed again.
  EXPECT_EQ(kInterpolationLow,
            Controller()->ChooseInterpolationQuality(
                *img, img->StyleRef(), GetSettings(img), test_image.Get(),
                test_image.Get(), LayoutSize(4, 4), FrameTime(img)));

  mock_timer->Fire();
  // The timer fired before painting at another size, so this doesn't count as
  // animation. Therefore not painting at low quality.
  EXPECT_EQ(kInterpolationMedium,
            Controller()->ChooseInterpolationQuality(
                *img, img->StyleRef(), GetSettings(img), test_image.Get(),
                test_image.Get(), LayoutSize(4, 4), FrameTime(img)));
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
      GetDocument().getElementById("myAnimatingImage")->GetLayoutObject());
  LayoutImage* non_animating_image = ToLayoutImage(
      GetDocument().getElementById("myNonAnimatingImage")->GetLayoutObject());

  RefPtr<TestImageLowQuality> test_image = AdoptRef(new TestImageLowQuality);
  std::unique_ptr<PaintController> paint_controller = PaintController::Create();
  GraphicsContext context(*paint_controller);

  // Paint once. This will kick off a timer to see if we resize it during that
  // timer's execution.
  EXPECT_EQ(
      kInterpolationMedium,
      Controller()->ChooseInterpolationQuality(
          *animating_image, animating_image->StyleRef(),
          GetSettings(animating_image), test_image.Get(), test_image.Get(),
          LayoutSize(2, 2), FrameTime(animating_image)));

  // Go into low-quality mode now that the size changed.
  EXPECT_EQ(
      kInterpolationLow,
      Controller()->ChooseInterpolationQuality(
          *animating_image, animating_image->StyleRef(),
          GetSettings(animating_image), test_image.Get(), test_image.Get(),
          LayoutSize(3, 3), FrameTime(animating_image)));

  // The non-animating image receives a medium-quality filter, even though the
  // other one is animating.
  EXPECT_EQ(
      kInterpolationMedium,
      Controller()->ChooseInterpolationQuality(
          *non_animating_image, non_animating_image->StyleRef(),
          GetSettings(non_animating_image), test_image.Get(), test_image.Get(),
          LayoutSize(4, 4), FrameTime(non_animating_image)));

  // Now the second image has animated, so it also gets painted with a
  // low-quality filter.
  EXPECT_EQ(
      kInterpolationLow,
      Controller()->ChooseInterpolationQuality(
          *non_animating_image, non_animating_image->StyleRef(),
          GetSettings(non_animating_image), test_image.Get(), test_image.Get(),
          LayoutSize(3, 3), FrameTime(non_animating_image)));

  mock_timer->Fire();
  // The timer fired before painting at another size, so this doesn't count as
  // animation. Therefore not painting at low quality for any image.
  EXPECT_EQ(
      kInterpolationMedium,
      Controller()->ChooseInterpolationQuality(
          *animating_image, animating_image->StyleRef(),
          GetSettings(animating_image), test_image.Get(), test_image.Get(),
          LayoutSize(4, 4), FrameTime(animating_image)));
  EXPECT_EQ(
      kInterpolationMedium,
      Controller()->ChooseInterpolationQuality(
          *non_animating_image, non_animating_image->StyleRef(),
          GetSettings(non_animating_image), test_image.Get(), test_image.Get(),
          LayoutSize(4, 4), FrameTime(non_animating_image)));
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
      ToLayoutImage(GetDocument().body()->firstChild()->GetLayoutObject());

  RefPtr<TestImageLowQuality> test_image = AdoptRef(new TestImageLowQuality);

  // Paint once. This will kick off a timer to see if we resize it during that
  // timer's execution.
  EXPECT_EQ(kInterpolationMedium,
            Controller()->ChooseInterpolationQuality(
                *img, img->StyleRef(), GetSettings(img), test_image.Get(),
                test_image.Get(), LayoutSize(2, 2), FrameTime(img)));

  // Go into low-quality mode now that the size changed.
  EXPECT_EQ(kInterpolationLow,
            Controller()->ChooseInterpolationQuality(
                *img, img->StyleRef(), GetSettings(img), test_image.Get(),
                test_image.Get(), LayoutSize(3, 3), FrameTime(img)));

  // Stay in low-quality mode since the size changed again.
  EXPECT_EQ(kInterpolationLow,
            Controller()->ChooseInterpolationQuality(
                *img, img->StyleRef(), GetSettings(img), test_image.Get(),
                test_image.Get(), LayoutSize(4, 4), FrameTime(img)));

  mock_timer->Stop();
  EXPECT_FALSE(mock_timer->IsActive());
  // Painted at the same size, so even though timer is still executing, don't go
  // to low quality.
  EXPECT_EQ(kInterpolationLow,
            Controller()->ChooseInterpolationQuality(
                *img, img->StyleRef(), GetSettings(img), test_image.Get(),
                test_image.Get(), LayoutSize(4, 4), FrameTime(img)));
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
      ToLayoutImage(GetDocument().body()->firstChild()->GetLayoutObject());

  RefPtr<TestImageLowQuality> test_image = AdoptRef(new TestImageLowQuality);

  // Paint once. This will kick off a timer to see if we resize it during that
  // timer's execution.
  mock_timer->SetTime(0.1);
  EXPECT_FALSE(Controller()->ShouldPaintAtLowQuality(
      *img, img->StyleRef(), GetSettings(img), test_image.Get(),
      test_image.Get(), LayoutSize(2, 2), 0.1));
  EXPECT_EQ(ImageQualityController::kCLowQualityTimeThreshold,
            mock_timer->NextFireInterval());

  // Go into low-quality mode now that the size changed.
  double next_time =
      0.1 + ImageQualityController::kCTimerRestartThreshold / 2.0;
  mock_timer->SetTime(next_time);
  EXPECT_EQ(true, Controller()->ShouldPaintAtLowQuality(
                      *img, img->StyleRef(), GetSettings(img), test_image.Get(),
                      test_image.Get(), LayoutSize(3, 3), next_time));
  // The fire interval has decreased, because we have not restarted the timer.
  EXPECT_EQ(ImageQualityController::kCLowQualityTimeThreshold -
                ImageQualityController::kCTimerRestartThreshold / 2.0,
            mock_timer->NextFireInterval());

  // This animation is far enough in the future to make the timer restart, since
  // it is half over.
  next_time = 0.1 + ImageQualityController::kCTimerRestartThreshold + 0.01;
  EXPECT_EQ(true, Controller()->ShouldPaintAtLowQuality(
                      *img, img->StyleRef(), GetSettings(img), test_image.Get(),
                      test_image.Get(), LayoutSize(4, 4), next_time));
  // Now the timer has restarted, leading to a larger fire interval.
  EXPECT_EQ(ImageQualityController::kCLowQualityTimeThreshold,
            mock_timer->NextFireInterval());
}

#endif

}  // namespace blink
