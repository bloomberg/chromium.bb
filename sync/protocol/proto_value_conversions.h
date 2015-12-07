// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep this file in sync with the .proto files in this directory.

#ifndef SYNC_PROTOCOL_PROTO_VALUE_CONVERSIONS_H_
#define SYNC_PROTOCOL_PROTO_VALUE_CONVERSIONS_H_

#include "base/memory/scoped_ptr.h"
#include "sync/base/sync_export.h"

namespace base {
class DictionaryValue;
}

namespace sync_pb {
class AppListSpecifics;
class AppNotification;
class AppNotificationSettings;
class AppSettingSpecifics;
class AppSpecifics;
class ArticleSpecifics;
class AttachmentIdProto;
class AutofillProfileSpecifics;
class AutofillSpecifics;
class AutofillWalletSpecifics;
class BookmarkSpecifics;
class ClientConfigParams;
class ClientToServerMessage;
class ClientToServerResponse;
class CollapsedInfo;
class DatatypeAssociationStats;
class DebugEventInfo;
class DebugInfo;
class DeviceInfoSpecifics;
class DeviceInformation;
class DictionarySpecifics;
class EncryptedData;
class EntitySpecifics;
class EverythingDirective;
class ExperimentsSpecifics;
class ExtensionSettingSpecifics;
class ExtensionSpecifics;
class FaviconImageSpecifics;
class FaviconTrackingSpecifics;
class GlobalIdDirective;
class HistoryDeleteDirectiveSpecifics;
class KeystoreEncryptionFlagsSpecifics;
class LinkedAppIconInfo;
class ManagedUserSettingSpecifics;
class ManagedUserSharedSettingSpecifics;
class ManagedUserSpecifics;
class ManagedUserWhitelistSpecifics;
class Media;
class NavigationRedirect;
class NigoriSpecifics;
class PasswordSpecifics;
class PasswordSpecificsData;
class PreferenceSpecifics;
class PriorityPreferenceSpecifics;
class SearchEngineSpecifics;
class SessionHeader;
class SessionSpecifics;
class SessionTab;
class SessionWindow;
class SimpleCollapsedLayout;
class SyncCycleCompletedEventInfo;
class SyncEntity;
class SyncedNotificationAppInfoSpecifics;
class SyncedNotificationSpecifics;
class TabNavigation;
class Target;
class ThemeSpecifics;
class TimeRangeDirective;
class TypedUrlSpecifics;
class WalletMaskedCreditCard;
class WalletMetadataSpecifics;
class WalletPostalAddress;
class WifiCredentialSpecifics;
}  // namespace sync_pb

// Utility functions to convert sync protocol buffers to dictionaries.
// Each protocol field is mapped to a key of the same name.  Repeated
// fields are mapped to array values and sub-messages are mapped to
// sub-dictionary values.
//
// TODO(akalin): Add has_* information.
//
// TODO(akalin): Improve enum support.

namespace syncer {

// Ownership of all returned DictionaryValues are transferred to the
// caller.

// TODO(akalin): Perhaps extend this to decrypt?
SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> EncryptedDataToValue(
    const sync_pb::EncryptedData& encrypted_data);

// Sub-protocol of AppListSpecifics.
SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> AppListSpecificsToValue(
    const sync_pb::AppListSpecifics& proto);

// Sub-protocols of AppSpecifics.
SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> AppSettingsToValue(
    const sync_pb::AppNotificationSettings& app_notification_settings);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> LinkedAppIconInfoToValue(
    const sync_pb::LinkedAppIconInfo& linked_app_icon_info);

// Sub-protocols of SessionSpecifics.

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> SessionHeaderToValue(
    const sync_pb::SessionHeader& session_header);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> SessionTabToValue(
    const sync_pb::SessionTab& session_tab);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> SessionWindowToValue(
    const sync_pb::SessionWindow& session_window);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> TabNavigationToValue(
    const sync_pb::TabNavigation& tab_navigation);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> NavigationRedirectToValue(
    const sync_pb::NavigationRedirect& navigation_redirect);

// Sub-protocol of PasswordSpecifics.

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
PasswordSpecificsDataToValue(
    const sync_pb::PasswordSpecificsData& password_specifics_data);

// Sub-protocol of NigoriSpecifics.

scoped_ptr<base::DictionaryValue> DeviceInformationToValue(
    const sync_pb::DeviceInformation& device_information);

// Sub-protocol of HistoryDeleteDirectiveSpecifics.

scoped_ptr<base::DictionaryValue> GlobalIdDirectiveToValue(
    const sync_pb::GlobalIdDirective& global_id_directive);

scoped_ptr<base::DictionaryValue> TimeRangeDirectiveToValue(
    const sync_pb::TimeRangeDirective& time_range_directive);

// Sub-protocol of Experiments.

scoped_ptr<base::DictionaryValue> KeystoreEncryptionToValue(
    const sync_pb::KeystoreEncryptionFlagsSpecifics& proto);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> SessionSpecificsToValue(
    const sync_pb::SessionSpecifics& session_specifics);

// Main *SpecificsToValue functions.

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> AppNotificationToValue(
    const sync_pb::AppNotification& app_notification_specifics);

scoped_ptr<base::DictionaryValue> AppSettingSpecificsToValue(
    const sync_pb::AppSettingSpecifics& app_setting_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> AppSpecificsToValue(
    const sync_pb::AppSpecifics& app_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> ArticleSpecificsToValue(
    const sync_pb::ArticleSpecifics& article_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> AutofillSpecificsToValue(
    const sync_pb::AutofillSpecifics& autofill_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
AutofillProfileSpecificsToValue(
    const sync_pb::AutofillProfileSpecifics& autofill_profile_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
WalletMetadataSpecificsToValue(
    const sync_pb::WalletMetadataSpecifics& wallet_metadata_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
AutofillWalletSpecificsToValue(
    const sync_pb::AutofillWalletSpecifics& autofill_wallet_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> BookmarkSpecificsToValue(
    const sync_pb::BookmarkSpecifics& bookmark_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
DeviceInfoSpecificsToValue(
    const sync_pb::DeviceInfoSpecifics& device_info_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
DictionarySpecificsToValue(
    const sync_pb::DictionarySpecifics& dictionary_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
ExperimentsSpecificsToValue(const sync_pb::ExperimentsSpecifics& proto);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
PriorityPreferenceSpecificsToValue(
    const sync_pb::PriorityPreferenceSpecifics& proto);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
ExtensionSettingSpecificsToValue(
    const sync_pb::ExtensionSettingSpecifics& extension_setting_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> ExtensionSpecificsToValue(
    const sync_pb::ExtensionSpecifics& extension_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
FaviconImageSpecificsToValue(
    const sync_pb::FaviconImageSpecifics& favicon_image_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
FaviconTrackingSpecificsToValue(
    const sync_pb::FaviconTrackingSpecifics& favicon_tracking_specifics);

SYNC_EXPORT scoped_ptr<base::DictionaryValue>
HistoryDeleteDirectiveSpecificsToValue(
    const sync_pb::HistoryDeleteDirectiveSpecifics&
        history_delete_directive_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
ManagedUserSettingSpecificsToValue(
    const sync_pb::ManagedUserSettingSpecifics& managed_user_setting_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
ManagedUserSpecificsToValue(
    const sync_pb::ManagedUserSpecifics& managed_user_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
ManagedUserSharedSettingSpecificsToValue(
    const sync_pb::ManagedUserSharedSettingSpecifics&
        managed_user_shared_setting_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
ManagedUserWhitelistSpecificsToValue(
    const sync_pb::ManagedUserWhitelistSpecifics&
        managed_user_whitelist_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> MediaToValue(
    const sync_pb::Media& media);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> NigoriSpecificsToValue(
    const sync_pb::NigoriSpecifics& nigori_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> PasswordSpecificsToValue(
    const sync_pb::PasswordSpecifics& password_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
PreferenceSpecificsToValue(
    const sync_pb::PreferenceSpecifics& password_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
SyncedNotificationAppInfoSpecificsToValue(
    const sync_pb::SyncedNotificationAppInfoSpecifics&
        synced_notification_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
SyncedNotificationSpecificsToValue(
    const sync_pb::SyncedNotificationSpecifics& synced_notification_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
SearchEngineSpecificsToValue(
    const sync_pb::SearchEngineSpecifics& search_engine_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> ThemeSpecificsToValue(
    const sync_pb::ThemeSpecifics& theme_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> TypedUrlSpecificsToValue(
    const sync_pb::TypedUrlSpecifics& typed_url_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
WalletMaskedCreditCardToValue(
    const sync_pb::WalletMaskedCreditCard& wallet_masked_card);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
WalletPostalAddressToValue(
    const sync_pb::WalletPostalAddress& wallet_postal_address);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
WifiCredentialSpecificsToValue(
    const sync_pb::WifiCredentialSpecifics& wifi_credential_specifics);

// Any present extensions are mapped to sub-dictionary values with the
// key equal to the extension name.
SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> EntitySpecificsToValue(
    const sync_pb::EntitySpecifics& specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> SyncEntityToValue(
    const sync_pb::SyncEntity& entity,
    bool include_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
ClientToServerMessageToValue(const sync_pb::ClientToServerMessage& proto,
                             bool include_specifics);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue>
ClientToServerResponseToValue(const sync_pb::ClientToServerResponse& proto,
                              bool include_specifics);

scoped_ptr<base::DictionaryValue> DatatypeAssociationStatsToValue(
    const sync_pb::DatatypeAssociationStats& proto);

scoped_ptr<base::DictionaryValue> DebugEventInfoToValue(
    const sync_pb::DebugEventInfo& proto);

scoped_ptr<base::DictionaryValue> DebugInfoToValue(
    const sync_pb::DebugInfo& proto);

scoped_ptr<base::DictionaryValue> SyncCycleCompletedEventInfoToValue(
    const sync_pb::SyncCycleCompletedEventInfo& proto);

scoped_ptr<base::DictionaryValue> ClientConfigParamsToValue(
    const sync_pb::ClientConfigParams& proto);

SYNC_EXPORT_PRIVATE scoped_ptr<base::DictionaryValue> AttachmentIdProtoToValue(
    const sync_pb::AttachmentIdProto& proto);

}  // namespace syncer

#endif  // SYNC_PROTOCOL_PROTO_VALUE_CONVERSIONS_H_
