// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_offer_sync_bridge.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/entity_data.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/protocol/autofill_offer_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/model/mock_model_type_change_processor.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using base::ScopedTempDir;
using sync_pb::AutofillOfferSpecifics;
using sync_pb::ModelTypeState;
using syncer::EntityData;
using syncer::MockModelTypeChangeProcessor;
using testing::NiceMock;
using testing::Return;

const char kLocaleString[] = "en-US";
const char kDefaultCacheGuid[] = "CacheGuid";

void ExtractAutofillOfferSpecificsFromDataBatch(
    std::unique_ptr<syncer::DataBatch> batch,
    std::vector<AutofillOfferSpecifics>* output) {
  while (batch->HasNext()) {
    const syncer::KeyAndData& data_pair = batch->Next();
    output->push_back(data_pair.second->specifics.autofill_offer());
  }
}

std::string AutofillOfferSpecificsAsDebugString(
    const AutofillOfferSpecifics& specifics) {
  std::ostringstream output;

  std::string offer_reward_amount_string;
  if (specifics.has_percentage_reward()) {
    offer_reward_amount_string = specifics.percentage_reward().percentage();
  } else if (specifics.has_fixed_amount_reward()) {
    offer_reward_amount_string = specifics.fixed_amount_reward().amount();
  }

  std::string domain_string;
  for (std::string merchant_domain : specifics.merchant_domain()) {
    base::StrAppend(&domain_string, {merchant_domain, ", "});
  }

  std::string instrument_id_string;
  for (int64_t eligible_instrument_id :
       specifics.card_linked_offer_data().instrument_id()) {
    base::StrAppend(&instrument_id_string,
                    {base::NumberToString(eligible_instrument_id), ", "});
  }

  output << "[id: " << specifics.id()
         << ", offer_expiry_date: " << specifics.offer_expiry_date()
         << ", offer_details_url: " << specifics.offer_details_url()
         << ", merchant_domain: " << domain_string << ", value_prop_text: "
         << specifics.display_strings().value_prop_text()
#if defined(OS_ANDROID) || defined(OS_IOS)
         << ", see_details_text: "
         << specifics.display_strings().see_details_text_mobile()
         << ", usage_instructions_text: "
         << specifics.display_strings().usage_instructions_text_mobile()
#else
         << ", see_details_text: "
         << specifics.display_strings().see_details_text_desktop()
         << ", usage_instructions_text: "
         << specifics.display_strings().usage_instructions_text_desktop()
#endif  // defined(OS_ANDROID) || defined(OS_IOS)
         << ", offer_reward_amount: " << offer_reward_amount_string
         << ", eligible_instrument_id: " << instrument_id_string
         << ", promo_code: " << specifics.promo_code_offer_data().promo_code()
         << "]";
  return output.str();
}

MATCHER_P(EqualsSpecifics, expected, "") {
  if (arg.SerializeAsString() != expected.SerializeAsString()) {
    *result_listener << "entry\n"
                     << AutofillOfferSpecificsAsDebugString(arg) << "\n"
                     << "did not match expected\n"
                     << AutofillOfferSpecificsAsDebugString(expected);
    return false;
  }
  return true;
}

}  // namespace

class AutofillWalletOfferSyncBridgeTest : public testing::Test {
 public:
  AutofillWalletOfferSyncBridgeTest() = default;
  ~AutofillWalletOfferSyncBridgeTest() override = default;
  AutofillWalletOfferSyncBridgeTest(const AutofillWalletOfferSyncBridgeTest&) =
      delete;
  AutofillWalletOfferSyncBridgeTest& operator=(
      const AutofillWalletOfferSyncBridgeTest&) = delete;

  void SetUp() override {
    CountryNames::SetLocaleString(kLocaleString);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
    ON_CALL(backend_, GetDatabase()).WillByDefault(Return(&db_));
    ResetProcessor();
    // Fake that initial sync has been done (so that the bridge immediately
    // records metrics).
    ResetBridge(/*initial_sync_done=*/true);
  }

  void ResetProcessor() {
    real_processor_ =
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            syncer::AUTOFILL_WALLET_OFFER, /*dump_stack=*/base::DoNothing());
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
  }

  void ResetBridge(bool initial_sync_done) {
    ModelTypeState model_type_state;
    model_type_state.set_initial_sync_done(initial_sync_done);
    model_type_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(syncer::AUTOFILL_WALLET_OFFER));
    model_type_state.set_cache_guid(kDefaultCacheGuid);
    EXPECT_TRUE(table()->UpdateModelTypeState(syncer::AUTOFILL_WALLET_OFFER,
                                              model_type_state));
    bridge_ = std::make_unique<AutofillWalletOfferSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &backend_);
  }

  void StartSyncing(
      const std::vector<AutofillOfferSpecifics>& remote_data = {}) {
    base::RunLoop loop;
    syncer::DataTypeActivationRequest request;
    request.error_handler = base::DoNothing();
    request.cache_guid = kDefaultCacheGuid;
    real_processor_->OnSyncStarting(
        request,
        base::BindLambdaForTesting(
            [&loop](std::unique_ptr<syncer::DataTypeActivationResponse>) {
              loop.Quit();
            }));
    loop.Run();

    // Initialize the processor with initial_sync_done.
    sync_pb::ModelTypeState state;
    state.set_initial_sync_done(true);
    state.mutable_progress_marker()
        ->mutable_gc_directive()
        ->set_version_watermark(1);
    syncer::UpdateResponseDataList initial_updates;
    for (const AutofillOfferSpecifics& specifics : remote_data) {
      initial_updates.push_back(SpecificsToUpdateResponse(specifics));
    }
    real_processor_->OnUpdateReceived(state, std::move(initial_updates));
  }

  std::vector<AutofillOfferSpecifics> GetAllLocalData() {
    std::vector<AutofillOfferSpecifics> data;
    // Perform an async call synchronously for testing.
    base::RunLoop loop;
    bridge()->GetAllDataForDebugging(base::BindLambdaForTesting(
        [&loop, &data](std::unique_ptr<syncer::DataBatch> batch) {
          ExtractAutofillOfferSpecificsFromDataBatch(std::move(batch), &data);
          loop.Quit();
        }));
    loop.Run();
    return data;
  }

  syncer::UpdateResponseData SpecificsToUpdateResponse(
      const AutofillOfferSpecifics& specifics) {
    syncer::UpdateResponseData data;
    data.entity = SpecificsToEntity(specifics);
    return data;
  }

  EntityData SpecificsToEntity(const AutofillOfferSpecifics& specifics) {
    EntityData data;
    *data.specifics.mutable_autofill_offer() = specifics;
    data.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
        syncer::AUTOFILL_WALLET_OFFER, bridge()->GetClientTag(data));
    return data;
  }

  AutofillTable* table() { return &table_; }

  AutofillWalletOfferSyncBridge* bridge() { return bridge_.get(); }

  MockAutofillWebDataBackend* backend() { return &backend_; }

 private:
  ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  NiceMock<MockAutofillWebDataBackend> backend_;
  AutofillTable table_;
  WebDatabase db_;
  NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::ClientTagBasedModelTypeProcessor> real_processor_;
  std::unique_ptr<AutofillWalletOfferSyncBridge> bridge_;
};

TEST_F(AutofillWalletOfferSyncBridgeTest, VerifyGetClientTag) {
  AutofillOfferSpecifics specifics;
  AutofillOfferData data = test::GetCardLinkedOfferData1();
  SetAutofillOfferSpecificsFromOfferData(data, &specifics);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            base::NumberToString(data.offer_id));
}

TEST_F(AutofillWalletOfferSyncBridgeTest, VerifyGetStorageKey) {
  AutofillOfferSpecifics specifics;
  AutofillOfferData data = test::GetCardLinkedOfferData1();
  SetAutofillOfferSpecificsFromOfferData(data, &specifics);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics)),
            base::NumberToString(data.offer_id));
}

// Tests that when a new offer data is sent by the server, the client only keeps
// the new data.
TEST_F(AutofillWalletOfferSyncBridgeTest, MergeSyncData_NewData) {
  // Create one offer data in the client table.
  AutofillOfferData old_data = test::GetCardLinkedOfferData1();
  table()->SetAutofillOffers({old_data});

  // Create a different one on the server.
  AutofillOfferSpecifics offer_specifics;
  SetAutofillOfferSpecificsFromOfferData(test::GetPromoCodeOfferData(),
                                         &offer_specifics);

  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  StartSyncing({offer_specifics});

  // Only the server offer should be present on the client.
  EXPECT_THAT(GetAllLocalData(),
              testing::UnorderedElementsAre(EqualsSpecifics(offer_specifics)));
}

// Tests that when no data is sent by the server, all local data should be
// deleted.
TEST_F(AutofillWalletOfferSyncBridgeTest, MergeSyncData_NoData) {
  // Create one offer data in the client table.
  AutofillOfferData client_data = test::GetCardLinkedOfferData1();
  table()->SetAutofillOffers({client_data});

  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  StartSyncing({});

  EXPECT_TRUE(GetAllLocalData().empty());
}

// Test to ensure whether the data being valid is logged correctly.
TEST_F(AutofillWalletOfferSyncBridgeTest, MergeSyncData_LogDataValidity) {
  AutofillOfferSpecifics offer_specifics1;
  SetAutofillOfferSpecificsFromOfferData(test::GetCardLinkedOfferData1(),
                                         &offer_specifics1);
  AutofillOfferSpecifics offer_specifics2;
  SetAutofillOfferSpecificsFromOfferData(test::GetCardLinkedOfferData2(),
                                         &offer_specifics2);
  offer_specifics2.clear_id();

  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  base::HistogramTester histogram_tester;
  StartSyncing({offer_specifics1, offer_specifics2});

  histogram_tester.ExpectBucketCount("Autofill.Offer.SyncedOfferDataBeingValid",
                                     true, 1);
  histogram_tester.ExpectBucketCount("Autofill.Offer.SyncedOfferDataBeingValid",
                                     false, 1);
}

// Tests that when sync is stopped and the data type is disabled, client should
// remove all client data.
TEST_F(AutofillWalletOfferSyncBridgeTest, ApplyStopSyncChanges_ClearAllData) {
  // Create one offer data in the client table.
  AutofillOfferData client_data = test::GetCardLinkedOfferData1();
  table()->SetAutofillOffers({client_data});

  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());

  // Passing in a non-null metadata change list indicates to the bridge that
  // sync is stopping but the data type is not disabled.
  bridge()->ApplyStopSyncChanges(/*delete_metadata_change_list=*/
                                 std::make_unique<
                                     syncer::InMemoryMetadataChangeList>());

  EXPECT_TRUE(GetAllLocalData().empty());
}

// Tests that when sync is stopped but the data type is not disabled, client
// should keep all the data.
TEST_F(AutofillWalletOfferSyncBridgeTest, ApplyStopSyncChanges_KeepAllData) {
  // Create one offer data in the client table.
  AutofillOfferData client_data = test::GetCardLinkedOfferData1();
  table()->SetAutofillOffers({client_data});

  // We do not write to DB at all, so we should not commit any changes.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);

  // Passing in a null metadata change list indicates to the bridge that
  // sync is stopping and the data type is disabled.
  bridge()->ApplyStopSyncChanges(/*delete_metadata_change_list=*/nullptr);

  EXPECT_FALSE(GetAllLocalData().empty());
}

}  // namespace autofill
