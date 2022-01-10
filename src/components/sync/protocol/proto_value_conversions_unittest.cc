// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/proto_value_conversions.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/app_setting_specifics.pb.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/extension_setting_specifics.pb.h"
#include "components/sync/protocol/extension_specifics.pb.h"
#include "components/sync/protocol/managed_user_setting_specifics.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/protocol/os_preference_specifics.pb.h"
#include "components/sync/protocol/os_priority_preference_specifics.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/priority_preference_specifics.pb.h"
#include "components/sync/protocol/search_engine_specifics.pb.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sharing_message_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync/protocol/typed_url_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

// Keep this file in sync with the .proto files in this directory.

#define DEFINE_SPECIFICS_TO_VALUE_TEST(Key)                         \
  TEST(ProtoValueConversionsTest, Proto_##Key##_SpecificsToValue) { \
    sync_pb::EntitySpecifics specifics;                             \
    specifics.mutable_##Key();                                      \
    std::unique_ptr<base::DictionaryValue> value(                   \
        EntitySpecificsToValue(specifics));                         \
    EXPECT_EQ(1, static_cast<int>(value->DictSize()));              \
  }

// We'd also like to check if we changed any field in our messages. However,
// that's hard to do: sizeof could work, but it's platform-dependent.
// default_instance().ByteSize() won't change for most changes, since most of
// our fields are optional. So we just settle for comments in the proto files.

DEFINE_SPECIFICS_TO_VALUE_TEST(encrypted)

static_assert(38 == syncer::GetNumModelTypes(),
              "When adding a new field, add a DEFINE_SPECIFICS_TO_VALUE_TEST "
              "for your field below, and optionally a test for the specific "
              "conversions.");

DEFINE_SPECIFICS_TO_VALUE_TEST(app)
DEFINE_SPECIFICS_TO_VALUE_TEST(app_list)
DEFINE_SPECIFICS_TO_VALUE_TEST(app_setting)
DEFINE_SPECIFICS_TO_VALUE_TEST(arc_package)
DEFINE_SPECIFICS_TO_VALUE_TEST(autofill)
DEFINE_SPECIFICS_TO_VALUE_TEST(autofill_offer)
DEFINE_SPECIFICS_TO_VALUE_TEST(autofill_profile)
DEFINE_SPECIFICS_TO_VALUE_TEST(autofill_wallet)
DEFINE_SPECIFICS_TO_VALUE_TEST(bookmark)
DEFINE_SPECIFICS_TO_VALUE_TEST(device_info)
DEFINE_SPECIFICS_TO_VALUE_TEST(dictionary)
DEFINE_SPECIFICS_TO_VALUE_TEST(extension)
DEFINE_SPECIFICS_TO_VALUE_TEST(extension_setting)
DEFINE_SPECIFICS_TO_VALUE_TEST(history_delete_directive)
DEFINE_SPECIFICS_TO_VALUE_TEST(managed_user_setting)
DEFINE_SPECIFICS_TO_VALUE_TEST(nigori)
DEFINE_SPECIFICS_TO_VALUE_TEST(os_preference)
DEFINE_SPECIFICS_TO_VALUE_TEST(os_priority_preference)
DEFINE_SPECIFICS_TO_VALUE_TEST(password)
DEFINE_SPECIFICS_TO_VALUE_TEST(preference)
DEFINE_SPECIFICS_TO_VALUE_TEST(printer)
DEFINE_SPECIFICS_TO_VALUE_TEST(priority_preference)
DEFINE_SPECIFICS_TO_VALUE_TEST(reading_list)
DEFINE_SPECIFICS_TO_VALUE_TEST(search_engine)
DEFINE_SPECIFICS_TO_VALUE_TEST(security_event)
DEFINE_SPECIFICS_TO_VALUE_TEST(send_tab_to_self)
DEFINE_SPECIFICS_TO_VALUE_TEST(session)
DEFINE_SPECIFICS_TO_VALUE_TEST(sharing_message)
DEFINE_SPECIFICS_TO_VALUE_TEST(theme)
DEFINE_SPECIFICS_TO_VALUE_TEST(typed_url)
DEFINE_SPECIFICS_TO_VALUE_TEST(user_consent)
DEFINE_SPECIFICS_TO_VALUE_TEST(user_event)
DEFINE_SPECIFICS_TO_VALUE_TEST(wallet_metadata)
DEFINE_SPECIFICS_TO_VALUE_TEST(web_app)
DEFINE_SPECIFICS_TO_VALUE_TEST(wifi_configuration)
DEFINE_SPECIFICS_TO_VALUE_TEST(workspace_desk)

TEST(ProtoValueConversionsTest, AutofillWalletSpecificsToValue) {
  sync_pb::AutofillWalletSpecifics specifics;
  specifics.mutable_masked_card()->set_name_on_card("Igloo");
  specifics.mutable_address()->set_recipient_name("John");
  specifics.mutable_customer_data()->set_id("123456");
  specifics.mutable_cloud_token_data()->set_masked_card_id("1111");

  specifics.set_type(sync_pb::AutofillWalletSpecifics::UNKNOWN);
  auto value = AutofillWalletSpecificsToValue(specifics);
  EXPECT_FALSE(value->Get("masked_card", nullptr));
  EXPECT_FALSE(value->Get("address", nullptr));
  EXPECT_FALSE(value->Get("customer_data", nullptr));
  EXPECT_FALSE(value->Get("cloud_token_data", nullptr));

  specifics.set_type(sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD);
  value = AutofillWalletSpecificsToValue(specifics);
  EXPECT_TRUE(value->Get("masked_card", nullptr));
  EXPECT_FALSE(value->Get("address", nullptr));
  EXPECT_FALSE(value->Get("customer_data", nullptr));
  EXPECT_FALSE(value->Get("cloud_token_data", nullptr));

  specifics.set_type(sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS);
  value = AutofillWalletSpecificsToValue(specifics);
  EXPECT_FALSE(value->Get("masked_card", nullptr));
  EXPECT_TRUE(value->Get("address", nullptr));
  EXPECT_FALSE(value->Get("customer_data", nullptr));
  EXPECT_FALSE(value->Get("cloud_token_data", nullptr));

  specifics.set_type(sync_pb::AutofillWalletSpecifics::CUSTOMER_DATA);
  value = AutofillWalletSpecificsToValue(specifics);
  EXPECT_FALSE(value->Get("masked_card", nullptr));
  EXPECT_FALSE(value->Get("address", nullptr));
  EXPECT_TRUE(value->Get("customer_data", nullptr));
  EXPECT_FALSE(value->Get("cloud_token_data", nullptr));

  specifics.set_type(
      sync_pb::AutofillWalletSpecifics::CREDIT_CARD_CLOUD_TOKEN_DATA);
  value = AutofillWalletSpecificsToValue(specifics);
  EXPECT_FALSE(value->Get("masked_card", nullptr));
  EXPECT_FALSE(value->Get("address", nullptr));
  EXPECT_FALSE(value->Get("customer_data", nullptr));
  EXPECT_TRUE(value->Get("cloud_token_data", nullptr));
}

TEST(ProtoValueConversionsTest, BookmarkSpecificsData) {
  const base::Time creation_time(base::Time::Now());
  const std::string icon_url = "http://www.google.com/favicon.ico";
  sync_pb::BookmarkSpecifics specifics;
  specifics.set_creation_time_us(creation_time.ToInternalValue());
  specifics.set_icon_url(icon_url);
  sync_pb::MetaInfo* meta_1 = specifics.add_meta_info();
  meta_1->set_key("key1");
  meta_1->set_value("value1");
  sync_pb::MetaInfo* meta_2 = specifics.add_meta_info();
  meta_2->set_key("key2");
  meta_2->set_value("value2");

  std::unique_ptr<base::DictionaryValue> value(
      BookmarkSpecificsToValue(specifics));
  EXPECT_FALSE(value->DictEmpty());
  std::string encoded_time;
  EXPECT_TRUE(value->GetString("creation_time_us", &encoded_time));
  EXPECT_EQ(base::NumberToString(creation_time.ToInternalValue()),
            encoded_time);
  std::string encoded_icon_url;
  EXPECT_TRUE(value->GetString("icon_url", &encoded_icon_url));
  EXPECT_EQ(icon_url, encoded_icon_url);
  base::ListValue* meta_info_list;
  ASSERT_TRUE(value->GetList("meta_info", &meta_info_list));
  EXPECT_EQ(2u, meta_info_list->GetList().size());
  const base::Value* meta_info_value;
  const base::DictionaryValue* meta_info;
  std::string meta_key;
  std::string meta_value;
  meta_info_value = &meta_info_list->GetList()[0];
  ASSERT_TRUE(meta_info_value->is_dict());
  meta_info = &base::Value::AsDictionaryValue(*meta_info_value);
  EXPECT_TRUE(meta_info->GetString("key", &meta_key));
  EXPECT_TRUE(meta_info->GetString("value", &meta_value));
  EXPECT_EQ("key1", meta_key);
  EXPECT_EQ("value1", meta_value);
  meta_info_value = &meta_info_list->GetList()[1];
  ASSERT_TRUE(meta_info_value->is_dict());
  meta_info = &base::Value::AsDictionaryValue(*meta_info_value);
  EXPECT_TRUE(meta_info->GetString("key", &meta_key));
  EXPECT_TRUE(meta_info->GetString("value", &meta_value));
  EXPECT_EQ("key2", meta_key);
  EXPECT_EQ("value2", meta_value);
}

TEST(ProtoValueConversionsTest, UniquePositionToValue) {
  sync_pb::SyncEntity entity;
  entity.mutable_unique_position()->set_custom_compressed_v1("test");

  auto value = SyncEntityToValue(entity, false);
  std::string unique_position;
  EXPECT_TRUE(value->GetString("unique_position", &unique_position));

  std::string expected_unique_position =
      UniquePosition::FromProto(entity.unique_position()).ToDebugString();
  EXPECT_EQ(expected_unique_position, unique_position);
}

TEST(ProtoValueConversionsTest, SyncEntityToValueIncludeSpecifics) {
  sync_pb::SyncEntity entity;
  entity.mutable_specifics();

  auto value = SyncEntityToValue(entity, true /* include_specifics */);
  EXPECT_TRUE(value->GetDictionary("specifics", nullptr));

  value = SyncEntityToValue(entity, false /* include_specifics */);
  EXPECT_FALSE(value->GetDictionary("specifics", nullptr));
}

namespace {
// Returns whether the given value has specifics under the entries in the given
// path.
bool ValueHasSpecifics(const base::DictionaryValue& value,
                       const std::string& path) {
  const base::ListValue* entities_list = nullptr;
  if (!value.GetList(path, &entities_list))
    return false;

  const base::Value& entry_dictionary_value = entities_list->GetList()[0];
  if (!entry_dictionary_value.is_dict())
    return false;

  const base::DictionaryValue& entry_dictionary =
      base::Value::AsDictionaryValue(entry_dictionary_value);
  const base::DictionaryValue* specifics_dictionary = nullptr;
  return entry_dictionary.GetDictionary("specifics", &specifics_dictionary);
}
}  // namespace

// Create a ClientToServerMessage with an EntitySpecifics.  Converting it to
// a value should respect the |include_specifics| flag.
TEST(ProtoValueConversionsTest, ClientToServerMessageToValue) {
  sync_pb::ClientToServerMessage message;
  sync_pb::CommitMessage* commit_message = message.mutable_commit();
  sync_pb::SyncEntity* entity = commit_message->add_entries();
  entity->mutable_specifics();

  std::unique_ptr<base::DictionaryValue> value_with_specifics(
      ClientToServerMessageToValue(message, true /* include_specifics */));
  EXPECT_FALSE(value_with_specifics->DictEmpty());
  EXPECT_TRUE(
      ValueHasSpecifics(*(value_with_specifics.get()), "commit.entries"));

  std::unique_ptr<base::DictionaryValue> value_without_specifics(
      ClientToServerMessageToValue(message, false /* include_specifics */));
  EXPECT_FALSE(value_without_specifics->DictEmpty());
  EXPECT_FALSE(
      ValueHasSpecifics(*(value_without_specifics.get()), "commit.entries"));
}

// Create a ClientToServerResponse with an EntitySpecifics.  Converting it to
// a value should respect the |include_specifics| flag.
TEST(ProtoValueConversionsTest, ClientToServerResponseToValue) {
  sync_pb::ClientToServerResponse message;
  sync_pb::GetUpdatesResponse* response = message.mutable_get_updates();
  sync_pb::SyncEntity* entity = response->add_entries();
  entity->mutable_specifics();

  std::unique_ptr<base::DictionaryValue> value_with_specifics(
      ClientToServerResponseToValue(message, true /* include_specifics */));
  EXPECT_FALSE(value_with_specifics->DictEmpty());
  EXPECT_TRUE(
      ValueHasSpecifics(*(value_with_specifics.get()), "get_updates.entries"));

  std::unique_ptr<base::DictionaryValue> value_without_specifics(
      ClientToServerResponseToValue(message, false /* include_specifics */));
  EXPECT_FALSE(value_without_specifics->DictEmpty());
  EXPECT_FALSE(ValueHasSpecifics(*(value_without_specifics.get()),
                                 "get_updates.entries"));
}

}  // namespace
}  // namespace syncer
