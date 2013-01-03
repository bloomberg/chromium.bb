// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep this file in sync with the .proto files in this directory.

#include "sync/protocol/proto_value_conversions.h"

#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/app_notification_specifics.pb.h"
#include "sync/protocol/app_setting_specifics.pb.h"
#include "sync/protocol/app_specifics.pb.h"
#include "sync/protocol/autofill_specifics.pb.h"
#include "sync/protocol/bookmark_specifics.pb.h"
#include "sync/protocol/device_info_specifics.pb.h"
#include "sync/protocol/encryption.pb.h"
#include "sync/protocol/experiments_specifics.pb.h"
#include "sync/protocol/extension_setting_specifics.pb.h"
#include "sync/protocol/extension_specifics.pb.h"
#include "sync/protocol/nigori_specifics.pb.h"
#include "sync/protocol/password_specifics.pb.h"
#include "sync/protocol/preference_specifics.pb.h"
#include "sync/protocol/search_engine_specifics.pb.h"
#include "sync/protocol/session_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/theme_specifics.pb.h"
#include "sync/protocol/typed_url_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class ProtoValueConversionsTest : public testing::Test {
 protected:
  template <class T>
  void TestSpecificsToValue(
      DictionaryValue* (*specifics_to_value)(const T&)) {
    const T& specifics(T::default_instance());
    scoped_ptr<DictionaryValue> value(specifics_to_value(specifics));
    // We can't do much but make sure that this doesn't crash.
  }
};

TEST_F(ProtoValueConversionsTest, ProtoChangeCheck) {
  // If this number changes, that means we added or removed a data
  // type.  Don't forget to add a unit test for {New
  // type}SpecificsToValue below.
  EXPECT_EQ(21, MODEL_TYPE_COUNT);

  // We'd also like to check if we changed any field in our messages.
  // However, that's hard to do: sizeof could work, but it's
  // platform-dependent.  default_instance().ByteSize() won't change
  // for most changes, since most of our fields are optional.  So we
  // just settle for comments in the proto files.
}

TEST_F(ProtoValueConversionsTest, EncryptedDataToValue) {
  TestSpecificsToValue(EncryptedDataToValue);
}

TEST_F(ProtoValueConversionsTest, SessionHeaderToValue) {
  TestSpecificsToValue(SessionHeaderToValue);
}

TEST_F(ProtoValueConversionsTest, SessionTabToValue) {
  TestSpecificsToValue(SessionTabToValue);
}

TEST_F(ProtoValueConversionsTest, SessionWindowToValue) {
  TestSpecificsToValue(SessionWindowToValue);
}

TEST_F(ProtoValueConversionsTest, TabNavigationToValue) {
  TestSpecificsToValue(TabNavigationToValue);
}

TEST_F(ProtoValueConversionsTest, PasswordSpecificsData) {
  sync_pb::PasswordSpecificsData specifics;
  specifics.set_password_value("secret");
  scoped_ptr<DictionaryValue> value(PasswordSpecificsDataToValue(specifics));
  EXPECT_FALSE(value->empty());
  std::string password_value;
  EXPECT_TRUE(value->GetString("password_value", &password_value));
  EXPECT_EQ("<redacted>", password_value);
}

TEST_F(ProtoValueConversionsTest, AppNotificationToValue) {
  TestSpecificsToValue(AppNotificationToValue);
}

TEST_F(ProtoValueConversionsTest, AppSettingSpecificsToValue) {
  sync_pb::AppNotificationSettings specifics;
  specifics.set_disabled(true);
  specifics.set_oauth_client_id("some_id_value");
  scoped_ptr<DictionaryValue> value(AppSettingsToValue(specifics));
  EXPECT_FALSE(value->empty());
  bool disabled_value = false;
  std::string oauth_client_id_value;
  EXPECT_TRUE(value->GetBoolean("disabled", &disabled_value));
  EXPECT_EQ(true, disabled_value);
  EXPECT_TRUE(value->GetString("oauth_client_id", &oauth_client_id_value));
  EXPECT_EQ("some_id_value", oauth_client_id_value);
}

TEST_F(ProtoValueConversionsTest, AppSpecificsToValue) {
  TestSpecificsToValue(AppSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, AutofillSpecificsToValue) {
  TestSpecificsToValue(AutofillSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, AutofillProfileSpecificsToValue) {
  TestSpecificsToValue(AutofillProfileSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, BookmarkSpecificsToValue) {
  TestSpecificsToValue(BookmarkSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, BookmarkSpecificsData) {
  const base::Time creation_time(base::Time::Now());
  const std::string icon_url = "http://www.google.com/favicon.ico";
  sync_pb::BookmarkSpecifics specifics;
  specifics.set_creation_time_us(creation_time.ToInternalValue());
  specifics.set_icon_url(icon_url);
  scoped_ptr<DictionaryValue> value(BookmarkSpecificsToValue(specifics));
  EXPECT_FALSE(value->empty());
  std::string encoded_time;
  EXPECT_TRUE(value->GetString("creation_time_us", &encoded_time));
  EXPECT_EQ(base::Int64ToString(creation_time.ToInternalValue()), encoded_time);
  std::string encoded_icon_url;
  EXPECT_TRUE(value->GetString("icon_url", &encoded_icon_url));
  EXPECT_EQ(icon_url, encoded_icon_url);
}

TEST_F(ProtoValueConversionsTest, DeviceInfoSpecificsToValue) {
  TestSpecificsToValue(DeviceInfoSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, ExperimentsSpecificsToValue) {
  TestSpecificsToValue(ExperimentsSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, ExtensionSettingSpecificsToValue) {
  TestSpecificsToValue(ExtensionSettingSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, ExtensionSpecificsToValue) {
  TestSpecificsToValue(ExtensionSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, HistoryDeleteDirectiveSpecificsToValue) {
  TestSpecificsToValue(HistoryDeleteDirectiveSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, NigoriSpecificsToValue) {
  TestSpecificsToValue(NigoriSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, PasswordSpecificsToValue) {
  TestSpecificsToValue(PasswordSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, PreferenceSpecificsToValue) {
  TestSpecificsToValue(PreferenceSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, SearchEngineSpecificsToValue) {
  TestSpecificsToValue(SearchEngineSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, SessionSpecificsToValue) {
  TestSpecificsToValue(SessionSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, SyncedNotificationSpecificsToValue) {
  TestSpecificsToValue(SyncedNotificationSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, ThemeSpecificsToValue) {
  TestSpecificsToValue(ThemeSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, TypedUrlSpecificsToValue) {
  TestSpecificsToValue(TypedUrlSpecificsToValue);
}

// TODO(akalin): Figure out how to better test EntitySpecificsToValue.

TEST_F(ProtoValueConversionsTest, EntitySpecificsToValue) {
  sync_pb::EntitySpecifics specifics;
  // Touch the extensions to make sure it shows up in the generated
  // value.
#define SET_FIELD(key) (void)specifics.mutable_##key()

  SET_FIELD(app);
  SET_FIELD(app_notification);
  SET_FIELD(app_setting);
  SET_FIELD(autofill);
  SET_FIELD(autofill_profile);
  SET_FIELD(bookmark);
  SET_FIELD(experiments);
  SET_FIELD(extension);
  SET_FIELD(extension_setting);
  SET_FIELD(history_delete_directive);
  SET_FIELD(nigori);
  SET_FIELD(password);
  SET_FIELD(device_info);
  SET_FIELD(preference);
  SET_FIELD(search_engine);
  SET_FIELD(session);
  SET_FIELD(synced_notification);
  SET_FIELD(theme);
  SET_FIELD(typed_url);

#undef SET_FIELD

  scoped_ptr<DictionaryValue> value(EntitySpecificsToValue(specifics));
  EXPECT_EQ(MODEL_TYPE_COUNT - FIRST_REAL_MODEL_TYPE,
            static_cast<int>(value->size()));
}

namespace {
// Returns whether the given value has specifics under the entries in the given
// path.
bool ValueHasSpecifics(const DictionaryValue& value,
                       const std::string& path) {
  const ListValue* entities_list = NULL;
  const DictionaryValue* entry_dictionary = NULL;
  const DictionaryValue* specifics_dictionary = NULL;

  if (!value.GetList(path, &entities_list))
    return false;

  if (!entities_list->GetDictionary(0, &entry_dictionary))
    return false;

  return entry_dictionary->GetDictionary("specifics",
                                         &specifics_dictionary);
}
}  // namespace

// Create a ClientToServerMessage with an EntitySpecifics.  Converting it to
// a value should respect the |include_specifics| flag.
TEST_F(ProtoValueConversionsTest, ClientToServerMessageToValue) {
  sync_pb::ClientToServerMessage message;
  sync_pb::CommitMessage* commit_message = message.mutable_commit();
  sync_pb::SyncEntity* entity = commit_message->add_entries();
  entity->mutable_specifics();

  scoped_ptr<DictionaryValue> value_with_specifics(
      ClientToServerMessageToValue(message, true /* include_specifics */));
  EXPECT_FALSE(value_with_specifics->empty());
  EXPECT_TRUE(ValueHasSpecifics(*(value_with_specifics.get()),
                                "commit.entries"));

  scoped_ptr<DictionaryValue> value_without_specifics(
      ClientToServerMessageToValue(message, false /* include_specifics */));
  EXPECT_FALSE(value_without_specifics->empty());
  EXPECT_FALSE(ValueHasSpecifics(*(value_without_specifics.get()),
                                 "commit.entries"));
}

// Create a ClientToServerResponse with an EntitySpecifics.  Converting it to
// a value should respect the |include_specifics| flag.
TEST_F(ProtoValueConversionsTest, ClientToServerResponseToValue) {
  sync_pb::ClientToServerResponse message;
  sync_pb::GetUpdatesResponse* response = message.mutable_get_updates();
  sync_pb::SyncEntity* entity = response->add_entries();
  entity->mutable_specifics();

  scoped_ptr<DictionaryValue> value_with_specifics(
      ClientToServerResponseToValue(message, true /* include_specifics */));
  EXPECT_FALSE(value_with_specifics->empty());
  EXPECT_TRUE(ValueHasSpecifics(*(value_with_specifics.get()),
                                "get_updates.entries"));

  scoped_ptr<DictionaryValue> value_without_specifics(
      ClientToServerResponseToValue(message, false /* include_specifics */));
  EXPECT_FALSE(value_without_specifics->empty());
  EXPECT_FALSE(ValueHasSpecifics(*(value_without_specifics.get()),
                                 "get_updates.entries"));
}

}  // namespace
}  // namespace syncer
