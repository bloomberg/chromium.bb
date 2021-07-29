// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_manager.h"

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observation.h"
#include "base/strings/sys_string_conversions.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/google/core/common/google_util.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_counter_wrapper.h"
#include "ios/chrome/browser/browsing_data/browsing_data_features.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover_factory.h"
#import "ios/chrome/browser/browsing_data/browsing_data_remover_observer_bridge.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#include "ios/chrome/browser/history/web_history_service_factory.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/cells/search_engine_item.h"
#import "ios/chrome/browser/ui/settings/cells/table_view_clear_browsing_data_item.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_counter_wrapper_producer.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_consumer.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/time_range_selector_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_link_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/common/channel_info.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/images/branded_image_provider.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Maximum number of times to show a notice about other forms of browsing
// history.
const int kMaxTimesHistoryNoticeShown = 1;
// TableViewClearBrowsingDataItem's selectedBackgroundViewBackgroundColorAlpha.
const CGFloat kSelectedBackgroundColorAlpha = 0.05;

// List of flags that have corresponding counters.
const std::vector<BrowsingDataRemoveMask> _browsingDataRemoveFlags = {
    // BrowsingDataRemoveMask::REMOVE_COOKIES not included; we don't have cookie
    // counters yet.
    BrowsingDataRemoveMask::REMOVE_HISTORY,
    BrowsingDataRemoveMask::REMOVE_CACHE,
    BrowsingDataRemoveMask::REMOVE_PASSWORDS,
    BrowsingDataRemoveMask::REMOVE_FORM_DATA,
};

}  // namespace

static NSDictionary* _imageNamesByItemTypes = @{
  [NSNumber numberWithInteger:ItemTypeDataTypeBrowsingHistory] :
      @"clear_browsing_data_history",
  [NSNumber numberWithInteger:ItemTypeDataTypeCookiesSiteData] :
      @"clear_browsing_data_cookies",
  [NSNumber numberWithInteger:ItemTypeDataTypeCache] :
      @"clear_browsing_data_cached_images",
  [NSNumber numberWithInteger:ItemTypeDataTypeSavedPasswords] : @"password_key",
  [NSNumber numberWithInteger:ItemTypeDataTypeAutofill] :
      @"clear_browsing_data_autofill",
};

@interface ClearBrowsingDataManager () <BrowsingDataRemoverObserving,
                                        PrefObserverDelegate> {
  // Access to the kDeleteTimePeriod preference.
  IntegerPrefMember _timeRangePref;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // Observer for browsing data removal events and associated
  // base::ScopedObservation used to track registration with
  // BrowsingDataRemover.
  std::unique_ptr<BrowsingDataRemoverObserver> _observer;
  std::unique_ptr<
      base::ScopedObservation<BrowsingDataRemover, BrowsingDataRemoverObserver>>
      _scoped_observation;

  // Corresponds browsing data counters to their masks/flags. Items are inserted
  // as clear data items are constructed.
  std::map<BrowsingDataRemoveMask, std::unique_ptr<BrowsingDataCounterWrapper>>
      _countersByMasks;
}

@property(nonatomic, assign) ChromeBrowserState* browserState;
// Whether to show alert about other forms of browsing history.
@property(nonatomic, assign)
    BOOL shouldShowNoticeAboutOtherFormsOfBrowsingHistory;
// Whether to show popup other forms of browsing history.
@property(nonatomic, assign)
    BOOL shouldPopupDialogAboutOtherFormsOfBrowsingHistory;

@property(nonatomic, strong) TableViewDetailIconItem* tableViewTimeRangeItem;

@property(nonatomic, strong)
    TableViewClearBrowsingDataItem* browsingHistoryItem;
@property(nonatomic, strong)
    TableViewClearBrowsingDataItem* cookiesSiteDataItem;
@property(nonatomic, strong) TableViewClearBrowsingDataItem* cacheItem;
@property(nonatomic, strong) TableViewClearBrowsingDataItem* savedPasswordsItem;
@property(nonatomic, strong) TableViewClearBrowsingDataItem* autofillItem;

@property(nonatomic, strong)
    BrowsingDataCounterWrapperProducer* counterWrapperProducer;

@end

@implementation ClearBrowsingDataManager
@synthesize browserState = _browserState;
@synthesize consumer = _consumer;
@synthesize shouldShowNoticeAboutOtherFormsOfBrowsingHistory =
    _shouldShowNoticeAboutOtherFormsOfBrowsingHistory;
@synthesize shouldPopupDialogAboutOtherFormsOfBrowsingHistory =
    _shouldPopupDialogAboutOtherFormsOfBrowsingHistory;

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  return [self initWithBrowserState:browserState
                     browsingDataRemover:BrowsingDataRemoverFactory::
                                             GetForBrowserState(browserState)
      browsingDataCounterWrapperProducer:[[BrowsingDataCounterWrapperProducer
                                             alloc] init]];
}

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                   browsingDataRemover:(BrowsingDataRemover*)remover
    browsingDataCounterWrapperProducer:
        (BrowsingDataCounterWrapperProducer*)producer {
  self = [super init];
  if (self) {
    _browserState = browserState;
    _counterWrapperProducer = producer;

    _timeRangePref.Init(browsing_data::prefs::kDeleteTimePeriod,
                        _browserState->GetPrefs());

    _observer = std::make_unique<BrowsingDataRemoverObserverBridge>(self);
    _scoped_observation = std::make_unique<base::ScopedObservation<
        BrowsingDataRemover, BrowsingDataRemoverObserver>>(_observer.get());
    _scoped_observation->Observe(remover);

    _prefChangeRegistrar.Init(_browserState->GetPrefs());
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    _prefObserverBridge->ObserveChangesForPreference(
        browsing_data::prefs::kDeleteTimePeriod, &_prefChangeRegistrar);

    _prefObserverBridge->ObserveChangesForPreference(
        browsing_data::prefs::kDeleteBrowsingHistory, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        browsing_data::prefs::kDeleteCookies, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        browsing_data::prefs::kDeleteCache, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        browsing_data::prefs::kDeletePasswords, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        browsing_data::prefs::kDeleteFormData, &_prefChangeRegistrar);
  }
  return self;
}

#pragma mark - Public Methods

- (void)loadModel:(ListModel*)model {
  self.tableViewTimeRangeItem = [self timeRangeItem];
  self.tableViewTimeRangeItem.useCustomSeparator =
      base::FeatureList::IsEnabled(kSettingsRefresh) ? NO : YES;

  [model addSectionWithIdentifier:SectionIdentifierTimeRange];
  [model addItem:self.tableViewTimeRangeItem
      toSectionWithIdentifier:SectionIdentifierTimeRange];
  [self addClearBrowsingDataItemsToModel:model];
  [self addSyncProfileItemsToModel:model];
}

// Add items for types of browsing data to clear.
- (void)addClearBrowsingDataItemsToModel:(ListModel*)model {
  // Data types section.
  [model addSectionWithIdentifier:SectionIdentifierDataTypes];
  self.browsingHistoryItem =
      [self clearDataItemWithType:ItemTypeDataTypeBrowsingHistory
                          titleID:IDS_IOS_CLEAR_BROWSING_HISTORY
                             mask:BrowsingDataRemoveMask::REMOVE_HISTORY
                         prefName:browsing_data::prefs::kDeleteBrowsingHistory];
  [model addItem:self.browsingHistoryItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];

  // This data type doesn't currently have an associated counter, but displays
  // an explanatory text instead.
  self.cookiesSiteDataItem =
      [self clearDataItemWithType:ItemTypeDataTypeCookiesSiteData
                          titleID:IDS_IOS_CLEAR_COOKIES
                             mask:BrowsingDataRemoveMask::REMOVE_SITE_DATA
                         prefName:browsing_data::prefs::kDeleteCookies];
  [model addItem:self.cookiesSiteDataItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];

  self.cacheItem =
      [self clearDataItemWithType:ItemTypeDataTypeCache
                          titleID:IDS_IOS_CLEAR_CACHE
                             mask:BrowsingDataRemoveMask::REMOVE_CACHE
                         prefName:browsing_data::prefs::kDeleteCache];
  [model addItem:self.cacheItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];

  self.savedPasswordsItem =
      [self clearDataItemWithType:ItemTypeDataTypeSavedPasswords
                          titleID:IDS_IOS_CLEAR_SAVED_PASSWORDS
                             mask:BrowsingDataRemoveMask::REMOVE_PASSWORDS
                         prefName:browsing_data::prefs::kDeletePasswords];
  [model addItem:self.savedPasswordsItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];

  self.autofillItem =
      [self clearDataItemWithType:ItemTypeDataTypeAutofill
                          titleID:IDS_IOS_CLEAR_AUTOFILL
                             mask:BrowsingDataRemoveMask::REMOVE_FORM_DATA
                         prefName:browsing_data::prefs::kDeleteFormData];
  [model addItem:self.autofillItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];
}

- (NSString*)counterTextFromResult:
    (const browsing_data::BrowsingDataCounter::Result&)result {
  if (!result.Finished()) {
    // The counter is still counting.
    return l10n_util::GetNSString(IDS_CLEAR_BROWSING_DATA_CALCULATING);
  }

  base::StringPiece prefName = result.source()->GetPrefName();
  if (prefName != browsing_data::prefs::kDeleteCache) {
    return base::SysUTF16ToNSString(
        browsing_data::GetCounterTextFromResult(&result));
  }

  browsing_data::BrowsingDataCounter::ResultInt cacheSizeBytes =
      static_cast<const browsing_data::BrowsingDataCounter::FinishedResult*>(
          &result)
          ->Value();

  // Three cases: Nonzero result for the entire cache, nonzero result for
  // a subset of cache (i.e. a finite time interval), and almost zero (less
  // than 1 MB). There is no exact information that the cache is empty so that
  // falls into the almost zero case, which is displayed as less than 1 MB.
  // Because of this, the lowest unit that can be used is MB.
  static const int kBytesInAMegabyte = 1 << 20;
  if (cacheSizeBytes >= kBytesInAMegabyte) {
    NSByteCountFormatter* formatter = [[NSByteCountFormatter alloc] init];
    formatter.allowedUnits = NSByteCountFormatterUseAll &
                             (~NSByteCountFormatterUseBytes) &
                             (~NSByteCountFormatterUseKB);
    formatter.countStyle = NSByteCountFormatterCountStyleMemory;
    NSString* formattedSize = [formatter stringFromByteCount:cacheSizeBytes];
    return _timeRangePref.GetValue() ==
                   static_cast<int>(browsing_data::TimePeriod::ALL_TIME)
               ? formattedSize
               : l10n_util::GetNSStringF(
                     IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE,
                     base::SysNSStringToUTF16(formattedSize));
  }

  return l10n_util::GetNSString(IDS_DEL_CACHE_COUNTER_ALMOST_EMPTY);
}

- (ActionSheetCoordinator*)
    actionSheetCoordinatorWithDataTypesToRemove:
        (BrowsingDataRemoveMask)dataTypeMaskToRemove
                             baseViewController:
                                 (UIViewController*)baseViewController
                                        browser:(Browser*)browser
                            sourceBarButtonItem:
                                (UIBarButtonItem*)sourceBarButtonItem {
  if (dataTypeMaskToRemove == BrowsingDataRemoveMask::REMOVE_NOTHING) {
    // Nothing to clear (no data types selected).
    return nil;
  }
  __weak ClearBrowsingDataManager* weakSelf = self;

  ActionSheetCoordinator* actionCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:baseViewController
                         browser:browser
                           title:l10n_util::GetNSString(
                                     IDS_IOS_CONFIRM_CLEAR_BUTTON_TITLE)
                         message:nil
                   barButtonItem:sourceBarButtonItem];
  actionCoordinator.popoverArrowDirection =
      UIPopoverArrowDirectionDown | UIPopoverArrowDirectionUp;
  [actionCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_CLEAR_BUTTON)
                action:^{
                  [weakSelf clearDataForDataTypes:dataTypeMaskToRemove];
                  [weakSelf signOutIfNotSyncing];
                }
                 style:UIAlertActionStyleDestructive];
  return actionCoordinator;
}

// Add footers about user's account data.
- (void)addSyncProfileItemsToModel:(ListModel*)model {
  // Google Account footer.
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(self.browserState);

  if (base::FeatureList::IsEnabled(kSearchHistoryLinkIOS)) {
    const BOOL loggedIn =
        identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin) ||
        identityManager->HasPrimaryAccount(signin::ConsentLevel::kSync);
    const TemplateURLService* templateURLService =
        ios::TemplateURLServiceFactory::GetForBrowserState(_browserState);
    const TemplateURL* defaultSearchEngine =
        templateURLService->GetDefaultSearchProvider();
    const BOOL isDefaultSearchEngineGoogle =
        defaultSearchEngine->GetEngineType(
            templateURLService->search_terms_data()) ==
        SearchEngineType::SEARCH_ENGINE_GOOGLE;
    // If the user has their DSE set to Google and is logged out
    // there is no additional data to delete, so omit this section.
    if (isDefaultSearchEngineGoogle && !loggedIn) {
      // Nothing to do.
    } else {
      // Show additional instructions for deleting data.
      [model addSectionWithIdentifier:SectionIdentifierGoogleAccount];
      [model setFooter:[self footerGoogleAccountDSEBasedItem:loggedIn
                                         defaultSearchEngine:defaultSearchEngine
                                 isDefaultSearchEngineGoogle:
                                     isDefaultSearchEngineGoogle]
          forSectionWithIdentifier:SectionIdentifierGoogleAccount];
    }
  } else {
    if (identityManager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
      [model addSectionWithIdentifier:SectionIdentifierGoogleAccount];
      [model setFooter:[self footerForGoogleAccountSectionItem]
          forSectionWithIdentifier:SectionIdentifierGoogleAccount];
    }
  }

  [model addSectionWithIdentifier:SectionIdentifierSavedSiteData];
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(self.browserState);
  if (syncService && syncService->IsSyncFeatureActive()) {
    [model setFooter:[self footerClearSyncAndSavedSiteDataItem]
        forSectionWithIdentifier:SectionIdentifierSavedSiteData];
  } else {
    [model setFooter:[self footerSavedSiteDataItem]
        forSectionWithIdentifier:SectionIdentifierSavedSiteData];
  }

  // If not signed in, no need to continue with profile syncing.
  if (!identityManager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return;
  }

  history::WebHistoryService* historyService =
      ios::WebHistoryServiceFactory::GetForBrowserState(_browserState);

  __weak ClearBrowsingDataManager* weakSelf = self;

  // The text notice at the bottom of the CBD selector is not needed when
  // the Search History Link feature is enabled. However, the popup notice
  // will be left for now.
  if (!base::FeatureList::IsEnabled(kSearchHistoryLinkIOS)) {
    browsing_data::ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
        syncService, historyService, base::BindOnce(^(bool shouldShowNotice) {
          ClearBrowsingDataManager* strongSelf = weakSelf;
          [strongSelf
              setShouldShowNoticeAboutOtherFormsOfBrowsingHistory:
                  shouldShowNotice
                                                         forModel:model];
        }));
  }

  browsing_data::ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
      syncService, historyService, GetChannel(),
      base::BindOnce(^(bool shouldShowPopup) {
        ClearBrowsingDataManager* strongSelf = weakSelf;
        [strongSelf setShouldPopupDialogAboutOtherFormsOfBrowsingHistory:
                        shouldShowPopup];
      }));
}

- (void)restartCounters:(BrowsingDataRemoveMask)mask {
  for (auto flag : _browsingDataRemoveFlags) {
    if (IsRemoveDataMaskSet(mask, flag)) {
      const auto it = _countersByMasks.find(flag);
      if (it != _countersByMasks.end()) {
        it->second->RestartCounter();
      }
    }
  }
}

#pragma mark Items

// Creates item of type |itemType| with |mask| of data to be cleared if
// selected, |prefName|, and |titleId| of item.
- (TableViewClearBrowsingDataItem*)
    clearDataItemWithType:(ClearBrowsingDataItemType)itemType
                  titleID:(int)titleMessageID
                     mask:(BrowsingDataRemoveMask)mask
                 prefName:(const char*)prefName {
  PrefService* prefs = self.browserState->GetPrefs();
  TableViewClearBrowsingDataItem* clearDataItem =
      [[TableViewClearBrowsingDataItem alloc] initWithType:itemType];
  clearDataItem.text = l10n_util::GetNSString(titleMessageID);
  clearDataItem.checked = prefs->GetBoolean(prefName);
  clearDataItem.accessibilityIdentifier =
      [self accessibilityIdentifierFromItemType:itemType];
  clearDataItem.dataTypeMask = mask;
  clearDataItem.prefName = prefName;
  clearDataItem.useCustomSeparator =
      base::FeatureList::IsEnabled(kSettingsRefresh) ? NO : YES;
  clearDataItem.checkedBackgroundColor = [[UIColor colorNamed:kBlueColor]
      colorWithAlphaComponent:kSelectedBackgroundColorAlpha];
  clearDataItem.imageName = [_imageNamesByItemTypes
      objectForKey:[NSNumber numberWithInteger:itemType]];
  if (itemType == ItemTypeDataTypeCookiesSiteData) {
    // Because there is no counter for cookies, an explanatory text is
    // displayed.
    clearDataItem.detailText = l10n_util::GetNSString(IDS_DEL_COOKIES_COUNTER);
  } else {
    // Having a placeholder |detailText| helps reduce the observable
    // row-height changes induced by the counter callbacks.
    clearDataItem.detailText = @"\u00A0";
    __weak ClearBrowsingDataManager* weakSelf = self;
    __weak TableViewClearBrowsingDataItem* weakTableClearDataItem =
        clearDataItem;
    BrowsingDataCounterWrapper::UpdateUICallback callback = base::BindRepeating(
        ^(const browsing_data::BrowsingDataCounter::Result& result) {
          weakTableClearDataItem.detailText =
              [weakSelf counterTextFromResult:result];
          [weakSelf.consumer updateCellsForItem:weakTableClearDataItem
                                         reload:YES];
        });
    std::unique_ptr<BrowsingDataCounterWrapper> counter =
        [self.counterWrapperProducer
            createCounterWrapperWithPrefName:prefName
                                browserState:self.browserState
                                 prefService:prefs
                            updateUiCallback:callback];
    _countersByMasks.emplace(mask, std::move(counter));
  }
  return clearDataItem;
}

- (TableViewLinkHeaderFooterItem*)footerForGoogleAccountSectionItem {
  return _shouldShowNoticeAboutOtherFormsOfBrowsingHistory
             ? [self footerGoogleAccountAndMyActivityItem]
             : [self footerGoogleAccountItem];
}

- (TableViewLinkHeaderFooterItem*)
    footerGoogleAccountDSEBasedItem:(const BOOL)loggedIn
                defaultSearchEngine:(const TemplateURL*)defaultSearchEngine
        isDefaultSearchEngineGoogle:(const BOOL)isDefaultSearchEngineGoogle {
  TableViewLinkHeaderFooterItem* footerItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypeFooterGoogleAccountDSEBased];
  if (loggedIn) {
    if (isDefaultSearchEngineGoogle) {
      footerItem.text =
          l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_GOOGLE_DSE);
      footerItem.urls = std::vector<GURL>{
          google_util::AppendGoogleLocaleParam(
              GURL(kClearBrowsingDataDSESearchUrlInFooterURL),
              GetApplicationContext()->GetApplicationLocale()),
          google_util::AppendGoogleLocaleParam(
              GURL(kClearBrowsingDataDSEMyActivityUrlInFooterURL),
              GetApplicationContext()->GetApplicationLocale())};
    } else if (defaultSearchEngine->prepopulate_id() > 0) {
      footerItem.text = l10n_util::GetNSStringF(
          IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_KNOWN_DSE_SIGNED_IN,
          defaultSearchEngine->short_name());
      footerItem.urls = std::vector<GURL>{google_util::AppendGoogleLocaleParam(
          GURL(kClearBrowsingDataDSEMyActivityUrlInFooterURL),
          GetApplicationContext()->GetApplicationLocale())};
    } else {
      footerItem.text = l10n_util::GetNSString(
          IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_UNKOWN_DSE_SIGNED_IN);
      footerItem.urls = std::vector<GURL>{google_util::AppendGoogleLocaleParam(
          GURL(kClearBrowsingDataDSEMyActivityUrlInFooterURL),
          GetApplicationContext()->GetApplicationLocale())};
    }
  } else {
    // Logged Out with Google DSE is handled in calling function since there
    // should be no account footer section in this case.
    if (defaultSearchEngine->prepopulate_id() > 0) {
      footerItem.text = l10n_util::GetNSStringF(
          IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_KNOWN_DSE_SIGNED_OUT,
          defaultSearchEngine->short_name());
    } else {
      footerItem.text = l10n_util::GetNSString(
          IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_UNKOWN_DSE_SIGNED_OUT);
    }
  }
  return footerItem;
}

- (TableViewLinkHeaderFooterItem*)footerGoogleAccountItem {
  TableViewLinkHeaderFooterItem* footerItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypeFooterGoogleAccount];
  footerItem.text =
      l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_ACCOUNT);
  return footerItem;
}

- (TableViewLinkHeaderFooterItem*)footerGoogleAccountAndMyActivityItem {
  UIImage* image = ios::GetChromeBrowserProvider()
                       .GetBrandedImageProvider()
                       ->GetClearBrowsingDataAccountActivityImage();
  return [self
      footerItemWithType:ItemTypeFooterGoogleAccountAndMyActivity
                 titleID:IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_ACCOUNT_AND_HISTORY
                     URL:kClearBrowsingDataMyActivityUrlInFooterURL
                   image:image];
}

- (TableViewLinkHeaderFooterItem*)footerSavedSiteDataItem {
  UIImage* image = ios::GetChromeBrowserProvider()
                       .GetBrandedImageProvider()
                       ->GetClearBrowsingDataSiteDataImage();
  return [self
      footerItemWithType:ItemTypeFooterSavedSiteData
                 titleID:IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_SAVED_SITE_DATA
                     URL:kClearBrowsingDataLearnMoreURL
                   image:image];
}

- (TableViewLinkHeaderFooterItem*)footerClearSyncAndSavedSiteDataItem {
  UIImage* infoIcon = [ChromeIcon infoIcon];
  UIImage* image = TintImage(infoIcon, [[MDCPalette greyPalette] tint500]);
  return [self
      footerItemWithType:ItemTypeFooterClearSyncAndSavedSiteData
                 titleID:
                     IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_CLEAR_SYNC_AND_SAVED_SITE_DATA
                     URL:kClearBrowsingDataLearnMoreURL
                   image:image];
}

- (TableViewLinkHeaderFooterItem*)footerItemWithType:
                                      (ClearBrowsingDataItemType)itemType
                                             titleID:(int)titleMessageID
                                                 URL:(const char[])URL
                                               image:(UIImage*)image {
  TableViewLinkHeaderFooterItem* footerItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:itemType];
  footerItem.text = l10n_util::GetNSString(titleMessageID);
  footerItem.urls = std::vector<GURL>{google_util::AppendGoogleLocaleParam(
      GURL(URL), GetApplicationContext()->GetApplicationLocale())};
  return footerItem;
}

- (TableViewDetailIconItem*)timeRangeItem {
  TableViewDetailIconItem* timeRangeItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeTimeRange];
  timeRangeItem.text = l10n_util::GetNSString(
      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TITLE);
  NSString* detailText = [TimeRangeSelectorTableViewController
      timePeriodLabelForPrefs:self.browserState->GetPrefs()];
  DCHECK(detailText);
  timeRangeItem.detailText = detailText;
  timeRangeItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  timeRangeItem.accessibilityTraits |= UIAccessibilityTraitButton;
  return timeRangeItem;
}

- (NSString*)accessibilityIdentifierFromItemType:(NSInteger)itemType {
  switch (itemType) {
    case ItemTypeDataTypeBrowsingHistory:
      return kClearBrowsingHistoryCellAccessibilityIdentifier;
    case ItemTypeDataTypeCookiesSiteData:
      return kClearCookiesCellAccessibilityIdentifier;
    case ItemTypeDataTypeCache:
      return kClearCacheCellAccessibilityIdentifier;
    case ItemTypeDataTypeSavedPasswords:
      return kClearSavedPasswordsCellAccessibilityIdentifier;
    case ItemTypeDataTypeAutofill:
      return kClearAutofillCellAccessibilityIdentifier;
    default: {
      NOTREACHED();
      return nil;
    }
  }
}

#pragma mark - Private Methods

// Signs the user out of Chrome if the sign-in state is |ConsentLevel::kSignin|.
- (void)signOutIfNotSyncing {
  DCHECK(self.browserState);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(self.browserState);
  DCHECK(identityManager);
  if (!identityManager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    AuthenticationService* authenticationService =
        AuthenticationServiceFactory::GetForBrowserState(_browserState);
    DCHECK(authenticationService);
    authenticationService->SignOut(
        signin_metrics::ProfileSignout::USER_DELETED_ACCOUNT_COOKIES,
        /*force_clear_browsing_data=*/false, nil);
  }
}

- (void)clearDataForDataTypes:(BrowsingDataRemoveMask)mask {
  DCHECK(mask != BrowsingDataRemoveMask::REMOVE_NOTHING);

  browsing_data::TimePeriod timePeriod =
      static_cast<browsing_data::TimePeriod>(_timeRangePref.GetValue());
  [self.consumer removeBrowsingDataForBrowserState:_browserState
                                        timePeriod:timePeriod
                                        removeMask:mask
                                   completionBlock:nil];

  // Send the "Cleared Browsing Data" event to the feature_engagement::Tracker
  // when the user initiates a clear browsing data action. No event is sent if
  // the browsing data is cleared without the user's input.
  feature_engagement::TrackerFactory::GetForBrowserState(_browserState)
      ->NotifyEvent(feature_engagement::events::kClearedBrowsingData);

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_HISTORY)) {
    PrefService* prefs = _browserState->GetPrefs();
    int noticeShownTimes = prefs->GetInteger(
        browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes);

    // When the deletion is complete, we might show an additional dialog with
    // a notice about other forms of browsing history. This is the case if
    const bool showDialog =
        // 1. The dialog is relevant for the user.
        _shouldPopupDialogAboutOtherFormsOfBrowsingHistory &&
        // 2. The notice has been shown less than |kMaxTimesHistoryNoticeShown|.
        noticeShownTimes < kMaxTimesHistoryNoticeShown;
    if (!showDialog) {
      return;
    }
    UMA_HISTOGRAM_BOOLEAN(
        "History.ClearBrowsingData.ShownHistoryNoticeAfterClearing",
        showDialog);

    // Increment the preference.
    prefs->SetInteger(
        browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes,
        noticeShownTimes + 1);
    [self.consumer showBrowsingHistoryRemovedDialog];
  }
}

#pragma mark Properties

- (void)setShouldShowNoticeAboutOtherFormsOfBrowsingHistory:(BOOL)showNotice
                                                   forModel:(ListModel*)model {
  _shouldShowNoticeAboutOtherFormsOfBrowsingHistory = showNotice;
  // Update the account footer if the model was already loaded.
  if (!model) {
    return;
  }
  UMA_HISTOGRAM_BOOLEAN(
      "History.ClearBrowsingData.HistoryNoticeShownInFooterWhenUpdated",
      _shouldShowNoticeAboutOtherFormsOfBrowsingHistory);

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(_browserState);
  if (!identityManager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return;
  }

  [model setFooter:[self footerForGoogleAccountSectionItem]
      forSectionWithIdentifier:SectionIdentifierGoogleAccount];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  PrefService* prefs = self.browserState->GetPrefs();
  if (preferenceName == browsing_data::prefs::kDeleteTimePeriod) {
    NSString* detailText =
        [TimeRangeSelectorTableViewController timePeriodLabelForPrefs:prefs];
    self.tableViewTimeRangeItem.detailText = detailText;
    [self.consumer updateCellsForItem:self.tableViewTimeRangeItem reload:YES];
  } else if (preferenceName == browsing_data::prefs::kDeleteBrowsingHistory) {
    self.browsingHistoryItem.checked = prefs->GetBoolean(preferenceName);
    [self.consumer updateCellsForItem:self.browsingHistoryItem reload:NO];
  } else if (preferenceName == browsing_data::prefs::kDeleteCookies) {
    self.cookiesSiteDataItem.checked = prefs->GetBoolean(preferenceName);
    [self.consumer updateCellsForItem:self.cookiesSiteDataItem reload:NO];
  } else if (preferenceName == browsing_data::prefs::kDeleteCache) {
    self.cacheItem.checked = prefs->GetBoolean(preferenceName);
    [self.consumer updateCellsForItem:self.cacheItem reload:NO];
  } else if (preferenceName == browsing_data::prefs::kDeletePasswords) {
    self.savedPasswordsItem.checked = prefs->GetBoolean(preferenceName);
    [self.consumer updateCellsForItem:self.savedPasswordsItem reload:NO];
  } else if (preferenceName == browsing_data::prefs::kDeleteFormData) {
    self.autofillItem.checked = prefs->GetBoolean(preferenceName);
    [self.consumer updateCellsForItem:self.autofillItem reload:NO];
  } else {
    DCHECK(false) << "Unxpected clear browsing data item type.";
  }
}

#pragma mark BrowsingDataRemoverObserving

- (void)browsingDataRemover:(BrowsingDataRemover*)remover
    didRemoveBrowsingDataWithMask:(BrowsingDataRemoveMask)mask {
  [self restartCounters:mask];
}

@end
