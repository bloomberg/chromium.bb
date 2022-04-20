// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_coordinator.h"

#import "base/ios/ios_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/reading_list/offline_page_tab_helper.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/ui/activity_services/activity_params.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_factory_delegate.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_view_controller_audience.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_view_controller_delegate.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_mediator.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_menu_provider.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_table_view_controller.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/table_view/table_view_animator.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/window_activities/window_activity_helpers.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/common/features.h"
#include "ios/web/public/navigation/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ReadingListCoordinator () <ReadingListMenuProvider,
                                      ReadingListListItemFactoryDelegate,
                                      ReadingListListViewControllerAudience,
                                      ReadingListListViewControllerDelegate,
                                      UIViewControllerTransitioningDelegate>

// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// The mediator that updates the table for model changes.
@property(nonatomic, strong) ReadingListMediator* mediator;
// The navigation controller displaying self.tableViewController.
@property(nonatomic, strong)
    TableViewNavigationController* navigationController;
// The view controller used to display the reading list.
@property(nonatomic, strong)
    ReadingListTableViewController* tableViewController;

// Coordinator in charge of handling sharing use cases.
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;

@end

@implementation ReadingListCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  if (self.started)
    return;

  // Create the mediator.
  ReadingListModel* model =
      ReadingListModelFactory::GetInstance()->GetForBrowserState(
          self.browser->GetBrowserState());
  ReadingListListItemFactory* itemFactory =
      [[ReadingListListItemFactory alloc] init];
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.mediator = [[ReadingListMediator alloc] initWithModel:model
                                               faviconLoader:faviconLoader
                                             listItemFactory:itemFactory];

  // Create the table.
  self.tableViewController = [[ReadingListTableViewController alloc] init];
  // Browser needs to be set before dataSource since the latter triggers a
  // reloadData call.
  self.tableViewController.browser = self.browser;
  self.tableViewController.delegate = self;
  self.tableViewController.audience = self;
  self.tableViewController.dataSource = self.mediator;

  self.tableViewController.menuProvider = self;

  itemFactory.accessibilityDelegate = self.tableViewController;

  // Add the "Done" button and hook it up to |stop|.
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismissButtonTapped)];
  [dismissButton
      setAccessibilityIdentifier:kTableViewNavigationDismissButtonId];
  self.tableViewController.navigationItem.rightBarButtonItem = dismissButton;

  // Present RecentTabsNavigationController.
  self.navigationController = [[TableViewNavigationController alloc]
      initWithTable:self.tableViewController];

  // The initial call to |readingListHasItems:| may have been received before
  // all UI elements were initialized. Call the callback directly to set up
  // everything correctly.
  [self readingListHasItems:self.mediator.hasElements];

  BOOL useCustomPresentation = YES;
      [self.navigationController
          setModalPresentationStyle:UIModalPresentationFormSheet];
      self.navigationController.presentationController.delegate =
          self.tableViewController;
      useCustomPresentation = NO;

  if (useCustomPresentation) {
    self.navigationController.transitioningDelegate = self;
    self.navigationController.modalPresentationStyle =
        UIModalPresentationCustom;
  }

  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];

  // Send the "Viewed Reading List" event to the feature_engagement::Tracker
  // when the user opens their reading list.
  feature_engagement::TrackerFactory::GetForBrowserState(
      self.browser->GetBrowserState())
      ->NotifyEvent(feature_engagement::events::kViewedReadingList);

  [super start];
  self.started = YES;
}

- (void)dismissButtonTapped {
  base::RecordAction(base::UserMetricsAction("MobileReadingListClose"));
  [self stop];
}

- (void)stop {
  if (!self.started)
    return;
  [self.tableViewController willBeDismissed];
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.tableViewController = nil;
  self.navigationController = nil;

  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;

  [super stop];
  self.started = NO;
}

#pragma mark - ReadingListListViewControllerAudience

- (void)readingListHasItems:(BOOL)hasItems {
  self.navigationController.toolbarHidden = !hasItems;
}

#pragma mark - ReadingListTableViewControllerDelegate

- (void)dismissReadingListListViewController:(UIViewController*)viewController {
  DCHECK_EQ(self.tableViewController, viewController);
  [self.tableViewController willBeDismissed];
  [self stop];
}

- (void)readingListListViewController:(UIViewController*)viewController
                             openItem:(id<ReadingListListItem>)item {
  DCHECK_EQ(self.tableViewController, viewController);
  const ReadingListEntry* entry = [self.mediator entryFromItem:item];
  if (!entry) {
    [self.tableViewController reloadData];
    return;
  }
  [self loadEntryURL:entry->URL()
          withOfflineURL:GURL::EmptyGURL()
      loadOfflineVersion:NO
                inNewTab:NO
               incognito:NO];
}

- (void)readingListListViewController:(UIViewController*)viewController
                     openItemInNewTab:(id<ReadingListListItem>)item
                            incognito:(BOOL)incognito {
  DCHECK_EQ(self.tableViewController, viewController);
  const ReadingListEntry* entry = [self.mediator entryFromItem:item];
  if (!entry) {
    [self.tableViewController reloadData];
    return;
  }
  [self loadEntryURL:entry->URL()
          withOfflineURL:GURL::EmptyGURL()
      loadOfflineVersion:NO
                inNewTab:YES
               incognito:incognito];
}

- (void)readingListListViewController:(UIViewController*)viewController
              openItemOfflineInNewTab:(id<ReadingListListItem>)item {
  DCHECK_EQ(self.tableViewController, viewController);
  [self openItemOfflineInNewTab:item];
}

#pragma mark - UIViewControllerTransitioningDelegate

- (UIPresentationController*)
presentationControllerForPresentedViewController:(UIViewController*)presented
                        presentingViewController:(UIViewController*)presenting
                            sourceViewController:(UIViewController*)source {
  return [[TableViewPresentationController alloc]
      initWithPresentedViewController:presented
             presentingViewController:presenting];
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForPresentedController:(UIViewController*)presented
                     presentingController:(UIViewController*)presenting
                         sourceController:(UIViewController*)source {
  UITraitCollection* traitCollection = presenting.traitCollection;
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact) {
    // Use the default animator for fullscreen presentations.
    return nil;
  }

  TableViewAnimator* animator = [[TableViewAnimator alloc] init];
  animator.presenting = YES;
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForDismissedController:(UIViewController*)dismissed {
  UITraitCollection* traitCollection = dismissed.traitCollection;
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact) {
    // Use the default animator for fullscreen presentations.
    return nil;
  }

  TableViewAnimator* animator = [[TableViewAnimator alloc] init];
  animator.presenting = NO;
  return animator;
}

#pragma mark - URL Loading Helpers

// Loads reading list URLs. If |offlineURL| is valid and |loadOfflineVersion| is
// true, the item will be loaded offline; otherwise |entryURL| is loaded.
// |newTab| and |incognito| can be used to optionally open the URL in a new tab
// or in incognito.  The coordinator is also stopped after the load is
// requested.
// NOTE: |loadOfflineVersion| may not be used with |inNewTab|.
// TODO(crbug.com/1313458):  Remove |inNewTab| and |withOfflineURL| when
// migration is complete.
- (void)loadEntryURL:(const GURL&)entryURL
        withOfflineURL:(const GURL&)offlineURL
    loadOfflineVersion:(BOOL)loadOfflineVersion
              inNewTab:(BOOL)newTab
             incognito:(BOOL)incognito {
  // Override incognito opening using enterprise policy.
  incognito = incognito || self.isIncognitoForced;
  incognito = incognito && self.isIncognitoAvailable;
  // Only open a new incognito tab when incognito is authenticated. Prompt for
  // auth otherwise.
  if (incognito) {
    IncognitoReauthSceneAgent* reauthAgent = [IncognitoReauthSceneAgent
        agentFromScene:SceneStateBrowserAgent::FromBrowser(self.browser)
                           ->GetSceneState()];
    __weak ReadingListCoordinator* weakSelf = self;
    if (reauthAgent.authenticationRequired) {
      // Copy C++ args to call later from the block.
      GURL copyEntryURL = GURL(entryURL);
      GURL copyOfflineURL = GURL(offlineURL);
      [reauthAgent
          authenticateIncognitoContentWithCompletionBlock:^(BOOL success) {
            if (success) {
              [weakSelf loadEntryURL:copyEntryURL
                      withOfflineURL:copyOfflineURL
                  loadOfflineVersion:YES
                            inNewTab:newTab
                           incognito:incognito];
            }
          }];
      return;
    }
  }

  DCHECK(entryURL.is_valid());
  base::RecordAction(base::UserMetricsAction("MobileReadingListOpen"));
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  new_tab_page_uma::RecordAction(
      self.browser->GetBrowserState(), activeWebState,
      new_tab_page_uma::ACTION_OPENED_READING_LIST_ENTRY);

  // Load the offline URL if available.
  GURL loadURL = entryURL;
  if (offlineURL.is_valid() && !loadOfflineVersion) {
    loadURL = offlineURL;
    // Offline URLs should always be opened in new tabs.
    newTab = YES;
    const GURL updateURL = entryURL;
    [self.mediator markEntryRead:updateURL];
  }

  // Prepare the table for dismissal.
  [self.tableViewController willBeDismissed];

  if (loadOfflineVersion) {
    DCHECK(!newTab);
    web::WebState* activeWebState =
        self.browser->GetWebStateList()->GetActiveWebState();
    OfflinePageTabHelper* offlinePageTabHelper =
        OfflinePageTabHelper::FromWebState(activeWebState);
    if (offlinePageTabHelper &&
        offlinePageTabHelper->CanHandleErrorLoadingURL(entryURL)) {
      offlinePageTabHelper->LoadOfflinePage(entryURL);
    }
    // Use a referrer with a specific URL to signal that this entry should not
    // be taken into account for the Most Visited tiles.
  } else if (newTab) {
    UrlLoadParams params = UrlLoadParams::InNewTab(loadURL, entryURL);
    params.in_incognito = incognito;
    params.web_params.referrer = web::Referrer(GURL(kReadingListReferrerURL),
                                               web::ReferrerPolicyDefault);
    UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
  } else {
    UrlLoadParams params = UrlLoadParams::InCurrentTab(loadURL);
    params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    params.web_params.referrer = web::Referrer(GURL(kReadingListReferrerURL),
                                               web::ReferrerPolicyDefault);
    UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
  }

  [self stop];
}

- (void)openItemOfflineInNewTab:(id<ReadingListListItem>)item {
  const ReadingListEntry* entry = [self.mediator entryFromItem:item];
  if (!entry)
    return;

  BOOL offTheRecord = self.browser->GetBrowserState()->IsOffTheRecord();

  if (entry->DistilledState() == ReadingListEntry::PROCESSED) {
    const GURL entryURL = entry->URL();
    GURL offlineURL = reading_list::OfflineURLForPath(
        entry->DistilledPath(), entryURL, entry->DistilledURL());

    if (web::features::IsLoadSimulatedRequestAPIEnabled()) {
      [self loadEntryURL:entryURL
              withOfflineURL:entryURL
          loadOfflineVersion:YES
                    inNewTab:NO
                   incognito:offTheRecord];
    } else {
      [self loadEntryURL:entryURL
              withOfflineURL:offlineURL
          loadOfflineVersion:NO
                    inNewTab:YES
                   incognito:offTheRecord];
    }
  }
}

#pragma mark - ReadingListMenuProvider

- (UIContextMenuConfiguration*)contextMenuConfigurationForItem:
                                   (id<ReadingListListItem>)item
                                                      withView:(UIView*)view {
  __weak id<ReadingListListItemAccessibilityDelegate> accessibilityDelegate =
      self.tableViewController;
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        if (!weakSelf) {
          // Return an empty menu.
          return [UIMenu menuWithTitle:@"" children:@[]];
        }

        ReadingListCoordinator* strongSelf = weakSelf;

        // Record that this context menu was shown to the user.
        RecordMenuShown(MenuScenario::kReadingListEntry);

        BrowserActionFactory* actionFactory = [[BrowserActionFactory alloc]
            initWithBrowser:strongSelf.browser
                   scenario:MenuScenario::kReadingListEntry];

        NSMutableArray<UIMenuElement*>* menuElements =
            [[NSMutableArray alloc] init];

        UIAction* openInNewTab = [actionFactory actionToOpenInNewTabWithBlock:^{
          if ([weakSelf isIncognitoForced])
            return;

          [weakSelf loadEntryURL:item.entryURL
                  withOfflineURL:GURL::EmptyGURL()
              loadOfflineVersion:NO
                        inNewTab:YES
                       incognito:NO];
        }];
        if ([self isIncognitoForced]) {
          openInNewTab.attributes = UIMenuElementAttributesDisabled;
        }
        [menuElements addObject:openInNewTab];

        UIAction* openInNewIncognitoTab =
            [actionFactory actionToOpenInNewIncognitoTabWithBlock:^{
              if (![weakSelf isIncognitoAvailable])
                return;

              [weakSelf loadEntryURL:item.entryURL
                      withOfflineURL:GURL::EmptyGURL()
                  loadOfflineVersion:NO
                            inNewTab:YES
                           incognito:YES];
            }];
        if (![self isIncognitoAvailable]) {
          openInNewIncognitoTab.attributes = UIMenuElementAttributesDisabled;
        }
        [menuElements addObject:openInNewIncognitoTab];

        const ReadingListEntry* entry = [self.mediator entryFromItem:item];
        if (entry->DistilledState() == ReadingListEntry::PROCESSED) {
          [menuElements
              addObject:[actionFactory
                            actionToOpenOfflineVersionInNewTabWithBlock:^{
                              [weakSelf openItemOfflineInNewTab:item];
                            }]];
        }

        if (base::ios::IsMultipleScenesSupported()) {
          [menuElements
              addObject:
                  [actionFactory
                      actionToOpenInNewWindowWithURL:item.entryURL
                                      activityOrigin:
                                          WindowActivityReadingListOrigin]];
        }

        if ([accessibilityDelegate isItemRead:item]) {
          [menuElements
              addObject:[actionFactory actionToMarkAsUnreadWithBlock:^{
                [accessibilityDelegate markItemUnread:item];
              }]];
        } else {
          [menuElements addObject:[actionFactory actionToMarkAsReadWithBlock:^{
                          [accessibilityDelegate markItemRead:item];
                        }]];
        }

        [menuElements addObject:[actionFactory actionToCopyURL:item.entryURL]];

        [menuElements addObject:[actionFactory actionToShareWithBlock:^{
                        [weakSelf shareURL:item.entryURL
                                     title:item.title
                                  fromView:view];
                      }]];

        [menuElements addObject:[actionFactory actionToDeleteWithBlock:^{
                        [accessibilityDelegate deleteItem:item];
                      }]];

        return [UIMenu menuWithTitle:@"" children:menuElements];
      };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

#pragma mark - Private

// Triggers the URL sharing flow for the given |URL| and |title|, with the
// origin |view| representing the UI component for that URL.
- (void)shareURL:(const GURL&)URL
           title:(NSString*)title
        fromView:(UIView*)view {
  ActivityParams* params =
      [[ActivityParams alloc] initWithURL:URL
                                    title:title
                                 scenario:ActivityScenario::ReadingListEntry];
  self.sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.tableViewController
                         browser:self.browser
                          params:params
                      originView:view];
  [self.sharingCoordinator start];
}

#pragma mark - ReadingListListItemFactoryDelegate

- (BOOL)isIncognitoForced {
  return IsIncognitoModeForced(self.browser->GetBrowserState()->GetPrefs());
}

- (BOOL)isIncognitoAvailable {
  return !IsIncognitoModeDisabled(self.browser->GetBrowserState()->GetPrefs());
}

@end
