// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/browser_coordinator.h"

#include <memory>

#include "base/scoped_observer.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/app_launcher/app_launcher_abuse_detector.h"
#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper.h"
#import "ios/chrome/browser/autofill/autofill_tab_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/download/download_directory_util.h"
#import "ios/chrome/browser/download/external_app_util.h"
#import "ios/chrome/browser/download/pass_kit_tab_helper.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/store_kit/store_kit_coordinator.h"
#import "ios/chrome/browser/store_kit/store_kit_tab_helper.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/chrome/browser/ui/activity_services/activity_service_coordinator.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_presentation.h"
#import "ios/chrome/browser/ui/alert_coordinator/repost_form_coordinator.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_all_password_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/badges/badge_popup_menu_coordinator.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_coordinator.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller+private.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller_dependency_factory.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#import "ios/chrome/browser/ui/commands/infobar_commands.h"
#import "ios/chrome/browser/ui/commands/page_info_commands.h"
#import "ios/chrome/browser/ui/commands/password_breach_commands.h"
#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#import "ios/chrome/browser/ui/commands/text_zoom_commands.h"
#import "ios/chrome/browser/ui/download/ar_quick_look_coordinator.h"
#import "ios/chrome/browser/ui/download/features.h"
#import "ios/chrome/browser/ui/download/pass_kit_coordinator.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_controller_ios.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_coordinator.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/open_in/open_in_mediator.h"
#import "ios/chrome/browser/ui/overlays/overlay_container_coordinator.h"
#import "ios/chrome/browser/ui/page_info/features.h"
#import "ios/chrome/browser/ui/page_info/page_info_coordinator.h"
#import "ios/chrome/browser/ui/page_info/page_info_legacy_coordinator.h"
#import "ios/chrome/browser/ui/passwords/password_breach_coordinator.h"
#import "ios/chrome/browser/ui/print/print_controller.h"
#import "ios/chrome/browser/ui/qr_generator/qr_generator_coordinator.h"
#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_legacy_coordinator.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_coordinator.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_coordinator.h"
#import "ios/chrome/browser/ui/snackbar/snackbar_coordinator.h"
#import "ios/chrome/browser/ui/text_zoom/text_zoom_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_presenter.h"
#import "ios/chrome/browser/ui/translate/legacy_translate_infobar_coordinator.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web/features.h"
#import "ios/chrome/browser/web/font_size_tab_helper.h"
#import "ios/chrome/browser/web/print_tab_helper.h"
#import "ios/chrome/browser/web/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/repost_form_tab_helper_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BrowserCoordinator () <ActivityServiceCommands,
                                  ActivityServicePresentation,
                                  AutofillSecurityAlertPresenter,
                                  BrowserCoordinatorCommands,
                                  FormInputAccessoryCoordinatorNavigator,
                                  PageInfoCommands,
                                  QRGenerationCommands,
                                  RepostFormTabHelperDelegate,
                                  ToolbarAccessoryCoordinatorDelegate,
                                  URLLoadingDelegate,
                                  WebStateListObserving>

// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;

// Handles command dispatching, provided by the Browser instance.
@property(nonatomic, weak) CommandDispatcher* dispatcher;

// The coordinator managing the container view controller.
@property(nonatomic, strong)
    BrowserContainerCoordinator* browserContainerCoordinator;

// Mediator between OpenIn TabHelper and OpenIn UI.
@property(nonatomic, strong) OpenInMediator* openInMediator;

// =================================================
// Child Coordinators, listed in alphabetical order.
// =================================================

// Presents a QLPreviewController in order to display USDZ format 3D models.
@property(nonatomic, strong) ARQuickLookCoordinator* ARQuickLookCoordinator;

// Coordinator for the activity view.
@property(nonatomic, strong)
    ActivityServiceCoordinator* activityServiceCoordinator;

// Coordinator to add new credit card.
@property(nonatomic, strong)
    AutofillAddCreditCardCoordinator* addCreditCardCoordinator;

// Coordinator for the badge popup menu.
@property(nonatomic, strong)
    BadgePopupMenuCoordinator* badgePopupMenuCoordinator;

// Coordinator for the find bar.
@property(nonatomic, strong) FindBarCoordinator* findBarCoordinator;

// Coordinator in charge of the presenting autofill options above the
// keyboard.
@property(nonatomic, strong)
    FormInputAccessoryCoordinator* formInputAccessoryCoordinator;

// The object in charge of interacting with the web view. Used to fill the data
// in the forms.
@property(nonatomic, strong) ManualFillInjectionHandler* injectionHandler;

// Coordinator in charge of the presenting password autofill options as a modal.
@property(nonatomic, strong)
    ManualFillAllPasswordCoordinator* allPasswordCoordinator;

// Weak reference for the next coordinator to be displayed over the toolbar.
@property(nonatomic, weak) ChromeCoordinator* nextToolbarCoordinator;

// Coordinator for Page Info UI.
@property(nonatomic, strong) ChromeCoordinator* pageInfoCoordinator;

// Coordinator for the PassKit UI presentation.
@property(nonatomic, strong) PassKitCoordinator* passKitCoordinator;

// Coordinator for the password breach UI presentation.
@property(nonatomic, strong)
    PasswordBreachCoordinator* passwordBreachCoordinator;

// Used to display the Print UI. Nil if not visible.
// TODO(crbug.com/910017): Convert to coordinator.
@property(nonatomic, strong) PrintController* printController;

// Coordinator for the QR generator UI.
@property(nonatomic, strong) QRGeneratorCoordinator* qrGeneratorCoordinator;

// Coordinator for the QR scanner.
@property(nonatomic, strong) QRScannerLegacyCoordinator* qrScannerCoordinator;

// Coordinator for displaying the Reading List.
@property(nonatomic, strong) ReadingListCoordinator* readingListCoordinator;

// Coordinator for Recent Tabs.
@property(nonatomic, strong) RecentTabsCoordinator* recentTabsCoordinator;

// Coordinator for displaying Repost Form dialog.
@property(nonatomic, strong) RepostFormCoordinator* repostFormCoordinator;

// Coordinator for displaying snackbars.
@property(nonatomic, strong) SnackbarCoordinator* snackbarCoordinator;

// Coordinator for presenting SKStoreProductViewController.
@property(nonatomic, strong) StoreKitCoordinator* storeKitCoordinator;

// Coordinator for Text Zoom.
@property(nonatomic, strong) TextZoomCoordinator* textZoomCoordinator;

// Coordinator for the translate infobar's language selection and translate
// option popup menus.
@property(nonatomic, strong)
    LegacyTranslateInfobarCoordinator* translateInfobarCoordinator;

// The container coordinators for the infobar modalities.
@property(nonatomic, strong)
    OverlayContainerCoordinator* infobarBannerOverlayContainerCoordinator;
@property(nonatomic, strong)
    OverlayContainerCoordinator* infobarModalOverlayContainerCoordinator;

@end

@implementation BrowserCoordinator {
  // Observers for WebStateList.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<ScopedObserver<WebStateList, WebStateListObserver>>
      _scopedWebStateListObserver;
}

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  if (self = [super initWithBaseViewController:viewController
                                       browser:browser]) {
    _dispatcher = browser->GetCommandDispatcher();
  }
  return self;
}
- (void)start {
  if (self.started)
    return;

  DCHECK(!self.viewController);
  [self startBrowserContainer];
  [self.dispatcher startDispatchingToTarget:self
                                forProtocol:@protocol(TextZoomCommands)];
  [self.dispatcher startDispatchingToTarget:self
                                forProtocol:@protocol(FindInPageCommands)];
  [self createViewController];
  [self startChildCoordinators];
  [self.dispatcher startDispatchingToTarget:self
                                forProtocol:@protocol(ActivityServiceCommands)];
  [self.dispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(BrowserCoordinatorCommands)];
  [self.dispatcher startDispatchingToTarget:self
                                forProtocol:@protocol(PageInfoCommands)];
  [self.dispatcher startDispatchingToTarget:self
                                forProtocol:@protocol(QRGenerationCommands)];
  [self installDelegatesForAllWebStates];
  [self installDelegatesForBrowser];
  [self addWebStateListObserver];
  [super start];
  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;
  [super stop];
  [self removeWebStateListObserver];
  [self uninstallDelegatesForBrowser];
  [self uninstallDelegatesForAllWebStates];
  [self.dispatcher stopDispatchingToTarget:self];
  [self stopChildCoordinators];
  [self destroyViewController];
  [self stopBrowserContainer];
  self.dispatcher = nil;
  self.started = NO;
}

#pragma mark - Public

- (void)setActive:(BOOL)active {
  DCHECK_EQ(_active, self.viewController.active);
  if (_active == active) {
    return;
  }
  _active = active;

  self.viewController.active = active;
}

- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  [self.activityServiceCoordinator stop];
  self.activityServiceCoordinator = nil;

  [self.passKitCoordinator stop];

  [self.openInMediator disableAll];

  [self.printController dismissAnimated:YES];

  [self.readingListCoordinator stop];
  self.readingListCoordinator = nil;

  [self.qrGeneratorCoordinator stop];
  self.qrGeneratorCoordinator = nil;

  [self.passwordBreachCoordinator stop];

  [self.pageInfoCoordinator stop];

  [self.viewController clearPresentedStateWithCompletion:completion
                                          dismissOmnibox:dismissOmnibox];
}

- (void)displayPopupMenuWithBadgeItems:(NSArray<id<BadgeItem>>*)badgeItems {
  self.badgePopupMenuCoordinator = [[BadgePopupMenuCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.badgePopupMenuCoordinator setBadgeItemsToShow:badgeItems];
  [self.badgePopupMenuCoordinator start];
}

#pragma mark - Private

// Instantiates a BrowserViewController.
- (void)createViewController {
  DCHECK(self.browserContainerCoordinator.viewController);
  BrowserViewControllerDependencyFactory* factory =
      [[BrowserViewControllerDependencyFactory alloc]
          initWithBrowser:self.browser];
  _viewController = [[BrowserViewController alloc]
                     initWithBrowser:self.browser
                   dependencyFactory:factory
      browserContainerViewController:self.browserContainerCoordinator
                                         .viewController];
}

// Shuts down the BrowserViewController.
- (void)destroyViewController {
  [self.viewController shutdown];
  _viewController = nil;
}

// Starts the browser container.
- (void)startBrowserContainer {
  self.browserContainerCoordinator = [[BrowserContainerCoordinator alloc]
      initWithBaseViewController:nil
                         browser:self.browser];
  [self.browserContainerCoordinator start];
}

// Stops the browser container.
- (void)stopBrowserContainer {
  [self.browserContainerCoordinator stop];
  self.browserContainerCoordinator = nil;
}

// Starts child coordinators.
- (void)startChildCoordinators {
  // Dispatcher should be instantiated so that it can be passed to child
  // coordinators.
  DCHECK(self.dispatcher);

  /* ActivityServiceCoordinator is created and started by a command. */

  self.ARQuickLookCoordinator = [[ARQuickLookCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.ARQuickLookCoordinator start];

  self.injectionHandler = [[ManualFillInjectionHandler alloc]
        initWithWebStateList:self.browser->GetWebStateList()
      securityAlertPresenter:self];
  self.formInputAccessoryCoordinator = [[FormInputAccessoryCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                injectionHandler:self.injectionHandler];
  self.formInputAccessoryCoordinator.navigator = self;
  [self.formInputAccessoryCoordinator start];

  self.snackbarCoordinator = [[SnackbarCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.snackbarCoordinator start];

  self.translateInfobarCoordinator = [[LegacyTranslateInfobarCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.translateInfobarCoordinator start];

  self.passKitCoordinator =
      [[PassKitCoordinator alloc] initWithBaseViewController:self.viewController
                                                     browser:self.browser];

  self.passwordBreachCoordinator = [[PasswordBreachCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self.passwordBreachCoordinator
                   forProtocol:@protocol(PasswordBreachCommands)];

  self.printController = [[PrintController alloc] init];

  self.qrScannerCoordinator = [[QRScannerLegacyCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.qrScannerCoordinator start];

  /* QRGeneratorCoordinator is created and started by a command. */

  /* ReadingListCoordinator is created and started by a BrowserCommand */

  /* RecentTabsCoordinator is created and started by a BrowserCommand */

  /* RepostFormCoordinator is created and started by a delegate method */

  self.storeKitCoordinator = [[StoreKitCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];

  self.addCreditCardCoordinator = [[AutofillAddCreditCardCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];

  if (base::FeatureList::IsEnabled(kInfobarOverlayUI)) {
    self.infobarBannerOverlayContainerCoordinator =
        [[OverlayContainerCoordinator alloc]
            initWithBaseViewController:self.viewController
                               browser:self.browser
                              modality:OverlayModality::kInfobarBanner];
    [self.infobarBannerOverlayContainerCoordinator start];
    self.viewController.infobarBannerOverlayContainerViewController =
        self.infobarBannerOverlayContainerCoordinator.viewController;

    self.infobarModalOverlayContainerCoordinator =
        [[OverlayContainerCoordinator alloc]
            initWithBaseViewController:self.viewController
                               browser:self.browser
                              modality:OverlayModality::kInfobarModal];
    [self.infobarModalOverlayContainerCoordinator start];
    self.viewController.infobarModalOverlayContainerViewController =
        self.infobarModalOverlayContainerCoordinator.viewController;
  }
}

// Stops child coordinators.
- (void)stopChildCoordinators {
  [self.activityServiceCoordinator stop];
  self.activityServiceCoordinator = nil;

  [self.allPasswordCoordinator stop];
  self.allPasswordCoordinator = nil;

  [self.ARQuickLookCoordinator stop];
  self.ARQuickLookCoordinator = nil;

  [self.findBarCoordinator stop];
  self.findBarCoordinator = nil;

  [self.formInputAccessoryCoordinator stop];
  self.formInputAccessoryCoordinator = nil;
  self.injectionHandler = nil;

  [self.pageInfoCoordinator stop];
  self.pageInfoCoordinator = nil;

  [self.passKitCoordinator stop];
  self.passKitCoordinator = nil;

  [self.passwordBreachCoordinator stop];
  [self.browser->GetCommandDispatcher()
      stopDispatchingToTarget:self.passwordBreachCoordinator];
  self.passwordBreachCoordinator = nil;

  self.printController = nil;

  [self.qrGeneratorCoordinator stop];
  self.qrGeneratorCoordinator = nil;

  [self.qrScannerCoordinator stop];
  self.qrScannerCoordinator = nil;

  [self.readingListCoordinator stop];
  self.readingListCoordinator = nil;

  [self.recentTabsCoordinator stop];
  self.recentTabsCoordinator = nil;

  [self.repostFormCoordinator stop];
  self.repostFormCoordinator = nil;

  [self.snackbarCoordinator stop];
  self.snackbarCoordinator = nil;

  [self.storeKitCoordinator stop];
  self.storeKitCoordinator = nil;

  [self.textZoomCoordinator stop];
  self.textZoomCoordinator = nil;

  [self.translateInfobarCoordinator stop];
  self.translateInfobarCoordinator = nil;

  [self.addCreditCardCoordinator stop];
  self.addCreditCardCoordinator = nil;

  [self.infobarBannerOverlayContainerCoordinator stop];
  self.infobarBannerOverlayContainerCoordinator = nil;

  [self.infobarModalOverlayContainerCoordinator stop];
  self.infobarModalOverlayContainerCoordinator = nil;
}

#pragma mark - AutofillSecurityAlertPresenter

- (void)presentSecurityWarningAlertWithText:(NSString*)body {
  NSString* alertTitle =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_TITLE);
  NSString* defaultActionTitle =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_OK_BUTTON);

  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:alertTitle
                                          message:body
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* defaultAction =
      [UIAlertAction actionWithTitle:defaultActionTitle
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* action){
                             }];
  [alert addAction:defaultAction];
  UIViewController* presenter = self.viewController;
  while (presenter.presentedViewController) {
    presenter = presenter.presentedViewController;
  }
  [presenter presentViewController:alert animated:YES completion:nil];
}

#pragma mark - ActivityServiceCommands

- (void)sharePage {
  self.activityServiceCoordinator = [[ActivityServiceCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.activityServiceCoordinator.positionProvider =
      [self.viewController activityServicePositioner];
  self.activityServiceCoordinator.presentationProvider = self;
  [self.activityServiceCoordinator start];
}

#pragma mark - ActivityServicePresentation

- (void)activityServiceDidEndPresenting {
  [self.activityServiceCoordinator stop];
  self.activityServiceCoordinator = nil;
}

#pragma mark - BrowserCoordinatorCommands

- (void)printTab {
  DCHECK(self.printController);
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  [self.printController printView:webState->GetView()
                        withTitle:tab_util::GetTabTitle(webState)];
}

- (void)showReadingList {
  self.readingListCoordinator = [[ReadingListCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.readingListCoordinator start];
}

- (void)showDownloadsFolder {
  if (base::FeatureList::IsEnabled(kOpenDownloadsInFilesApp)) {
    NSURL* URL = GetFilesAppUrl();
    if (!URL)
      return;

    [[UIApplication sharedApplication] openURL:URL
                                       options:@{}
                             completionHandler:nil];
    return;
  }

  base::FilePath download_dir;
  if (!GetTempDownloadsDirectory(&download_dir)) {
    return;
  }

  UIDocumentPickerViewController* documentPicker =
      [[UIDocumentPickerViewController alloc]
          initWithDocumentTypes:@[ @"public.data" ]
                         inMode:UIDocumentPickerModeImport];
  documentPicker.modalPresentationStyle = UIModalPresentationFormSheet;
  [self.viewController presentViewController:documentPicker
                                    animated:YES
                                  completion:nil];
}

- (void)showRecentTabs {
  // TODO(crbug.com/825431): If BVC's clearPresentedState is ever called (such
  // as in tearDown after a failed egtest), then this coordinator is left in a
  // started state even though its corresponding VC is no longer on screen.
  // That causes issues when the coordinator is started again and we destroy the
  // old mediator without disconnecting it first.  Temporarily work around these
  // issues by not having a long lived coordinator.  A longer-term solution will
  // require finding a way to stop this coordinator so that the mediator is
  // properly disconnected and destroyed and does not live longer than its
  // associated VC.
  if (self.recentTabsCoordinator) {
    [self.recentTabsCoordinator stop];
    self.recentTabsCoordinator = nil;
  }

  self.recentTabsCoordinator = [[RecentTabsCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.recentTabsCoordinator.loadStrategy = UrlLoadStrategy::NORMAL;
  [self.recentTabsCoordinator start];
}

- (void)showAddCreditCard {
  [self.formInputAccessoryCoordinator reset];
  [self.addCreditCardCoordinator start];
}

#pragma mark - FindInPageCommands

- (void)openFindInPage {
  if (!self.canShowFindBar)
    return;

  self.findBarCoordinator =
      [[FindBarCoordinator alloc] initWithBaseViewController:self.viewController
                                                     browser:self.browser];
  self.findBarCoordinator.presenter =
      self.viewController.toolbarAccessoryPresenter;
  self.findBarCoordinator.delegate = self;
  self.findBarCoordinator.presentationDelegate = self.viewController;

  if (self.viewController.toolbarAccessoryPresenter.isPresenting) {
    self.nextToolbarCoordinator = self.findBarCoordinator;
    [self closeTextZoom];
    return;
  }

  [self.findBarCoordinator start];
}

- (void)closeFindInPage {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();

  if (currentWebState) {
    FindTabHelper* findTabHelper = FindTabHelper::FromWebState(currentWebState);
    if (findTabHelper->IsFindUIActive()) {
      findTabHelper->StopFinding();
    } else {
      [self.findBarCoordinator stop];
    }
  }
}

- (void)showFindUIIfActive {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  auto* findHelper = FindTabHelper::FromWebState(currentWebState);
  if (findHelper && findHelper->IsFindUIActive() &&
      !self.findBarCoordinator.presenter.isPresenting) {
    [self.findBarCoordinator start];
  }
}

- (void)hideFindUI {
  [self.findBarCoordinator stop];
}

- (void)defocusFindInPage {
  [self.findBarCoordinator defocusFindBar];
}

- (void)searchFindInPage {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  DCHECK(currentWebState);
  FindTabHelper* helper = FindTabHelper::FromWebState(currentWebState);
  helper->StartFinding([self.findBarCoordinator.findBarController searchTerm]);

  if (!self.browser->GetBrowserState()->IsOffTheRecord())
    helper->PersistSearchTerm();
}

- (void)findNextStringInPage {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  DCHECK(currentWebState);
  // TODO(crbug.com/603524): Reshow find bar if necessary.
  FindTabHelper::FromWebState(currentWebState)
      ->ContinueFinding(FindTabHelper::FORWARD);
}

- (void)findPreviousStringInPage {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  DCHECK(currentWebState);
  // TODO(crbug.com/603524): Reshow find bar if necessary.
  FindTabHelper::FromWebState(currentWebState)
      ->ContinueFinding(FindTabHelper::REVERSE);
}

#pragma mark - FindInPageCommands Helpers

- (BOOL)canShowFindBar {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return NO;
  }

  auto* helper = FindTabHelper::FromWebState(currentWebState);
  return (helper && helper->CurrentPageSupportsFindInPage() &&
          !helper->IsFindUIActive());
}

#pragma mark - PageInfoCommands

- (void)legacyShowPageInfoForOriginPoint:(CGPoint)originPoint {
  PageInfoLegacyCoordinator* pageInfoCoordinator =
      [[PageInfoLegacyCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  pageInfoCoordinator.presentationProvider = self.viewController;
  pageInfoCoordinator.originPoint = originPoint;
  self.pageInfoCoordinator = pageInfoCoordinator;
  [self.pageInfoCoordinator start];
}

- (void)showPageInfo {
  DCHECK(base::FeatureList::IsEnabled(kPageInfoRefactoring));
  PageInfoCoordinator* pageInfoCoordinator = [[PageInfoCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  pageInfoCoordinator.presentationProvider = self.viewController;
  self.pageInfoCoordinator = pageInfoCoordinator;
  [self.pageInfoCoordinator start];
}

- (void)hidePageInfo {
  [self.pageInfoCoordinator stop];
  self.pageInfoCoordinator = nil;
}

- (void)showSecurityHelpPage {
  UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kPageInfoHelpCenterURL));
  params.in_incognito = self.browser->GetBrowserState()->IsOffTheRecord();
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
  [self hidePageInfo];
}

#pragma mark - QRGenerationCommands

- (void)generateQRCode:(GenerateQRCodeCommand*)command {
  DCHECK(base::FeatureList::IsEnabled(kQRCodeGeneration));
  self.qrGeneratorCoordinator = [[QRGeneratorCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                           title:command.title
                             URL:command.URL];
  [self.qrGeneratorCoordinator start];
}

- (void)hideQRCode {
  [self.qrGeneratorCoordinator stop];
  self.qrGeneratorCoordinator = nil;
}

#pragma mark - FormInputAccessoryCoordinatorNavigator

- (void)openPasswordSettings {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showSavedPasswordsSettingsFromViewController:self.viewController];
}

- (void)openAddressSettings {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showProfileSettingsFromViewController:self.viewController];
}

- (void)openCreditCardSettings {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showCreditCardSettingsFromViewController:self.viewController];
}

- (void)openAllPasswordsPicker {
  self.allPasswordCoordinator = [[ManualFillAllPasswordCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                injectionHandler:self.injectionHandler];
  [self.allPasswordCoordinator start];
}

#pragma mark - RepostFormTabHelperDelegate

- (void)repostFormTabHelper:(RepostFormTabHelper*)helper
    presentRepostFormDialogForWebState:(web::WebState*)webState
                         dialogAtPoint:(CGPoint)location
                     completionHandler:(void (^)(BOOL))completion {
  self.repostFormCoordinator = [[RepostFormCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                  dialogLocation:location
                        webState:webState
               completionHandler:completion];
  [self.repostFormCoordinator start];
}

- (void)repostFormTabHelperDismissRepostFormDialog:
    (RepostFormTabHelper*)helper {
  [self.repostFormCoordinator stop];
  self.repostFormCoordinator = nil;
}

#pragma mark - ToolbarAccessoryCoordinatorDelegate

- (void)toolbarAccessoryCoordinatorDidDismissUI:
    (ChromeCoordinator*)coordinator {
  if (!self.nextToolbarCoordinator) {
    return;
  }
  if (self.nextToolbarCoordinator == self.findBarCoordinator) {
    [self openFindInPage];
    self.nextToolbarCoordinator = nil;
  } else if (self.nextToolbarCoordinator == self.textZoomCoordinator) {
    [self openTextZoom];
    self.nextToolbarCoordinator = nil;
  }
}

#pragma mark - TextZoomCommands

- (void)openTextZoom {
  self.textZoomCoordinator = [[TextZoomCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.textZoomCoordinator.presenter =
      self.viewController.toolbarAccessoryPresenter;
  self.textZoomCoordinator.delegate = self;

  if (self.viewController.toolbarAccessoryPresenter.isPresenting) {
    self.nextToolbarCoordinator = self.textZoomCoordinator;
    [self closeFindInPage];
    return;
  }

  [self.textZoomCoordinator start];
}

- (void)closeTextZoom {
  if (!base::FeatureList::IsEnabled(web::kWebPageTextAccessibility)) {
    return;
  }

  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (currentWebState) {
    FontSizeTabHelper* fontSizeTabHelper =
        FontSizeTabHelper::FromWebState(currentWebState);
    fontSizeTabHelper->SetTextZoomUIActive(false);
  }
  [self.textZoomCoordinator stop];
}

- (void)showTextZoomUIIfActive {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return;
  }

  FontSizeTabHelper* fontSizeTabHelper =
      FontSizeTabHelper::FromWebState(currentWebState);
  if (fontSizeTabHelper && fontSizeTabHelper->IsTextZoomUIActive() &&
      !self.textZoomCoordinator.presenter.isPresenting) {
    [self.textZoomCoordinator start];
  }
}

- (void)hideTextZoomUI {
  [self.textZoomCoordinator stop];
}

#pragma mark - URLLoadingServiceDelegate

- (void)animateOpenBackgroundTabFromParams:(const UrlLoadParams&)params
                                completion:(void (^)())completion {
  [self.viewController
      animateOpenBackgroundTabFromOriginPoint:params.origin_point
                                   completion:completion];
}

// TODO(crbug.com/906525) : Move WebStateListObserving out of
// BrowserCoordinator.
#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [self installDelegatesForWebState:webState];
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  [self uninstallDelegatesForWebState:oldWebState];
  [self installDelegatesForWebState:newWebState];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  [self uninstallDelegatesForWebState:webState];
}

// TODO(crbug.com/906525) : Move out of BrowserCoordinator along with
// WebStateListObserving.
#pragma mark - Private WebState management methods

// Adds observer for WebStateList.
- (void)addWebStateListObserver {
  _webStateListObserverBridge =
      std::make_unique<WebStateListObserverBridge>(self);
  _scopedWebStateListObserver =
      std::make_unique<ScopedObserver<WebStateList, WebStateListObserver>>(
          _webStateListObserverBridge.get());
  _scopedWebStateListObserver->Add(self.browser->GetWebStateList());
}

// Removes observer for WebStateList.
- (void)removeWebStateListObserver {
  _scopedWebStateListObserver.reset();
  _webStateListObserverBridge.reset();
}

// Installs delegates for each WebState in WebStateList.
- (void)installDelegatesForAllWebStates {
  self.openInMediator = [[OpenInMediator alloc] initWithBrowser:self.browser];

  for (int i = 0; i < self.browser->GetWebStateList()->count(); i++) {
    web::WebState* webState = self.browser->GetWebStateList()->GetWebStateAt(i);
    [self installDelegatesForWebState:webState];
  }
}

// Installs delegates for self.browser.
- (void)installDelegatesForBrowser {
  UrlLoadingBrowserAgent* loadingAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  if (loadingAgent) {
    loadingAgent->SetDelegate(self);
  }
}

// Uninstalls delegates for self.browser.
- (void)uninstallDelegatesForBrowser {
  UrlLoadingBrowserAgent* loadingAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  if (loadingAgent) {
    loadingAgent->SetDelegate(nil);
  }
}

// Uninstalls delegates for each WebState in WebStateList.
- (void)uninstallDelegatesForAllWebStates {
  // OpenInMediator is controlled directly monitors the webStateList and should
  // be deleted.
  self.openInMediator = nil;
  for (int i = 0; i < self.browser->GetWebStateList()->count(); i++) {
    web::WebState* webState = self.browser->GetWebStateList()->GetWebStateAt(i);
    [self uninstallDelegatesForWebState:webState];
  }
}

// Install delegates for |webState|.
- (void)installDelegatesForWebState:(web::WebState*)webState {
  if (AutofillTabHelper::FromWebState(webState)) {
    AutofillTabHelper::FromWebState(webState)->SetBaseViewController(
        self.viewController);
  }

  PassKitTabHelper::CreateForWebState(webState, self.passKitCoordinator);

  if (PrintTabHelper::FromWebState(webState)) {
    PrintTabHelper::FromWebState(webState)->set_printer(self.printController);
  }

  RepostFormTabHelper::CreateForWebState(webState, self);

  if (StoreKitTabHelper::FromWebState(webState)) {
    StoreKitTabHelper::FromWebState(webState)->SetLauncher(
        self.storeKitCoordinator);
  }
}

// Uninstalls delegates for |webState|.
- (void)uninstallDelegatesForWebState:(web::WebState*)webState {
  if (PrintTabHelper::FromWebState(webState)) {
    PrintTabHelper::FromWebState(webState)->set_printer(nil);
  }

  if (StoreKitTabHelper::FromWebState(webState)) {
    StoreKitTabHelper::FromWebState(webState)->SetLauncher(nil);
  }
}

@end
