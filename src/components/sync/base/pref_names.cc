// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/pref_names.h"

namespace syncer {

namespace prefs {

// 64-bit integer serialization of the base::Time when the last sync occurred.
const char kSyncLastSyncedTime[] = "sync.last_synced_time";

// 64-bit integer serialization of the base::Time of the last sync poll.
const char kSyncLastPollTime[] = "sync.last_poll_time";

// 64-bit integer serialization of base::TimeDelta storing poll intervals
// received by the server (in seconds). For historic reasons, this is called
// "short_poll_interval", but it's worth the hassle to rename it.
const char kSyncPollIntervalSeconds[] = "sync.short_poll_interval";

// Boolean specifying whether the user finished setting up sync at least once.
const char kSyncFirstSetupComplete[] = "sync.has_setup_completed";

// Boolean specifying whether to automatically sync all data types (including
// future ones, as they're added).  If this is true, the following preferences
// (kSyncBookmarks, kSyncPasswords, etc.) can all be ignored.
const char kSyncKeepEverythingSynced[] = "sync.keep_everything_synced";

// Booleans specifying whether the user has selected to sync the following
// datatypes.
const char kSyncAppList[] = "sync.app_list";
const char kSyncAppNotifications[] = "sync.app_notifications";
const char kSyncAppSettings[] = "sync.app_settings";
const char kSyncApps[] = "sync.apps";
const char kSyncArcPackage[] = "sync.arc_package";
const char kSyncArticles[] = "sync.articles";
const char kSyncAutofillProfile[] = "sync.autofill_profile";
const char kSyncAutofillWallet[] = "sync.autofill_wallet";
const char kSyncAutofillWalletMetadata[] = "sync.autofill_wallet_metadata";
const char kSyncAutofill[] = "sync.autofill";
const char kSyncBookmarks[] = "sync.bookmarks";
const char kSyncDeviceInfo[] = "sync.device_info";
const char kSyncDictionary[] = "sync.dictionary";
const char kSyncExtensionSettings[] = "sync.extension_settings";
const char kSyncExtensions[] = "sync.extensions";
const char kSyncFaviconImages[] = "sync.favicon_images";
const char kSyncFaviconTracking[] = "sync.favicon_tracking";
const char kSyncHistoryDeleteDirectives[] = "sync.history_delete_directives";
const char kSyncMountainShares[] = "sync.mountain_shares";
const char kSyncPasswords[] = "sync.passwords";
const char kSyncPreferences[] = "sync.preferences";
const char kSyncPrinters[] = "sync.printers";
const char kSyncPriorityPreferences[] = "sync.priority_preferences";
const char kSyncReadingList[] = "sync.reading_list";
const char kSyncSearchEngines[] = "sync.search_engines";
const char kSyncSendTabToSelf[] = "sync.send_tab_to_self";
const char kSyncSessions[] = "sync.sessions";
const char kSyncSupervisedUserSettings[] = "sync.managed_user_settings";
const char kSyncSupervisedUserSharedSettings[] =
    "sync.managed_user_shared_settings";
const char kSyncSupervisedUserWhitelists[] = "sync.managed_user_whitelists";
const char kSyncSupervisedUsers[] = "sync.managed_users";
const char kSyncSyncedNotificationAppInfo[] =
    "sync.synced_notification_app_info";
const char kSyncSyncedNotifications[] = "sync.synced_notifications";
const char kSyncTabs[] = "sync.tabs";
const char kSyncThemes[] = "sync.themes";
const char kSyncTypedUrls[] = "sync.typed_urls";
const char kSyncUserConsents[] = "sync.user_consents";
const char kSyncUserEvents[] = "sync.user_events";
const char kSyncWifiConfigurations[] = "sync.wifi_configurations";
const char kSyncWifiCredentials[] = "sync.wifi_credentials";

// Boolean used by enterprise configuration management in order to lock down
// sync.
const char kSyncManaged[] = "sync.managed";

// Boolean whether has requested sync to be enabled. This is set early in the
// sync setup flow, after the user has pressed "turn on sync" but before they
// have accepted the confirmation dialog (that maps to kSyncFirstSetupComplete).
// This is also set to false when sync is disabled by the user in sync settings,
// or when sync was reset from the dashboard.
const char kSyncRequested[] = "sync.requested";

// A string that can be used to restore sync encryption infrastructure on
// startup so that the user doesn't need to provide credentials on each start.
const char kSyncEncryptionBootstrapToken[] = "sync.encryption_bootstrap_token";

// Same as kSyncEncryptionBootstrapToken, but derived from the keystore key,
// so we don't have to do a GetKey command at restart.
const char kSyncKeystoreEncryptionBootstrapToken[] =
    "sync.keystore_encryption_bootstrap_token";

const char kSyncCacheGuid[] = "sync.cache_guid";
const char kSyncBirthday[] = "sync.birthday";
const char kSyncBagOfChips[] = "sync.bag_of_chips";

// Stores whether a platform specific passphrase error prompt has been shown to
// the user (e.g. an Android system notification). Used for out of band prompts
// that we only want to use once.
const char kSyncPassphrasePrompted[] = "sync.passphrase_prompted";

// Dictionary of last seen invalidation versions for each model type.
const char kSyncInvalidationVersions[] = "sync.invalidation_versions";

// The product version from the last restart of Chrome.
const char kSyncLastRunVersion[] = "sync.last_run_version";

// Enabled the local sync backend implemented by the LoopbackServer.
const char kEnableLocalSyncBackend[] = "sync.enable_local_sync_backend";

// Specifies the local sync backend directory. The name is chosen to mimic
// user-data-dir etc. This flag only matters if the enable-local-sync-backend
// flag is present.
const char kLocalSyncBackendDir[] = "sync.local_sync_backend_dir";

// Root dictionary pref to store the user's birth year and gender that are
// provided by the sync server. This is a read-only syncable priority pref, sent
// from the sync server to the client.
const char kSyncDemographics[] = "sync.demographics";

// This pref value is subordinate to the kSyncDemographics dictionary pref and
// is synced to the client. It stores the self-reported birth year of the
// syncing user. as provided by the sync server. This value should not be logged
// to UMA directly; instead, it should be summed with the
// kSyncDemographicsBirthYearNoiseOffset.
const char kSyncDemographics_BirthYearPath[] = "birth_year";

// This pref value is subordinate to the kSyncDemographics dictionary pref and
// is synced to the client. It stores the self-reported gender of the syncing
// user, as provided by the sync server. The gender is encoded using the Gender
// enum defined in metrics::UserDemographicsProto
// (see third_party/metrics_proto/user_demographics.proto).
const char kSyncDemographics_GenderPath[] = "gender";

// Stores a "secret" offset that is used to randomize the birth year for metrics
// reporting. This value should not be logged to UMA directly; instead, it
// should be summed with the kSyncDemographicsBirthYear. This value is generated
// locally on the client the first time a user begins to merge birth year data
// into their UMA reports. The value is synced to the user's other devices so
// that the user consistently uses the same offset across login/logout events
// and after clearing their other browser data.
const char kSyncDemographicsBirthYearOffset[] =
    "sync.demographics_birth_year_offset";

}  // namespace prefs

}  // namespace syncer
