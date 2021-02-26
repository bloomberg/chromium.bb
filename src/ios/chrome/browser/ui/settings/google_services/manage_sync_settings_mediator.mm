// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.h"

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/mac/foundation_util.h"
#include "base/notreached.h"
#include "components/autofill/core/common/autofill_prefs.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_observer_bridge.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/google_services/sync_error_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

namespace {

// Enterprise icon.
NSString* kGoogleServicesEnterpriseImage = @"google_services_enterprise";
// Sync error icon.
NSString* kGoogleServicesSyncErrorImage = @"google_services_sync_error";
}  // namespace

@interface ManageSyncSettingsMediator () <BooleanObserver,
                                          IdentityManagerObserverBridgeDelegate,
                                          SyncObserverModelBridge> {
  // Sync observer.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  // Whether Sync State changes should be currently ignored.
  BOOL _ignoreSyncStateChanges;
}

// Preference value for kAutofillWalletImportEnabled.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* autocompleteWalletPreference;
// Sync service.
@property(nonatomic, assign) syncer::SyncService* syncService;
// Model item for sync everything.
@property(nonatomic, strong) SyncSwitchItem* syncEverythingItem;
// Model item for each data types.
@property(nonatomic, strong) NSArray<SyncSwitchItem*>* syncSwitchItems;
// Autocomplete wallet item.
@property(nonatomic, strong) SyncSwitchItem* autocompleteWalletItem;
// Encryption item.
@property(nonatomic, strong) TableViewImageItem* encryptionItem;
// Sync error item.
@property(nonatomic, strong) TableViewItem* syncErrorItem;
// Returns YES if the sync data items should be enabled.
@property(nonatomic, assign, readonly) BOOL shouldSyncDataItemEnabled;
// Returns whether the Sync settings should be disabled because of a Sync error.
@property(nonatomic, assign, readonly) BOOL disabledBecauseOfSyncError;
// Returns YES if the user cannot turn on sync for enterprise policy reasons.
@property(nonatomic, assign, readonly) BOOL isSyncDisabledByAdministrator;
// Returns YES if the user is authenticated.
@property(nonatomic, assign, readonly) BOOL isAuthenticated;

@end

@implementation ManageSyncSettingsMediator

- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
                    userPrefService:(PrefService*)userPrefService {
  self = [super init];
  if (self) {
    DCHECK(syncService);
    self.syncService = syncService;
    _syncObserver.reset(new SyncObserverBridge(self, syncService));
    _autocompleteWalletPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:autofill::prefs::kAutofillWalletImportEnabled];
    _autocompleteWalletPreference.observer = self;
  }
  return self;
}

#pragma mark - Loads sync data type section

// Loads the sync data type section.
- (void)loadSyncDataTypeSection {
  TableViewModel* model = self.consumer.tableViewModel;
  [model addSectionWithIdentifier:SyncDataTypeSectionIdentifier];
  self.syncEverythingItem =
      [[SyncSwitchItem alloc] initWithType:SyncEverythingItemType];
  self.syncEverythingItem.text = GetNSString(IDS_IOS_SYNC_EVERYTHING_TITLE);
  [self updateSyncEverythingItemNotifyConsumer:NO];
  [model addItem:self.syncEverythingItem
      toSectionWithIdentifier:SyncDataTypeSectionIdentifier];
  self.syncSwitchItems = @[
    [self switchItemWithDataType:SyncSetupService::kSyncAutofill],
    [self switchItemWithDataType:SyncSetupService::kSyncBookmarks],
    [self switchItemWithDataType:SyncSetupService::kSyncOmniboxHistory],
    [self switchItemWithDataType:SyncSetupService::kSyncOpenTabs],
    [self switchItemWithDataType:SyncSetupService::kSyncPasswords],
    [self switchItemWithDataType:SyncSetupService::kSyncReadingList],
    [self switchItemWithDataType:SyncSetupService::kSyncPreferences]
  ];
  for (SyncSwitchItem* switchItem in self.syncSwitchItems) {
    [model addItem:switchItem
        toSectionWithIdentifier:SyncDataTypeSectionIdentifier];
  }
  self.autocompleteWalletItem =
      [[SyncSwitchItem alloc] initWithType:AutocompleteWalletItemType];
  self.autocompleteWalletItem.text =
      GetNSString(IDS_AUTOFILL_ENABLE_PAYMENTS_INTEGRATION_CHECKBOX_LABEL);
  [model addItem:self.autocompleteWalletItem
      toSectionWithIdentifier:SyncDataTypeSectionIdentifier];
  [self updateSyncItemsNotifyConsumer:NO];
}

// Updates the sync everything item, and notify the consumer if |notifyConsumer|
// is set to YES.
- (void)updateSyncEverythingItemNotifyConsumer:(BOOL)notifyConsumer {
  BOOL shouldSyncEverythingBeEditable =
      self.syncSetupService->IsSyncEnabled() &&
      (!self.disabledBecauseOfSyncError || self.syncSettingsNotConfirmed);
  BOOL shouldSyncEverythingItemBeOn =
      self.syncSetupService->IsSyncEnabled() &&
      self.syncSetupService->IsSyncingAllDataTypes();
  BOOL needsUpdate =
      (self.syncEverythingItem.on != shouldSyncEverythingItemBeOn) ||
      (self.syncEverythingItem.enabled != shouldSyncEverythingBeEditable);
  self.syncEverythingItem.on = shouldSyncEverythingItemBeOn;
  self.syncEverythingItem.enabled = shouldSyncEverythingBeEditable;
  if (needsUpdate && notifyConsumer) {
    [self.consumer reloadItem:self.syncEverythingItem];
  }
}

// Updates all the items related to sync (sync data items and autocomplete
// wallet item). The consumer is notified if |notifyConsumer| is set to YES.
- (void)updateSyncItemsNotifyConsumer:(BOOL)notifyConsumer {
  [self updateSyncDataItemsNotifyConsumer:notifyConsumer];
  [self updateAutocompleteWalletItemNotifyConsumer:notifyConsumer];
}

// Updates all the sync data type items, and notify the consumer if
// |notifyConsumer| is set to YES.
- (void)updateSyncDataItemsNotifyConsumer:(BOOL)notifyConsumer {
  for (SyncSwitchItem* syncSwitchItem in self.syncSwitchItems) {
    SyncSetupService::SyncableDatatype dataType =
        static_cast<SyncSetupService::SyncableDatatype>(
            syncSwitchItem.dataType);
    syncer::ModelType modelType = self.syncSetupService->GetModelType(dataType);
    BOOL isDataTypeSynced =
        self.syncSetupService->IsDataTypePreferred(modelType);
    BOOL needsUpdate =
        (syncSwitchItem.on != isDataTypeSynced) ||
        (syncSwitchItem.isEnabled != self.shouldSyncDataItemEnabled);
    syncSwitchItem.on = isDataTypeSynced;
    syncSwitchItem.enabled = self.shouldSyncDataItemEnabled;
    if (needsUpdate && notifyConsumer) {
      [self.consumer reloadItem:syncSwitchItem];
    }
  }
}

// Updates the autocomplete wallet item. The consumer is notified if
// |notifyConsumer| is set to YES.
- (void)updateAutocompleteWalletItemNotifyConsumer:(BOOL)notifyConsumer {
  syncer::ModelType autofillModelType =
      self.syncSetupService->GetModelType(SyncSetupService::kSyncAutofill);
  BOOL isAutofillOn =
      self.syncSetupService->IsDataTypePreferred(autofillModelType);
  BOOL autocompleteWalletEnabled =
      isAutofillOn && self.shouldSyncDataItemEnabled;
  BOOL autocompleteWalletOn = self.autocompleteWalletPreference.value;
  BOOL needsUpdate =
      (self.autocompleteWalletItem.enabled != autocompleteWalletEnabled) ||
      (self.autocompleteWalletItem.on != autocompleteWalletOn);
  self.autocompleteWalletItem.enabled = autocompleteWalletEnabled;
  self.autocompleteWalletItem.on = autocompleteWalletOn;
  if (needsUpdate && notifyConsumer) {
    [self.consumer reloadItem:self.autocompleteWalletItem];
  }
}

#pragma mark - Loads the advanced settings section

// Loads the advanced settings section.
- (void)loadAdvancedSettingsSection {
  TableViewModel* model = self.consumer.tableViewModel;
  [model addSectionWithIdentifier:AdvancedSettingsSectionIdentifier];
  // EncryptionItemType.
  self.encryptionItem =
      [[TableViewImageItem alloc] initWithType:EncryptionItemType];
  self.encryptionItem.title = GetNSString(IDS_IOS_MANAGE_SYNC_ENCRYPTION);
  // For kSyncServiceNeedsTrustedVaultKey, the disclosure indicator should not
  // be shown since the reauth dialog for the trusted vault is presented from
  // the bottom, and is not part of navigation controller.
  BOOL hasDisclosureIndicator =
      self.syncSetupService->GetSyncServiceState() !=
      SyncSetupService::kSyncServiceNeedsTrustedVaultKey;
  self.encryptionItem.accessoryType =
      hasDisclosureIndicator ? UITableViewCellAccessoryDisclosureIndicator
                             : UITableViewCellAccessoryNone;
  [model addItem:self.encryptionItem
      toSectionWithIdentifier:AdvancedSettingsSectionIdentifier];
  [self updateEncryptionItem:NO];

  // GoogleActivityControlsItemType.
  TableViewImageItem* googleActivityControlsItem =
      [[TableViewImageItem alloc] initWithType:GoogleActivityControlsItemType];
  googleActivityControlsItem.title =
      GetNSString(IDS_IOS_MANAGE_SYNC_GOOGLE_ACTIVITY_CONTROLS_TITLE);
  googleActivityControlsItem.detailText =
      GetNSString(IDS_IOS_MANAGE_SYNC_GOOGLE_ACTIVITY_CONTROLS_DESCRIPTION);
  googleActivityControlsItem.accessibilityTraits |= UIAccessibilityTraitButton;
  [model addItem:googleActivityControlsItem
      toSectionWithIdentifier:AdvancedSettingsSectionIdentifier];

  // AdvancedSettingsSectionIdentifier.
  TableViewImageItem* dataFromChromeSyncItem =
      [[TableViewImageItem alloc] initWithType:DataFromChromeSync];
  dataFromChromeSyncItem.title =
      GetNSString(IDS_IOS_MANAGE_SYNC_DATA_FROM_CHROME_SYNC_TITLE);
  dataFromChromeSyncItem.detailText =
      GetNSString(IDS_IOS_MANAGE_SYNC_DATA_FROM_CHROME_SYNC_DESCRIPTION);
  dataFromChromeSyncItem.accessibilityIdentifier =
      kDataFromChromeSyncAccessibilityIdentifier;
  dataFromChromeSyncItem.accessibilityTraits |= UIAccessibilityTraitButton;
  [model addItem:dataFromChromeSyncItem
      toSectionWithIdentifier:AdvancedSettingsSectionIdentifier];
}

// Updates encryption item, and notifies the consumer if |notifyConsumer| is set
// to YES.
- (void)updateEncryptionItem:(BOOL)notifyConsumer {
  BOOL needsUpdate =
      self.shouldEncryptionItemBeEnabled &&
      (self.encryptionItem.enabled != self.shouldEncryptionItemBeEnabled);
  if (self.shouldEncryptionItemBeEnabled &&
      self.syncSetupService->GetSyncServiceState() ==
          SyncSetupService::kSyncServiceNeedsPassphrase) {
    needsUpdate = needsUpdate || self.encryptionItem.image == nil;
    self.encryptionItem.image =
        [UIImage imageNamed:kGoogleServicesSyncErrorImage];
    self.encryptionItem.detailText = GetNSString(
        IDS_IOS_GOOGLE_SERVICES_SETTINGS_ENTER_PASSPHRASE_TO_START_SYNC);
  } else if (self.shouldEncryptionItemBeEnabled &&
             self.syncSetupService->GetSyncServiceState() ==
                 SyncSetupService::kSyncServiceNeedsTrustedVaultKey) {
    needsUpdate = needsUpdate || self.encryptionItem.image == nil;
    self.encryptionItem.image =
        [UIImage imageNamed:kGoogleServicesSyncErrorImage];
    self.encryptionItem.detailText =
        GetNSString(self.syncSetupService->IsEncryptEverythingEnabled()
                        ? IDS_IOS_SYNC_ERROR_DESCRIPTION
                        : IDS_IOS_SYNC_PASSWORDS_ERROR_DESCRIPTION);
  } else {
    needsUpdate = needsUpdate || self.encryptionItem.image != nil;
    self.encryptionItem.image = nil;
    self.encryptionItem.detailText = nil;
  }
  self.encryptionItem.enabled = self.shouldEncryptionItemBeEnabled;
  if (self.shouldEncryptionItemBeEnabled) {
    self.encryptionItem.textColor = nil;
  } else {
    self.encryptionItem.textColor = UIColor.cr_secondaryLabelColor;
  }
  if (needsUpdate && notifyConsumer) {
    [self.consumer reloadItem:self.self.encryptionItem];
  }
}

#pragma mark - Private

// Creates a SyncSwitchItem instance.
- (SyncSwitchItem*)switchItemWithDataType:
    (SyncSetupService::SyncableDatatype)dataType {
  NSInteger itemType = 0;
  int textStringID = 0;
  switch (dataType) {
    case SyncSetupService::kSyncBookmarks:
      itemType = BookmarksDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_BOOKMARKS;
      break;
    case SyncSetupService::kSyncOmniboxHistory:
      itemType = HistoryDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_TYPED_URLS;
      break;
    case SyncSetupService::kSyncPasswords:
      itemType = PasswordsDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_PASSWORDS;
      break;
    case SyncSetupService::kSyncOpenTabs:
      itemType = OpenTabsDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_TABS;
      break;
    case SyncSetupService::kSyncAutofill:
      itemType = AutofillDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_AUTOFILL;
      break;
    case SyncSetupService::kSyncPreferences:
      itemType = SettingsDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_PREFERENCES;
      break;
    case SyncSetupService::kSyncReadingList:
      itemType = ReadingListDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_READING_LIST;
      break;
    case SyncSetupService::kNumberOfSyncableDatatypes:
      NOTREACHED();
      break;
  }
  DCHECK_NE(itemType, 0);
  DCHECK_NE(textStringID, 0);
  SyncSwitchItem* switchItem = [[SyncSwitchItem alloc] initWithType:itemType];
  switchItem.text = GetNSString(textStringID);
  switchItem.dataType = dataType;
  return switchItem;
}

#pragma mark - Properties

- (BOOL)syncSettingsNotConfirmed {
  SyncSetupService::SyncServiceState state =
      self.syncSetupService->GetSyncServiceState();
  return state == SyncSetupService::kSyncSettingsNotConfirmed;
}

- (BOOL)disabledBecauseOfSyncError {
  SyncSetupService::SyncServiceState state =
      self.syncSetupService->GetSyncServiceState();
  return state != SyncSetupService::kNoSyncServiceError &&
         state != SyncSetupService::kSyncServiceNeedsPassphrase &&
         state != SyncSetupService::kSyncServiceNeedsTrustedVaultKey;
}

- (BOOL)shouldSyncDataItemEnabled {
  return (!self.syncSetupService->IsSyncingAllDataTypes() &&
          self.syncSetupService->IsSyncEnabled() &&
          (!self.disabledBecauseOfSyncError || self.syncSettingsNotConfirmed));
}

- (BOOL)shouldEncryptionItemBeEnabled {
  return self.syncService->IsEngineInitialized() &&
         self.syncSetupService->IsSyncEnabled() &&
         !self.disabledBecauseOfSyncError;
}

#pragma mark - ManageSyncSettingsTableViewControllerModelDelegate

- (void)manageSyncSettingsTableViewControllerLoadModel:
    (id<ManageSyncSettingsConsumer>)controller {
  DCHECK_EQ(self.consumer, controller);
  [self loadSyncErrorsSection];
  [self loadSyncDataTypeSection];
  [self loadAdvancedSettingsSection];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self updateAutocompleteWalletItemNotifyConsumer:YES];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (_ignoreSyncStateChanges) {
    // The UI should not updated so the switch animations can run smoothly.
    return;
  }
  [self updateSyncErrorsSection:YES];
  [self updateSyncEverythingItemNotifyConsumer:YES];
  [self updateSyncItemsNotifyConsumer:YES];
  [self updateEncryptionItem:YES];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountSet:(const CoreAccountInfo&)primaryAccountInfo {
  [self updateSyncErrorsSection:YES];
}

- (void)onPrimaryAccountCleared:
    (const CoreAccountInfo&)previousPrimaryAccountInfo {
  [self updateSyncErrorsSection:YES];
}

#pragma mark - ManageSyncSettingsServiceDelegate

- (void)toggleSwitchItem:(TableViewItem*)item withValue:(BOOL)value {
  {
    // The notifications should be ignored to get smooth switch animations.
    // Notifications are sent by SyncObserverModelBridge while changing
    // settings.
    base::AutoReset<BOOL> autoReset(&_ignoreSyncStateChanges, YES);
    SyncSwitchItem* syncSwitchItem = base::mac::ObjCCast<SyncSwitchItem>(item);
    syncSwitchItem.on = value;
    SyncSettingsItemType itemType =
        static_cast<SyncSettingsItemType>(item.type);
    switch (itemType) {
      case SyncEverythingItemType:
        self.syncSetupService->SetSyncingAllDataTypes(value);
        if (value) {
          // When sync everything is turned on, the autocomplete wallet
          // should be turned on. This code can be removed once
          // crbug.com/937234 is fixed.
          self.autocompleteWalletPreference.value = true;
        }
        break;
      case AutofillDataTypeItemType:
      case BookmarksDataTypeItemType:
      case HistoryDataTypeItemType:
      case OpenTabsDataTypeItemType:
      case PasswordsDataTypeItemType:
      case ReadingListDataTypeItemType:
      case SettingsDataTypeItemType: {
        DCHECK(syncSwitchItem);
        SyncSetupService::SyncableDatatype dataType =
            static_cast<SyncSetupService::SyncableDatatype>(
                syncSwitchItem.dataType);
        syncer::ModelType modelType =
            self.syncSetupService->GetModelType(dataType);
        self.syncSetupService->SetDataTypeEnabled(modelType, value);
        if (dataType == SyncSetupService::kSyncAutofill) {
          // When the auto fill data type is updated, the autocomplete wallet
          // should be updated too. Autocomplete wallet should not be enabled
          // when auto fill data type disabled. This behaviour not be
          // implemented in the UI code. This code can be removed once
          // crbug.com/937234 is fixed.
          self.autocompleteWalletPreference.value = value;
        }
        break;
      }
      case AutocompleteWalletItemType:
        self.autocompleteWalletPreference.value = value;
        break;
      case EncryptionItemType:
      case GoogleActivityControlsItemType:
      case DataFromChromeSync:
      case RestartAuthenticationFlowErrorItemType:
      case ReauthDialogAsSyncIsInAuthErrorItemType:
      case ShowPassphraseDialogErrorItemType:
      case SyncNeedsTrustedVaultKeyErrorItemType:
      case SyncDisabledByAdministratorErrorItemType:
        NOTREACHED();
        break;
    }
  }
  [self updateSyncEverythingItemNotifyConsumer:YES];
  [self updateSyncItemsNotifyConsumer:YES];
}

- (void)didSelectItem:(TableViewItem*)item {
  SyncSettingsItemType itemType = static_cast<SyncSettingsItemType>(item.type);
  switch (itemType) {
    case EncryptionItemType:
      if (self.syncSetupService->GetSyncServiceState() ==
          SyncSetupService::kSyncServiceNeedsTrustedVaultKey) {
        [self.syncErrorHandler openTrustedVaultReauth];
        break;
      }
      [self.syncErrorHandler openPassphraseDialog];
      break;
    case GoogleActivityControlsItemType:
      [self.commandHandler openWebAppActivityDialog];
      break;
    case DataFromChromeSync:
      [self.commandHandler openDataFromChromeSyncWebPage];
      break;
    case RestartAuthenticationFlowErrorItemType:
      [self.syncErrorHandler restartAuthenticationFlow];
      break;
    case ReauthDialogAsSyncIsInAuthErrorItemType:
      [self.syncErrorHandler openReauthDialogAsSyncIsInAuthError];
      break;
    case ShowPassphraseDialogErrorItemType:
      [self.syncErrorHandler openPassphraseDialog];
      break;
    case SyncNeedsTrustedVaultKeyErrorItemType:
      [self.syncErrorHandler openTrustedVaultReauth];
      break;
    case SyncEverythingItemType:
    case AutofillDataTypeItemType:
    case BookmarksDataTypeItemType:
    case HistoryDataTypeItemType:
    case OpenTabsDataTypeItemType:
    case PasswordsDataTypeItemType:
    case ReadingListDataTypeItemType:
    case SettingsDataTypeItemType:
    case AutocompleteWalletItemType:
    case SyncDisabledByAdministratorErrorItemType:
      // Nothing to do.
      break;
  }
}

// Creates an item to display the sync error. |itemType| should only be one of
// those types:
//   + RestartAuthenticationFlowErrorItemType
//   + ReauthDialogAsSyncIsInAuthErrorItemType
//   + ShowPassphraseDialogErrorItemType
//   + SyncNeedsTrustedVaultKeyErrorItemType
- (TableViewItem*)createSyncErrorItemWithItemType:(NSInteger)itemType {
  DCHECK(itemType == RestartAuthenticationFlowErrorItemType ||
         itemType == ReauthDialogAsSyncIsInAuthErrorItemType ||
         itemType == ShowPassphraseDialogErrorItemType ||
         itemType == SyncNeedsTrustedVaultKeyErrorItemType);
  SettingsImageDetailTextItem* syncErrorItem =
      [[SettingsImageDetailTextItem alloc] initWithType:itemType];
  syncErrorItem.text = GetNSString(IDS_IOS_SYNC_ERROR_TITLE);
  syncErrorItem.detailText =
      GetSyncErrorDescriptionForSyncSetupService(self.syncSetupService);
  if (itemType == ShowPassphraseDialogErrorItemType) {
    // Special case only for the sync passphrase error message. The regular
    // error message should be still be displayed in the first settings screen.
    syncErrorItem.detailText = GetNSString(
        IDS_IOS_GOOGLE_SERVICES_SETTINGS_ENTER_PASSPHRASE_TO_START_SYNC);
  } else if (itemType == SyncNeedsTrustedVaultKeyErrorItemType) {
    // Special case only for the sync encryption key error message. The regular
    // error message should be still be displayed in the first settings screen.
    syncErrorItem.detailText =
        GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_ENCRYPTION_FIX_NOW);

    // Also override the title to be more accurate, if only passwords are being
    // encrypted.
    if (!self.syncSetupService->IsEncryptEverythingEnabled()) {
      syncErrorItem.text = GetNSString(IDS_IOS_SYNC_PASSWORDS_ERROR_TITLE);
    }
  }
  syncErrorItem.image = [UIImage imageNamed:kGoogleServicesSyncErrorImage];
  return syncErrorItem;
}

// Loads the sync errors section.
- (void)loadSyncErrorsSection {
  [self.consumer.tableViewModel
      addSectionWithIdentifier:SyncErrorsSectionIdentifier];
  [self updateSyncErrorsSection:NO];
}

// Updates the sync errors section. If |notifyConsumer| is YES, the consumer is
// notified about model changes.
- (void)updateSyncErrorsSection:(BOOL)notifyConsumer {
  if (!base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency)) {
    return;
  }
  BOOL needsSyncErrorItemsUpdate = [self updateSyncErrorItems];
  if (notifyConsumer && needsSyncErrorItemsUpdate) {
    NSUInteger sectionIndex = [self.consumer.tableViewModel
        sectionForSectionIdentifier:SyncErrorsSectionIdentifier];
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
    [self.consumer reloadSections:indexSet];
  }
}

// Adds, removes and updates the sync error item in the model as needed. Returns
// YES if the consumer should be notified.
- (BOOL)updateSyncErrorItems {
  TableViewModel* model = self.consumer.tableViewModel;
  BOOL hasError = NO;
  SyncSettingsItemType type;

  if (self.isSyncDisabledByAdministrator) {
    type = SyncDisabledByAdministratorErrorItemType;
    hasError = YES;
  } else if (self.isAuthenticated && self.syncSetupService->IsSyncEnabled()) {
    switch (self.syncSetupService->GetSyncServiceState()) {
      case SyncSetupService::kSyncServiceUnrecoverableError:
        type = RestartAuthenticationFlowErrorItemType;
        hasError = YES;
        break;
      case SyncSetupService::kSyncServiceSignInNeedsUpdate:
        type = ReauthDialogAsSyncIsInAuthErrorItemType;
        hasError = YES;
        break;
      case SyncSetupService::kSyncServiceNeedsPassphrase:
        type = ShowPassphraseDialogErrorItemType;
        hasError = YES;
        break;
      case SyncSetupService::kSyncServiceNeedsTrustedVaultKey:
        type = SyncNeedsTrustedVaultKeyErrorItemType;
        hasError = YES;
        break;
      case SyncSetupService::kSyncSettingsNotConfirmed:
      case SyncSetupService::kNoSyncServiceError:
      case SyncSetupService::kSyncServiceCouldNotConnect:
      case SyncSetupService::kSyncServiceServiceUnavailable:
        break;
    }
  }

  if ((!hasError && !self.syncErrorItem) ||
      (hasError && self.syncErrorItem && type == self.syncErrorItem.type)) {
    // Nothing to update.
    return NO;
  }

  if (self.syncErrorItem) {
    // Remove the previous sync error item, since it is either the wrong error
    // (if hasError is YES), or there is no error anymore.
    [model removeItemWithType:self.syncErrorItem.type
        fromSectionWithIdentifier:SyncErrorsSectionIdentifier];
    self.syncErrorItem = nil;
    if (!hasError)
      return YES;
  }
  // Add the sync error item and its section.
  if (type == SyncDisabledByAdministratorErrorItemType) {
    self.syncErrorItem = [self createSyncDisabledByAdministratorErrorItem];
  } else {
    self.syncErrorItem = [self createSyncErrorItemWithItemType:type];
  }
  [model insertItem:self.syncErrorItem
      inSectionWithIdentifier:SyncErrorsSectionIdentifier
                      atIndex:0];
  return YES;
}

// Returns an item to show to the user the sync cannot be turned on for an
// enterprise policy reason.
- (TableViewItem*)createSyncDisabledByAdministratorErrorItem {
  TableViewImageItem* item = [[TableViewImageItem alloc]
      initWithType:SyncDisabledByAdministratorErrorItemType];
  item.image = [UIImage imageNamed:kGoogleServicesEnterpriseImage];
  item.title = GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_DISABLBED_BY_ADMINISTRATOR_TITLE);
  item.enabled = NO;
  item.textColor = UIColor.cr_secondaryLabelColor;
  return item;
}

#pragma mark - Properties

- (BOOL)isSyncDisabledByAdministrator {
  return self.syncService->GetDisableReasons().Has(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

- (BOOL)isAuthenticated {
  return self.authService->IsAuthenticated();
}

@end
