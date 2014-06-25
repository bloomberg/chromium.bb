// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/model_type.h"

#include "base/strings/string_split.h"
#include "base/values.h"
#include "sync/protocol/app_notification_specifics.pb.h"
#include "sync/protocol/app_setting_specifics.pb.h"
#include "sync/protocol/app_specifics.pb.h"
#include "sync/protocol/autofill_specifics.pb.h"
#include "sync/protocol/bookmark_specifics.pb.h"
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
#include "sync/syncable/syncable_proto_util.h"

namespace syncer {

void AddDefaultFieldValue(ModelType datatype,
                          sync_pb::EntitySpecifics* specifics) {
  if (!ProtocolTypes().Has(datatype)) {
    NOTREACHED() << "Only protocol types have field values.";
    return;
  }
  switch (datatype) {
    case BOOKMARKS:
      specifics->mutable_bookmark();
      break;
    case PASSWORDS:
      specifics->mutable_password();
      break;
    case PREFERENCES:
      specifics->mutable_preference();
      break;
    case AUTOFILL:
      specifics->mutable_autofill();
      break;
    case AUTOFILL_PROFILE:
      specifics->mutable_autofill_profile();
      break;
    case THEMES:
      specifics->mutable_theme();
      break;
    case TYPED_URLS:
      specifics->mutable_typed_url();
      break;
    case EXTENSIONS:
      specifics->mutable_extension();
      break;
    case NIGORI:
      specifics->mutable_nigori();
      break;
    case SEARCH_ENGINES:
      specifics->mutable_search_engine();
      break;
    case SESSIONS:
      specifics->mutable_session();
      break;
    case APPS:
      specifics->mutable_app();
      break;
    case APP_LIST:
      specifics->mutable_app_list();
      break;
    case APP_SETTINGS:
      specifics->mutable_app_setting();
      break;
    case EXTENSION_SETTINGS:
      specifics->mutable_extension_setting();
      break;
    case APP_NOTIFICATIONS:
      specifics->mutable_app_notification();
      break;
    case HISTORY_DELETE_DIRECTIVES:
      specifics->mutable_history_delete_directive();
      break;
    case SYNCED_NOTIFICATIONS:
      specifics->mutable_synced_notification();
      break;
    case SYNCED_NOTIFICATION_APP_INFO:
      specifics->mutable_synced_notification_app_info();
      break;
    case DEVICE_INFO:
      specifics->mutable_device_info();
      break;
    case EXPERIMENTS:
      specifics->mutable_experiments();
      break;
    case PRIORITY_PREFERENCES:
      specifics->mutable_priority_preference();
      break;
    case DICTIONARY:
      specifics->mutable_dictionary();
      break;
    case FAVICON_IMAGES:
      specifics->mutable_favicon_image();
      break;
    case FAVICON_TRACKING:
      specifics->mutable_favicon_tracking();
      break;
    case SUPERVISED_USER_SETTINGS:
      specifics->mutable_managed_user_setting();
      break;
    case SUPERVISED_USERS:
      specifics->mutable_managed_user();
      break;
    case SUPERVISED_USER_SHARED_SETTINGS:
      specifics->mutable_managed_user_shared_setting();
      break;
    case ARTICLES:
      specifics->mutable_article();
      break;
    default:
      NOTREACHED() << "No known extension for model type.";
  }
}

ModelType GetModelTypeFromSpecificsFieldNumber(int field_number) {
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelTypeSet::Iterator iter = protocol_types.First(); iter.Good();
       iter.Inc()) {
    if (GetSpecificsFieldNumberFromModelType(iter.Get()) == field_number)
      return iter.Get();
  }
  return UNSPECIFIED;
}

int GetSpecificsFieldNumberFromModelType(ModelType model_type) {
  DCHECK(ProtocolTypes().Has(model_type))
      << "Only protocol types have field values.";
  switch (model_type) {
    case BOOKMARKS:
      return sync_pb::EntitySpecifics::kBookmarkFieldNumber;
    case PASSWORDS:
      return sync_pb::EntitySpecifics::kPasswordFieldNumber;
    case PREFERENCES:
      return sync_pb::EntitySpecifics::kPreferenceFieldNumber;
    case AUTOFILL:
      return sync_pb::EntitySpecifics::kAutofillFieldNumber;
    case AUTOFILL_PROFILE:
      return sync_pb::EntitySpecifics::kAutofillProfileFieldNumber;
    case THEMES:
      return sync_pb::EntitySpecifics::kThemeFieldNumber;
    case TYPED_URLS:
      return sync_pb::EntitySpecifics::kTypedUrlFieldNumber;
    case EXTENSIONS:
      return sync_pb::EntitySpecifics::kExtensionFieldNumber;
    case NIGORI:
      return sync_pb::EntitySpecifics::kNigoriFieldNumber;
    case SEARCH_ENGINES:
      return sync_pb::EntitySpecifics::kSearchEngineFieldNumber;
    case SESSIONS:
      return sync_pb::EntitySpecifics::kSessionFieldNumber;
    case APPS:
      return sync_pb::EntitySpecifics::kAppFieldNumber;
    case APP_LIST:
      return sync_pb::EntitySpecifics::kAppListFieldNumber;
    case APP_SETTINGS:
      return sync_pb::EntitySpecifics::kAppSettingFieldNumber;
    case EXTENSION_SETTINGS:
      return sync_pb::EntitySpecifics::kExtensionSettingFieldNumber;
    case APP_NOTIFICATIONS:
      return sync_pb::EntitySpecifics::kAppNotificationFieldNumber;
    case HISTORY_DELETE_DIRECTIVES:
      return sync_pb::EntitySpecifics::kHistoryDeleteDirectiveFieldNumber;
    case SYNCED_NOTIFICATIONS:
      return sync_pb::EntitySpecifics::kSyncedNotificationFieldNumber;
    case SYNCED_NOTIFICATION_APP_INFO:
      return sync_pb::EntitySpecifics::kSyncedNotificationAppInfoFieldNumber;
    case DEVICE_INFO:
      return sync_pb::EntitySpecifics::kDeviceInfoFieldNumber;
    case EXPERIMENTS:
      return sync_pb::EntitySpecifics::kExperimentsFieldNumber;
    case PRIORITY_PREFERENCES:
      return sync_pb::EntitySpecifics::kPriorityPreferenceFieldNumber;
    case DICTIONARY:
      return sync_pb::EntitySpecifics::kDictionaryFieldNumber;
    case FAVICON_IMAGES:
      return sync_pb::EntitySpecifics::kFaviconImageFieldNumber;
    case FAVICON_TRACKING:
      return sync_pb::EntitySpecifics::kFaviconTrackingFieldNumber;
    case SUPERVISED_USER_SETTINGS:
      return sync_pb::EntitySpecifics::kManagedUserSettingFieldNumber;
    case SUPERVISED_USERS:
      return sync_pb::EntitySpecifics::kManagedUserFieldNumber;
    case SUPERVISED_USER_SHARED_SETTINGS:
      return sync_pb::EntitySpecifics::kManagedUserSharedSettingFieldNumber;
    case ARTICLES:
      return sync_pb::EntitySpecifics::kArticleFieldNumber;
    default:
      NOTREACHED() << "No known extension for model type.";
      return 0;
  }
}

FullModelTypeSet ToFullModelTypeSet(ModelTypeSet in) {
  FullModelTypeSet out;
  for (ModelTypeSet::Iterator i = in.First(); i.Good(); i.Inc()) {
    out.Put(i.Get());
  }
  return out;
}

// Note: keep this consistent with GetModelType in entry.cc!
ModelType GetModelType(const sync_pb::SyncEntity& sync_entity) {
  DCHECK(!IsRoot(sync_entity));  // Root shouldn't ever go over the wire.

  // Backwards compatibility with old (pre-specifics) protocol.
  if (sync_entity.has_bookmarkdata())
    return BOOKMARKS;

  ModelType specifics_type = GetModelTypeFromSpecifics(sync_entity.specifics());
  if (specifics_type != UNSPECIFIED)
    return specifics_type;

  // Loose check for server-created top-level folders that aren't
  // bound to a particular model type.
  if (!sync_entity.server_defined_unique_tag().empty() &&
      IsFolder(sync_entity)) {
    return TOP_LEVEL_FOLDER;
  }

  // This is an item of a datatype we can't understand. Maybe it's
  // from the future?  Either we mis-encoded the object, or the
  // server sent us entries it shouldn't have.
  NOTREACHED() << "Unknown datatype in sync proto.";
  return UNSPECIFIED;
}

ModelType GetModelTypeFromSpecifics(const sync_pb::EntitySpecifics& specifics) {
  if (specifics.has_bookmark())
    return BOOKMARKS;

  if (specifics.has_password())
    return PASSWORDS;

  if (specifics.has_preference())
    return PREFERENCES;

  if (specifics.has_autofill())
    return AUTOFILL;

  if (specifics.has_autofill_profile())
    return AUTOFILL_PROFILE;

  if (specifics.has_theme())
    return THEMES;

  if (specifics.has_typed_url())
    return TYPED_URLS;

  if (specifics.has_extension())
    return EXTENSIONS;

  if (specifics.has_nigori())
    return NIGORI;

  if (specifics.has_app())
    return APPS;

  if (specifics.has_app_list())
    return APP_LIST;

  if (specifics.has_search_engine())
    return SEARCH_ENGINES;

  if (specifics.has_session())
    return SESSIONS;

  if (specifics.has_app_setting())
    return APP_SETTINGS;

  if (specifics.has_extension_setting())
    return EXTENSION_SETTINGS;

  if (specifics.has_app_notification())
    return APP_NOTIFICATIONS;

  if (specifics.has_history_delete_directive())
    return HISTORY_DELETE_DIRECTIVES;

  if (specifics.has_synced_notification())
    return SYNCED_NOTIFICATIONS;

  if (specifics.has_synced_notification_app_info())
    return SYNCED_NOTIFICATION_APP_INFO;

  if (specifics.has_device_info())
    return DEVICE_INFO;

  if (specifics.has_experiments())
    return EXPERIMENTS;

  if (specifics.has_priority_preference())
    return PRIORITY_PREFERENCES;

  if (specifics.has_dictionary())
    return DICTIONARY;

  if (specifics.has_favicon_image())
    return FAVICON_IMAGES;

  if (specifics.has_favicon_tracking())
    return FAVICON_TRACKING;

  if (specifics.has_managed_user_setting())
    return SUPERVISED_USER_SETTINGS;

  if (specifics.has_managed_user())
    return SUPERVISED_USERS;

  if (specifics.has_managed_user_shared_setting())
    return SUPERVISED_USER_SHARED_SETTINGS;

  if (specifics.has_article())
    return ARTICLES;

  return UNSPECIFIED;
}

ModelTypeSet ProtocolTypes() {
  ModelTypeSet set = ModelTypeSet::All();
  set.RemoveAll(ProxyTypes());
  return set;
}

ModelTypeSet UserTypes() {
  ModelTypeSet set;
  // TODO(sync): We should be able to build the actual enumset's internal
  // bitset value here at compile time, rather than performing an iteration
  // every time.
  for (int i = FIRST_USER_MODEL_TYPE; i <= LAST_USER_MODEL_TYPE; ++i) {
    set.Put(ModelTypeFromInt(i));
  }
  return set;
}

ModelTypeSet UserSelectableTypes() {
  ModelTypeSet set;
  // Although the order doesn't technically matter here, it's clearer to keep
  // these in the same order as their definition in the ModelType enum.
  set.Put(BOOKMARKS);
  set.Put(PREFERENCES);
  set.Put(PASSWORDS);
  set.Put(AUTOFILL);
  set.Put(THEMES);
  set.Put(TYPED_URLS);
  set.Put(EXTENSIONS);
  set.Put(APPS);
  set.Put(PROXY_TABS);
  return set;
}

bool IsUserSelectableType(ModelType model_type) {
  return UserSelectableTypes().Has(model_type);
}

ModelTypeSet EncryptableUserTypes() {
  ModelTypeSet encryptable_user_types = UserTypes();
  // We never encrypt history delete directives.
  encryptable_user_types.Remove(HISTORY_DELETE_DIRECTIVES);
  // Synced notifications are not encrypted since the server must see changes.
  encryptable_user_types.Remove(SYNCED_NOTIFICATIONS);
  // Synced Notification App Info does not have private data, so it is not
  // encrypted.
  encryptable_user_types.Remove(SYNCED_NOTIFICATION_APP_INFO);
  // Priority preferences are not encrypted because they might be synced before
  // encryption is ready.
  encryptable_user_types.Remove(PRIORITY_PREFERENCES);
  // Supervised user settings are not encrypted since they are set server-side.
  encryptable_user_types.Remove(SUPERVISED_USER_SETTINGS);
  // Supervised users are not encrypted since they are managed server-side.
  encryptable_user_types.Remove(SUPERVISED_USERS);
  // Supervised user shared settings are not encrypted since they are managed
  // server-side and shared between manager and supervised user.
  encryptable_user_types.Remove(SUPERVISED_USER_SHARED_SETTINGS);
  // Proxy types have no sync representation and are therefore not encrypted.
  // Note however that proxy types map to one or more protocol types, which
  // may or may not be encrypted themselves.
  encryptable_user_types.RemoveAll(ProxyTypes());
  return encryptable_user_types;
}

ModelTypeSet PriorityUserTypes() {
  return ModelTypeSet(PRIORITY_PREFERENCES);
}

ModelTypeSet ControlTypes() {
  ModelTypeSet set;
  // TODO(sync): We should be able to build the actual enumset's internal
  // bitset value here at compile time, rather than performing an iteration
  // every time.
  for (int i = FIRST_CONTROL_MODEL_TYPE; i <= LAST_CONTROL_MODEL_TYPE; ++i) {
    set.Put(ModelTypeFromInt(i));
  }

  return set;
}

ModelTypeSet ProxyTypes() {
  ModelTypeSet set;
  set.Put(PROXY_TABS);
  return set;
}

bool IsControlType(ModelType model_type) {
  return ControlTypes().Has(model_type);
}

ModelTypeSet CoreTypes() {
  syncer::ModelTypeSet result;
  result.PutAll(PriorityCoreTypes());

  // The following are low priority core types.
  result.Put(SYNCED_NOTIFICATIONS);
  result.Put(SYNCED_NOTIFICATION_APP_INFO);
  result.Put(SUPERVISED_USER_SHARED_SETTINGS);

  return result;
}

ModelTypeSet PriorityCoreTypes() {
  syncer::ModelTypeSet result;
  result.PutAll(ControlTypes());

  // The following are non-control core types.
  result.Put(SUPERVISED_USERS);
  result.Put(SUPERVISED_USER_SETTINGS);

  return result;
}

ModelTypeSet BackupTypes() {
  ModelTypeSet result;
  result.Put(BOOKMARKS);
  result.Put(PREFERENCES);
  result.Put(THEMES);
  result.Put(EXTENSIONS);
  result.Put(SEARCH_ENGINES);
  result.Put(APPS);
  result.Put(APP_LIST);
  result.Put(APP_SETTINGS);
  result.Put(EXTENSION_SETTINGS);
  result.Put(PRIORITY_PREFERENCES);
  return result;
}

const char* ModelTypeToString(ModelType model_type) {
  // This is used in serialization routines as well as for displaying debug
  // information.  Do not attempt to change these string values unless you know
  // what you're doing.
  switch (model_type) {
    case TOP_LEVEL_FOLDER:
      return "Top Level Folder";
    case UNSPECIFIED:
      return "Unspecified";
    case BOOKMARKS:
      return "Bookmarks";
    case PREFERENCES:
      return "Preferences";
    case PASSWORDS:
      return "Passwords";
    case AUTOFILL:
      return "Autofill";
    case THEMES:
      return "Themes";
    case TYPED_URLS:
      return "Typed URLs";
    case EXTENSIONS:
      return "Extensions";
    case NIGORI:
      return "Encryption keys";
    case SEARCH_ENGINES:
      return "Search Engines";
    case SESSIONS:
      return "Sessions";
    case APPS:
      return "Apps";
    case APP_LIST:
      return "App List";
    case AUTOFILL_PROFILE:
      return "Autofill Profiles";
    case APP_SETTINGS:
      return "App settings";
    case EXTENSION_SETTINGS:
      return "Extension settings";
    case APP_NOTIFICATIONS:
      return "App Notifications";
    case HISTORY_DELETE_DIRECTIVES:
      return "History Delete Directives";
    case SYNCED_NOTIFICATIONS:
      return "Synced Notifications";
    case SYNCED_NOTIFICATION_APP_INFO:
      return "Synced Notification App Info";
    case DEVICE_INFO:
      return "Device Info";
    case EXPERIMENTS:
      return "Experiments";
    case PRIORITY_PREFERENCES:
      return "Priority Preferences";
    case DICTIONARY:
      return "Dictionary";
    case FAVICON_IMAGES:
      return "Favicon Images";
    case FAVICON_TRACKING:
      return "Favicon Tracking";
    case SUPERVISED_USER_SETTINGS:
      return "Managed User Settings";
    case SUPERVISED_USERS:
      return "Managed Users";
    case SUPERVISED_USER_SHARED_SETTINGS:
      return "Managed User Shared Settings";
    case ARTICLES:
      return "Articles";
    case PROXY_TABS:
      return "Tabs";
    default:
      break;
  }
  NOTREACHED() << "No known extension for model type.";
  return "INVALID";
}

// The normal rules about histograms apply here.  Always append to the bottom of
// the list, and be careful to not reuse integer values that have already been
// assigned.  Don't forget to update histograms.xml when you make changes to
// this list.
int ModelTypeToHistogramInt(ModelType model_type) {
  switch (model_type) {
    case UNSPECIFIED:
      return 0;
    case TOP_LEVEL_FOLDER:
      return 1;
    case BOOKMARKS:
      return 2;
    case PREFERENCES:
      return 3;
    case PASSWORDS:
      return 4;
    case AUTOFILL_PROFILE:
      return 5;
    case AUTOFILL:
      return 6;
    case THEMES:
      return 7;
    case TYPED_URLS:
      return 8;
    case EXTENSIONS:
      return 9;
    case SEARCH_ENGINES:
      return 10;
    case SESSIONS:
      return 11;
    case APPS:
      return 12;
    case APP_SETTINGS:
      return 13;
    case EXTENSION_SETTINGS:
      return 14;
    case APP_NOTIFICATIONS:
      return 15;
    case HISTORY_DELETE_DIRECTIVES:
      return 16;
    case NIGORI:
      return 17;
    case DEVICE_INFO:
      return 18;
    case EXPERIMENTS:
      return 19;
    case SYNCED_NOTIFICATIONS:
      return 20;
    case PRIORITY_PREFERENCES:
      return 21;
    case DICTIONARY:
      return 22;
    case FAVICON_IMAGES:
      return 23;
    case FAVICON_TRACKING:
      return 24;
    case PROXY_TABS:
      return 25;
    case SUPERVISED_USER_SETTINGS:
      return 26;
    case SUPERVISED_USERS:
      return 27;
    case ARTICLES:
      return 28;
    case APP_LIST:
      return 29;
    case SUPERVISED_USER_SHARED_SETTINGS:
      return 30;
    case SYNCED_NOTIFICATION_APP_INFO:
      return 31;
    // Silence a compiler warning.
    case MODEL_TYPE_COUNT:
      return 0;
  }
  return 0;
}

base::StringValue* ModelTypeToValue(ModelType model_type) {
  if (model_type >= FIRST_REAL_MODEL_TYPE) {
    return new base::StringValue(ModelTypeToString(model_type));
  } else if (model_type == TOP_LEVEL_FOLDER) {
    return new base::StringValue("Top-level folder");
  } else if (model_type == UNSPECIFIED) {
    return new base::StringValue("Unspecified");
  }
  NOTREACHED();
  return new base::StringValue(std::string());
}

ModelType ModelTypeFromValue(const base::Value& value) {
  if (value.IsType(base::Value::TYPE_STRING)) {
    std::string result;
    CHECK(value.GetAsString(&result));
    return ModelTypeFromString(result);
  } else if (value.IsType(base::Value::TYPE_INTEGER)) {
    int result;
    CHECK(value.GetAsInteger(&result));
    return ModelTypeFromInt(result);
  } else {
    NOTREACHED() << "Unsupported value type: " << value.GetType();
    return UNSPECIFIED;
  }
}

ModelType ModelTypeFromString(const std::string& model_type_string) {
  if (model_type_string == "Bookmarks")
    return BOOKMARKS;
  else if (model_type_string == "Preferences")
    return PREFERENCES;
  else if (model_type_string == "Passwords")
    return PASSWORDS;
  else if (model_type_string == "Autofill")
    return AUTOFILL;
  else if (model_type_string == "Autofill Profiles")
    return AUTOFILL_PROFILE;
  else if (model_type_string == "Themes")
    return THEMES;
  else if (model_type_string == "Typed URLs")
    return TYPED_URLS;
  else if (model_type_string == "Extensions")
    return EXTENSIONS;
  else if (model_type_string == "Encryption keys")
    return NIGORI;
  else if (model_type_string == "Search Engines")
    return SEARCH_ENGINES;
  else if (model_type_string == "Sessions")
    return SESSIONS;
  else if (model_type_string == "Apps")
    return APPS;
  else if (model_type_string == "App List")
    return APP_LIST;
  else if (model_type_string == "App settings")
    return APP_SETTINGS;
  else if (model_type_string == "Extension settings")
    return EXTENSION_SETTINGS;
  else if (model_type_string == "App Notifications")
    return APP_NOTIFICATIONS;
  else if (model_type_string == "History Delete Directives")
    return HISTORY_DELETE_DIRECTIVES;
  else if (model_type_string == "Synced Notifications")
    return SYNCED_NOTIFICATIONS;
  else if (model_type_string == "Synced Notification App Info")
    return SYNCED_NOTIFICATION_APP_INFO;
  else if (model_type_string == "Device Info")
    return DEVICE_INFO;
  else if (model_type_string == "Experiments")
    return EXPERIMENTS;
  else if (model_type_string == "Priority Preferences")
    return PRIORITY_PREFERENCES;
  else if (model_type_string == "Dictionary")
    return DICTIONARY;
  else if (model_type_string == "Favicon Images")
    return FAVICON_IMAGES;
  else if (model_type_string == "Favicon Tracking")
    return FAVICON_TRACKING;
  else if (model_type_string == "Managed User Settings")
    return SUPERVISED_USER_SETTINGS;
  else if (model_type_string == "Managed Users")
    return SUPERVISED_USERS;
  else if (model_type_string == "Managed User Shared Settings")
    return SUPERVISED_USER_SHARED_SETTINGS;
  else if (model_type_string == "Articles")
    return ARTICLES;
  else if (model_type_string == "Tabs")
    return PROXY_TABS;
  else
    NOTREACHED() << "No known model type corresponding to "
                 << model_type_string << ".";
  return UNSPECIFIED;
}

std::string ModelTypeSetToString(ModelTypeSet model_types) {
  std::string result;
  for (ModelTypeSet::Iterator it = model_types.First(); it.Good(); it.Inc()) {
    if (!result.empty()) {
      result += ", ";
    }
    result += ModelTypeToString(it.Get());
  }
  return result;
}

ModelTypeSet ModelTypeSetFromString(const std::string& model_types_string) {
  std::string working_copy = model_types_string;
  ModelTypeSet model_types;
  while (!working_copy.empty()) {
    // Remove any leading spaces.
    working_copy = working_copy.substr(working_copy.find_first_not_of(' '));
    if (working_copy.empty())
      break;
    std::string type_str;
    size_t end = working_copy.find(',');
    if (end == std::string::npos) {
      end = working_copy.length() - 1;
      type_str = working_copy;
    } else {
      type_str = working_copy.substr(0, end);
    }
    syncer::ModelType type = ModelTypeFromString(type_str);
    if (IsRealDataType(type))
      model_types.Put(type);
    working_copy = working_copy.substr(end + 1);
  }
  return model_types;
}

base::ListValue* ModelTypeSetToValue(ModelTypeSet model_types) {
  base::ListValue* value = new base::ListValue();
  for (ModelTypeSet::Iterator it = model_types.First(); it.Good(); it.Inc()) {
    value->Append(new base::StringValue(ModelTypeToString(it.Get())));
  }
  return value;
}

ModelTypeSet ModelTypeSetFromValue(const base::ListValue& value) {
  ModelTypeSet result;
  for (base::ListValue::const_iterator i = value.begin();
       i != value.end(); ++i) {
    result.Put(ModelTypeFromValue(**i));
  }
  return result;
}

// TODO(zea): remove all hardcoded tags in model associators and have them use
// this instead.
// NOTE: Proxy types should return empty strings (so that we don't NOTREACHED
// in tests when we verify they have no root node).
std::string ModelTypeToRootTag(ModelType type) {
  switch (type) {
    case BOOKMARKS:
      return "google_chrome_bookmarks";
    case PREFERENCES:
      return "google_chrome_preferences";
    case PASSWORDS:
      return "google_chrome_passwords";
    case AUTOFILL:
      return "google_chrome_autofill";
    case THEMES:
      return "google_chrome_themes";
    case TYPED_URLS:
      return "google_chrome_typed_urls";
    case EXTENSIONS:
      return "google_chrome_extensions";
    case NIGORI:
      return "google_chrome_nigori";
    case SEARCH_ENGINES:
      return "google_chrome_search_engines";
    case SESSIONS:
      return "google_chrome_sessions";
    case APPS:
      return "google_chrome_apps";
    case APP_LIST:
      return "google_chrome_app_list";
    case AUTOFILL_PROFILE:
      return "google_chrome_autofill_profiles";
    case APP_SETTINGS:
      return "google_chrome_app_settings";
    case EXTENSION_SETTINGS:
      return "google_chrome_extension_settings";
    case APP_NOTIFICATIONS:
      return "google_chrome_app_notifications";
    case HISTORY_DELETE_DIRECTIVES:
      return "google_chrome_history_delete_directives";
    case SYNCED_NOTIFICATIONS:
      return "google_chrome_synced_notifications";
    case SYNCED_NOTIFICATION_APP_INFO:
      return "google_chrome_synced_notification_app_info";
    case DEVICE_INFO:
      return "google_chrome_device_info";
    case EXPERIMENTS:
      return "google_chrome_experiments";
    case PRIORITY_PREFERENCES:
      return "google_chrome_priority_preferences";
    case DICTIONARY:
      return "google_chrome_dictionary";
    case FAVICON_IMAGES:
      return "google_chrome_favicon_images";
    case FAVICON_TRACKING:
      return "google_chrome_favicon_tracking";
    case SUPERVISED_USER_SETTINGS:
      return "google_chrome_managed_user_settings";
    case SUPERVISED_USERS:
      return "google_chrome_managed_users";
    case SUPERVISED_USER_SHARED_SETTINGS:
      return "google_chrome_managed_user_shared_settings";
    case ARTICLES:
      return "google_chrome_articles";
    case PROXY_TABS:
      return std::string();
    default:
      break;
  }
  NOTREACHED() << "No known extension for model type.";
  return "INVALID";
}

// TODO(akalin): Figure out a better way to do these mappings.
// Note: Do not include proxy types in this list. They should never receive
// or trigger notifications.
namespace {
const char kBookmarkNotificationType[] = "BOOKMARK";
const char kPreferenceNotificationType[] = "PREFERENCE";
const char kPasswordNotificationType[] = "PASSWORD";
const char kAutofillNotificationType[] = "AUTOFILL";
const char kThemeNotificationType[] = "THEME";
const char kTypedUrlNotificationType[] = "TYPED_URL";
const char kExtensionNotificationType[] = "EXTENSION";
const char kExtensionSettingNotificationType[] = "EXTENSION_SETTING";
const char kNigoriNotificationType[] = "NIGORI";
const char kAppSettingNotificationType[] = "APP_SETTING";
const char kAppNotificationType[] = "APP";
const char kAppListNotificationType[] = "APP_LIST";
const char kSearchEngineNotificationType[] = "SEARCH_ENGINE";
const char kSessionNotificationType[] = "SESSION";
const char kAutofillProfileNotificationType[] = "AUTOFILL_PROFILE";
const char kAppNotificationNotificationType[] = "APP_NOTIFICATION";
const char kHistoryDeleteDirectiveNotificationType[] =
    "HISTORY_DELETE_DIRECTIVE";
const char kSyncedNotificationType[] = "SYNCED_NOTIFICATION";
const char kSyncedNotificationAppInfoType[] = "SYNCED_NOTIFICATION_APP_INFO";
const char kDeviceInfoNotificationType[] = "DEVICE_INFO";
const char kExperimentsNotificationType[] = "EXPERIMENTS";
const char kPriorityPreferenceNotificationType[] = "PRIORITY_PREFERENCE";
const char kDictionaryNotificationType[] = "DICTIONARY";
const char kFaviconImageNotificationType[] = "FAVICON_IMAGE";
const char kFaviconTrackingNotificationType[] = "FAVICON_TRACKING";
const char kSupervisedUserSettingNotificationType[] = "MANAGED_USER_SETTING";
const char kSupervisedUserNotificationType[] = "MANAGED_USER";
const char kSupervisedUserSharedSettingNotificationType[] =
    "MANAGED_USER_SHARED_SETTING";
const char kArticleNotificationType[] = "ARTICLE";
}  // namespace

bool RealModelTypeToNotificationType(ModelType model_type,
                                     std::string* notification_type) {
  switch (model_type) {
    case BOOKMARKS:
      *notification_type = kBookmarkNotificationType;
      return true;
    case PREFERENCES:
      *notification_type = kPreferenceNotificationType;
      return true;
    case PASSWORDS:
      *notification_type = kPasswordNotificationType;
      return true;
    case AUTOFILL:
      *notification_type = kAutofillNotificationType;
      return true;
    case THEMES:
      *notification_type = kThemeNotificationType;
      return true;
    case TYPED_URLS:
      *notification_type = kTypedUrlNotificationType;
      return true;
    case EXTENSIONS:
      *notification_type = kExtensionNotificationType;
      return true;
    case NIGORI:
      *notification_type = kNigoriNotificationType;
      return true;
    case APP_SETTINGS:
      *notification_type = kAppSettingNotificationType;
      return true;
    case APPS:
      *notification_type = kAppNotificationType;
      return true;
    case APP_LIST:
      *notification_type = kAppListNotificationType;
      return true;
    case SEARCH_ENGINES:
      *notification_type = kSearchEngineNotificationType;
      return true;
    case SESSIONS:
      *notification_type = kSessionNotificationType;
      return true;
    case AUTOFILL_PROFILE:
      *notification_type = kAutofillProfileNotificationType;
      return true;
    case EXTENSION_SETTINGS:
      *notification_type = kExtensionSettingNotificationType;
      return true;
    case APP_NOTIFICATIONS:
      *notification_type = kAppNotificationNotificationType;
      return true;
    case HISTORY_DELETE_DIRECTIVES:
      *notification_type = kHistoryDeleteDirectiveNotificationType;
      return true;
    case SYNCED_NOTIFICATIONS:
      *notification_type = kSyncedNotificationType;
      return true;
    case SYNCED_NOTIFICATION_APP_INFO:
      *notification_type = kSyncedNotificationAppInfoType;
      return true;
    case DEVICE_INFO:
      *notification_type = kDeviceInfoNotificationType;
      return true;
    case EXPERIMENTS:
      *notification_type = kExperimentsNotificationType;
      return true;
    case PRIORITY_PREFERENCES:
      *notification_type = kPriorityPreferenceNotificationType;
      return true;
    case DICTIONARY:
      *notification_type = kDictionaryNotificationType;
      return true;
    case FAVICON_IMAGES:
      *notification_type = kFaviconImageNotificationType;
      return true;
    case FAVICON_TRACKING:
      *notification_type = kFaviconTrackingNotificationType;
      return true;
    case SUPERVISED_USER_SETTINGS:
      *notification_type = kSupervisedUserSettingNotificationType;
      return true;
    case SUPERVISED_USERS:
      *notification_type = kSupervisedUserNotificationType;
      return true;
    case SUPERVISED_USER_SHARED_SETTINGS:
      *notification_type = kSupervisedUserSharedSettingNotificationType;
      return true;
    case ARTICLES:
      *notification_type = kArticleNotificationType;
      return true;
    default:
      break;
  }
  notification_type->clear();
  return false;
}

bool NotificationTypeToRealModelType(const std::string& notification_type,
                                     ModelType* model_type) {
  if (notification_type == kBookmarkNotificationType) {
    *model_type = BOOKMARKS;
    return true;
  } else if (notification_type == kPreferenceNotificationType) {
    *model_type = PREFERENCES;
    return true;
  } else if (notification_type == kPasswordNotificationType) {
    *model_type = PASSWORDS;
    return true;
  } else if (notification_type == kAutofillNotificationType) {
    *model_type = AUTOFILL;
    return true;
  } else if (notification_type == kThemeNotificationType) {
    *model_type = THEMES;
    return true;
  } else if (notification_type == kTypedUrlNotificationType) {
    *model_type = TYPED_URLS;
    return true;
  } else if (notification_type == kExtensionNotificationType) {
    *model_type = EXTENSIONS;
    return true;
  } else if (notification_type == kNigoriNotificationType) {
    *model_type = NIGORI;
    return true;
  } else if (notification_type == kAppNotificationType) {
    *model_type = APPS;
    return true;
  } else if (notification_type == kAppListNotificationType) {
    *model_type = APP_LIST;
    return true;
  } else if (notification_type == kSearchEngineNotificationType) {
    *model_type = SEARCH_ENGINES;
    return true;
  } else if (notification_type == kSessionNotificationType) {
    *model_type = SESSIONS;
    return true;
  } else if (notification_type == kAutofillProfileNotificationType) {
    *model_type = AUTOFILL_PROFILE;
    return true;
  } else if (notification_type == kAppSettingNotificationType) {
    *model_type = APP_SETTINGS;
    return true;
  } else if (notification_type == kExtensionSettingNotificationType) {
    *model_type = EXTENSION_SETTINGS;
    return true;
  } else if (notification_type == kAppNotificationNotificationType) {
    *model_type = APP_NOTIFICATIONS;
    return true;
  } else if (notification_type == kHistoryDeleteDirectiveNotificationType) {
    *model_type = HISTORY_DELETE_DIRECTIVES;
    return true;
  } else if (notification_type == kSyncedNotificationType) {
    *model_type = SYNCED_NOTIFICATIONS;
    return true;
  } else if (notification_type == kSyncedNotificationAppInfoType) {
    *model_type = SYNCED_NOTIFICATION_APP_INFO;
    return true;
  } else if (notification_type == kDeviceInfoNotificationType) {
    *model_type = DEVICE_INFO;
    return true;
  } else if (notification_type == kExperimentsNotificationType) {
    *model_type = EXPERIMENTS;
    return true;
  } else if (notification_type == kPriorityPreferenceNotificationType) {
    *model_type = PRIORITY_PREFERENCES;
    return true;
  } else if (notification_type == kDictionaryNotificationType) {
    *model_type = DICTIONARY;
    return true;
  } else if (notification_type == kFaviconImageNotificationType) {
    *model_type = FAVICON_IMAGES;
    return true;
  } else if (notification_type == kFaviconTrackingNotificationType) {
    *model_type = FAVICON_TRACKING;
    return true;
  } else if (notification_type == kSupervisedUserSettingNotificationType) {
    *model_type = SUPERVISED_USER_SETTINGS;
    return true;
  } else if (notification_type == kSupervisedUserNotificationType) {
    *model_type = SUPERVISED_USERS;
    return true;
  } else if (notification_type ==
      kSupervisedUserSharedSettingNotificationType) {
    *model_type = SUPERVISED_USER_SHARED_SETTINGS;
    return true;
  } else if (notification_type == kArticleNotificationType) {
    *model_type = ARTICLES;
    return true;
  }
  *model_type = UNSPECIFIED;
  return false;
}

bool IsRealDataType(ModelType model_type) {
  return model_type >= FIRST_REAL_MODEL_TYPE && model_type < MODEL_TYPE_COUNT;
}

bool IsProxyType(ModelType model_type) {
  return model_type >= FIRST_PROXY_TYPE && model_type <= LAST_PROXY_TYPE;
}

bool IsActOnceDataType(ModelType model_type) {
  return model_type == HISTORY_DELETE_DIRECTIVES;
}

}  // namespace syncer
