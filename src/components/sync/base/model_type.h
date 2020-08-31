// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_MODEL_TYPE_H_
#define COMPONENTS_SYNC_BASE_MODEL_TYPE_H_

#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>

#include "base/logging.h"
#include "components/sync/base/enum_set.h"

namespace base {
class ListValue;
class Value;
}

namespace sync_pb {
class EntitySpecifics;
class SyncEntity;
}

namespace syncer {

// Enumerate the various item subtypes that are supported by sync.
// Each sync object is expected to have an immutable object type.
// An object's type is inferred from the type of data it holds.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.sync
//
// |kModelTypeInfoMap| struct entries are in the same order as their definition
// in ModelType enum. When you make changes in ModelType enum, don't forget to
// update the |kModelTypeInfoMap| struct in model_type.cc and also the
// SyncModelType histogram suffix in histograms.xml
enum ModelType {
  // Object type unknown.  Objects may transition through
  // the unknown state during their initial creation, before
  // their properties are set.  After deletion, object types
  // are generally preserved.
  UNSPECIFIED,
  // A permanent folder whose children may be of mixed
  // datatypes (e.g. the "Google Chrome" folder).
  TOP_LEVEL_FOLDER,

  // ------------------------------------ Start of "real" model types.
  // The model types declared before here are somewhat special, as they
  // they do not correspond to any browser data model.  The remaining types
  // are bona fide model types; all have a related browser data model and
  // can be represented in the protocol using a specific Message type in the
  // EntitySpecifics protocol buffer.
  //
  // A bookmark folder or a bookmark URL object.
  BOOKMARKS,
  FIRST_USER_MODEL_TYPE = BOOKMARKS,  // Declared 2nd, for debugger prettiness.
  FIRST_REAL_MODEL_TYPE = FIRST_USER_MODEL_TYPE,

  // A preference object, a.k.a. "Settings".
  PREFERENCES,
  // A password object.
  PASSWORDS,
  // An autofill_profile object, i.e. an address.
  AUTOFILL_PROFILE,
  // An autofill object, i.e. an autocomplete entry keyed to an HTML form field.
  AUTOFILL,
  // Credit cards and addresses from the user's account. These are read-only on
  // the client.
  AUTOFILL_WALLET_DATA,
  // Usage counts and last use dates for Wallet cards and addresses. This data
  // is both readable and writable.
  AUTOFILL_WALLET_METADATA,
  // A theme object.
  THEMES,
  // A typed_url object, i.e. a URL the user has typed into the Omnibox.
  TYPED_URLS,
  // An extension object.
  EXTENSIONS,
  // An object representing a custom search engine.
  SEARCH_ENGINES,
  // An object representing a browser session, e.g. an open tab. This is used
  // for both "History" (together with TYPED_URLS) and "Tabs" (depending on
  // PROXY_TABS).
  SESSIONS,
  // An app object.
  APPS,
  // An app setting from the extension settings API.
  APP_SETTINGS,
  // An extension setting from the extension settings API.
  EXTENSION_SETTINGS,
  // History delete directives, used to propagate history deletions (e.g. based
  // on a time range).
  HISTORY_DELETE_DIRECTIVES,
  // Custom spelling dictionary entries.
  DICTIONARY,
  // Favicon images, including both the image URL and the actual pixels.
  DEPRECATED_FAVICON_IMAGES,
  // Favicon tracking information, i.e. metadata such as last visit date.
  DEPRECATED_FAVICON_TRACKING,
  // Client-specific metadata, synced before other user types.
  DEVICE_INFO,
  // These preferences are synced before other user types and are never
  // encrypted.
  PRIORITY_PREFERENCES,
  // Supervised user settings. Cannot be encrypted.
  SUPERVISED_USER_SETTINGS,
  // App List items, used by the ChromeOS app launcher.
  APP_LIST,
  // Supervised user whitelists. Each item contains a CRX ID (like an extension
  // ID) and a name.
  SUPERVISED_USER_WHITELISTS,
  // ARC package items, i.e. Android apps on ChromeOS.
  ARC_PACKAGE,
  // Printer device information. ChromeOS only.
  PRINTERS,
  // Reading list items. iOS only.
  READING_LIST,
  // Commit only user events.
  USER_EVENTS,
  // Commit only user consents.
  USER_CONSENTS,
  // Tabs sent between devices.
  SEND_TAB_TO_SELF,
  // Commit only security events.
  SECURITY_EVENTS,
  // Wi-Fi network configurations + credentials
  WIFI_CONFIGURATIONS,
  // A web app object.
  WEB_APPS,
  // OS-specific preferences (a.k.a. "OS settings"). Chrome OS only.
  OS_PREFERENCES,
  // Synced before other user types. Never encrypted. Chrome OS only.
  OS_PRIORITY_PREFERENCES,
  // Commit only sharing message object.
  SHARING_MESSAGE,

  // ---- Proxy types ----
  // Proxy types are excluded from the sync protocol, but are still considered
  // real user types. By convention, we prefix them with 'PROXY_' to distinguish
  // them from normal protocol types.

  // Tab sync. This is a placeholder type, so that Sessions can be implicitly
  // enabled for history sync and tabs sync.
  PROXY_TABS,
  FIRST_PROXY_TYPE = PROXY_TABS,
  LAST_PROXY_TYPE = PROXY_TABS,
  LAST_USER_MODEL_TYPE = PROXY_TABS,

  // ---- Control Types ----
  // An object representing a set of Nigori keys.
  NIGORI,
  DEPRECATED_EXPERIMENTS,
  LAST_REAL_MODEL_TYPE = DEPRECATED_EXPERIMENTS,

  NUM_ENTRIES,
};

using ModelTypeSet =
    EnumSet<ModelType, FIRST_REAL_MODEL_TYPE, LAST_REAL_MODEL_TYPE>;
using FullModelTypeSet = EnumSet<ModelType, UNSPECIFIED, LAST_REAL_MODEL_TYPE>;
using ModelTypeNameMap = std::map<ModelType, const char*>;

inline ModelType ModelTypeFromInt(int i) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, ModelType::NUM_ENTRIES);
  return static_cast<ModelType>(i);
}

// A version of the ModelType enum for use in histograms. ModelType does not
// have stable values (e.g. new ones may be inserted in the middle), so it can't
// be recorded directly.
// Instead of using entries from this enum directly, you'll usually want to get
// them via ModelTypeHistogramValue(model_type).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. When you add a new entry or when you
// deprecate an existing one, also update SyncModelTypes in enums.xml and
// SyncModelType suffix in histograms.xml.
enum class ModelTypeForHistograms {
  kUnspecified = 0,
  kTopLevelFolder = 1,
  kBookmarks = 2,
  kPreferences = 3,
  kPasswords = 4,
  kAutofillProfile = 5,
  kAutofill = 6,
  kThemes = 7,
  kTypedUrls = 8,
  kExtensions = 9,
  kSearchEngines = 10,
  kSessions = 11,
  kApps = 12,
  kAppSettings = 13,
  kExtensionSettings = 14,
  // kDeprecatedAppNotifications = 15,
  kHistoryDeleteDirectices = 16,
  kNigori = 17,
  kDeviceInfo = 18,
  kDeprecatedExperiments = 19,
  // kDeprecatedSyncedNotifications = 20,
  kPriorityPreferences = 21,
  kDictionary = 22,
  kFaviconImages = 23,
  kFaviconTracking = 24,
  kProxyTabs = 25,
  kSupervisedUserSettings = 26,
  // kDeprecatedSupervisedUsers = 27,
  // kDeprecatedArticles = 28,
  kAppList = 29,
  // kDeprecatedSupervisedUserSharedSettings = 30,
  // kDeprecatedSyncedNotificationAppInfo = 31,
  // kDeprecatedWifiCredentials = 32,
  kSupervisedUserWhitelists = 33,
  kAutofillWalletData = 34,
  kAutofillWalletMetadata = 35,
  kArcPackage = 36,
  kPrinters = 37,
  kReadingList = 38,
  kUserEvents = 39,
  // kDeprecatedMountainShares = 40,
  kUserConsents = 41,
  kSendTabToSelf = 42,
  kSecurityEvents = 43,
  kWifiConfigurations = 44,
  kWebApps = 45,
  kOsPreferences = 46,
  kOsPriorityPreferences = 47,
  kSharingMessage = 48,
  kMaxValue = kSharingMessage
};

// Used to mark the type of EntitySpecifics that has no actual data.
void AddDefaultFieldValue(ModelType type, sync_pb::EntitySpecifics* specifics);

// Extract the model type of a SyncEntity protocol buffer.  ModelType is a
// local concept: the enum is not in the protocol.  The SyncEntity's ModelType
// is inferred from the presence of particular datatype field in the
// entity specifics.
ModelType GetModelType(const sync_pb::SyncEntity& sync_entity);

// Extract the model type from an EntitySpecifics field.  Note that there
// are some ModelTypes (like TOP_LEVEL_FOLDER) that can't be inferred this way;
// prefer using GetModelType where possible.
ModelType GetModelTypeFromSpecifics(const sync_pb::EntitySpecifics& specifics);

// Protocol types are those types that have actual protocol buffer
// representations. This distinguishes them from Proxy types, which have no
// protocol representation and are never sent to the server.
constexpr ModelTypeSet ProtocolTypes() {
  return ModelTypeSet(
      BOOKMARKS, PREFERENCES, PASSWORDS, AUTOFILL_PROFILE, AUTOFILL,
      AUTOFILL_WALLET_DATA, AUTOFILL_WALLET_METADATA, THEMES, TYPED_URLS,
      EXTENSIONS, SEARCH_ENGINES, SESSIONS, APPS, APP_SETTINGS,
      EXTENSION_SETTINGS, HISTORY_DELETE_DIRECTIVES, DICTIONARY,
      DEPRECATED_FAVICON_IMAGES, DEPRECATED_FAVICON_TRACKING, DEVICE_INFO,
      PRIORITY_PREFERENCES, SUPERVISED_USER_SETTINGS, APP_LIST,
      SUPERVISED_USER_WHITELISTS, ARC_PACKAGE, PRINTERS, READING_LIST,
      USER_EVENTS, NIGORI, DEPRECATED_EXPERIMENTS, USER_CONSENTS,
      SEND_TAB_TO_SELF, SECURITY_EVENTS, WEB_APPS, WIFI_CONFIGURATIONS,
      OS_PREFERENCES, OS_PRIORITY_PREFERENCES, SHARING_MESSAGE);
}

// These are the normal user-controlled types. This is to distinguish from
// ControlTypes which are always enabled.  Note that some of these share a
// preference flag, so not all of them are individually user-selectable.
constexpr ModelTypeSet UserTypes() {
  return ModelTypeSet::FromRange(FIRST_USER_MODEL_TYPE, LAST_USER_MODEL_TYPE);
}

// User types, which are not user-controlled.
constexpr ModelTypeSet AlwaysPreferredUserTypes() {
  return ModelTypeSet(DEVICE_INFO, USER_CONSENTS, SECURITY_EVENTS,
                      SUPERVISED_USER_SETTINGS, SUPERVISED_USER_WHITELISTS,
                      SHARING_MESSAGE);
}

// User types which are always encrypted.
constexpr ModelTypeSet AlwaysEncryptedUserTypes() {
  // If you add a new model type here that is conceptually different from a
  // password, make sure you audit UI code that refers to these types as
  // passwords, e.g. consumers of IsEncryptEverythingEnabled().
  return ModelTypeSet(PASSWORDS, WIFI_CONFIGURATIONS);
}

// This is the subset of UserTypes() that have priority over other types.  These
// types are synced before other user types and are never encrypted.
constexpr ModelTypeSet PriorityUserTypes() {
  return ModelTypeSet(DEVICE_INFO, PRIORITY_PREFERENCES,
                      SUPERVISED_USER_SETTINGS, SUPERVISED_USER_WHITELISTS,
                      OS_PRIORITY_PREFERENCES, SHARING_MESSAGE);
}

// Proxy types are placeholder types for handling implicitly enabling real
// types. They do not exist at the server, and are simply used for
// UI/Configuration logic.
constexpr ModelTypeSet ProxyTypes() {
  return ModelTypeSet::FromRange(FIRST_PROXY_TYPE, LAST_PROXY_TYPE);
}

// Returns a list of all control types.
//
// The control types are intended to contain metadata nodes that are essential
// for the normal operation of the syncer.  As such, they have the following
// special properties:
// - They are downloaded early during SyncBackend initialization.
// - They are always enabled.  Users may not disable these types.
// - Their contents are not encrypted automatically.
// - They support custom update application and conflict resolution logic.
// - All change processing occurs on the sync thread (GROUP_PASSIVE).
constexpr ModelTypeSet ControlTypes() {
  return ModelTypeSet(NIGORI);
}

// Returns true if this is a control type.
//
// See comment above for more information on what makes these types special.
constexpr bool IsControlType(ModelType model_type) {
  return ControlTypes().Has(model_type);
}

// Types that may commit data, but should never be included in a GetUpdates.
// These are never encrypted.
constexpr ModelTypeSet CommitOnlyTypes() {
  return ModelTypeSet(USER_EVENTS, USER_CONSENTS, SECURITY_EVENTS,
                      SHARING_MESSAGE);
}

// User types that can be encrypted, which is a subset of UserTypes() and a
// superset of AlwaysEncryptedUserTypes();
ModelTypeSet EncryptableUserTypes();

// Determine a model type from the field number of its associated
// EntitySpecifics field.  Returns UNSPECIFIED if the field number is
// not recognized.
//
// If you're putting the result in a ModelTypeSet, you should use the
// following pattern:
//
//   ModelTypeSet model_types;
//   // Say we're looping through a list of items, each of which has a
//   // field number.
//   for (...) {
//     int field_number = ...;
//     ModelType model_type =
//         GetModelTypeFromSpecificsFieldNumber(field_number);
//     if (!IsRealDataType(model_type)) {
//       DLOG(WARNING) << "Unknown field number " << field_number;
//       continue;
//     }
//     model_types.Put(model_type);
//   }
ModelType GetModelTypeFromSpecificsFieldNumber(int field_number);

// Return the field number of the EntitySpecifics field associated with
// a model type.
int GetSpecificsFieldNumberFromModelType(ModelType model_type);

// TODO(sync): The functions below badly need some cleanup.

// Returns a string with application lifetime that represents the name of
// |model_type|.
const char* ModelTypeToString(ModelType model_type);

// Returns a string with application lifetime that is used as the histogram
// suffix for |model_type|.
const char* ModelTypeToHistogramSuffix(ModelType model_type);

// Some histograms take an integer parameter that represents a model type.
// The mapping from ModelType to integer is defined here. It defines a
// completely different order than the ModelType enum itself. The mapping should
// match the SyncModelTypes mapping from integer to labels defined in enums.xml.
ModelTypeForHistograms ModelTypeHistogramValue(ModelType model_type);

// Returns for every model_type a positive unique integer that is stable over
// time and thus can be used when persisting data.
int ModelTypeToStableIdentifier(ModelType model_type);

// Handles all model types, and not just real ones.
std::unique_ptr<base::Value> ModelTypeToValue(ModelType model_type);

// Returns the ModelType corresponding to the name |model_type_string|.
ModelType ModelTypeFromString(const std::string& model_type_string);

// Returns the comma-separated string representation of |model_types|.
std::string ModelTypeSetToString(ModelTypeSet model_types);

// Necessary for compatibility with EXPECT_EQ and the like.
std::ostream& operator<<(std::ostream& out, ModelTypeSet model_type_set);

// Returns the set of comma-separated model types from |model_type_string|.
ModelTypeSet ModelTypeSetFromString(const std::string& model_type_string);

// Generates a base::ListValue from |model_types|.
std::unique_ptr<base::ListValue> ModelTypeSetToValue(ModelTypeSet model_types);

// Returns a string corresponding to the syncable tag for this datatype.
std::string ModelTypeToRootTag(ModelType type);

// Returns root_tag for |model_type| in ModelTypeInfo.
// Difference with ModelTypeToRootTag(), this just simply returns root_tag in
// ModelTypeInfo.
const char* GetModelTypeRootTag(ModelType model_type);

// Convert a real model type to a notification type (used for
// subscribing to server-issued notifications).  Returns true iff
// |model_type| was a real model type and |notification_type| was
// filled in.
bool RealModelTypeToNotificationType(ModelType model_type,
                                     std::string* notification_type);

// Converts a notification type to a real model type.  Returns true
// iff |notification_type| was the notification type of a real model
// type and |model_type| was filled in.
bool NotificationTypeToRealModelType(const std::string& notification_type,
                                     ModelType* model_type);

// Returns true if |model_type| is a real datatype
bool IsRealDataType(ModelType model_type);

// Returns true if |model_type| is a proxy type
bool IsProxyType(ModelType model_type);

// Returns true if |model_type| is an act-once type. Act once types drop
// entities after applying them. Drops are deletes that are not synced to other
// clients.
// TODO(haitaol): Make entries of act-once data types immutable.
bool IsActOnceDataType(ModelType model_type);

// Returns true if |model_type| requires its root folder to be explicitly
// created on the server during initial sync.
bool IsTypeWithServerGeneratedRoot(ModelType model_type);

// Returns true if root folder for |model_type| is created on the client when
// that type is initially synced.
bool IsTypeWithClientGeneratedRoot(ModelType model_type);

// Returns true if |model_type| supports parent-child hierarchy or entries.
bool TypeSupportsHierarchy(ModelType model_type);

// Returns true if |model_type| supports ordering of sibling entries.
bool TypeSupportsOrdering(ModelType model_type);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_MODEL_TYPE_H_
