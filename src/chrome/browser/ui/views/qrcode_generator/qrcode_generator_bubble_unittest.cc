// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_bubble.h"

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_observer.h"
#include "url/gurl.h"

namespace qrcode_generator {

namespace {

using QRCodeGeneratorBubbleTest = testing::Test;

TEST_F(QRCodeGeneratorBubbleTest, SuggestedDownloadURLNoIP) {
  EXPECT_EQ(QRCodeGeneratorBubble::GetQRCodeFilenameForURL(GURL("10.1.2.3")),
            u"qrcode_chrome.png");

  EXPECT_EQ(QRCodeGeneratorBubble::GetQRCodeFilenameForURL(
                GURL("https://chromium.org")),
            u"qrcode_chromium.org.png");

  EXPECT_EQ(
      QRCodeGeneratorBubble::GetQRCodeFilenameForURL(GURL("text, not url")),
      u"qrcode_chrome.png");
}

TEST_F(QRCodeGeneratorBubbleTest, GeneratedCodeHasQuietZone) {
  const int kBaseSizeDip = 16;
  const int kQuietZoneTiles = 4;
  const int kTileToDip = 2;
  const int kQuietZoneDip = kQuietZoneTiles * kTileToDip;

  SkBitmap base_bitmap;
  base_bitmap.allocN32Pixels(kBaseSizeDip, kBaseSizeDip);
  base_bitmap.eraseColor(SK_ColorRED);
  auto base_image = gfx::ImageSkia::CreateFrom1xBitmap(base_bitmap);

  auto image = QRCodeGeneratorBubble::AddQRCodeQuietZone(
      base_image,
      gfx::Size(kBaseSizeDip / kTileToDip, kBaseSizeDip / kTileToDip));

  EXPECT_EQ(base_image.width(), kBaseSizeDip);
  EXPECT_EQ(base_image.height(), kBaseSizeDip);
  EXPECT_EQ(image.width(), kBaseSizeDip + kQuietZoneDip * 2);
  EXPECT_EQ(image.height(), kBaseSizeDip + kQuietZoneDip * 2);

  EXPECT_EQ(SK_ColorRED, base_image.bitmap()->getColor(0, 0));

  EXPECT_EQ(SK_ColorWHITE, image.bitmap()->getColor(0, 0));
  EXPECT_EQ(SK_ColorWHITE,
            image.bitmap()->getColor(kQuietZoneDip, kQuietZoneDip - 1));
  EXPECT_EQ(SK_ColorWHITE,
            image.bitmap()->getColor(kQuietZoneDip - 1, kQuietZoneDip));
  EXPECT_EQ(SK_ColorRED,
            image.bitmap()->getColor(kQuietZoneDip, kQuietZoneDip));
}

// Test-fake implementation of QRCodeGeneratorService; the real implementation
// can't be used in these tests because it requires spawning a service process.
class FakeQRCodeGeneratorService : public mojom::QRCodeGeneratorService {
 public:
  FakeQRCodeGeneratorService() = default;
  void GenerateQRCode(mojom::GenerateQRCodeRequestPtr request,
                      GenerateQRCodeCallback callback) override {
    pending_callback_ = std::move(callback);
    if (run_loop_)
      run_loop_->Quit();
  }

  void WaitForRequest() {
    if (HasPendingRequest())
      return;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  bool HasPendingRequest() const { return bool(pending_callback_); }

  void DeliverResponse(mojom::GenerateQRCodeResponsePtr response) {
    CHECK(pending_callback_);
    std::move(pending_callback_).Run(std::move(response));
  }

 private:
  GenerateQRCodeCallback pending_callback_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class ViewVisibilityWaiter : public views::ViewObserver {
 public:
  explicit ViewVisibilityWaiter(views::View* view) {
    observation_.Observe(view);
  }

  void Wait() { run_loop_.Run(); }

  void OnViewVisibilityChanged(views::View* view,
                               views::View* starting_view) override {
    run_loop_.Quit();
  }

 private:
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
  base::RunLoop run_loop_;
};

class QRCodeGeneratorBubbleUITest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    anchor_widget_.reset(CreateTestWidget().release());
    anchor_view_ =
        anchor_widget_->SetContentsView(std::make_unique<views::View>());
    CHECK(anchor_view_);
    auto bubble = std::make_unique<QRCodeGeneratorBubble>(
        anchor_view_, nullptr, base::DoNothing(), base::DoNothing(),
        GURL("https://www.chromium.org/a"));
    bubble->SetQRCodeServiceForTesting(
        mojo::Remote<mojom::QRCodeGeneratorService>(
            receiver_.BindNewPipeAndPassRemote()));
    bubble_ = bubble.get();
    bubble_widget_.reset(
        views::BubbleDialogDelegateView::CreateBubble(std::move(bubble)));
  }

  void TearDown() override {
    bubble_widget_.reset();
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  QRCodeGeneratorBubble* bubble() { return bubble_; }
  views::ImageView* image() { return bubble_->image_for_testing(); }
  views::Textfield* textfield() { return bubble_->textfield_for_testing(); }

  FakeQRCodeGeneratorService* service() { return &fake_service_; }

 private:
  WidgetAutoclosePtr anchor_widget_;
  raw_ptr<views::View> anchor_view_;
  raw_ptr<QRCodeGeneratorBubble> bubble_;
  WidgetAutoclosePtr bubble_widget_;

  FakeQRCodeGeneratorService fake_service_;
  mojo::Receiver<mojom::QRCodeGeneratorService> receiver_{&fake_service_};
};

// This test is a bit fiddly because mojo imposes asynchronicity on both sender
// and receiver sides of a service. That means that:
// 1. We need to wait for QR code generation requests to arrive at the service
// 2. We need to wait for observable changes to the ImageView to know when
//    responses have been delivered
TEST_F(QRCodeGeneratorBubbleUITest, ImageShowsAfterErrorState) {
  bubble()->Show();

  auto image_showing = [&]() {
    return image()->GetVisible() && image()->GetPreferredSize().height() > 0 &&
           image()->GetPreferredSize().width() > 0;
  };

  EXPECT_TRUE(image_showing());

  service()->WaitForRequest();
  ASSERT_TRUE(service()->HasPendingRequest());
  auto error_response = mojom::GenerateQRCodeResponse::New();
  error_response->error_code = mojom::QRCodeGeneratorError::UNKNOWN_ERROR;

  EXPECT_TRUE(image_showing());

  {
    ViewVisibilityWaiter waiter(image());
    service()->DeliverResponse(std::move(error_response));
    waiter.Wait();

    EXPECT_FALSE(image_showing());
  }

  // The UI regenerates the QR code when the user types new text, so synthesize
  // that.
  textfield()->InsertOrReplaceText(u"https://www.chromium.org/b");
  service()->WaitForRequest();

  auto ok_response = mojom::GenerateQRCodeResponse::New();
  ok_response->error_code = mojom::QRCodeGeneratorError::NONE;
  ok_response->bitmap.allocN32Pixels(16, 16);
  ok_response->data.resize(16 * 16);
  ok_response->data_size = gfx::Size(16, 16);

  {
    ViewVisibilityWaiter waiter(image());
    service()->DeliverResponse(std::move(ok_response));
    waiter.Wait();

    EXPECT_TRUE(image_showing());
  }
}

}  // namespace

}  // namespace qrcode_generator
