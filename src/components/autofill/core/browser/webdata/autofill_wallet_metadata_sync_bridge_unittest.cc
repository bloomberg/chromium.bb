// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_sync_bridge.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_test_util.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using base::ScopedTempDir;
using sync_pb::WalletMetadataSpecifics;
using syncer::DataBatch;
using syncer::EntityData;
using syncer::EntityDataPtr;
using syncer::KeyAndData;
using syncer::MockModelTypeChangeProcessor;
using syncer::ModelType;
using testing::_;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::IsEmpty;
using testing::Return;
using testing::UnorderedElementsAre;

// Non-UTF8 server IDs.
const char kAddr1ServerId[] = "addr1\xEF\xBF\xBE";
const char kAddr2ServerId[] = "addr2\xEF\xBF\xBE";
const char kCard1ServerId[] = "card1\xEF\xBF\xBE";
const char kCard2ServerId[] = "card2\xEF\xBF\xBE";

// Base64 encodings of the server IDs, used as ids in WalletMetadataSpecifics
// (these are suitable for syncing, because they are valid UTF-8).
const char kAddr1SpecificsId[] = "YWRkcjHvv74=";
const char kAddr2SpecificsId[] = "YWRkcjLvv74=";
const char kCard1SpecificsId[] = "Y2FyZDHvv74=";
const char kCard2SpecificsId[] = "Y2FyZDLvv74=";

const std::string kAddr1StorageKey =
    GetStorageKeyForWalletMetadataTypeAndSpecificsId(
        WalletMetadataSpecifics::ADDRESS,
        kAddr1SpecificsId);
const std::string kCard1StorageKey =
    GetStorageKeyForWalletMetadataTypeAndSpecificsId(
        WalletMetadataSpecifics::CARD,
        kCard1SpecificsId);

// Unique sync tags for the server IDs.
const char kAddr1SyncTag[] = "address-YWRkcjHvv74=";
const char kCard1SyncTag[] = "card-Y2FyZDHvv74=";

const char kLocalAddr1ServerId[] = "e171e3ed-858a-4dd5-9bf3-8517f14ba5fc";
const char kLocalAddr2ServerId[] = "fa232b9a-f248-4e5a-8d76-d46f821c0c5f";

const char kLocaleString[] = "en-US";
const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);

base::Time UseDateFromProtoValue(int64_t use_date_proto_value) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(use_date_proto_value));
}

int64_t UseDateToProtoValue(base::Time use_date) {
  return use_date.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

std::string GetAddressStorageKey(const std::string& specifics_id) {
  return GetStorageKeyForWalletMetadataTypeAndSpecificsId(
      WalletMetadataSpecifics::ADDRESS, specifics_id);
}

std::string GetCardStorageKey(const std::string& specifics_id) {
  return GetStorageKeyForWalletMetadataTypeAndSpecificsId(
      WalletMetadataSpecifics::CARD, specifics_id);
}

WalletMetadataSpecifics CreateWalletMetadataSpecificsForAddressWithDetails(
    const std::string& specifics_id,
    size_t use_count,
    int64_t use_date,
    bool has_converted = false) {
  WalletMetadataSpecifics specifics;
  specifics.set_id(specifics_id);
  specifics.set_type(WalletMetadataSpecifics::ADDRESS);
  specifics.set_use_count(use_count);
  specifics.set_use_date(use_date);
  // False is the default value according to the constructor of AutofillProfile.
  specifics.set_address_has_converted(has_converted);
  return specifics;
}

WalletMetadataSpecifics CreateWalletMetadataSpecificsForAddress(
    const std::string& specifics_id) {
  // Set default values according to the constructor of AutofillProfile (the
  // clock value is overrided by TestAutofillClock in the test fixture).
  return CreateWalletMetadataSpecificsForAddressWithDetails(
      specifics_id, /*use_count=*/1,
      /*use_date=*/UseDateToProtoValue(kJune2017));
}

WalletMetadataSpecifics CreateWalletMetadataSpecificsForCardWithDetails(
    const std::string& specifics_id,
    size_t use_count,
    int64_t use_date,
    const std::string& billing_address_id = "") {
  WalletMetadataSpecifics specifics;
  specifics.set_id(specifics_id);
  specifics.set_type(WalletMetadataSpecifics::CARD);
  specifics.set_use_count(use_count);
  specifics.set_use_date(use_date);
  // "" is the default value according to the constructor of AutofillProfile;
  // this field is Base64 encoded in the protobuf.
  specifics.set_card_billing_address_id(GetBase64EncodedId(billing_address_id));
  return specifics;
}

WalletMetadataSpecifics CreateWalletMetadataSpecificsForCard(
    const std::string& specifics_id) {
  // Set default values according to the constructor of AutofillProfile (the
  // clock value is overrided by TestAutofillClock in the test fixture).
  return CreateWalletMetadataSpecificsForCardWithDetails(
      specifics_id, /*use_count=*/1,
      /*use_date=*/UseDateToProtoValue(kJune2017));
}

AutofillProfile CreateServerProfileWithDetails(const std::string& server_id,
                                               size_t use_count,
                                               int64_t use_date,
                                               bool has_converted = false) {
  AutofillProfile profile = CreateServerProfile(server_id);
  profile.set_use_count(use_count);
  profile.set_use_date(UseDateFromProtoValue(use_date));
  profile.set_has_converted(has_converted);
  return profile;
}

CreditCard CreateServerCreditCardWithDetails(
    const std::string& server_id,
    size_t use_count,
    int64_t use_date,
    const std::string& billing_address_id = "") {
  CreditCard card = CreateServerCreditCard(server_id);
  card.set_use_count(use_count);
  card.set_use_date(UseDateFromProtoValue(use_date));
  card.set_billing_address_id(billing_address_id);
  return card;
}

AutofillProfile CreateServerProfileFromSpecifics(
    const WalletMetadataSpecifics& specifics) {
  return CreateServerProfileWithDetails(
      GetBase64DecodedId(specifics.id()), specifics.use_count(),
      specifics.use_date(), specifics.address_has_converted());
}

CreditCard CreateServerCreditCardFromSpecifics(
    const WalletMetadataSpecifics& specifics) {
  return CreateServerCreditCardWithDetails(
      GetBase64DecodedId(specifics.id()), specifics.use_count(),
      specifics.use_date(),
      GetBase64DecodedId(specifics.card_billing_address_id()));
}

void ExtractWalletMetadataSpecificsFromDataBatch(
    std::unique_ptr<DataBatch> batch,
    std::vector<WalletMetadataSpecifics>* output) {
  while (batch->HasNext()) {
    const KeyAndData& data_pair = batch->Next();
    output->push_back(data_pair.second->specifics.wallet_metadata());
  }
}

std::string WalletMetadataSpecificsAsDebugString(
    const WalletMetadataSpecifics& specifics) {
  std::ostringstream output;
  output << "[id: " << specifics.id()
         << ", type: " << static_cast<int>(specifics.type())
         << ", use_count: " << specifics.use_count()
         << ", use_date: " << specifics.use_date()
         << ", card_billing_address_id: "
         << (specifics.has_card_billing_address_id()
                 ? specifics.card_billing_address_id()
                 : "not_set")
         << ", address_has_converted: "
         << (specifics.has_address_has_converted()
                 ? (specifics.address_has_converted() ? "true" : "false")
                 : "not_set")
         << "]";
  return output.str();
}

std::vector<std::string> GetSortedSerializedSpecifics(
    const std::vector<WalletMetadataSpecifics>& specifics) {
  std::vector<std::string> serialized;
  for (const WalletMetadataSpecifics& entry : specifics) {
    serialized.push_back(entry.SerializeAsString());
  }
  std::sort(serialized.begin(), serialized.end());
  return serialized;
}

MATCHER_P(EqualsSpecifics, expected, "") {
  if (arg.SerializeAsString() != expected.SerializeAsString()) {
    *result_listener << "entry\n"
                     << WalletMetadataSpecificsAsDebugString(arg) << "\n"
                     << "did not match expected\n"
                     << WalletMetadataSpecificsAsDebugString(expected);
    return false;
  }
  return true;
}

MATCHER_P(HasSpecifics, expected, "") {
  const WalletMetadataSpecifics& arg_specifics =
      arg->specifics.wallet_metadata();

  if (arg_specifics.SerializeAsString() != expected.SerializeAsString()) {
    *result_listener << "entry\n"
                     << WalletMetadataSpecificsAsDebugString(arg_specifics)
                     << "\ndid not match expected\n"
                     << WalletMetadataSpecificsAsDebugString(expected);
    return false;
  }
  return true;
}

}  // namespace

class AutofillWalletMetadataSyncBridgeTest : public testing::Test {
 public:
  AutofillWalletMetadataSyncBridgeTest() {}
  ~AutofillWalletMetadataSyncBridgeTest() override {}

  void SetUp() override {
    // Fix a time for implicitly constructed use_dates in AutofillProfile.
    test_clock_.SetNow(kJune2017);
    CountryNames::SetLocaleString(kLocaleString);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
    ON_CALL(*backend(), GetDatabase()).WillByDefault(Return(&db_));
    ResetProcessor();
  }

  void ResetProcessor() {
    real_processor_ =
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            syncer::AUTOFILL_WALLET_METADATA, /*dump_stack=*/base::DoNothing(),
            /*commit_only=*/false);
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
  }

  void ResetBridge(bool initial_sync_done = true) {
    sync_pb::ModelTypeState model_type_state;
    model_type_state.set_initial_sync_done(initial_sync_done);
    EXPECT_TRUE(table()->UpdateModelTypeState(syncer::AUTOFILL_WALLET_METADATA,
                                              model_type_state));
    bridge_.reset(new AutofillWalletMetadataSyncBridge(
        mock_processor_.CreateForwardingProcessor(), &backend_));
  }

  void StartSyncing(
      const std::vector<WalletMetadataSpecifics>& remote_data = {}) {
    base::RunLoop loop;
    syncer::DataTypeActivationRequest request;
    request.error_handler = base::DoNothing();
    real_processor_->OnSyncStarting(
        request,
        base::BindLambdaForTesting(
            [&loop](std::unique_ptr<syncer::DataTypeActivationResponse>) {
              loop.Quit();
            }));
    loop.Run();

    ReceiveUpdates(remote_data);
  }

  void ReceiveUpdates(
      const std::vector<WalletMetadataSpecifics>& remote_data = {}) {
    // Make sure each update has an updated response version so that it does not
    // get filtered out as reflection by the processor.
    ++response_version;
    // After this update initial sync is for sure done.
    sync_pb::ModelTypeState state;
    state.set_initial_sync_done(true);

    syncer::UpdateResponseDataList updates;
    for (const WalletMetadataSpecifics& specifics : remote_data) {
      updates.push_back(SpecificsToUpdateResponse(specifics));
    }
    real_processor_->OnUpdateReceived(state, updates);
  }

  EntityData SpecificsToEntity(const WalletMetadataSpecifics& specifics) {
    EntityData data;
    *data.specifics.mutable_wallet_metadata() = specifics;
    data.client_tag_hash = syncer::GenerateSyncableHash(
        syncer::AUTOFILL_WALLET_METADATA, bridge()->GetClientTag(data));
    return data;
  }

  syncer::UpdateResponseData SpecificsToUpdateResponse(
      const WalletMetadataSpecifics& specifics) {
    syncer::UpdateResponseData data;
    data.entity = SpecificsToEntity(specifics).PassToPtr();
    data.response_version = response_version;
    return data;
  }

  std::vector<WalletMetadataSpecifics> GetAllLocalData() {
    std::vector<WalletMetadataSpecifics> data;
    // Perform an async call synchronously for testing.
    base::RunLoop loop;
    bridge()->GetAllDataForDebugging(base::BindLambdaForTesting(
        [&loop, &data](std::unique_ptr<DataBatch> batch) {
          ExtractWalletMetadataSpecificsFromDataBatch(std::move(batch), &data);
          loop.Quit();
        }));
    loop.Run();
    return data;
  }

  // Like GetAllData() but it also checks that cache is consistent with the disk
  // content.
  std::vector<WalletMetadataSpecifics> GetAllLocalDataInclRestart() {
    std::vector<WalletMetadataSpecifics> data_before = GetAllLocalData();
    ResetProcessor();
    ResetBridge();
    std::vector<WalletMetadataSpecifics> data_after = GetAllLocalData();
    EXPECT_THAT(GetSortedSerializedSpecifics(data_before),
                ElementsAreArray(GetSortedSerializedSpecifics(data_after)));
    return data_after;
  }

  std::vector<WalletMetadataSpecifics> GetLocalData(
      AutofillWalletMetadataSyncBridge::StorageKeyList storage_keys) {
    std::vector<WalletMetadataSpecifics> data;
    // Perform an async call synchronously for testing.
    base::RunLoop loop;
    bridge()->GetData(storage_keys,
                      base::BindLambdaForTesting(
                          [&loop, &data](std::unique_ptr<DataBatch> batch) {
                            ExtractWalletMetadataSpecificsFromDataBatch(
                                std::move(batch), &data);
                            loop.Quit();
                          }));
    loop.Run();
    return data;
  }

  AutofillWalletMetadataSyncBridge* bridge() { return bridge_.get(); }

  syncer::MockModelTypeChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  AutofillTable* table() { return &table_; }

  MockAutofillWebDataBackend* backend() { return &backend_; }

 private:
  int response_version = 0;
  autofill::TestAutofillClock test_clock_;
  ScopedTempDir temp_dir_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  testing::NiceMock<MockAutofillWebDataBackend> backend_;
  AutofillTable table_;
  WebDatabase db_;
  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::ClientTagBasedModelTypeProcessor> real_processor_;
  std::unique_ptr<AutofillWalletMetadataSyncBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletMetadataSyncBridgeTest);
};

// The following 2 tests make sure client tags stay stable.
TEST_F(AutofillWalletMetadataSyncBridgeTest, GetClientTagForAddress) {
  ResetBridge();
  WalletMetadataSpecifics specifics =
      CreateWalletMetadataSpecificsForAddress(kAddr1SpecificsId);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            kAddr1SyncTag);
}

TEST_F(AutofillWalletMetadataSyncBridgeTest, GetClientTagForCard) {
  ResetBridge();
  WalletMetadataSpecifics specifics =
      CreateWalletMetadataSpecificsForCard(kCard1SpecificsId);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            kCard1SyncTag);
}

// The following 2 tests make sure storage keys stay stable.
TEST_F(AutofillWalletMetadataSyncBridgeTest, GetStorageKeyForAddress) {
  ResetBridge();
  WalletMetadataSpecifics specifics =
      CreateWalletMetadataSpecificsForAddress(kAddr1SpecificsId);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics)),
            GetAddressStorageKey(kAddr1SpecificsId));
}

TEST_F(AutofillWalletMetadataSyncBridgeTest, GetStorageKeyForCard) {
  ResetBridge();
  WalletMetadataSpecifics specifics =
      CreateWalletMetadataSpecificsForCard(kCard1SpecificsId);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics)),
            GetCardStorageKey(kCard1SpecificsId));
}

TEST_F(AutofillWalletMetadataSyncBridgeTest,
       GetAllDataForDebugging_ShouldReturnAllData) {
  table()->SetServerProfiles({CreateServerProfile(kAddr1ServerId),
                              CreateServerProfile(kAddr2ServerId)});
  table()->SetServerCreditCards({CreateServerCreditCard(kCard1ServerId),
                                 CreateServerCreditCard(kCard2ServerId)});
  ResetBridge();

  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(
          EqualsSpecifics(
              CreateWalletMetadataSpecificsForAddress(kAddr1SpecificsId)),
          EqualsSpecifics(
              CreateWalletMetadataSpecificsForAddress(kAddr2SpecificsId)),
          EqualsSpecifics(
              CreateWalletMetadataSpecificsForCard(kCard1SpecificsId)),
          EqualsSpecifics(
              CreateWalletMetadataSpecificsForCard(kCard2SpecificsId))));
}

TEST_F(AutofillWalletMetadataSyncBridgeTest,
       GetData_ShouldNotReturnNonexistentData) {
  ResetBridge();
  EXPECT_THAT(GetLocalData({GetAddressStorageKey(kAddr1SpecificsId)}),
              IsEmpty());
}

TEST_F(AutofillWalletMetadataSyncBridgeTest, GetData_ShouldReturnSelectedData) {
  table()->SetServerProfiles({CreateServerProfile(kAddr1ServerId),
                              CreateServerProfile(kAddr2ServerId)});
  table()->SetServerCreditCards({CreateServerCreditCard(kCard1ServerId),
                                 CreateServerCreditCard(kCard2ServerId)});
  ResetBridge();

  EXPECT_THAT(GetLocalData({GetAddressStorageKey(kAddr1SpecificsId),
                            GetCardStorageKey(kCard1SpecificsId)}),
              UnorderedElementsAre(
                  EqualsSpecifics(CreateWalletMetadataSpecificsForAddress(
                      kAddr1SpecificsId)),
                  EqualsSpecifics(CreateWalletMetadataSpecificsForCard(
                      kCard1SpecificsId))));
}

TEST_F(AutofillWalletMetadataSyncBridgeTest, GetData_ShouldReturnCompleteData) {
  AutofillProfile profile = CreateServerProfile(kAddr1ServerId);
  profile.set_use_count(5);
  profile.set_use_date(UseDateFromProtoValue(2));
  profile.set_has_converted(true);
  table()->SetServerProfiles({profile});

  CreditCard card = CreateServerCreditCard(kCard1ServerId);
  card.set_use_count(6);
  card.set_use_date(UseDateFromProtoValue(3));
  card.set_billing_address_id(kAddr1ServerId);
  table()->SetServerCreditCards({card});
  ResetBridge();

  // Expect to retrieve following specifics:
  WalletMetadataSpecifics profile_specifics =
      CreateWalletMetadataSpecificsForAddress(kAddr1SpecificsId);
  profile_specifics.set_use_count(5);
  profile_specifics.set_use_date(2);
  profile_specifics.set_address_has_converted(true);

  WalletMetadataSpecifics card_specifics =
      CreateWalletMetadataSpecificsForCard(kCard1SpecificsId);
  card_specifics.set_use_count(6);
  card_specifics.set_use_date(3);
  card_specifics.set_card_billing_address_id(kAddr1SpecificsId);

  EXPECT_THAT(GetLocalData({GetAddressStorageKey(kAddr1SpecificsId),
                            GetCardStorageKey(kCard1SpecificsId)}),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                                   EqualsSpecifics(card_specifics)));
}

// Verify that lower values of metadata are not sent to the sync server when
// local metadata is updated.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       DontSendLowerValueToServerOnUpdate) {
  table()->SetServerProfiles({CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/2, /*use_date=*/5)});
  table()->SetServerCreditCards({CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/3, /*use_date=*/6)});
  ResetBridge();

  AutofillProfile updated_profile = CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/1, /*use_date=*/4);
  CreditCard updated_card = CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/2, /*use_date=*/5);

  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);

  bridge()->AutofillProfileChanged(
      AutofillProfileChange(AutofillProfileChange::UPDATE,
                            updated_profile.server_id(), &updated_profile));
  bridge()->CreditCardChanged(CreditCardChange(
      CreditCardChange::UPDATE, updated_card.server_id(), &updated_card));

  // Check that also the local metadata did not get updated.
  EXPECT_THAT(
      GetAllLocalDataInclRestart(),
      UnorderedElementsAre(
          EqualsSpecifics(CreateWalletMetadataSpecificsForAddressWithDetails(
              kAddr1SpecificsId, /*use_count=*/2, /*use_date=*/5)),
          EqualsSpecifics(CreateWalletMetadataSpecificsForCardWithDetails(
              kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6))));
}

// Verify that lower values of metadata are not sent to the sync server when
// local metadata is created (tests the case when metadata with higher use
// counts arrive before the data, the data bridge later notifies about creation
// for data that is already there).
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       DontSendLowerValueToServerOnCreation) {
  table()->SetServerProfiles({CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/2, /*use_date=*/5)});
  table()->SetServerCreditCards({CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/3, /*use_date=*/6)});
  ResetBridge();

  AutofillProfile updated_profile = CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/1, /*use_date=*/4);
  CreditCard updated_card = CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/2, /*use_date=*/5);

  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);

  bridge()->AutofillProfileChanged(
      AutofillProfileChange(AutofillProfileChange::ADD,
                            updated_profile.server_id(), &updated_profile));
  bridge()->CreditCardChanged(CreditCardChange(
      CreditCardChange::ADD, updated_card.server_id(), &updated_card));

  // Check that also the local metadata did not get updated.
  EXPECT_THAT(
      GetAllLocalDataInclRestart(),
      UnorderedElementsAre(
          EqualsSpecifics(CreateWalletMetadataSpecificsForAddressWithDetails(
              kAddr1SpecificsId, /*use_count=*/2, /*use_date=*/5)),
          EqualsSpecifics(CreateWalletMetadataSpecificsForCardWithDetails(
              kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6))));
}

// Verify that higher values of metadata are sent to the sync server when local
// metadata is updated.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       SendHigherValuesToServerOnLocalUpdate) {
  table()->SetServerProfiles({CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/1, /*use_date=*/2)});
  table()->SetServerCreditCards({CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/3, /*use_date=*/4)});
  ResetBridge();

  AutofillProfile updated_profile = CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/10, /*use_date=*/20);
  CreditCard updated_card = CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/30, /*use_date=*/40);

  WalletMetadataSpecifics expected_profile_specifics =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/20);
  WalletMetadataSpecifics expected_card_specifics =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/40);

  EXPECT_CALL(
      mock_processor(),
      Put(kAddr1StorageKey, HasSpecifics(expected_profile_specifics), _));
  EXPECT_CALL(mock_processor(),
              Put(kCard1StorageKey, HasSpecifics(expected_card_specifics), _));

  bridge()->AutofillProfileChanged(
      AutofillProfileChange(AutofillProfileChange::UPDATE,
                            updated_profile.server_id(), &updated_profile));
  bridge()->CreditCardChanged(CreditCardChange(
      CreditCardChange::UPDATE, updated_card.server_id(), &updated_card));

  // Check that the local metadata got update as well.
  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(expected_profile_specifics),
                                   EqualsSpecifics(expected_card_specifics)));
}

// Verify that one-off addition of metadata is sent to the sync server.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       SendNewDataToServerOnLocalAddition) {
  ResetBridge();
  AutofillProfile new_profile = CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/10, /*use_date=*/20);
  CreditCard new_card = CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/30, /*use_date=*/40);

  WalletMetadataSpecifics expected_profile_specifics =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/20);
  WalletMetadataSpecifics expected_card_specifics =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/40);

  EXPECT_CALL(
      mock_processor(),
      Put(kAddr1StorageKey, HasSpecifics(expected_profile_specifics), _));
  EXPECT_CALL(mock_processor(),
              Put(kCard1StorageKey, HasSpecifics(expected_card_specifics), _));

  bridge()->AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::ADD, new_profile.server_id(), &new_profile));
  bridge()->CreditCardChanged(
      CreditCardChange(CreditCardChange::ADD, new_card.server_id(), &new_card));

  // Check that the new metadata got created as well.
  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(expected_profile_specifics),
                                   EqualsSpecifics(expected_card_specifics)));
}

// Verify that one-off addition of metadata is sent to the sync server (even
// though it is notified as an update). This tests that the bridge is robust and
// recreates metadata that may get deleted in the mean-time).
TEST_F(AutofillWalletMetadataSyncBridgeTest, SendNewDataToServerOnLocalUpdate) {
  ResetBridge();
  AutofillProfile new_profile = CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/10, /*use_date=*/20);
  CreditCard new_card = CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/30, /*use_date=*/40);

  WalletMetadataSpecifics expected_profile_specifics =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/20);
  WalletMetadataSpecifics expected_card_specifics =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/40);

  EXPECT_CALL(
      mock_processor(),
      Put(kAddr1StorageKey, HasSpecifics(expected_profile_specifics), _));
  EXPECT_CALL(mock_processor(),
              Put(kCard1StorageKey, HasSpecifics(expected_card_specifics), _));

  bridge()->AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::UPDATE, new_profile.server_id(), &new_profile));
  bridge()->CreditCardChanged(CreditCardChange(
      CreditCardChange::UPDATE, new_card.server_id(), &new_card));

  // Check that the new metadata got created as well.
  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(expected_profile_specifics),
                                   EqualsSpecifics(expected_card_specifics)));
}

// Verify that one-off deletion of existing metadata is sent to the sync server.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       DeleteExistingDataFromServerOnLocalDeletion) {
  AutofillProfile existing_profile = CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/10, /*use_date=*/20);
  CreditCard existing_card = CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/30, /*use_date=*/40);
  table()->SetServerProfiles({existing_profile});
  table()->SetServerCreditCards({existing_card});
  ResetBridge();

  EXPECT_CALL(mock_processor(), Delete(kAddr1StorageKey, _));
  EXPECT_CALL(mock_processor(), Delete(kCard1StorageKey, _));

  bridge()->AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::REMOVE, existing_profile.server_id(), nullptr));
  bridge()->CreditCardChanged(CreditCardChange(
      CreditCardChange::REMOVE, existing_card.server_id(), nullptr));

  // Check that there is no metadata anymore.
  EXPECT_THAT(GetAllLocalDataInclRestart(), IsEmpty());
}

// Verify that deletion of non-existing metadata is not sent to the sync server.
TEST_F(AutofillWalletMetadataSyncBridgeTest,
       DoNotDeleteNonExistingDataFromServerOnLocalDeletion) {
  AutofillProfile existing_profile = CreateServerProfileWithDetails(
      kAddr1ServerId, /*use_count=*/10, /*use_date=*/20);
  CreditCard existing_card = CreateServerCreditCardWithDetails(
      kCard1ServerId, /*use_count=*/30, /*use_date=*/40);
  // Save only data and not metadata.
  table()->SetServerAddressesData({existing_profile});
  table()->SetServerCardsData({existing_card});
  ResetBridge();

  // Check that there is no metadata, from start on.
  ASSERT_THAT(GetAllLocalDataInclRestart(), IsEmpty());

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);

  bridge()->AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::REMOVE, existing_profile.server_id(), nullptr));
  bridge()->CreditCardChanged(CreditCardChange(
      CreditCardChange::REMOVE, existing_card.server_id(), nullptr));

  // Check that there is also no metadata at the end.
  EXPECT_THAT(GetAllLocalDataInclRestart(), IsEmpty());
}

enum RemoteChangesMode {
  INITIAL_SYNC_ADD,  // Initial sync -> ADD changes.
  LATER_SYNC_ADD,    // Later sync; the client receives the data for the first
                     // time -> ADD changes.
  LATER_SYNC_UPDATE  // Later sync; the client has received the data before ->
                     // UPDATE changes.
};

// Parametrized fixture for tests that apply in the same way for all
// RemoteChangesModes.
class AutofillWalletMetadataSyncBridgeRemoteChangesTest
    : public testing::WithParamInterface<RemoteChangesMode>,
      public AutofillWalletMetadataSyncBridgeTest {
 public:
  AutofillWalletMetadataSyncBridgeRemoteChangesTest() {}
  ~AutofillWalletMetadataSyncBridgeRemoteChangesTest() override {}

  void ResetBridgeWithPotentialInitialSync(
      const std::vector<WalletMetadataSpecifics>& remote_data) {
    AutofillWalletMetadataSyncBridgeTest::ResetBridge(
        /*initial_sync_done=*/GetParam() != INITIAL_SYNC_ADD);

    if (GetParam() == LATER_SYNC_UPDATE) {
      StartSyncing(remote_data);
    }
  }

  void ReceivePotentiallyInitialUpdates(
      const std::vector<WalletMetadataSpecifics>& remote_data = {}) {
    if (GetParam() != LATER_SYNC_UPDATE) {
      StartSyncing(remote_data);
    } else {
      AutofillWalletMetadataSyncBridgeTest::ReceiveUpdates(remote_data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillWalletMetadataSyncBridgeRemoteChangesTest);
};

// No upstream communication or local DB change happens if the server sends an
// empty update.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest, EmptyUpdateIgnored) {
  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);

  ResetBridgeWithPotentialInitialSync({});
  ReceivePotentiallyInitialUpdates({});

  EXPECT_THAT(GetAllLocalDataInclRestart(), IsEmpty());
}

// No upstream communication or local DB change happens if the server sends the
// same data as we have locally.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest, SameDataIgnored) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/2, /*use_date=*/5);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({profile, card});

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);

  ReceivePotentiallyInitialUpdates({profile, card});

  EXPECT_THAT(
      GetAllLocalDataInclRestart(),
      UnorderedElementsAre(EqualsSpecifics(profile), EqualsSpecifics(card)));
}

// Tests that if the remote use stats are higher / newer, they should win over
// local stats.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_PreferHigherValues_RemoteWins) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/2, /*use_date=*/5);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({profile, card});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/20, /*use_date=*/50);
  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/60);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);

  ReceivePotentiallyInitialUpdates(
      {updated_remote_profile, updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(updated_remote_profile),
                                   EqualsSpecifics(updated_remote_card)));
}

// Tests that if the local use stats are higher / newer, they should win over
// remote stats.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_PreferHigherValues_LocalWins) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/20, /*use_date=*/50);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/60);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({profile, card});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/2, /*use_date=*/5);
  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(),
              Put(kAddr1StorageKey, HasSpecifics(profile), _));
  EXPECT_CALL(mock_processor(), Put(kCard1StorageKey, HasSpecifics(card), _));

  ReceivePotentiallyInitialUpdates(
      {updated_remote_profile, updated_remote_card});

  EXPECT_THAT(
      GetAllLocalDataInclRestart(),
      UnorderedElementsAre(EqualsSpecifics(profile), EqualsSpecifics(card)));
}

// Tests that the conflicts are resolved component-wise (a higher use_count is
// taken from local data, a newer use_data is taken from remote data).
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_PreferHigherValues_BothWin1) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/20, /*use_date=*/5);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/6);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({profile, card});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/2, /*use_date=*/50);
  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/60);

  WalletMetadataSpecifics merged_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/20, /*use_date=*/50);
  WalletMetadataSpecifics merged_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/60);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(),
              Put(kAddr1StorageKey, HasSpecifics(merged_profile), _));
  EXPECT_CALL(mock_processor(),
              Put(kCard1StorageKey, HasSpecifics(merged_card), _));

  ReceivePotentiallyInitialUpdates(
      {updated_remote_profile, updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(merged_profile),
                                   EqualsSpecifics(merged_card)));
}

// Tests that the conflicts are resolved component-wise, like the previous test,
// only the other way around (a higher use_count is taken from remote data, a
// newer use_data is taken from local data).
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_PreferHigherValues_BothWin2) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/2, /*use_date=*/50);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/60);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({profile, card});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/20, /*use_date=*/5);
  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/6);

  WalletMetadataSpecifics merged_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/20, /*use_date=*/50);
  WalletMetadataSpecifics merged_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/60);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(),
              Put(kAddr1StorageKey, HasSpecifics(merged_profile), _));
  EXPECT_CALL(mock_processor(),
              Put(kCard1StorageKey, HasSpecifics(merged_card), _));

  ReceivePotentiallyInitialUpdates(
      {updated_remote_profile, updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(merged_profile),
                                   EqualsSpecifics(merged_card)));
}

// No merge logic is applied if local data has initial use_count (=1). In this
// situation, we just take over the remote entity completely.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_PreferRemoteIfLocalHasInitialUseCount) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/1, /*use_date=*/50);
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/1, /*use_date=*/60);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({profile, card});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/20, /*use_date=*/5);
  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/6);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);

  ReceivePotentiallyInitialUpdates(
      {updated_remote_profile, updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(updated_remote_profile),
                                   EqualsSpecifics(updated_remote_card)));
}

// Tests that with a conflict in billing_address_id, we prefer an ID of a local
// profile over an ID of a server profile. In this test, the preferred ID is in
// the remote update that we need to store locally.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Card_PreferLocalBillingAddressId_RemoteWins) {
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6,
          /*billing_address_id=*/kAddr1ServerId);

  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({card});

  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6,
          /*billing_address_id=*/kLocalAddr1ServerId);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);

  ReceivePotentiallyInitialUpdates({updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(updated_remote_card)));
}

// Tests that with a conflict in billing_address_id, we prefer an ID of a local
// profile over an ID of a server profile. In this test, the preferred ID is in
// the local data that we need to upstream.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Card_PreferLocalBillingAddressId_LocalWins) {
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6,
          /*billing_address_id=*/kLocalAddr1ServerId);

  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({card});

  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6,
          /*billing_address_id=*/kAddr1ServerId);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(), Put(kCard1StorageKey, HasSpecifics(card), _));

  ReceivePotentiallyInitialUpdates({updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(card)));
}

// Tests that if both addresses have billing address ids of local profiles, we
// prefer the one from the most recently used entity. In this test, it is the
// remote entity.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Card_PreferNewerBillingAddressOutOfLocalIds_RemoteWins) {
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6,
          /*billing_address_id=*/kLocalAddr1ServerId);

  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({card});

  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/60,
          /*billing_address_id=*/kLocalAddr2ServerId);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);

  ReceivePotentiallyInitialUpdates({updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(updated_remote_card)));
}

// Tests that if both addresses have billing address ids of local profiles, we
// prefer the one from the most recently used entity. In this test, it is the
// local entity.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Card_PreferNewerBillingAddressOutOfLocalIds_LocalWins) {
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/60,
          /*billing_address_id=*/kLocalAddr1ServerId);

  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({card});

  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6,
          /*billing_address_id=*/kLocalAddr2ServerId);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(), Put(kCard1StorageKey, HasSpecifics(card), _));

  ReceivePotentiallyInitialUpdates({updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(card)));
}

// Tests that if both addresses have billing address ids of server profiles, we
// prefer the one from the most recently used entity. In this test, it is the
// remote entity.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Card_PreferNewerBillingAddressOutOfServerIds_RemoteWins) {
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6,
          /*billing_address_id=*/kAddr1ServerId);

  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({card});

  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/60,
          /*billing_address_id=*/kAddr2ServerId);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);

  ReceivePotentiallyInitialUpdates({updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(updated_remote_card)));
}

// Tests that if both addresses have billing address ids of server profiles, we
// prefer the one from the most recently used entity. In this test, it is the
// local entity.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Card_PreferNewerBillingAddressOutOfServerIds_LocalWins) {
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/60,
          /*billing_address_id=*/kAddr1ServerId);

  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({card});

  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/6,
          /*billing_address_id=*/kAddr2ServerId);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(), Put(kCard1StorageKey, HasSpecifics(card), _));

  ReceivePotentiallyInitialUpdates({updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(card)));
}

// Tests that the conflict resolution happens component-wise. To avoid
// combinatorial explosion, this only tests when both have billing address ids
// of server profiles, one entity is more recently used but the other entity has
// a higher use_count. We should pick the billing_address_id of the newer one
// but have the use_count updated to the maximum as well. In this test, the
// remote entity is the more recently used.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Card_PreferNewerBillingAddressOutOfServerIds_BothWin1) {
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/6,
          /*billing_address_id=*/kAddr1ServerId);

  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({card});

  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/60,
          /*billing_address_id=*/kAddr2ServerId);

  WalletMetadataSpecifics merged_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/60,
          /*billing_address_id=*/kAddr2ServerId);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(),
              Put(kCard1StorageKey, HasSpecifics(merged_card), _));

  ReceivePotentiallyInitialUpdates({updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(merged_card)));
}

// Tests that the conflict resolution happens component-wise. To avoid
// combinatorial explosion, this only tests when both have billing address ids
// of server profiles, one entity is more recently used but the other entity has
// a higher use_count. We should pick the billing_address_id of the newer one
// but have the use_count updated to the maximum as well. In this test, the
// local entity is the more recently used.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Card_PreferNewerBillingAddressOutOfServerIds_BothWin2) {
  WalletMetadataSpecifics card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/3, /*use_date=*/60,
          /*billing_address_id=*/kAddr1ServerId);

  table()->SetServerCreditCards({CreateServerCreditCardFromSpecifics(card)});
  ResetBridgeWithPotentialInitialSync({card});

  WalletMetadataSpecifics updated_remote_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/6,
          /*billing_address_id=*/kAddr2ServerId);

  WalletMetadataSpecifics merged_card =
      CreateWalletMetadataSpecificsForCardWithDetails(
          kCard1SpecificsId, /*use_count=*/30, /*use_date=*/60,
          /*billing_address_id=*/kAddr1ServerId);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(),
              Put(kCard1StorageKey, HasSpecifics(merged_card), _));

  ReceivePotentiallyInitialUpdates({updated_remote_card});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(merged_card)));
}

// Tests that if the has_converted bit differs, we always end up with the true
// value. This test has the remote entity converted which should get updated in
// the local DB.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Address_HasConverted_RemoteWins) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/1, /*use_date=*/50,
          /*has_converted=*/false);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  ResetBridgeWithPotentialInitialSync({profile});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/1, /*use_date=*/50,
          /*has_converted=*/true);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);

  ReceivePotentiallyInitialUpdates({updated_remote_profile});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(updated_remote_profile)));
}

// Tests that if the has_converted bit differs, we always end up with the true
// value. This test has the local entity converted which should get upstreamed.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Address_HasConverted_LocalWins) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/1, /*use_date=*/50,
          /*has_converted=*/true);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  ResetBridgeWithPotentialInitialSync({profile});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/1, /*use_date=*/50,
          /*has_converted=*/false);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(),
              Put(kAddr1StorageKey, HasSpecifics(profile), _));

  ReceivePotentiallyInitialUpdates({updated_remote_profile});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(profile)));
}

// Tests that the conflict resolution happens component-wise. If one entity
// has_converted but the other entity has higher use_count, we should end up
// with an entity that has_converted and has the higher use_count. This test has
// the remote entity converted.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Address_HasConverted_BothWin1) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/50,
          /*has_converted=*/false);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  ResetBridgeWithPotentialInitialSync({profile});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/1, /*use_date=*/50,
          /*has_converted=*/true);

  WalletMetadataSpecifics merged_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/50,
          /*has_converted=*/true);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(),
              Put(kAddr1StorageKey, HasSpecifics(merged_profile), _));

  ReceivePotentiallyInitialUpdates({updated_remote_profile});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(merged_profile)));
}

// Tests that the conflict resolution happens component-wise. If one entity
// has_converted but the other entity has higher use_count, we should end up
// with an entity that has_converted and has the higher use_count. This test has
// the local entity converted.
TEST_P(AutofillWalletMetadataSyncBridgeRemoteChangesTest,
       Conflict_Address_HasConverted_BothWin2) {
  WalletMetadataSpecifics profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/1, /*use_date=*/50,
          /*has_converted=*/true);

  table()->SetServerProfiles({CreateServerProfileFromSpecifics(profile)});
  ResetBridgeWithPotentialInitialSync({profile});

  WalletMetadataSpecifics updated_remote_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/50,
          /*has_converted=*/false);

  WalletMetadataSpecifics merged_profile =
      CreateWalletMetadataSpecificsForAddressWithDetails(
          kAddr1SpecificsId, /*use_count=*/10, /*use_date=*/50,
          /*has_converted=*/true);

  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(mock_processor(),
              Put(kAddr1StorageKey, HasSpecifics(merged_profile), _));

  ReceivePotentiallyInitialUpdates({updated_remote_profile});

  EXPECT_THAT(GetAllLocalDataInclRestart(),
              UnorderedElementsAre(EqualsSpecifics(merged_profile)));
}

INSTANTIATE_TEST_SUITE_P(,
                         AutofillWalletMetadataSyncBridgeRemoteChangesTest,
                         ::testing::Values(INITIAL_SYNC_ADD,
                                           LATER_SYNC_ADD,
                                           LATER_SYNC_UPDATE));

}  // namespace autofill

namespace sync_pb {

// Makes the GMock matchers print out a readable version of the protobuf.
void PrintTo(const WalletMetadataSpecifics& specifics, std::ostream* os) {
  *os << autofill::WalletMetadataSpecificsAsDebugString(specifics);
}

}  // namespace sync_pb
