// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cached_image_fetcher.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/image_fetcher/core/fake_image_decoder.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "components/image_fetcher/core/storage/image_cache.h"
#include "components/image_fetcher/core/storage/image_data_store_disk.h"
#include "components/image_fetcher/core/storage/image_metadata_store_leveldb.h"
#include "components/image_fetcher/core/storage/proto/cached_image_metadata.pb.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/prefs/testing_pref_service.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using testing::_;
using leveldb_proto::test::FakeDB;

namespace image_fetcher {

class FakeImageDecoder;

namespace {

const GURL kImageUrl = GURL("http://gstatic.img.com/foo.jpg");
constexpr char kImageData[] = "data";

}  // namespace

class ComponentizedCachedImageFetcherTest : public testing::Test {
 public:
  ComponentizedCachedImageFetcherTest() {}

  ~ComponentizedCachedImageFetcherTest() override {
    cached_image_fetcher_.reset();
    // We need to run until idle after deleting the database, because
    // ProtoDatabaseImpl deletes the actual LevelDB asynchronously.
    RunUntilIdle();
  }

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

    auto db =
        std::make_unique<FakeDB<CachedImageMetadataProto>>(&metadata_store_);
    db_ = db.get();

    auto metadata_store = std::make_unique<ImageMetadataStoreLevelDB>(
        base::FilePath(), std::move(db), &clock_);
    auto data_store = std::make_unique<ImageDataStoreDisk>(
        data_dir_.GetPath(), base::SequencedTaskRunnerHandle::Get());

    ImageCache::RegisterProfilePrefs(test_prefs_.registry());
    auto image_cache = std::make_unique<ImageCache>(
        std::move(data_store), std::move(metadata_store), &test_prefs_, &clock_,
        base::SequencedTaskRunnerHandle::Get());
    image_cache_ = image_cache.get();

    image_cache_->SaveImage(kImageUrl.spec(), kImageData);
    RunUntilIdle();
    db_->InitCallback(true);
    image_cache_->DeleteImage(kImageUrl.spec());
    RunUntilIdle();

    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    auto decoder = std::make_unique<FakeImageDecoder>();
    fake_image_decoder_ = decoder.get();

    cached_image_fetcher_ = std::make_unique<CachedImageFetcher>(
        std::make_unique<image_fetcher::ImageFetcherImpl>(std::move(decoder),
                                                          shared_factory_),
        std::move(image_cache));

    RunUntilIdle();
  }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

  CachedImageFetcher* cached_image_fetcher() {
    return cached_image_fetcher_.get();
  }
  ImageCache* image_cache() { return image_cache_; }
  FakeImageDecoder* image_decoder() { return fake_image_decoder_; }
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  MOCK_METHOD1(OnImageLoaded, void(std::string));

 private:
  std::unique_ptr<CachedImageFetcher> cached_image_fetcher_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  FakeImageDecoder* fake_image_decoder_;

  ImageCache* image_cache_;
  base::SimpleTestClock clock_;
  TestingPrefServiceSimple test_prefs_;
  base::ScopedTempDir data_dir_;
  FakeDB<CachedImageMetadataProto>* db_;
  std::map<std::string, CachedImageMetadataProto> metadata_store_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ComponentizedCachedImageFetcherTest);
};

MATCHER(EmptyImage, "") {
  return arg.IsEmpty();
}

MATCHER(NonEmptyImage, "") {
  return !arg.IsEmpty();
}

MATCHER(NonEmptyString, "") {
  return !arg.empty();
}

// TODO(wylieb): Rename these tests CachedImageFetcherTest* when ntp_snippets/-
//               remote/cached_image_fetcher has been migrated.
TEST_F(ComponentizedCachedImageFetcherTest, FetchImageFromCache) {
  // Save the image in the database.
  image_cache()->SaveImage(kImageUrl.spec(), kImageData);
  RunUntilIdle();

  base::MockCallback<ImageDataFetcherCallback> data_callback;
  base::MockCallback<ImageFetcherCallback> image_callback;

  EXPECT_CALL(data_callback, Run(kImageData, _));
  EXPECT_CALL(image_callback, Run(kImageUrl.spec(), NonEmptyImage(), _));
  cached_image_fetcher()->FetchImageAndData(
      kImageUrl.spec(), kImageUrl, data_callback.Get(), image_callback.Get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);

  RunUntilIdle();
}

TEST_F(ComponentizedCachedImageFetcherTest, FetchImagePopulatesCache) {
  // Expect the image to be fetched by URL.
  {
    test_url_loader_factory()->AddResponse(kImageUrl.spec(), kImageData);

    base::MockCallback<ImageDataFetcherCallback> data_callback;
    base::MockCallback<ImageFetcherCallback> image_callback;

    EXPECT_CALL(data_callback, Run(NonEmptyString(), _));
    EXPECT_CALL(image_callback, Run(kImageUrl.spec(), NonEmptyImage(), _));
    cached_image_fetcher()->FetchImageAndData(
        kImageUrl.spec(), kImageUrl, data_callback.Get(), image_callback.Get(),
        TRAFFIC_ANNOTATION_FOR_TESTS);

    RunUntilIdle();
  }
  // Make sure the image data is in the database.
  {
    EXPECT_CALL(*this, OnImageLoaded(NonEmptyString()));
    image_cache()->LoadImage(
        kImageUrl.spec(),
        base::BindOnce(&ComponentizedCachedImageFetcherTest::OnImageLoaded,
                       base::Unretained(this)));
    RunUntilIdle();
  }
  // Fetch again. The cache should be populated, no network request is needed.
  {
    test_url_loader_factory()->ClearResponses();

    base::MockCallback<ImageDataFetcherCallback> data_callback;
    base::MockCallback<ImageFetcherCallback> image_callback;

    EXPECT_CALL(data_callback, Run(NonEmptyString(), _));
    EXPECT_CALL(image_callback, Run(kImageUrl.spec(), NonEmptyImage(), _));
    cached_image_fetcher()->FetchImageAndData(
        kImageUrl.spec(), kImageUrl, data_callback.Get(), image_callback.Get(),
        TRAFFIC_ANNOTATION_FOR_TESTS);

    RunUntilIdle();
  }
}

TEST_F(ComponentizedCachedImageFetcherTest, DecodingErrorWillDeleteCache) {
  // Save the image in the database.
  image_cache()->SaveImage(kImageUrl.spec(), kImageData);
  RunUntilIdle();
  {
    // Set decoding always error.
    image_decoder()->SetDecodingValid(false);

    base::MockCallback<ImageFetcherCallback> image_callback;
    cached_image_fetcher()->FetchImage(kImageUrl.spec(), kImageUrl,
                                       image_callback.Get(),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

    RunUntilIdle();
  }
  // Make sure the image data was deleted from database.
  {
    EXPECT_CALL(*this, OnImageLoaded(std::string()));
    image_cache()->LoadImage(
        kImageUrl.spec(),
        base::BindOnce(&ComponentizedCachedImageFetcherTest::OnImageLoaded,
                       base::Unretained(this)));
    RunUntilIdle();
  }
}

}  // namespace image_fetcher
