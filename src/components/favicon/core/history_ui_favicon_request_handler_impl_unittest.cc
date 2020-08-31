// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/history_ui_favicon_request_handler_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"

namespace favicon {
namespace {

using testing::_;
using testing::Return;

// TODO(victorvianna): Rename to kPageUrl and kIconUrl.
const char kDummyPageUrl[] = "https://www.example.com";
const char kDummyIconUrl[] = "https://www.example.com/favicon16.png";
const HistoryUiFaviconRequestOrigin kDummyOrigin =
    HistoryUiFaviconRequestOrigin::kHistory;
const char kDummyOriginHistogramSuffix[] = ".HISTORY";
base::CancelableTaskTracker::TaskId kDummyTaskId = 1;
const char kAvailabilityHistogramName[] =
    "Sync.SyncedHistoryFaviconAvailability";
const char kLatencyHistogramName[] = "Sync.SyncedHistoryFaviconLatency";
const int kDefaultDesiredSizeInPixel = 16;
const SkColor kTestColor = SK_ColorRED;

SkBitmap CreateTestSkBitmap(int desired_size_in_pixel) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(desired_size_in_pixel, desired_size_in_pixel);
  bitmap.eraseColor(kTestColor);
  return bitmap;
}

favicon_base::FaviconRawBitmapResult CreateTestBitmapResult(
    int desired_size_in_pixel) {
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  gfx::PNGCodec::EncodeBGRASkBitmap(CreateTestSkBitmap(desired_size_in_pixel),
                                    false, &data->data());
  favicon_base::FaviconRawBitmapResult result;
  result.bitmap_data = data;
  result.icon_url = GURL(kDummyIconUrl);
  result.pixel_size = gfx::Size(desired_size_in_pixel, desired_size_in_pixel);
  return result;
}

favicon_base::FaviconRawBitmapResult CreateTestBitmapResult() {
  return CreateTestBitmapResult(kDefaultDesiredSizeInPixel);
}

favicon_base::FaviconImageResult CreateTestImageResult() {
  favicon_base::FaviconImageResult result;
  result.image = gfx::Image::CreateFrom1xBitmap(
      CreateTestSkBitmap(kDefaultDesiredSizeInPixel));
  result.icon_url = GURL(kDummyIconUrl);
  return result;
}

void StoreBitmap(favicon_base::FaviconRawBitmapResult* destination,
                 const favicon_base::FaviconRawBitmapResult& result) {
  *destination = result;
}

void StoreImage(favicon_base::FaviconImageResult* destination,
                const favicon_base::FaviconImageResult& result) {
  *destination = result;
}

class MockFaviconServiceWithFake : public MockFaviconService {
 public:
  MockFaviconServiceWithFake() {
    // Fake won't respond with any icons at first.
    ON_CALL(*this, GetRawFaviconForPageURL(_, _, _, _, _, _))
        .WillByDefault([](auto, auto, auto, auto,
                          favicon_base::FaviconRawBitmapCallback callback,
                          auto) {
          std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
          return kDummyTaskId;
        });
    ON_CALL(*this, GetFaviconImageForPageURL(_, _, _))
        .WillByDefault(
            [](auto, favicon_base::FaviconImageCallback callback, auto) {
              std::move(callback).Run(favicon_base::FaviconImageResult());
              return kDummyTaskId;
            });
  }

  ~MockFaviconServiceWithFake() override = default;

  // Will make the object start responding with a valid favicon for |page_url|.
  void StoreMockLocalFavicon(const GURL& page_url) {
    ON_CALL(*this, GetRawFaviconForPageURL(GURL(page_url), _,
                                           kDefaultDesiredSizeInPixel, _, _, _))
        .WillByDefault([](auto, auto, auto, auto,
                          favicon_base::FaviconRawBitmapCallback callback,
                          auto) {
          std::move(callback).Run(CreateTestBitmapResult());
          return kDummyTaskId;
        });
    ON_CALL(*this, GetFaviconImageForPageURL(GURL(page_url), _, _))
        .WillByDefault(
            [](auto, favicon_base::FaviconImageCallback callback, auto) {
              std::move(callback).Run(CreateTestImageResult());
              return kDummyTaskId;
            });
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFaviconServiceWithFake);
};

class MockLargeIconServiceWithFake : public LargeIconService {
 public:
  explicit MockLargeIconServiceWithFake(
      MockFaviconServiceWithFake* mock_favicon_service_with_fake)
      : mock_favicon_service_with_fake_(mock_favicon_service_with_fake) {
    // Fake won't respond with any icons at first.
    ON_CALL(*this,
            GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                _, _, _, _, _))
        .WillByDefault(
            [](auto, auto, auto, auto,
               favicon_base::GoogleFaviconServerCallback server_callback) {
              std::move(server_callback)
                  .Run(favicon_base::GoogleFaviconServerRequestStatus::
                           FAILURE_HTTP_ERROR);
            });
  }

  ~MockLargeIconServiceWithFake() override = default;

  MOCK_METHOD5(GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache,
               void(const GURL& page_url,
                    bool may_page_url_be_private,
                    bool should_trim_page_url_path,
                    const net::NetworkTrafficAnnotationTag& traffic_annotation,
                    favicon_base::GoogleFaviconServerCallback callback));

  MOCK_METHOD5(GetLargeIconRawBitmapOrFallbackStyleForPageUrl,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   int min_source_size_in_pixel,
                   int desired_size_in_pixel,
                   favicon_base::LargeIconCallback callback,
                   base::CancelableTaskTracker* tracker));

  MOCK_METHOD5(GetLargeIconImageOrFallbackStyleForPageUrl,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   int min_source_size_in_pixel,
                   int desired_size_in_pixel,
                   favicon_base::LargeIconImageCallback callback,
                   base::CancelableTaskTracker* tracker));

  MOCK_METHOD5(GetLargeIconRawBitmapOrFallbackStyleForIconUrl,
               base::CancelableTaskTracker::TaskId(
                   const GURL& icon_url,
                   int min_source_size_in_pixel,
                   int desired_size_in_pixel,
                   favicon_base::LargeIconCallback callback,
                   base::CancelableTaskTracker* tracker));

  MOCK_METHOD4(GetIconRawBitmapOrFallbackStyleForPageUrl,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   int desired_size_in_pixel,
                   favicon_base::LargeIconCallback callback,
                   base::CancelableTaskTracker* tracker));

  MOCK_METHOD1(TouchIconFromGoogleServer, void(const GURL& icon_url));

  // Will make the object respond by storing a valid local favicon for
  // |page_url| in |mock_favicon_server_with_fake_|.
  void StoreMockGoogleServerFavicon(const GURL& page_url) {
    ON_CALL(*this,
            GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                _, _, _, _, _))
        .WillByDefault(
            [=](auto, auto, auto, auto,
                favicon_base::GoogleFaviconServerCallback server_callback) {
              mock_favicon_service_with_fake_->StoreMockLocalFavicon(page_url);
              std::move(server_callback)
                  .Run(favicon_base::GoogleFaviconServerRequestStatus::SUCCESS);
            });
  }

 private:
  MockFaviconServiceWithFake* const mock_favicon_service_with_fake_;

  DISALLOW_COPY_AND_ASSIGN(MockLargeIconServiceWithFake);
};

class HistoryUiFaviconRequestHandlerImplTest : public ::testing::Test {
 public:
  HistoryUiFaviconRequestHandlerImplTest()
      : mock_large_icon_service_(&mock_favicon_service_),
        history_ui_favicon_request_handler_(can_send_history_data_getter_.Get(),
                                            &mock_favicon_service_,
                                            &mock_large_icon_service_) {
    // Allow sending history data by default.
    ON_CALL(can_send_history_data_getter_, Run()).WillByDefault(Return(true));
  }

 protected:
  testing::NiceMock<MockFaviconServiceWithFake> mock_favicon_service_;
  testing::NiceMock<MockLargeIconServiceWithFake> mock_large_icon_service_;
  testing::NiceMock<base::MockCallback<
      HistoryUiFaviconRequestHandlerImpl::CanSendHistoryDataGetter>>
      can_send_history_data_getter_;
  base::HistogramTester histogram_tester_;
  HistoryUiFaviconRequestHandlerImpl history_ui_favicon_request_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryUiFaviconRequestHandlerImplTest);
};

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetEmptyBitmap) {
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                                      kDefaultDesiredSizeInPixel, _, _, _));
  favicon_base::FaviconRawBitmapResult result;
  history_ui_favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDefaultDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), kDummyOrigin);
  EXPECT_FALSE(result.is_valid());
  histogram_tester_.ExpectUniqueSample(
      std::string(kAvailabilityHistogramName) + kDummyOriginHistogramSuffix,
      FaviconAvailability::kNotAvailable, 1);
  histogram_tester_.ExpectTotalCount(
      std::string(kLatencyHistogramName) + kDummyOriginHistogramSuffix, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetLocalBitmap) {
  mock_favicon_service_.StoreMockLocalFavicon(GURL(kDummyPageUrl));
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                                      kDefaultDesiredSizeInPixel, _, _, _));
  EXPECT_CALL(mock_large_icon_service_,
              TouchIconFromGoogleServer(GURL(kDummyIconUrl)));
  favicon_base::FaviconRawBitmapResult result;
  history_ui_favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDefaultDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), kDummyOrigin);
  EXPECT_TRUE(result.is_valid());
  histogram_tester_.ExpectUniqueSample(
      std::string(kAvailabilityHistogramName) + kDummyOriginHistogramSuffix,
      FaviconAvailability::kLocal, 1);
  histogram_tester_.ExpectTotalCount(
      std::string(kLatencyHistogramName) + kDummyOriginHistogramSuffix, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetGoogleServerBitmap) {
  mock_large_icon_service_.StoreMockGoogleServerFavicon(GURL(kDummyPageUrl));
  EXPECT_CALL(can_send_history_data_getter_, Run());
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                                      kDefaultDesiredSizeInPixel, _, _, _))
      .Times(2);
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  GURL(kDummyPageUrl), _,
                  /*should_trim_url_path=*/false, _, _));
  favicon_base::FaviconRawBitmapResult result;
  history_ui_favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDefaultDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), kDummyOrigin);
  EXPECT_TRUE(result.is_valid());
  histogram_tester_.ExpectUniqueSample(
      std::string(kAvailabilityHistogramName) + kDummyOriginHistogramSuffix,
      FaviconAvailability::kLocal, 1);
  histogram_tester_.ExpectTotalCount(
      std::string(kLatencyHistogramName) + kDummyOriginHistogramSuffix, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetEmptyImage) {
  EXPECT_CALL(mock_favicon_service_,
              GetFaviconImageForPageURL(GURL(kDummyPageUrl), _, _));
  favicon_base::FaviconImageResult result;
  history_ui_favicon_request_handler_.GetFaviconImageForPageURL(
      GURL(kDummyPageUrl), base::BindOnce(&StoreImage, &result), kDummyOrigin);
  EXPECT_TRUE(result.image.IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      std::string(kAvailabilityHistogramName) + kDummyOriginHistogramSuffix,
      FaviconAvailability::kNotAvailable, 1);
  histogram_tester_.ExpectTotalCount(
      std::string(kLatencyHistogramName) + kDummyOriginHistogramSuffix, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetLocalImage) {
  mock_favicon_service_.StoreMockLocalFavicon(GURL(kDummyPageUrl));
  EXPECT_CALL(mock_favicon_service_,
              GetFaviconImageForPageURL(GURL(kDummyPageUrl), _, _));
  EXPECT_CALL(mock_large_icon_service_,
              TouchIconFromGoogleServer(GURL(kDummyIconUrl)));
  favicon_base::FaviconImageResult result;
  history_ui_favicon_request_handler_.GetFaviconImageForPageURL(
      GURL(kDummyPageUrl), base::BindOnce(&StoreImage, &result), kDummyOrigin);
  EXPECT_FALSE(result.image.IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      std::string(kAvailabilityHistogramName) + kDummyOriginHistogramSuffix,
      FaviconAvailability::kLocal, 1);
  histogram_tester_.ExpectTotalCount(
      std::string(kLatencyHistogramName) + kDummyOriginHistogramSuffix, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetGoogleServerImage) {
  mock_large_icon_service_.StoreMockGoogleServerFavicon(GURL(kDummyPageUrl));
  EXPECT_CALL(can_send_history_data_getter_, Run());
  EXPECT_CALL(mock_favicon_service_,
              GetFaviconImageForPageURL(GURL(kDummyPageUrl), _, _))
      .Times(2);
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  GURL(kDummyPageUrl), _,
                  /*should_trim_url_path=*/false, _, _));
  favicon_base::FaviconImageResult result;
  history_ui_favicon_request_handler_.GetFaviconImageForPageURL(
      GURL(kDummyPageUrl), base::BindOnce(&StoreImage, &result), kDummyOrigin);
  EXPECT_FALSE(result.image.IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      std::string(kAvailabilityHistogramName) + kDummyOriginHistogramSuffix,
      FaviconAvailability::kLocal, 1);
  histogram_tester_.ExpectTotalCount(
      std::string(kLatencyHistogramName) + kDummyOriginHistogramSuffix, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest,
       ShouldNotQueryGoogleServerIfCannotSendData) {
  EXPECT_CALL(can_send_history_data_getter_, Run()).WillOnce([]() {
    return false;
  });
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                                      kDefaultDesiredSizeInPixel, _, _, _))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  _, _, _, _, _))
      .Times(0);
  favicon_base::FaviconRawBitmapResult result;
  history_ui_favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDefaultDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), kDummyOrigin);
}

}  // namespace
}  // namespace favicon
