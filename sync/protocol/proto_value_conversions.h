// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep this file in sync with the .proto files in this directory.

#ifndef SYNC_PROTOCOL_PROTO_VALUE_CONVERSIONS_H_
#define SYNC_PROTOCOL_PROTO_VALUE_CONVERSIONS_H_

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
class BookmarkSpecifics;
class ClientConfigParams;
class ClientToServerMessage;
class ClientToServerResponse;
class CoalescedSyncedNotification;
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
class Media;
class ManagedUserSettingSpecifics;
class ManagedUserSharedSettingSpecifics;
class ManagedUserSpecifics;
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
class SyncedNotification;
class SyncedNotificationAction;
class SyncedNotificationAppInfo;
class SyncedNotificationAppInfoSpecifics;
class SyncedNotificationDestination;
class SyncedNotificationImage;
class SyncedNotificationProfileImage;
class SyncedNotificationRenderInfo;
class SyncedNotificationSpecifics;
class TabNavigation;
class Target;
class ThemeSpecifics;
class TimeRangeDirective;
class TypedUrlSpecifics;
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
SYNC_EXPORT_PRIVATE base::DictionaryValue* EncryptedDataToValue(
    const sync_pb::EncryptedData& encrypted_data);

// Sub-protocol of AppListSpecifics.
SYNC_EXPORT_PRIVATE base::DictionaryValue* AppListSpecificsToValue(
    const sync_pb::AppListSpecifics& proto);

// Sub-protocol of AppSpecifics.
SYNC_EXPORT_PRIVATE base::DictionaryValue* AppSettingsToValue(
    const sync_pb::AppNotificationSettings& app_notification_settings);

// Sub-protocols of SessionSpecifics.

SYNC_EXPORT_PRIVATE base::DictionaryValue* SessionHeaderToValue(
    const sync_pb::SessionHeader& session_header);

SYNC_EXPORT_PRIVATE base::DictionaryValue* SessionTabToValue(
    const sync_pb::SessionTab& session_tab);

SYNC_EXPORT_PRIVATE base::DictionaryValue* SessionWindowToValue(
    const sync_pb::SessionWindow& session_window);

SYNC_EXPORT_PRIVATE base::DictionaryValue* TabNavigationToValue(
    const sync_pb::TabNavigation& tab_navigation);

SYNC_EXPORT_PRIVATE base::DictionaryValue* NavigationRedirectToValue(
    const sync_pb::NavigationRedirect& navigation_redirect);

// Sub-protocol of PasswordSpecifics.

SYNC_EXPORT_PRIVATE base::DictionaryValue* PasswordSpecificsDataToValue(
    const sync_pb::PasswordSpecificsData& password_specifics_data);

// Sub-protocol of NigoriSpecifics.

base::DictionaryValue* DeviceInformationToValue(
    const sync_pb::DeviceInformation& device_information);

// Sub-protocol of HistoryDeleteDirectiveSpecifics.

base::DictionaryValue* GlobalIdDirectiveToValue(
    const sync_pb::GlobalIdDirective& global_id_directive);

base::DictionaryValue* TimeRangeDirectiveToValue(
    const sync_pb::TimeRangeDirective& time_range_directive);

// Sub-protocol of Experiments.

base::DictionaryValue* KeystoreEncryptionToValue(
    const sync_pb::KeystoreEncryptionFlagsSpecifics& proto);

// Sub protocols of SyncedNotifications.

base::DictionaryValue* SimpleCollapsedLayoutToValue(
    const sync_pb::SimpleCollapsedLayout& proto);

base::DictionaryValue* CollapsedInfoToValue(
    const sync_pb::CollapsedInfo& proto);

base::DictionaryValue* RenderInfoToValue(
    const sync_pb::SyncedNotificationRenderInfo& proto);

base::DictionaryValue* CoalescedNotificationToValue(
    const sync_pb::CoalescedSyncedNotification& proto);

base::DictionaryValue* SyncedNotificationActionToValue(
    const sync_pb::SyncedNotificationAction& action);

base::DictionaryValue* SyncedNotificationDestinationToValue(
    const sync_pb::SyncedNotificationDestination& destination);

base::DictionaryValue* SyncedNotificationToValue(
    const sync_pb::SyncedNotification& notification);

SYNC_EXPORT_PRIVATE base::DictionaryValue* SessionSpecificsToValue(
    const sync_pb::SessionSpecifics& session_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* SyncedNotificationImageToValue(
    const sync_pb::SyncedNotificationImage& image);

SYNC_EXPORT_PRIVATE base::DictionaryValue*
    SyncedNotificationProfileImageToValue(
        const sync_pb::SyncedNotificationProfileImage& image);

SYNC_EXPORT_PRIVATE base::DictionaryValue* TargetToValue(
    const sync_pb::Target& target);

// Main *SpecificsToValue functions.

SYNC_EXPORT_PRIVATE base::DictionaryValue* AppNotificationToValue(
    const sync_pb::AppNotification& app_notification_specifics);

base::DictionaryValue* AppSettingSpecificsToValue(
    const sync_pb::AppSettingSpecifics& app_setting_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* AppSpecificsToValue(
    const sync_pb::AppSpecifics& app_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* ArticleSpecificsToValue(
    const sync_pb::ArticleSpecifics& article_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* AutofillSpecificsToValue(
    const sync_pb::AutofillSpecifics& autofill_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* AutofillProfileSpecificsToValue(
    const sync_pb::AutofillProfileSpecifics& autofill_profile_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* BookmarkSpecificsToValue(
    const sync_pb::BookmarkSpecifics& bookmark_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* DeviceInfoSpecificsToValue(
    const sync_pb::DeviceInfoSpecifics& device_info_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* DictionarySpecificsToValue(
    const sync_pb::DictionarySpecifics& dictionary_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* ExperimentsSpecificsToValue(
    const sync_pb::ExperimentsSpecifics& proto);

SYNC_EXPORT_PRIVATE base::DictionaryValue* PriorityPreferenceSpecificsToValue(
    const sync_pb::PriorityPreferenceSpecifics& proto);

SYNC_EXPORT_PRIVATE base::DictionaryValue* ExtensionSettingSpecificsToValue(
    const sync_pb::ExtensionSettingSpecifics& extension_setting_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* ExtensionSpecificsToValue(
    const sync_pb::ExtensionSpecifics& extension_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* FaviconImageSpecificsToValue(
    const sync_pb::FaviconImageSpecifics& favicon_image_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* FaviconTrackingSpecificsToValue(
    const sync_pb::FaviconTrackingSpecifics& favicon_tracking_specifics);

SYNC_EXPORT base::DictionaryValue* HistoryDeleteDirectiveSpecificsToValue(
    const sync_pb::HistoryDeleteDirectiveSpecifics&
        history_delete_directive_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* ManagedUserSettingSpecificsToValue(
    const sync_pb::ManagedUserSettingSpecifics& managed_user_setting_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* ManagedUserSpecificsToValue(
    const sync_pb::ManagedUserSpecifics& managed_user_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue*
    ManagedUserSharedSettingSpecificsToValue(
        const sync_pb::ManagedUserSharedSettingSpecifics&
            managed_user_shared_setting_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* MediaToValue(
    const sync_pb::Media& media);

SYNC_EXPORT_PRIVATE base::DictionaryValue* NigoriSpecificsToValue(
    const sync_pb::NigoriSpecifics& nigori_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* PasswordSpecificsToValue(
    const sync_pb::PasswordSpecifics& password_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* PreferenceSpecificsToValue(
    const sync_pb::PreferenceSpecifics& password_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue*
    SyncedNotificationAppInfoSpecificsToValue(
        const sync_pb::SyncedNotificationAppInfoSpecifics&
        synced_notification_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* SyncedNotificationSpecificsToValue(
    const sync_pb::SyncedNotificationSpecifics&
    synced_notification_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* SearchEngineSpecificsToValue(
    const sync_pb::SearchEngineSpecifics& search_engine_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* ThemeSpecificsToValue(
    const sync_pb::ThemeSpecifics& theme_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* TypedUrlSpecificsToValue(
    const sync_pb::TypedUrlSpecifics& typed_url_specifics);

// Any present extensions are mapped to sub-dictionary values with the
// key equal to the extension name.
SYNC_EXPORT_PRIVATE base::DictionaryValue* EntitySpecificsToValue(
    const sync_pb::EntitySpecifics& specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* SyncEntityToValue(
    const sync_pb::SyncEntity& entity,
    bool include_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* ClientToServerMessageToValue(
    const sync_pb::ClientToServerMessage& proto,
    bool include_specifics);

SYNC_EXPORT_PRIVATE base::DictionaryValue* ClientToServerResponseToValue(
    const sync_pb::ClientToServerResponse& proto,
    bool include_specifics);

base::DictionaryValue* DatatypeAssociationStatsToValue(
    const sync_pb::DatatypeAssociationStats& proto);

base::DictionaryValue* DebugEventInfoToValue(
    const sync_pb::DebugEventInfo& proto);

base::DictionaryValue* DebugInfoToValue(
    const sync_pb::DebugInfo& proto);

base::DictionaryValue* SyncCycleCompletedEventInfoToValue(
    const sync_pb::SyncCycleCompletedEventInfo& proto);

base::DictionaryValue* ClientConfigParamsToValue(
    const sync_pb::ClientConfigParams& proto);

SYNC_EXPORT_PRIVATE base::DictionaryValue* AttachmentIdProtoToValue(
    const sync_pb::AttachmentIdProto& proto);

}  // namespace syncer

#endif  // SYNC_PROTOCOL_PROTO_VALUE_CONVERSIONS_H_
