// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/user_activity_handler.h"

#import <CoreSpotlight/CoreSpotlight.h>
#import <Intents/Intents.h>
#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/handoff/handoff_utility.h"
#include "components/search_engines/template_url_service.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#include "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/intents/OpenInChromeIncognitoIntent.h"
#import "ios/chrome/app/intents/OpenInChromeIntent.h"
#import "ios/chrome/app/intents/SearchInChromeIntent.h"
#import "ios/chrome/app/spotlight/actions_spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#include "ios/chrome/app/startup/chrome_app_startup_parameters.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#include "ios/chrome/browser/metrics/first_user_action_recorder.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/u2f/u2f_tab_helper.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/connection_information.h"
#import "ios/chrome/browser/url_loading/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {
// Constants for 3D touch application static shortcuts.
NSString* const kShortcutNewSearch = @"OpenNewSearch";
NSString* const kShortcutNewIncognitoSearch = @"OpenIncognitoSearch";
NSString* const kShortcutVoiceSearch = @"OpenVoiceSearch";
NSString* const kShortcutQRScanner = @"OpenQRScanner";

// Constants for Siri shortcut.
NSString* const kSiriShortcutOpenInChrome = @"OpenInChromeIntent";
NSString* const kSiriShortcutSearchInChrome = @"SearchInChromeIntent";
NSString* const kSiriShortcutOpenInIncognito = @"OpenInChromeIncognitoIntent";

std::vector<GURL> createGURLVectorFromIntentURLs(NSArray<NSURL*>* intentURLs) {
  std::vector<GURL> URLs;
  for (NSURL* URL in intentURLs) {
    URLs.push_back(net::GURLWithNSURL(URL));
  }
  return URLs;
}

}  // namespace

@interface UserActivityHandler ()
// Handles the 3D touch application static items. Does nothing if in first run.
+ (BOOL)handleShortcutItem:(UIApplicationShortcutItem*)shortcutItem
     connectionInformation:(id<ConnectionInformation>)connectionInformation
        startupInformation:(id<StartupInformation>)startupInformation;
// Routes Universal 2nd Factor (U2F) callback to the correct Tab.
+ (void)routeU2FURL:(const GURL&)URL
       browserState:(ChromeBrowserState*)browserState;
@end

@implementation UserActivityHandler

#pragma mark - Public methods.

+ (BOOL)continueUserActivity:(NSUserActivity*)userActivity
         applicationIsActive:(BOOL)applicationIsActive
                   tabOpener:(id<TabOpening>)tabOpener
       connectionInformation:(id<ConnectionInformation>)connectionInformation
          startupInformation:(id<StartupInformation>)startupInformation
                browserState:(ChromeBrowserState*)browserState {
  NSURL* webpageURL = userActivity.webpageURL;

  if ([userActivity.activityType
          isEqualToString:handoff::kChromeHandoffActivityType] ||
      [userActivity.activityType
          isEqualToString:NSUserActivityTypeBrowsingWeb]) {
    // App was launched by iOS as a result of Handoff.
    NSString* originString = base::mac::ObjCCast<NSString>(
        userActivity.userInfo[handoff::kOriginKey]);
    handoff::Origin origin = handoff::OriginFromString(originString);
    UMA_HISTOGRAM_ENUMERATION("IOS.Handoff.Origin", origin,
                              handoff::ORIGIN_COUNT);
  } else if (spotlight::IsSpotlightAvailable() &&
             [userActivity.activityType
                 isEqualToString:CSSearchableItemActionType]) {
    // App was launched by iOS as the result of a tap on a Spotlight Search
    // result.
    NSString* itemID =
        [userActivity.userInfo objectForKey:CSSearchableItemActivityIdentifier];
    spotlight::Domain domain = spotlight::SpotlightDomainFromString(itemID);
    UMA_HISTOGRAM_ENUMERATION("IOS.Spotlight.Origin", domain,
                              spotlight::DOMAIN_COUNT);

    if (!itemID) {
      return NO;
    }
    if (domain == spotlight::DOMAIN_ACTIONS) {
      webpageURL =
          [NSURL URLWithString:base::SysUTF8ToNSString(kChromeUINewTabURL)];
      AppStartupParameters* startupParams = [[AppStartupParameters alloc]
          initWithExternalURL:GURL(kChromeUINewTabURL)
                  completeURL:GURL(kChromeUINewTabURL)];
      BOOL startupParamsSet = spotlight::SetStartupParametersForSpotlightAction(
          itemID, startupParams);
      if (!startupParamsSet) {
        return NO;
      }
      [connectionInformation setStartupParameters:startupParams];
    } else if (!webpageURL) {
      spotlight::GetURLForSpotlightItemID(itemID, ^(NSURL* contentURL) {
        if (!contentURL) {
          return;
        }
        dispatch_async(dispatch_get_main_queue(), ^{
          // Update the isActive flag as it may have changed during the async
          // calls.
          BOOL isActive = [[UIApplication sharedApplication]
                              applicationState] == UIApplicationStateActive;
          [self continueUserActivityURL:contentURL
                    applicationIsActive:isActive
                              tabOpener:tabOpener
                  connectionInformation:connectionInformation
                     startupInformation:startupInformation
                           browserState:browserState];
        });
      });
      return YES;
    }
  } else if ([userActivity.activityType
                 isEqualToString:kSiriShortcutSearchInChrome]) {
    base::RecordAction(UserMetricsAction("IOSLaunchedBySearchInChromeIntent"));

    AppStartupParameters* startupParams = [[AppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
                completeURL:GURL(kChromeUINewTabURL)];

    SearchInChromeIntent* intent =
        base::mac::ObjCCastStrict<SearchInChromeIntent>(
            userActivity.interaction.intent);

    if (!intent) {
      return NO;
    }

    id searchPhrase = [intent valueForKey:@"searchPhrase"];

    if ([searchPhrase isKindOfClass:[NSString class]] &&
        [searchPhrase
            stringByTrimmingCharactersInSet:[NSCharacterSet
                                                whitespaceCharacterSet]]
                .length > 0) {
      startupParams.textQuery = searchPhrase;
    } else {
      startupParams.postOpeningAction = FOCUS_OMNIBOX;
    }

    [connectionInformation setStartupParameters:startupParams];
    webpageURL =
        [NSURL URLWithString:base::SysUTF8ToNSString(kChromeUINewTabURL)];

  } else if ([userActivity.activityType
                 isEqualToString:kSiriShortcutOpenInChrome]) {
    base::RecordAction(UserMetricsAction("IOSLaunchedByOpenInChromeIntent"));
    OpenInChromeIntent* intent = base::mac::ObjCCastStrict<OpenInChromeIntent>(
        userActivity.interaction.intent);

    if (!intent.url) {
      return NO;
    }

    std::vector<GURL> URLs;

    if ([intent.url isKindOfClass:[NSURL class]]) {
      // Old intent version where |url| is of type NSURL rather than an array.
      GURL webpageGURL(
          net::GURLWithNSURL(base::mac::ObjCCastStrict<NSURL>(intent.url)));
      if (!webpageGURL.is_valid())
        return NO;
      URLs.push_back(webpageGURL);
    } else if ([intent.url isKindOfClass:[NSArray class]] &&
               intent.url.count > 0) {
      URLs = createGURLVectorFromIntentURLs(intent.url);
    } else {
      // Unknown or invalid intent object.
      return NO;
    }

    AppStartupParameters* startupParams =
        [[AppStartupParameters alloc] initWithURLs:URLs];

    [connectionInformation setStartupParameters:startupParams];
    return [self continueUserActivityURLs:URLs
                      applicationIsActive:applicationIsActive
                                tabOpener:tabOpener
                    connectionInformation:connectionInformation
                       startupInformation:startupInformation
                                Incognito:NO];

  } else if ([userActivity.activityType
                 isEqualToString:kSiriShortcutOpenInIncognito]) {
    base::RecordAction(UserMetricsAction("IOSLaunchedByOpenInIncognitoIntent"));
    OpenInChromeIncognitoIntent* intent =
        base::mac::ObjCCastStrict<OpenInChromeIncognitoIntent>(
            userActivity.interaction.intent);

    if (!intent.url || intent.url.count == 0) {
      return NO;
    }

    std::vector<GURL> URLs = createGURLVectorFromIntentURLs(intent.url);

    AppStartupParameters* startupParams =
        [[AppStartupParameters alloc] initWithURLs:URLs];

    startupParams.launchInIncognito = YES;
    [connectionInformation setStartupParameters:startupParams];
    return [self continueUserActivityURLs:URLs
                      applicationIsActive:applicationIsActive
                                tabOpener:tabOpener
                    connectionInformation:connectionInformation
                       startupInformation:startupInformation
                                Incognito:YES];

  } else {
    // Do nothing for unknown activity type.
    return NO;
  }

  return [self continueUserActivityURL:webpageURL
                   applicationIsActive:applicationIsActive
                             tabOpener:tabOpener
                 connectionInformation:connectionInformation
                    startupInformation:startupInformation
                          browserState:browserState];
}

+ (BOOL)continueUserActivityURL:(NSURL*)webpageURL
            applicationIsActive:(BOOL)applicationIsActive
                      tabOpener:(id<TabOpening>)tabOpener
          connectionInformation:(id<ConnectionInformation>)connectionInformation
             startupInformation:(id<StartupInformation>)startupInformation
                   browserState:(ChromeBrowserState*)browserState {
  if (!webpageURL)
    return NO;

  GURL webpageGURL(net::GURLWithNSURL(webpageURL));
  if (!webpageGURL.is_valid())
    return NO;

  if (applicationIsActive && ![startupInformation isPresentingFirstRunUI]) {
    // The app is already active so the applicationDidBecomeActive: method will
    // never be called. Open the requested URL immediately.
    ApplicationModeForTabOpening targetMode =
        [[connectionInformation startupParameters] applicationMode];
    UrlLoadParams params = UrlLoadParams::InNewTab(webpageGURL);

    if (connectionInformation.startupParameters.textQuery) {
      NSString* query = connectionInformation.startupParameters.textQuery;

      GURL result = [self generateResultGURLFromSearchQuery:query
                                               browserState:browserState];
      params.web_params.url = result;
    }

    if (![[connectionInformation startupParameters] launchInIncognito] &&
        [tabOpener URLIsOpenedInRegularMode:webpageGURL]) {
      // Record metric.
    }
    [tabOpener dismissModalsAndOpenSelectedTabInMode:targetMode
                                   withUrlLoadParams:params
                                      dismissOmnibox:YES
                                          completion:^{
                                            [connectionInformation
                                                setStartupParameters:nil];
                                          }];
    return YES;
  }

  // Don't record the first action as a user action, since it will not be
  // initiated by the user.
  [startupInformation resetFirstUserActionRecorder];

  if (![connectionInformation startupParameters]) {
    AppStartupParameters* startupParams =
        [[AppStartupParameters alloc] initWithExternalURL:webpageGURL
                                              completeURL:webpageGURL];
    [connectionInformation setStartupParameters:startupParams];
  }
  return YES;
}

+ (void)openMultipleTabsWithConnectionInformation:
            (id<ConnectionInformation>)connectionInformation
                                        tabOpener:(id<TabOpening>)tabOpener {
  ApplicationModeForTabOpening mode =
      connectionInformation.startupParameters.launchInIncognito
          ? ApplicationModeForTabOpening::INCOGNITO
          : ApplicationModeForTabOpening::NORMAL;

  BOOL dismissOmnibox = [[connectionInformation startupParameters]
                            postOpeningAction] != FOCUS_OMNIBOX;

  // Using a weak reference to |connectionInformation| to solve a memory leak
  // issue. |tabOpener| and |connectionInformation| are the same object in
  // some cases (SceneController). This retains the object while the block
  // exists. Then this block is passed around and in some cases it ends up
  // stored in BrowserViewController. This results in a memory leak that looks
  // like this: SceneController -> BrowserViewWrangler -> BrowserCoordinator
  // -> BrowserViewController -> SceneController
  __weak id<ConnectionInformation> weakConnectionInfo = connectionInformation;

  [tabOpener
      dismissModalsAndOpenMultipleTabsInMode:mode
                                        URLs:weakConnectionInfo
                                                 .startupParameters.URLs
                              dismissOmnibox:dismissOmnibox
                                  completion:^{
                                    weakConnectionInfo.startupParameters = nil;
                                  }];
}

+ (BOOL)continueUserActivityURLs:(const std::vector<GURL>&)webpageURLs
             applicationIsActive:(BOOL)applicationIsActive
                       tabOpener:(id<TabOpening>)tabOpener
           connectionInformation:
               (id<ConnectionInformation>)connectionInformation
              startupInformation:(id<StartupInformation>)startupInformation
                       Incognito:(BOOL)Incognito {
  if (applicationIsActive && ![startupInformation isPresentingFirstRunUI]) {
    // The app is already active so the applicationDidBecomeActive: method will
    // never be called. Open the requested URLs immediately.
    [self openMultipleTabsWithConnectionInformation:connectionInformation
                                          tabOpener:tabOpener];
    return YES;
  }

  // Don't record the first action as a user action, since it will not be
  // initiated by the user.
  [startupInformation resetFirstUserActionRecorder];

  if (![connectionInformation startupParameters]) {
    AppStartupParameters* startupParams =
        [[AppStartupParameters alloc] initWithURLs:webpageURLs];

    startupParams.launchInIncognito = Incognito;
    [connectionInformation setStartupParameters:startupParams];
  }
  return YES;
}

+ (void)performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                   completionHandler:(void (^)(BOOL succeeded))completionHandler
                           tabOpener:(id<TabOpening>)tabOpener
               connectionInformation:
                   (id<ConnectionInformation>)connectionInformation
                  startupInformation:(id<StartupInformation>)startupInformation
                   interfaceProvider:
                       (id<BrowserInterfaceProvider>)interfaceProvider {
  BOOL handledShortcutItem =
      [UserActivityHandler handleShortcutItem:shortcutItem
                        connectionInformation:connectionInformation
                           startupInformation:startupInformation];
  BOOL isActive = [[UIApplication sharedApplication] applicationState] ==
                  UIApplicationStateActive;
  if (handledShortcutItem && isActive) {
    [UserActivityHandler
        handleStartupParametersWithTabOpener:tabOpener
                       connectionInformation:connectionInformation
                          startupInformation:startupInformation
                                browserState:interfaceProvider.currentInterface
                                                 .browserState];
  }
  if (completionHandler) {
    completionHandler(handledShortcutItem);
  }
}

+ (BOOL)willContinueUserActivityWithType:(NSString*)userActivityType {
  return
      [userActivityType isEqualToString:handoff::kChromeHandoffActivityType] ||
      (spotlight::IsSpotlightAvailable() &&
       [userActivityType isEqualToString:CSSearchableItemActionType]);
}

+ (GURL)generateResultGURLFromSearchQuery:(NSString*)searchQuery
                             browserState:(ChromeBrowserState*)browserState {
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(browserState);

  const TemplateURL* defaultURL =
      templateURLService->GetDefaultSearchProvider();
  DCHECK(!defaultURL->url().empty());
  DCHECK(
      defaultURL->url_ref().IsValid(templateURLService->search_terms_data()));
  base::string16 queryString = base::SysNSStringToUTF16(searchQuery);
  TemplateURLRef::SearchTermsArgs search_args(queryString);

  GURL result(defaultURL->url_ref().ReplaceSearchTerms(
      search_args, templateURLService->search_terms_data()));

  return result;
}

+ (void)handleStartupParametersWithTabOpener:(id<TabOpening>)tabOpener
                       connectionInformation:
                           (id<ConnectionInformation>)connectionInformation
                          startupInformation:
                              (id<StartupInformation>)startupInformation
                                browserState:(ChromeBrowserState*)browserState {
  // Do not load the external URL if the user has not accepted the terms of
  // service. This corresponds to the case when the user installed Chrome,
  // has never launched it and attempts to open an external URL in Chrome.
  if ([startupInformation isPresentingFirstRunUI]) {
    return;
  }

  if (!connectionInformation.startupParameters.URLs.empty()) {
    [self openMultipleTabsWithConnectionInformation:connectionInformation
                                          tabOpener:tabOpener];
    return;
  }

  GURL externalURL = connectionInformation.startupParameters.externalURL;
  // Check if it's an U2F call. If so, route it to correct tab.
  // If not, open or reuse tab in main BVC.
  if (U2FTabHelper::IsU2FUrl(externalURL)) {
    [UserActivityHandler routeU2FURL:externalURL browserState:browserState];
    // It's OK to clear startup parameters here because routeU2FURL works
    // synchronously.
    [connectionInformation setStartupParameters:nil];
  } else {
    // TODO(crbug.com/935019): Exacly the same copy of this code is present in
    // +[URLOpener
    // openURL:applicationActive:options:tabOpener:startupInformation:]

    // The app is already active so the applicationDidBecomeActive: method
    // will never be called. Open the requested URL after all modal UIs have
    // been dismissed. |_startupParameters| must be retained until all deferred
    // modal UIs are dismissed and tab opened with requested URL.
    ApplicationModeForTabOpening targetMode =
        [[connectionInformation startupParameters] applicationMode];
    GURL URL;
    GURL virtualURL;
    GURL completeURL = connectionInformation.startupParameters.completeURL;
    if (completeURL.SchemeIsFile()) {
      // External URL will be loaded by WebState, which expects |completeURL|.
      // Omnibox however suppose to display |externalURL|, which is used as
      // virtual URL.
      URL = completeURL;
      virtualURL = externalURL;
    } else {
      URL = externalURL;
    }
    UrlLoadParams params = UrlLoadParams::InNewTab(URL, virtualURL);

    if (connectionInformation.startupParameters.imageSearchData) {
      TemplateURLService* templateURLService =
          ios::TemplateURLServiceFactory::GetForBrowserState(browserState);

      NSData* imageData =
          connectionInformation.startupParameters.imageSearchData;
      web::NavigationManager::WebLoadParams webLoadParams =
          ImageSearchParamGenerator::LoadParamsForImageData(imageData, GURL(),
                                                            templateURLService);

      params.web_params = webLoadParams;
    } else if (connectionInformation.startupParameters.textQuery) {
      NSString* query = connectionInformation.startupParameters.textQuery;

      GURL result = [self generateResultGURLFromSearchQuery:query
                                               browserState:browserState];
      params.web_params.url = result;
    }

    if (![[connectionInformation startupParameters] launchInIncognito] &&
        [tabOpener URLIsOpenedInRegularMode:params.web_params.url]) {
      // Record metric.
    }

    [tabOpener dismissModalsAndOpenSelectedTabInMode:targetMode
                                   withUrlLoadParams:params
                                      dismissOmnibox:[[connectionInformation
                                                         startupParameters]
                                                         postOpeningAction] !=
                                                     FOCUS_OMNIBOX
                                          completion:^{
                                            [connectionInformation
                                                setStartupParameters:nil];
                                          }];
  }
}

#pragma mark - Internal methods.

+ (BOOL)handleShortcutItem:(UIApplicationShortcutItem*)shortcutItem
     connectionInformation:(id<ConnectionInformation>)connectionInformation
        startupInformation:(id<StartupInformation>)startupInformation {
  if ([startupInformation isPresentingFirstRunUI])
    return NO;

  AppStartupParameters* startupParams = [[AppStartupParameters alloc]
      initWithExternalURL:GURL(kChromeUINewTabURL)
              completeURL:GURL(kChromeUINewTabURL)];

  if ([shortcutItem.type isEqualToString:kShortcutNewSearch]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.NewSearchPressed"));
    startupParams.postOpeningAction = FOCUS_OMNIBOX;
    connectionInformation.startupParameters = startupParams;
    return YES;

  } else if ([shortcutItem.type isEqualToString:kShortcutNewIncognitoSearch]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.NewIncognitoSearchPressed"));
    startupParams.launchInIncognito = YES;
    startupParams.postOpeningAction = FOCUS_OMNIBOX;
    connectionInformation.startupParameters = startupParams;
    return YES;

  } else if ([shortcutItem.type isEqualToString:kShortcutVoiceSearch]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.VoiceSearchPressed"));
    startupParams.postOpeningAction = START_VOICE_SEARCH;
    connectionInformation.startupParameters = startupParams;
    return YES;

  } else if ([shortcutItem.type isEqualToString:kShortcutQRScanner]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.ScanQRCodePressed"));
    startupParams.postOpeningAction = START_QR_CODE_SCANNER;
    connectionInformation.startupParameters = startupParams;
    return YES;
  }

  NOTREACHED();
  return NO;
}

+ (void)routeU2FURL:(const GURL&)URL
       browserState:(ChromeBrowserState*)browserState {
  DCHECK(browserState);
  // Retrieve the designated TabID from U2F URL.
  NSString* tabID = U2FTabHelper::GetTabIdFromU2FUrl(URL);
  if (!tabID) {
    return;
  }

  // Iterate through regular Browser and OTR Browser to find the corresponding
  // tab.
  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(browserState);
  std::set<Browser*> regularBrowsers = browserList->AllRegularBrowsers();
  std::set<Browser*> otrBrowsers = browserList->AllIncognitoBrowsers();
  std::set<Browser*> browsers(regularBrowsers);
  browsers.insert(otrBrowsers.begin(), otrBrowsers.end());

  for (Browser* browser : browsers) {
    WebStateList* webStateList = browser->GetWebStateList();
    for (int index = 0; index < webStateList->count(); ++index) {
      web::WebState* webState = webStateList->GetWebStateAt(index);
      NSString* currentTabID = TabIdTabHelper::FromWebState(webState)->tab_id();
      if ([currentTabID isEqualToString:tabID]) {
        U2FTabHelper::FromWebState(webState)->EvaluateU2FResult(URL);
        return;
      }
    }
  }
}

@end
