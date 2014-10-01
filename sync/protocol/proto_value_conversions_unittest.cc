// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep this file in sync with the .proto files in this directory.

#include "sync/protocol/proto_value_conversions.h"

#include "base/base64.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/app_notification_specifics.pb.h"
#include "sync/protocol/app_setting_specifics.pb.h"
#include "sync/protocol/app_specifics.pb.h"
#include "sync/protocol/autofill_specifics.pb.h"
#include "sync/protocol/bookmark_specifics.pb.h"
#include "sync/protocol/device_info_specifics.pb.h"
#include "sync/protocol/encryption.pb.h"
#include "sync/protocol/enhanced_bookmark_specifics.pb.h"
#include "sync/protocol/experiments_specifics.pb.h"
#include "sync/protocol/extension_setting_specifics.pb.h"
#include "sync/protocol/extension_specifics.pb.h"
#include "sync/protocol/favicon_image_specifics.pb.h"
#include "sync/protocol/favicon_tracking_specifics.pb.h"
#include "sync/protocol/managed_user_setting_specifics.pb.h"
#include "sync/protocol/managed_user_shared_setting_specifics.pb.h"
#include "sync/protocol/managed_user_specifics.pb.h"
#include "sync/protocol/nigori_specifics.pb.h"
#include "sync/protocol/password_specifics.pb.h"
#include "sync/protocol/preference_specifics.pb.h"
#include "sync/protocol/priority_preference_specifics.pb.h"
#include "sync/protocol/search_engine_specifics.pb.h"
#include "sync/protocol/session_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/theme_specifics.pb.h"
#include "sync/protocol/typed_url_specifics.pb.h"
#include "sync/util/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class ProtoValueConversionsTest : public testing::Test {
 protected:
  template <class T>
  void TestSpecificsToValue(
      base::DictionaryValue* (*specifics_to_value)(const T&)) {
    const T& specifics(T::default_instance());
    scoped_ptr<base::DictionaryValue> value(specifics_to_value(specifics));
    // We can't do much but make sure that this doesn't crash.
  }
};

TEST_F(ProtoValueConversionsTest, ProtoChangeCheck) {
  // If this number changes, that means we added or removed a data
  // type.  Don't forget to add a unit test for {New
  // type}SpecificsToValue below.
  EXPECT_EQ(33, MODEL_TYPE_COUNT);

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

TEST_F(ProtoValueConversionsTest, NavigationRedirectToValue) {
  TestSpecificsToValue(NavigationRedirectToValue);
}

TEST_F(ProtoValueConversionsTest, PasswordSpecificsData) {
  sync_pb::PasswordSpecificsData specifics;
  specifics.set_password_value("secret");
  scoped_ptr<base::DictionaryValue> value(
      PasswordSpecificsDataToValue(specifics));
  EXPECT_FALSE(value->empty());
  std::string password_value;
  EXPECT_TRUE(value->GetString("password_value", &password_value));
  EXPECT_EQ("<redacted>", password_value);
}

TEST_F(ProtoValueConversionsTest, AppListSpecificsToValue) {
  TestSpecificsToValue(AppListSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, AppNotificationToValue) {
  TestSpecificsToValue(AppNotificationToValue);
}

TEST_F(ProtoValueConversionsTest, AppSettingSpecificsToValue) {
  sync_pb::AppNotificationSettings specifics;
  specifics.set_disabled(true);
  specifics.set_oauth_client_id("some_id_value");
  scoped_ptr<base::DictionaryValue> value(AppSettingsToValue(specifics));
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
  sync_pb::MetaInfo* meta_1 = specifics.add_meta_info();
  meta_1->set_key("key1");
  meta_1->set_value("value1");
  sync_pb::MetaInfo* meta_2 = specifics.add_meta_info();
  meta_2->set_key("key2");
  meta_2->set_value("value2");

  scoped_ptr<base::DictionaryValue> value(BookmarkSpecificsToValue(specifics));
  EXPECT_FALSE(value->empty());
  std::string encoded_time;
  EXPECT_TRUE(value->GetString("creation_time_us", &encoded_time));
  EXPECT_EQ(base::Int64ToString(creation_time.ToInternalValue()), encoded_time);
  std::string encoded_icon_url;
  EXPECT_TRUE(value->GetString("icon_url", &encoded_icon_url));
  EXPECT_EQ(icon_url, encoded_icon_url);
  base::ListValue* meta_info_list;
  ASSERT_TRUE(value->GetList("meta_info", &meta_info_list));
  EXPECT_EQ(2u, meta_info_list->GetSize());
  base::DictionaryValue* meta_info;
  std::string meta_key;
  std::string meta_value;
  ASSERT_TRUE(meta_info_list->GetDictionary(0, &meta_info));
  EXPECT_TRUE(meta_info->GetString("key", &meta_key));
  EXPECT_TRUE(meta_info->GetString("value", &meta_value));
  EXPECT_EQ("key1", meta_key);
  EXPECT_EQ("value1", meta_value);
  ASSERT_TRUE(meta_info_list->GetDictionary(1, &meta_info));
  EXPECT_TRUE(meta_info->GetString("key", &meta_key));
  EXPECT_TRUE(meta_info->GetString("value", &meta_value));
  EXPECT_EQ("key2", meta_key);
  EXPECT_EQ("value2", meta_value);
}

TEST_F(ProtoValueConversionsTest, PriorityPreferenceSpecificsToValue) {
  TestSpecificsToValue(PriorityPreferenceSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, DeviceInfoSpecificsToValue) {
  TestSpecificsToValue(DeviceInfoSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, EnhancedBookmarkSpecificsToValue) {
  TestSpecificsToValue(EnhancedBookmarkSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, EnhancedBookmarkSpecificsClipData) {
  sync_pb::EnhancedBookmarkSpecifics specifics;
  sync_pb::ChromeSyncClip* clip = specifics.mutable_clip();
  clip->set_id("id1");
  clip->set_note("note1");

  sync_pb::ChromeSyncFolioInfo* folio_info1 = clip->add_folio_info();
  folio_info1->set_folio_id("folio id1");
  folio_info1->set_position("position1");
  folio_info1->set_display_size(sync_pb::ChromeSyncFolioInfo::SMALL);
  sync_pb::ChromeSyncFolioInfo* folio_info2 = clip->add_folio_info();
  folio_info2->set_folio_id("folio id2");
  folio_info2->set_position("position2");
  folio_info2->set_display_size(sync_pb::ChromeSyncFolioInfo::MEDIUM);

  clip->set_url("http://www.google.com/");
  clip->set_title("title1");
  clip->set_snippet("snippet1");

  sync_pb::ChromeSyncImageData* image =
      clip->mutable_image();
  sync_pb::ChromeSyncImageData::ImageInfo* original_info =
      image->mutable_original_info();
  original_info->set_url("http://example.com/original");
  original_info->set_width(1);
  original_info->set_height(2);
  sync_pb::ChromeSyncImageData::ImageInfo* thumbnail_info =
      image->mutable_thumbnail_info();
  thumbnail_info->set_url("http://example.com/thumbnail");
  thumbnail_info->set_width(3);
  thumbnail_info->set_height(4);
  const base::Time expiration_time(base::Time::Now());
  image->set_expiration_timestamp_us(syncer::TimeToProtoTime(expiration_time));

  const base::Time created_time(base::Time::Now());
  clip->set_created_timestamp_us(created_time.ToInternalValue());
  clip->set_is_malware(true);
  clip->set_fetch_error_reason(sync_pb::ChromeSyncClip::LIKELY_404);
  clip->set_client_version("10");

  sync_pb::ChromeSyncClip::LegacyBookmarkData* bookmark_data =
      clip->mutable_bookmark_data();
  bookmark_data->set_favicon("favicon data");
  bookmark_data->set_icon_url("http://www.google.com/favicon.ico");

  sync_pb::MetaInfo* meta_1 = clip->add_meta_info();
  meta_1->set_key("key1");
  meta_1->set_value("value1");
  sync_pb::MetaInfo* meta_2 = clip->add_meta_info();
  meta_2->set_key("key2");
  meta_2->set_value("value2");

  scoped_ptr<base::DictionaryValue> value(
      EnhancedBookmarkSpecificsToValue(specifics));
  EXPECT_FALSE(value->empty());
  base::DictionaryValue* clip_value = nullptr;
  ASSERT_TRUE(value->GetDictionary("clip", &clip_value));
  EXPECT_FALSE(clip_value->empty());
  std::string id;
  EXPECT_TRUE(clip_value->GetString("id", &id));
  EXPECT_EQ("id1", id);
  std::string note;
  EXPECT_TRUE(clip_value->GetString("note", &note));
  EXPECT_EQ("note1", note);

  base::ListValue* folio_info_list;
  ASSERT_TRUE(clip_value->GetList("folio_info", &folio_info_list));
  EXPECT_EQ(2u, folio_info_list->GetSize());
  base::DictionaryValue* folio_info;
  ASSERT_TRUE(folio_info_list->GetDictionary(0, &folio_info));
  std::string folio_id;
  std::string position;
  std::string display_size;
  EXPECT_TRUE(folio_info->GetString("folio_id", &folio_id));
  EXPECT_TRUE(folio_info->GetString("position", &position));
  EXPECT_TRUE(folio_info->GetString("display_size", &display_size));
  EXPECT_EQ("folio id1", folio_id);
  EXPECT_EQ("position1", position);
  EXPECT_EQ("SMALL", display_size);
  ASSERT_TRUE(folio_info_list->GetDictionary(1, &folio_info));
  EXPECT_TRUE(folio_info->GetString("folio_id", &folio_id));
  EXPECT_TRUE(folio_info->GetString("position", &position));
  EXPECT_TRUE(folio_info->GetString("display_size", &display_size));
  EXPECT_EQ("folio id2", folio_id);
  EXPECT_EQ("position2", position);
  EXPECT_EQ("MEDIUM", display_size);

  std::string url;
  EXPECT_TRUE(clip_value->GetString("url", &url));
  EXPECT_EQ("http://www.google.com/", url);
  std::string title;
  EXPECT_TRUE(clip_value->GetString("title", &title));
  EXPECT_EQ("title1", title);
  std::string snippet;
  EXPECT_TRUE(clip_value->GetString("snippet", &snippet));
  EXPECT_EQ("snippet1", snippet);

  base::DictionaryValue* image_value = nullptr;
  ASSERT_TRUE(clip_value->GetDictionary("image", &image_value));
  base::DictionaryValue* image_info_value = nullptr;
  ASSERT_TRUE(image_value->GetDictionary("original_info",
                                         &image_info_value));
  EXPECT_TRUE(image_info_value->GetString("url", &url));
  EXPECT_EQ("http://example.com/original", url);
  std::string width;
  std::string height;
  EXPECT_TRUE(image_info_value->GetString("width", &width));
  EXPECT_EQ("1", width);
  EXPECT_TRUE(image_info_value->GetString("height", &height));
  EXPECT_EQ("2", height);
  ASSERT_TRUE(image_value->GetDictionary("thumbnail_info",
                                         &image_info_value));
  EXPECT_TRUE(image_info_value->GetString("url", &url));
  EXPECT_EQ("http://example.com/thumbnail", url);
  EXPECT_TRUE(image_info_value->GetString("width", &width));
  EXPECT_EQ("3", width);
  EXPECT_TRUE(image_info_value->GetString("height", &height));
  EXPECT_EQ("4", height);
  std::string encoded_time_str;
  EXPECT_TRUE(image_value->GetString("expiration_timestamp_us",
                                     &encoded_time_str));
  int64 encoded_time;
  EXPECT_TRUE(base::StringToInt64(encoded_time_str, &encoded_time));
  EXPECT_EQ(syncer::TimeToProtoTime(expiration_time), encoded_time);

  EXPECT_TRUE(clip_value->GetString("created_timestamp_us", &encoded_time_str));
  EXPECT_EQ(base::Int64ToString(created_time.ToInternalValue()),
            encoded_time_str);
  bool is_malware = false;
  EXPECT_TRUE(clip_value->GetBoolean("is_malware", &is_malware));
  EXPECT_TRUE(is_malware);
  std::string fetch_error_reason;
  EXPECT_TRUE(clip_value->GetString("fetch_error_reason", &fetch_error_reason));
  EXPECT_EQ("LIKELY_404", fetch_error_reason);
  std::string client_version;
  EXPECT_TRUE(clip_value->GetString("client_version", &client_version));
  EXPECT_EQ("10", client_version);
  base::DictionaryValue* bookmark_data_value = nullptr;
  ASSERT_TRUE(clip_value->GetDictionary("bookmark_data", &bookmark_data_value));
  std::string favicon_base64;
  EXPECT_TRUE(bookmark_data_value->GetString("favicon", &favicon_base64));
  std::string favicon;
  EXPECT_TRUE(base::Base64Decode(favicon_base64, &favicon));
  EXPECT_EQ("favicon data", favicon);
  std::string icon_url;
  EXPECT_TRUE(bookmark_data_value->GetString("icon_url", &icon_url));
  EXPECT_EQ("http://www.google.com/favicon.ico", icon_url);

  base::ListValue* meta_info_list;
  ASSERT_TRUE(clip_value->GetList("meta_info", &meta_info_list));
  EXPECT_EQ(2u, meta_info_list->GetSize());
  base::DictionaryValue* meta_info;
  std::string meta_key;
  std::string meta_value;
  ASSERT_TRUE(meta_info_list->GetDictionary(0, &meta_info));
  EXPECT_TRUE(meta_info->GetString("key", &meta_key));
  EXPECT_TRUE(meta_info->GetString("value", &meta_value));
  EXPECT_EQ("key1", meta_key);
  EXPECT_EQ("value1", meta_value);
  ASSERT_TRUE(meta_info_list->GetDictionary(1, &meta_info));
  EXPECT_TRUE(meta_info->GetString("key", &meta_key));
  EXPECT_TRUE(meta_info->GetString("value", &meta_value));
  EXPECT_EQ("key2", meta_key);
  EXPECT_EQ("value2", meta_value);
}

TEST_F(ProtoValueConversionsTest, EnhancedBookmarkSpecificsFolioData) {
  sync_pb::EnhancedBookmarkSpecifics specifics;
  sync_pb::ChromeSyncFolio* folio = specifics.mutable_folio();
  folio->set_id("id1");
  folio->set_parent_id("parent id1");
  folio->set_title("title1");
  folio->set_description("description1");
  folio->set_is_public(true);
  const base::Time created_time(base::Time::Now());
  folio->set_created_timestamp_us(created_time.ToInternalValue());
  folio->set_position("position1");
  folio->set_client_version("10");
  sync_pb::MetaInfo* meta_1 = folio->add_meta_info();
  meta_1->set_key("key1");
  meta_1->set_value("value1");
  sync_pb::MetaInfo* meta_2 = folio->add_meta_info();
  meta_2->set_key("key2");
  meta_2->set_value("value2");

  scoped_ptr<base::DictionaryValue> value(
      EnhancedBookmarkSpecificsToValue(specifics));
  EXPECT_FALSE(value->empty());
  base::DictionaryValue* folio_value = nullptr;
  ASSERT_TRUE(value->GetDictionary("folio", &folio_value));
  EXPECT_FALSE(folio_value->empty());
  std::string id;
  EXPECT_TRUE(folio_value->GetString("id", &id));
  EXPECT_EQ("id1", id);
  std::string parent_id;
  EXPECT_TRUE(folio_value->GetString("parent_id", &id));
  EXPECT_EQ("parent id1", id);
  std::string title;
  EXPECT_TRUE(folio_value->GetString("title", &title));
  EXPECT_EQ("title1", title);
  std::string description;
  EXPECT_TRUE(folio_value->GetString("description", &description));
  EXPECT_EQ("description1", description);
  bool is_public = false;
  EXPECT_TRUE(folio_value->GetBoolean("is_public", &is_public));
  EXPECT_TRUE(is_public);
  std::string encoded_time_str;
  EXPECT_TRUE(folio_value->GetString("created_timestamp_us",
                                     &encoded_time_str));
  EXPECT_EQ(base::Int64ToString(created_time.ToInternalValue()),
            encoded_time_str);
  std::string position;
  EXPECT_TRUE(folio_value->GetString("position", &position));
  EXPECT_EQ("position1", position);
  std::string client_version;
  EXPECT_TRUE(folio_value->GetString("client_version", &client_version));
  EXPECT_EQ("10", client_version);

  base::ListValue* meta_info_list;
  ASSERT_TRUE(folio_value->GetList("meta_info", &meta_info_list));
  EXPECT_EQ(2u, meta_info_list->GetSize());
  base::DictionaryValue* meta_info;
  std::string meta_key;
  std::string meta_value;
  ASSERT_TRUE(meta_info_list->GetDictionary(0, &meta_info));
  EXPECT_TRUE(meta_info->GetString("key", &meta_key));
  EXPECT_TRUE(meta_info->GetString("value", &meta_value));
  EXPECT_EQ("key1", meta_key);
  EXPECT_EQ("value1", meta_value);
  ASSERT_TRUE(meta_info_list->GetDictionary(1, &meta_info));
  EXPECT_TRUE(meta_info->GetString("key", &meta_key));
  EXPECT_TRUE(meta_info->GetString("value", &meta_value));
  EXPECT_EQ("key2", meta_key);
  EXPECT_EQ("value2", meta_value);
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

TEST_F(ProtoValueConversionsTest, FaviconImageSpecificsToValue) {
  TestSpecificsToValue(FaviconImageSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, FaviconTrackingSpecificsToValue) {
  TestSpecificsToValue(FaviconTrackingSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, HistoryDeleteDirectiveSpecificsToValue) {
  TestSpecificsToValue(HistoryDeleteDirectiveSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, ManagedUserSettingSpecificsToValue) {
  TestSpecificsToValue(ManagedUserSettingSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, ManagedUserSpecificsToValue) {
  TestSpecificsToValue(ManagedUserSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, ManagedUserSharedSettingSpecificsToValue) {
  TestSpecificsToValue(ManagedUserSharedSettingSpecificsToValue);
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

TEST_F(ProtoValueConversionsTest, SyncedNotificationAppInfoSpecificsToValue) {
  TestSpecificsToValue(SyncedNotificationAppInfoSpecificsToValue);
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

TEST_F(ProtoValueConversionsTest, DictionarySpecificsToValue) {
  TestSpecificsToValue(DictionarySpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, ArticleSpecificsToValue) {
  TestSpecificsToValue(ArticleSpecificsToValue);
}

// TODO(akalin): Figure out how to better test EntitySpecificsToValue.

TEST_F(ProtoValueConversionsTest, EntitySpecificsToValue) {
  sync_pb::EntitySpecifics specifics;
  // Touch the extensions to make sure it shows up in the generated
  // value.
#define SET_FIELD(key) (void)specifics.mutable_##key()

  SET_FIELD(app);
  SET_FIELD(app_list);
  SET_FIELD(app_notification);
  SET_FIELD(app_setting);
  SET_FIELD(article);
  SET_FIELD(autofill);
  SET_FIELD(autofill_profile);
  SET_FIELD(bookmark);
  SET_FIELD(device_info);
  SET_FIELD(dictionary);
  SET_FIELD(enhanced_bookmark);
  SET_FIELD(experiments);
  SET_FIELD(extension);
  SET_FIELD(extension_setting);
  SET_FIELD(favicon_image);
  SET_FIELD(favicon_tracking);
  SET_FIELD(history_delete_directive);
  SET_FIELD(managed_user_setting);
  SET_FIELD(managed_user_shared_setting);
  SET_FIELD(managed_user);
  SET_FIELD(nigori);
  SET_FIELD(password);
  SET_FIELD(preference);
  SET_FIELD(priority_preference);
  SET_FIELD(search_engine);
  SET_FIELD(session);
  SET_FIELD(synced_notification);
  SET_FIELD(synced_notification_app_info);
  SET_FIELD(theme);
  SET_FIELD(typed_url);

#undef SET_FIELD

  scoped_ptr<base::DictionaryValue> value(EntitySpecificsToValue(specifics));
  EXPECT_EQ(MODEL_TYPE_COUNT - FIRST_REAL_MODEL_TYPE -
            (LAST_PROXY_TYPE - FIRST_PROXY_TYPE + 1),
            static_cast<int>(value->size()));
}

namespace {
// Returns whether the given value has specifics under the entries in the given
// path.
bool ValueHasSpecifics(const base::DictionaryValue& value,
                       const std::string& path) {
  const base::ListValue* entities_list = NULL;
  const base::DictionaryValue* entry_dictionary = NULL;
  const base::DictionaryValue* specifics_dictionary = NULL;

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

  scoped_ptr<base::DictionaryValue> value_with_specifics(
      ClientToServerMessageToValue(message, true /* include_specifics */));
  EXPECT_FALSE(value_with_specifics->empty());
  EXPECT_TRUE(ValueHasSpecifics(*(value_with_specifics.get()),
                                "commit.entries"));

  scoped_ptr<base::DictionaryValue> value_without_specifics(
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

  scoped_ptr<base::DictionaryValue> value_with_specifics(
      ClientToServerResponseToValue(message, true /* include_specifics */));
  EXPECT_FALSE(value_with_specifics->empty());
  EXPECT_TRUE(ValueHasSpecifics(*(value_with_specifics.get()),
                                "get_updates.entries"));

  scoped_ptr<base::DictionaryValue> value_without_specifics(
      ClientToServerResponseToValue(message, false /* include_specifics */));
  EXPECT_FALSE(value_without_specifics->empty());
  EXPECT_FALSE(ValueHasSpecifics(*(value_without_specifics.get()),
                                 "get_updates.entries"));
}

TEST_F(ProtoValueConversionsTest, AttachmentIdProtoToValue) {
  TestSpecificsToValue(AttachmentIdProtoToValue);
}

}  // namespace
}  // namespace syncer
