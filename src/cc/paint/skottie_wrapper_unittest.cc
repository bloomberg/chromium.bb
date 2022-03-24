// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_wrapper.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/skottie_mru_resource_provider.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/test/lottie_test_data.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_skcanvas.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSize.h"

namespace cc {
namespace {

using ::testing::_;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::Mock;
using ::testing::Ne;
using ::testing::NotNull;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

class MockFrameDataCallback {
 public:
  MOCK_METHOD(SkottieWrapper::FrameDataFetchResult,
              OnAssetLoaded,
              (SkottieResourceIdHash asset_id_hash,
               float t,
               sk_sp<SkImage>& image_out,
               SkSamplingOptions& sampling_out));

  SkottieWrapper::FrameDataCallback Get() {
    return base::BindRepeating(&MockFrameDataCallback::OnAssetLoaded,
                               base::Unretained(this));
  }
};

TEST(SkottieWrapperTest, LoadsValidLottieFileNonSerializable) {
  scoped_refptr<SkottieWrapper> skottie =
      SkottieWrapper::CreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()),
          kLottieDataWithoutAssets1.length()));
  EXPECT_TRUE(skottie->is_valid());
}

TEST(SkottieWrapperTest, LoadsValidLottieFileSerializable) {
  scoped_refptr<SkottieWrapper> skottie =
      SkottieWrapper::CreateSerializable(std::vector<uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()),
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()) +
              kLottieDataWithoutAssets1.length()));
  EXPECT_TRUE(skottie->is_valid());
}

TEST(SkottieWrapperTest, DetectsInvalidLottieFile) {
  static constexpr base::StringPiece kInvalidJson = "this is invalid json";
  scoped_refptr<SkottieWrapper> skottie =
      SkottieWrapper::CreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kInvalidJson.data()),
          kInvalidJson.length()));
  EXPECT_FALSE(skottie->is_valid());
}

TEST(SkottieWrapperTest, IdMatchesForSameLottieFile) {
  scoped_refptr<SkottieWrapper> skottie_1 =
      SkottieWrapper::CreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()),
          kLottieDataWithoutAssets1.length()));
  scoped_refptr<SkottieWrapper> skottie_2 =
      SkottieWrapper::CreateSerializable(std::vector<uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()),
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()) +
              kLottieDataWithoutAssets1.length()));
  ASSERT_TRUE(skottie_1->is_valid());
  ASSERT_TRUE(skottie_2->is_valid());
  EXPECT_THAT(skottie_1->id(), Eq(skottie_2->id()));
}

TEST(SkottieWrapperTest, IdDoesNotMatchForDifferentLottieFile) {
  scoped_refptr<SkottieWrapper> skottie_1 =
      SkottieWrapper::CreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()),
          kLottieDataWithoutAssets1.length()));
  scoped_refptr<SkottieWrapper> skottie_2 =
      SkottieWrapper::CreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets2.data()),
          kLottieDataWithoutAssets2.length()));
  ASSERT_TRUE(skottie_1->is_valid());
  ASSERT_TRUE(skottie_2->is_valid());
  EXPECT_THAT(skottie_1->id(), Ne(skottie_2->id()));
}

TEST(SkottieWrapperTest, LoadsImageAssetsMetadata) {
  scoped_refptr<SkottieWrapper> skottie =
      SkottieWrapper::CreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWith2Assets.data()),
          kLottieDataWith2Assets.length()));
  ASSERT_TRUE(skottie->is_valid());
  SkottieResourceMetadataMap metadata = skottie->GetImageAssetMetadata();
  EXPECT_THAT(
      metadata.asset_storage(),
      UnorderedElementsAre(
          Pair("image_0", base::FilePath(FILE_PATH_LITERAL("images/img_0.jpg"))
                              .NormalizePathSeparators()),
          Pair("image_1", base::FilePath(FILE_PATH_LITERAL("images/img_1.jpg"))
                              .NormalizePathSeparators())));
}

TEST(SkottieWrapperTest, LoadsCorrectAssetsForDraw) {
  scoped_refptr<SkottieWrapper> skottie =
      CreateSkottieFromString(kLottieDataWith2Assets);
  ASSERT_TRUE(skottie->is_valid());
  ::testing::NiceMock<MockCanvas> canvas;
  MockFrameDataCallback mock_callback;
  EXPECT_CALL(mock_callback,
              OnAssetLoaded(HashSkottieResourceId("image_0"), _, _, _));
  skottie->Draw(&canvas, /*t=*/0.25, SkRect::MakeWH(500, 500),
                mock_callback.Get());
  Mock::VerifyAndClearExpectations(&mock_callback);

  EXPECT_CALL(mock_callback,
              OnAssetLoaded(HashSkottieResourceId("image_1"), _, _, _));
  skottie->Draw(&canvas, /*t=*/0.75, SkRect::MakeWH(500, 500),
                mock_callback.Get());
  Mock::VerifyAndClearExpectations(&mock_callback);
}

TEST(SkottieWrapperTest, AllowsNullFrameDataCallbackForDraw) {
  scoped_refptr<SkottieWrapper> skottie =
      CreateSkottieFromString(kLottieDataWithoutAssets1);
  ASSERT_TRUE(skottie->is_valid());
  // Just verify that this call does not cause a CHECK failure.
  ::testing::NiceMock<MockCanvas> canvas;
  skottie->Draw(&canvas, /*t=*/0, SkRect::MakeWH(500, 500),
                SkottieWrapper::FrameDataCallback());
}

TEST(SkottieWrapperTest, LoadsCorrectAssetsForSeek) {
  scoped_refptr<SkottieWrapper> skottie =
      CreateSkottieFromString(kLottieDataWith2Assets);
  ASSERT_TRUE(skottie->is_valid());
  ::testing::NiceMock<MockCanvas> canvas;
  MockFrameDataCallback mock_callback;
  EXPECT_CALL(mock_callback,
              OnAssetLoaded(HashSkottieResourceId("image_0"), _, _, _));
  skottie->Seek(/*t=*/0.25, mock_callback.Get());
  Mock::VerifyAndClearExpectations(&mock_callback);

  EXPECT_CALL(mock_callback,
              OnAssetLoaded(HashSkottieResourceId("image_1"), _, _, _));
  skottie->Seek(/*t=*/0.75, mock_callback.Get());
  Mock::VerifyAndClearExpectations(&mock_callback);
}

}  // namespace
}  // namespace cc
