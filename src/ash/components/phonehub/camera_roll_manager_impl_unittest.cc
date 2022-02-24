// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/camera_roll_manager_impl.h"

#include "ash/components/phonehub/camera_roll_download_manager.h"
#include "ash/components/phonehub/camera_roll_item.h"
#include "ash/components/phonehub/camera_roll_thumbnail_decoder_impl.h"
#include "ash/components/phonehub/fake_camera_roll_download_manager.h"
#include "ash/components/phonehub/fake_message_receiver.h"
#include "ash/components/phonehub/fake_message_sender.h"
#include "ash/components/phonehub/pref_names.h"
#include "ash/components/phonehub/proto/phonehub_api.pb.h"
#include "ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace phonehub {

namespace {

using BatchDecodeResult = CameraRollThumbnailDecoder::BatchDecodeResult;
using BatchDecodeCallback =
    base::OnceCallback<void(BatchDecodeResult,
                            const std::vector<CameraRollItem>&)>;
using FeatureState = chromeos::multidevice_setup::mojom::FeatureState;
using FileTransferStatus = chromeos::secure_channel::mojom::FileTransferStatus;

class FakeObserver : public CameraRollManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  // CameraRollManager::Observer
  void OnCameraRollViewUiStateUpdated() override {
    on_camera_roll_items_changed_call_count_++;
  }

  int on_camera_roll_items_changed_call_count() const {
    return on_camera_roll_items_changed_call_count_;
  }

  void OnCameraRollDownloadError(
      CameraRollManager::Observer::DownloadErrorType error_type,
      const proto::CameraRollItemMetadata& metadata) override {
    last_download_error_ = absl::make_optional(error_type);
  }

  CameraRollManager::Observer::DownloadErrorType last_download_error() const {
    return last_download_error_.value();
  }

 private:
  int on_camera_roll_items_changed_call_count_ = 0;
  absl::optional<CameraRollManager::Observer::DownloadErrorType>
      last_download_error_ = absl::nullopt;
};

// Registers preferences for
void RegisterHasDismissedOnBoardingUiPreferences(
    TestingPrefServiceSimple* pref_service) {
  DCHECK(pref_service);
  pref_service->registry()->RegisterBooleanPref(
      prefs::kHasDismissedCameraRollOnboardingUi, false);
}

void PopulateItemProto(proto::CameraRollItem* item_proto, std::string key) {
  proto::CameraRollItemMetadata* metadata = item_proto->mutable_metadata();
  metadata->set_key(key);
  metadata->set_mime_type("image/png");
  metadata->set_last_modified_millis(123456789L);
  metadata->set_file_size_bytes(123456789L);

  proto::CameraRollItemThumbnail* thumbnail = item_proto->mutable_thumbnail();
  thumbnail->set_format(proto::CameraRollItemThumbnail_Format_JPEG);
  thumbnail->set_data("encoded_thumbnail_data");
}

// Verifies that the metadata of a generated item matches the corresponding
// proto input.
void VerifyMetadataEqual(const proto::CameraRollItemMetadata& expected,
                         const proto::CameraRollItemMetadata& actual) {
  EXPECT_EQ(expected.key(), actual.key());
  EXPECT_EQ(expected.mime_type(), actual.mime_type());
  EXPECT_EQ(expected.last_modified_millis(), actual.last_modified_millis());
  EXPECT_EQ(expected.file_size_bytes(), actual.file_size_bytes());
}

}  // namespace

class FakeThumbnailDecoder : public CameraRollThumbnailDecoder {
 public:
  FakeThumbnailDecoder() = default;
  ~FakeThumbnailDecoder() override = default;

  void BatchDecode(const proto::FetchCameraRollItemsResponse& response,
                   const std::vector<CameraRollItem>& current_items,
                   BatchDecodeCallback callback) override {
    if (!pending_callback_.is_null()) {
      CompletePendingCallback(BatchDecodeResult::kCancelled);
    }
    last_response_ = response;
    pending_callback_ = std::move(callback);
  }

  void CompletePendingCallback(BatchDecodeResult result) {
    std::vector<CameraRollItem> items;
    if (result == BatchDecodeResult::kCompleted) {
      for (const proto::CameraRollItem& item_proto : last_response_.items()) {
        SkBitmap test_bitmap;
        test_bitmap.allocN32Pixels(1, 1);
        gfx::ImageSkia image_skia =
            gfx::ImageSkia::CreateFrom1xBitmap(test_bitmap);
        image_skia.MakeThreadSafe();
        gfx::Image thumbnail(image_skia);
        items.emplace_back(item_proto.metadata(), thumbnail);
      }
    }
    std::move(pending_callback_).Run(result, items);
  }

 private:
  proto::FetchCameraRollItemsResponse last_response_;
  BatchDecodeCallback pending_callback_;
};

class CameraRollManagerImplTest : public testing::Test {
 protected:
  CameraRollManagerImplTest() = default;
  CameraRollManagerImplTest(const CameraRollManagerImplTest&) = delete;
  CameraRollManagerImplTest& operator=(const CameraRollManagerImplTest&) =
      delete;
  ~CameraRollManagerImplTest() override = default;

  void SetUp() override {
    RegisterHasDismissedOnBoardingUiPreferences(&pref_service_);
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
    fake_connection_manager_ =
        std::make_unique<secure_channel::FakeConnectionManager>();
    std::unique_ptr<FakeCameraRollDownloadManager>
        fake_camera_roll_download_manager =
            std::make_unique<FakeCameraRollDownloadManager>();
    fake_camera_roll_download_manager_ =
        fake_camera_roll_download_manager.get();

    SetCameraRollFeatureState(FeatureState::kEnabledByUser);
    camera_roll_manager_ = std::make_unique<CameraRollManagerImpl>(
        &pref_service_, &fake_message_receiver_, &fake_message_sender_,
        fake_multidevice_setup_client_.get(), fake_connection_manager_.get(),
        std::move(fake_camera_roll_download_manager));
    camera_roll_manager_->thumbnail_decoder_ =
        std::make_unique<FakeThumbnailDecoder>();
    camera_roll_manager_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    camera_roll_manager_->RemoveObserver(&fake_observer_);
  }

  int GetOnCameraRollViewUiStateUpdatedCallCount() const {
    return fake_observer_.on_camera_roll_items_changed_call_count();
  }

  CameraRollManager::Observer::DownloadErrorType GetLastDownloadError() const {
    return fake_observer_.last_download_error();
  }

  int GetCurrentItemsCount() const {
    return camera_roll_manager_->current_items().size();
  }

  size_t GetSentFetchCameraRollItemsRequestCount() const {
    return fake_message_sender_.GetFetchCameraRollItemsRequestCallCount();
  }

  const proto::FetchCameraRollItemsRequest& GetSentFetchCameraRollItemsRequest()
      const {
    return fake_message_sender_.GetRecentFetchCameraRollItemsRequest();
  }

  size_t GetSentFetchCameraRollItemDataRequestCount() const {
    return fake_message_sender_.GetFetchCameraRollItemDataRequestCallCount();
  }

  const proto::FetchCameraRollItemDataRequest&
  GetRecentFetchCameraRollItemDataRequest() const {
    return fake_message_sender_.GetRecentFetchCameraRollItemDataRequest();
  }

  size_t GetSentInitiateCameraRollItemTransferRequestCount() const {
    return fake_message_sender_
        .GetInitiateCameraRollItemTransferRequestCallCount();
  }

  const proto::InitiateCameraRollItemTransferRequest&
  GetRecentInitiateCameraRollItemTransferRequest() const {
    return fake_message_sender_
        .GetRecentInitiateCameraRollItemTransferRequest();
  }

  // Verifies current items match the list of items in the last received
  // |FetchCameraRollItemsResponse|, and their thumbnails have been properly
  // decoded.
  void VerifyCurrentItemsMatchResponse(
      const proto::FetchCameraRollItemsResponse& response) const {
    EXPECT_EQ(response.items_size(), GetCurrentItemsCount());
    for (int i = 0; i < GetCurrentItemsCount(); i++) {
      const CameraRollItem& current_item =
          camera_roll_manager_->current_items()[i];
      VerifyMetadataEqual(response.items(i).metadata(),
                          current_item.metadata());
      EXPECT_FALSE(current_item.thumbnail().IsEmpty());
    }
  }

  void CompleteThumbnailDecoding(BatchDecodeResult result) {
    static_cast<FakeThumbnailDecoder*>(
        camera_roll_manager_->thumbnail_decoder_.get())
        ->CompletePendingCallback(result);
  }

  void SetCameraRollFeatureState(FeatureState feature_state) {
    fake_multidevice_setup_client_->SetFeatureState(
        chromeos::multidevice_setup::mojom::Feature::kPhoneHubCameraRoll,
        feature_state);
  }

  void UngrantAndroidStoragePermission() {
    android_storage_permission_granted_ = false;
  }

  void SendPhoneStatusSnapshot() {
    proto::PhoneStatusSnapshot snapshot;
    proto::CameraRollAccessState* access_state =
        snapshot.mutable_properties()->mutable_camera_roll_access_state();
    access_state->set_storage_permission_granted(
        android_storage_permission_granted_);
    fake_message_receiver_.NotifyPhoneStatusSnapshotReceived(snapshot);
  }

  void SendPhoneStatusUpdate(bool has_camera_roll_updates) {
    proto::PhoneStatusUpdate update;
    update.set_has_camera_roll_updates(has_camera_roll_updates);
    proto::CameraRollAccessState* access_state =
        update.mutable_properties()->mutable_camera_roll_access_state();
    access_state->set_storage_permission_granted(
        android_storage_permission_granted_);
    fake_message_receiver_.NotifyPhoneStatusUpdateReceived(update);
  }

  void SendFetchCameraRollItemDataResponse(
      const proto::CameraRollItemMetadata& item_metadata,
      proto::FetchCameraRollItemDataResponse::FileAvailability
          file_availability,
      int64_t payload_id) {
    proto::FetchCameraRollItemDataResponse response;
    *response.mutable_metadata() = item_metadata;
    response.set_file_availability(file_availability);
    response.set_payload_id(payload_id);
    fake_message_receiver_.NotifyFetchCameraRollItemDataResponseReceived(
        response);
  }

  void SendFileTransferUpdate(int64_t payload_id,
                              FileTransferStatus status,
                              uint64_t total_bytes,
                              uint64_t bytes_transferred) {
    fake_connection_manager_->SendFileTransferUpdate(
        chromeos::secure_channel::mojom::FileTransferUpdate::New(
            payload_id, status, total_bytes, bytes_transferred));
  }

  void VerifyFileTransferProgress(int64_t payload_id,
                                  FileTransferStatus status,
                                  uint64_t total_bytes,
                                  uint64_t bytes_transferred) {
    const chromeos::secure_channel::mojom::FileTransferUpdatePtr&
        latest_update = fake_camera_roll_download_manager_
                            ->GetFileTransferUpdates(payload_id)
                            .back();
    EXPECT_EQ(payload_id, latest_update->payload_id);
    EXPECT_EQ(status, latest_update->status);
    EXPECT_EQ(total_bytes, latest_update->total_bytes);
    EXPECT_EQ(bytes_transferred, latest_update->bytes_transferred);
  }

  CameraRollManager* camera_roll_manager() {
    return camera_roll_manager_.get();
  }

  secure_channel::FakeConnectionManager* fake_connection_manager() {
    return fake_connection_manager_.get();
  }

  FakeCameraRollDownloadManager* fake_camera_roll_download_manager() {
    return fake_camera_roll_download_manager_;
  }

  FakeMessageReceiver fake_message_receiver_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;

 private:
  TestingPrefServiceSimple pref_service_;
  FakeMessageSender fake_message_sender_;
  std::unique_ptr<secure_channel::FakeConnectionManager>
      fake_connection_manager_;
  FakeCameraRollDownloadManager* fake_camera_roll_download_manager_;
  std::unique_ptr<CameraRollManagerImpl> camera_roll_manager_;
  FakeObserver fake_observer_;

  bool android_storage_permission_granted_ = true;
};

TEST_F(CameraRollManagerImplTest, OnCameraRollItemsReceived) {
  SendPhoneStatusSnapshot();

  task_environment_.FastForwardBy(base::Seconds(10));
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key3");
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");

  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);

  EXPECT_EQ(2, GetOnCameraRollViewUiStateUpdatedCallCount());
  VerifyCurrentItemsMatchResponse(response);
  histogram_tester_.ExpectUniqueTimeSample(
      "PhoneHub.CameraRoll.Latency.RefreshItems", base::Seconds(10),
      /*expected_bucket_count=*/1);
}

TEST_F(CameraRollManagerImplTest,
       OnCameraRollItemsReceivedWithCancelledThumbnailDecodingRequest) {
  SendPhoneStatusSnapshot();

  task_environment_.FastForwardBy(base::Seconds(10));
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key3");
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");

  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCancelled);

  EXPECT_EQ(1, GetOnCameraRollViewUiStateUpdatedCallCount());
  EXPECT_EQ(0, GetCurrentItemsCount());
  EXPECT_TRUE(histogram_tester_
                  .GetAllSamples("PhoneHub.CameraRoll.Latency.RefreshItems")
                  .empty());
}

TEST_F(CameraRollManagerImplTest,
       OnCameraRollItemsReceivedWithPendingThumbnailDecodedRequest) {
  SendPhoneStatusSnapshot();

  task_environment_.FastForwardBy(base::Seconds(10));
  proto::FetchCameraRollItemsResponse first_response;
  PopulateItemProto(first_response.add_items(), "key2");
  PopulateItemProto(first_response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(
      first_response);

  task_environment_.FastForwardBy(base::Seconds(5));
  proto::FetchCameraRollItemsResponse second_response;
  PopulateItemProto(second_response.add_items(), "key4");
  PopulateItemProto(second_response.add_items(), "key3");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(
      second_response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);

  // The first thumbnail decode request should be cancelled and the current item
  // set should be updated only once after the second request completes.
  EXPECT_EQ(2, GetOnCameraRollViewUiStateUpdatedCallCount());
  VerifyCurrentItemsMatchResponse(second_response);
  histogram_tester_.ExpectUniqueTimeSample(
      "PhoneHub.CameraRoll.Latency.RefreshItems", base::Seconds(15),
      /*expected_bucket_count=*/1);
}

TEST_F(CameraRollManagerImplTest, OnCameraRollItemsReceivedWithExistingItems) {
  SendPhoneStatusSnapshot();
  task_environment_.FastForwardBy(base::Seconds(10));
  proto::FetchCameraRollItemsResponse first_response;
  PopulateItemProto(first_response.add_items(), "key3");
  PopulateItemProto(first_response.add_items(), "key2");
  PopulateItemProto(first_response.add_items(), "key1");

  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(
      first_response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);
  VerifyCurrentItemsMatchResponse(first_response);

  SendPhoneStatusUpdate(/*has_camera_roll_updates=*/true);
  task_environment_.FastForwardBy(base::Seconds(5));
  proto::FetchCameraRollItemsResponse second_response;
  PopulateItemProto(second_response.add_items(), "key4");
  // Thumbnails won't be sent with the proto if an item's data is already
  // available and up-to-date.
  PopulateItemProto(second_response.add_items(), "key3");
  second_response.mutable_items(1)->clear_thumbnail();
  PopulateItemProto(second_response.add_items(), "key2");
  second_response.mutable_items(2)->clear_thumbnail();

  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(
      second_response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);
  EXPECT_EQ(3, GetOnCameraRollViewUiStateUpdatedCallCount());
  VerifyCurrentItemsMatchResponse(second_response);
  histogram_tester_.ExpectTimeBucketCount(
      "PhoneHub.CameraRoll.Latency.RefreshItems", base::Seconds(10),
      /*count=*/1);
  histogram_tester_.ExpectTimeBucketCount(
      "PhoneHub.CameraRoll.Latency.RefreshItems", base::Seconds(5),
      /*count=*/1);
}

TEST_F(CameraRollManagerImplTest,
       OnPhoneStatusUpdateReceivedWithoutCameraRollUpdates) {
  SendPhoneStatusUpdate(/*has_camera_roll_updates=*/false);

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(CameraRollManager::CameraRollUiState::SHOULD_HIDE,
            camera_roll_manager()->ui_state());
  EXPECT_EQ(1, GetOnCameraRollViewUiStateUpdatedCallCount());
}

TEST_F(CameraRollManagerImplTest,
       OnPhoneStatusUpdateReceivedWithCameraRollUpdates) {
  SendPhoneStatusUpdate(/*has_camera_roll_updates=*/true);

  EXPECT_EQ(1UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(0,
            GetSentFetchCameraRollItemsRequest().current_item_metadata_size());
  EXPECT_EQ(CameraRollManager::CameraRollUiState::SHOULD_HIDE,
            camera_roll_manager()->ui_state());
  EXPECT_EQ(1, GetOnCameraRollViewUiStateUpdatedCallCount());
}

TEST_F(CameraRollManagerImplTest,
       OnPhoneStatusUpdateReceivedWithExistingItems) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key3");
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);

  SendPhoneStatusUpdate(/*has_camera_roll_updates=*/true);

  EXPECT_EQ(1UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(CameraRollManager::CameraRollUiState::ITEMS_VISIBLE,
            camera_roll_manager()->ui_state());
  EXPECT_EQ(2, GetOnCameraRollViewUiStateUpdatedCallCount());
  EXPECT_EQ(3,
            GetSentFetchCameraRollItemsRequest().current_item_metadata_size());
  VerifyMetadataEqual(
      response.items(0).metadata(),
      GetSentFetchCameraRollItemsRequest().current_item_metadata(0));
  VerifyMetadataEqual(
      response.items(1).metadata(),
      GetSentFetchCameraRollItemsRequest().current_item_metadata(1));
  VerifyMetadataEqual(
      response.items(2).metadata(),
      GetSentFetchCameraRollItemsRequest().current_item_metadata(2));
}

TEST_F(CameraRollManagerImplTest,
       OnPhoneStatusUpdateReceivedWithCameraRollSettingsDisabled) {
  SetCameraRollFeatureState(FeatureState::kDisabledByUser);
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);

  SendPhoneStatusUpdate(/*has_camera_roll_updates=*/true);

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(CameraRollManager::CameraRollUiState::CAN_OPT_IN,
            camera_roll_manager()->ui_state());
  EXPECT_EQ(4, GetOnCameraRollViewUiStateUpdatedCallCount());
  EXPECT_EQ(0, GetCurrentItemsCount());
}

TEST_F(CameraRollManagerImplTest,
       OnPhoneStatusUpdateReceivedWithoutStoragePermission) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);

  UngrantAndroidStoragePermission();
  SendPhoneStatusUpdate(/*has_camera_roll_updates=*/false);

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(CameraRollManager::CameraRollUiState::NO_STORAGE_PERMISSION,
            camera_roll_manager()->ui_state());
  EXPECT_EQ(2, GetOnCameraRollViewUiStateUpdatedCallCount());
  EXPECT_EQ(0, GetCurrentItemsCount());
}

TEST_F(CameraRollManagerImplTest, OnPhoneStatusSnapshotReceived) {
  SendPhoneStatusSnapshot();

  EXPECT_EQ(1UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(CameraRollManager::CameraRollUiState::SHOULD_HIDE,
            camera_roll_manager()->ui_state());
  EXPECT_EQ(1, GetOnCameraRollViewUiStateUpdatedCallCount());
}

TEST_F(CameraRollManagerImplTest,
       OnPhoneStatusSnapshotReceivedWithCameraRollSettingDisabled) {
  SetCameraRollFeatureState(FeatureState::kDisabledByUser);
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);

  SendPhoneStatusSnapshot();

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(CameraRollManager::CameraRollUiState::CAN_OPT_IN,
            camera_roll_manager()->ui_state());
  EXPECT_EQ(4, GetOnCameraRollViewUiStateUpdatedCallCount());
  EXPECT_EQ(0, GetCurrentItemsCount());
}

TEST_F(CameraRollManagerImplTest,
       OnPhoneStatusSnapshotReceivedWithoutStoragePermission) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);

  UngrantAndroidStoragePermission();
  SendPhoneStatusSnapshot();

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(CameraRollManager::CameraRollUiState::NO_STORAGE_PERMISSION,
            camera_roll_manager()->ui_state());
  EXPECT_EQ(2, GetOnCameraRollViewUiStateUpdatedCallCount());
  EXPECT_EQ(0, GetCurrentItemsCount());
}

TEST_F(CameraRollManagerImplTest, OnFeatureOnFeatureStatesChangedToDisabled) {
  SendPhoneStatusSnapshot();
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);

  SetCameraRollFeatureState(FeatureState::kDisabledByUser);

  EXPECT_EQ(CameraRollManager::CameraRollUiState::CAN_OPT_IN,
            camera_roll_manager()->ui_state());
  EXPECT_EQ(3, GetOnCameraRollViewUiStateUpdatedCallCount());
  EXPECT_EQ(0, GetCurrentItemsCount());
}

TEST_F(CameraRollManagerImplTest, FeatureProhibitedByPolicy) {
  SetCameraRollFeatureState(FeatureState::kProhibitedByPolicy);

  proto::PhoneStatusSnapshot snapshot;
  proto::CameraRollAccessState* access_state =
      snapshot.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_storage_permission_granted(true);
  fake_message_receiver_.NotifyPhoneStatusSnapshotReceived(snapshot);

  EXPECT_EQ(CameraRollManager::CameraRollUiState::SHOULD_HIDE,
            camera_roll_manager()->ui_state());
}

TEST_F(CameraRollManagerImplTest, EnableFromOptInDialog) {
  SetCameraRollFeatureState(FeatureState::kDisabledByUser);
  proto::PhoneStatusSnapshot snapshot;
  proto::CameraRollAccessState* access_state =
      snapshot.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_storage_permission_granted(true);
  fake_message_receiver_.NotifyPhoneStatusSnapshotReceived(snapshot);
  EXPECT_EQ(CameraRollManager::CameraRollUiState::CAN_OPT_IN,
            camera_roll_manager()->ui_state());

  camera_roll_manager()->EnableCameraRollFeatureInSystemSetting();
  // Verify that the CameraRollManager attempted to enable the feature via the
  // MultideviceSetupClient. Then actually set the feature state to
  // kEnabledByUser since the FakeMultideviceSetupClient doesn't do that.
  fake_multidevice_setup_client_->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/chromeos::multidevice_setup::mojom::Feature::
          kPhoneHubCameraRoll,
      /*expected_enabled=*/true,
      /*expected_auth_token=*/absl::nullopt,
      /*success=*/true);
  SetCameraRollFeatureState(FeatureState::kEnabledByUser);
  // The UI should change into the loading view after the setting is enabled and
  // before the items are received.
  EXPECT_EQ(CameraRollManager::CameraRollUiState::LOADING_VIEW,
            camera_roll_manager()->ui_state());

  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);
  EXPECT_EQ(CameraRollManager::CameraRollUiState::ITEMS_VISIBLE,
            camera_roll_manager()->ui_state());
}

TEST_F(CameraRollManagerImplTest, DismissOptInDialog) {
  SetCameraRollFeatureState(FeatureState::kDisabledByUser);
  proto::PhoneStatusSnapshot snapshot;
  proto::CameraRollAccessState* access_state =
      snapshot.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_storage_permission_granted(true);
  fake_message_receiver_.NotifyPhoneStatusSnapshotReceived(snapshot);
  EXPECT_EQ(CameraRollManager::CameraRollUiState::CAN_OPT_IN,
            camera_roll_manager()->ui_state());

  camera_roll_manager()->OnCameraRollOnboardingUiDismissed();

  EXPECT_EQ(CameraRollManager::CameraRollUiState::SHOULD_HIDE,
            camera_roll_manager()->ui_state());
}

TEST_F(CameraRollManagerImplTest, DownloadItem) {
  // Make an item available to CameraRollManager.
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);
  const CameraRollItem& item_to_download =
      camera_roll_manager()->current_items().back();

  // Request to download the item that was added.
  camera_roll_manager()->DownloadItem(item_to_download.metadata());
  EXPECT_EQ(1UL, GetSentFetchCameraRollItemDataRequestCount());
  EXPECT_EQ("key1", GetRecentFetchCameraRollItemDataRequest().metadata().key());

  // CameraRollManager should initiate transfer of the item after receiving
  // FetchCameraRollItemDataResponse.
  SendFetchCameraRollItemDataResponse(
      item_to_download.metadata(),
      proto::FetchCameraRollItemDataResponse::AVAILABLE,
      /*payload_id=*/1234);
  EXPECT_EQ(1UL, GetSentInitiateCameraRollItemTransferRequestCount());
  EXPECT_EQ("key1",
            GetRecentInitiateCameraRollItemTransferRequest().metadata().key());
  EXPECT_EQ(1234,
            GetRecentInitiateCameraRollItemTransferRequest().payload_id());

  // Now the CameraRollManager should be ready to receive updates of the item
  // transfer.
  SendFileTransferUpdate(/*payload_id=*/1234, FileTransferStatus::kInProgress,
                         /*total_bytes=*/1000, /*bytes_transferred=*/200);
  VerifyFileTransferProgress(/*payload_id=*/1234,
                             FileTransferStatus::kInProgress,
                             /*total_bytes=*/1000,
                             /*bytes_transferred=*/200);

  SendFileTransferUpdate(/*payload_id=*/1234, FileTransferStatus::kSuccess,
                         /*total_bytes=*/1000, /*bytes_transferred=*/1000);
  VerifyFileTransferProgress(/*payload_id=*/1234, FileTransferStatus::kSuccess,
                             /*total_bytes=*/1000,
                             /*bytes_transferred=*/1000);
}

TEST_F(CameraRollManagerImplTest,
       DownloadItemWhenFileNoLongerAvailableOnPhone) {
  // Make an item available to CameraRollManager.
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);
  const CameraRollItem& item_to_download =
      camera_roll_manager()->current_items().back();

  // Request to download the item that was added.
  camera_roll_manager()->DownloadItem(item_to_download.metadata());
  EXPECT_EQ(1UL, GetSentFetchCameraRollItemDataRequestCount());
  EXPECT_EQ("key1", GetRecentFetchCameraRollItemDataRequest().metadata().key());

  SendFetchCameraRollItemDataResponse(
      item_to_download.metadata(),
      proto::FetchCameraRollItemDataResponse::NOT_FOUND,
      /*payload_id=*/1234);
  EXPECT_EQ(0UL, GetSentInitiateCameraRollItemTransferRequestCount());
}

TEST_F(CameraRollManagerImplTest, DownloadItemFailedWithGenericError) {
  // Make an item available to CameraRollManager.
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);
  const CameraRollItem& item_to_download =
      camera_roll_manager()->current_items().back();

  // Request to download the item that was added.
  camera_roll_manager()->DownloadItem(item_to_download.metadata());
  fake_camera_roll_download_manager()->set_expected_create_payload_files_result(
      CameraRollDownloadManager::CreatePayloadFilesResult::kInvalidFileName);
  SendFetchCameraRollItemDataResponse(
      item_to_download.metadata(),
      proto::FetchCameraRollItemDataResponse::AVAILABLE,
      /*payload_id=*/1234);

  EXPECT_EQ(0UL, GetSentInitiateCameraRollItemTransferRequestCount());
  EXPECT_EQ(CameraRollManager::Observer::DownloadErrorType::kGenericError,
            GetLastDownloadError());
}

TEST_F(CameraRollManagerImplTest, DownloadItemFailedWithInsufficientStorage) {
  // Make an item available to CameraRollManager.
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);
  const CameraRollItem& item_to_download =
      camera_roll_manager()->current_items().back();

  // Request to download the item that was added.
  camera_roll_manager()->DownloadItem(item_to_download.metadata());
  fake_camera_roll_download_manager()->set_expected_create_payload_files_result(
      CameraRollDownloadManager::CreatePayloadFilesResult::
          kInsufficientDiskSpace);
  SendFetchCameraRollItemDataResponse(
      item_to_download.metadata(),
      proto::FetchCameraRollItemDataResponse::AVAILABLE,
      /*payload_id=*/1234);

  EXPECT_EQ(0UL, GetSentInitiateCameraRollItemTransferRequestCount());
  EXPECT_EQ(
      CameraRollManager::Observer::DownloadErrorType::kInsufficientStorage,
      GetLastDownloadError());
}

TEST_F(CameraRollManagerImplTest, DownloadItemFailedWithNetworkError) {
  // Make an item available to CameraRollManager.
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);
  const CameraRollItem& item_to_download =
      camera_roll_manager()->current_items().back();

  // Request to download the item that was added.
  camera_roll_manager()->DownloadItem(item_to_download.metadata());
  SendFetchCameraRollItemDataResponse(
      item_to_download.metadata(),
      proto::FetchCameraRollItemDataResponse::AVAILABLE,
      /*payload_id=*/1234);
  SendFileTransferUpdate(/*payload_id=*/1234, FileTransferStatus::kInProgress,
                         /*total_bytes=*/1000, /*bytes_transferred=*/200);
  SendFileTransferUpdate(/*payload_id=*/1234, FileTransferStatus::kFailure,
                         /*total_bytes=*/0, /*bytes_transferred=*/0);

  EXPECT_EQ(CameraRollManager::Observer::DownloadErrorType::kNetworkConnection,
            GetLastDownloadError());
}

TEST_F(CameraRollManagerImplTest, DownloadItemAndRegisterPayloadFileFail) {
  // Make an item available to CameraRollManager.
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);
  const CameraRollItem& item_to_download =
      camera_roll_manager()->current_items().back();

  // Request to download the item that was added.
  camera_roll_manager()->DownloadItem(item_to_download.metadata());
  EXPECT_EQ(1UL, GetSentFetchCameraRollItemDataRequestCount());
  EXPECT_EQ("key1", GetRecentFetchCameraRollItemDataRequest().metadata().key());

  fake_connection_manager()->set_register_payload_file_result(false);
  SendFetchCameraRollItemDataResponse(
      item_to_download.metadata(),
      proto::FetchCameraRollItemDataResponse::AVAILABLE,
      /*payload_id=*/1234);
  EXPECT_EQ(0UL, GetSentInitiateCameraRollItemTransferRequestCount());
}

TEST_F(CameraRollManagerImplTest, ItemsClearedWhenDeviceDisconnected) {
  fake_connection_manager()->SetStatus(
      secure_channel::ConnectionManager::Status::kConnected);

  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key3");
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kCompleted);

  EXPECT_EQ(3, GetCurrentItemsCount());

  fake_connection_manager()->SetStatus(
      secure_channel::ConnectionManager::Status::kDisconnected);

  EXPECT_EQ(0, GetCurrentItemsCount());
}

}  // namespace phonehub
}  // namespace ash
