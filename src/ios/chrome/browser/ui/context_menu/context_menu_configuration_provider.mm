// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/context_menu_configuration_provider.h"

#include "base/ios/ios_util.h"
#include "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/policy_util.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/search_engines/search_engines_util.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_utils.h"
#import "ios/chrome/browser/ui/image_util/image_copier.h"
#import "ios/chrome/browser/ui/image_util/image_saver.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_commands.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/lens/lens_availability.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/ui/util/url_with_title.h"
#import "ios/chrome/browser/url_loading/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/context_menu/context_menu_api.h"
#include "ios/public/provider/chrome/browser/lens/lens_api.h"
#include "ios/web/common/features.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Maximum length for a context menu title formed from a URL.
const NSUInteger kContextMenuMaxURLTitleLength = 100;
// Character to append to context menut titles that are truncated.
NSString* const kContextMenuEllipsis = @"…";

}  // namespace

@interface ContextMenuConfigurationProvider ()

// Helper for saving images.
@property(nonatomic, strong) ImageSaver* imageSaver;
// Helper for copying images.
@property(nonatomic, strong) ImageCopier* imageCopier;

@property(nonatomic, assign) Browser* browser;

@property(nonatomic, weak) UIViewController* baseViewController;

@property(nonatomic, assign, readonly) web::WebState* currentWebState;

@end

@implementation ContextMenuConfigurationProvider

- (instancetype)initWithBrowser:(Browser*)browser
             baseViewController:(UIViewController*)baseViewController {
  self = [super init];
  if (self) {
    _browser = browser;
    _baseViewController = baseViewController;
    _imageSaver = [[ImageSaver alloc] initWithBrowser:self.browser];
    _imageCopier = [[ImageCopier alloc] initWithBrowser:self.browser];
  }
  return self;
}

// TODO(crbug.com/1318432): rafactor long method.
- (UIContextMenuConfiguration*)
    contextMenuConfigurationForWebState:(web::WebState*)webState
                                 params:(web::ContextMenuParams)params {
  // Reset the URL.
  _URLToLoad = GURL();

  // Prevent context menu from displaying for a tab which is no longer the
  // current one.
  if (webState != self.currentWebState) {
    return nil;
  }

  const GURL linkURL = params.link_url;
  const bool isLink = linkURL.is_valid();
  const GURL imageURL = params.src_url;
  const bool isImage = imageURL.is_valid();

  DCHECK(self.browser->GetBrowserState());
  const bool isOffTheRecord = self.browser->GetBrowserState()->IsOffTheRecord();

  const GURL& lastCommittedURL = webState->GetLastCommittedURL();
  web::Referrer referrer(lastCommittedURL, web::ReferrerPolicyDefault);

  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];
  // TODO(crbug.com/1299758) add scenario for not a link and not an image.
  MenuScenario menuScenario = isImage && isLink
                                  ? MenuScenario::kContextMenuImageLink
                                  : isImage ? MenuScenario::kContextMenuImage
                                            : MenuScenario::kContextMenuLink;

  BrowserActionFactory* actionFactory =
      [[BrowserActionFactory alloc] initWithBrowser:self.browser
                                           scenario:menuScenario];

  __weak __typeof(self) weakSelf = self;

  if (isLink) {
    _URLToLoad = linkURL;
    base::RecordAction(
        base::UserMetricsAction("MobileWebContextMenuLinkImpression"));
    if (web::UrlHasWebScheme(linkURL)) {
      // Open in New Tab.
      UrlLoadParams loadParams = UrlLoadParams::InNewTab(linkURL);
      loadParams.SetInBackground(YES);
      loadParams.in_incognito = isOffTheRecord;
      loadParams.append_to = kCurrentTab;
      loadParams.web_params.referrer = referrer;
      loadParams.origin_point = [params.view convertPoint:params.location
                                                   toView:nil];
      UIAction* openNewTab = [actionFactory actionToOpenInNewTabWithBlock:^{
        ContextMenuConfigurationProvider* strongSelf = weakSelf;
        if (!strongSelf)
          return;
        UrlLoadingBrowserAgent::FromBrowser(strongSelf.browser)
            ->Load(loadParams);
      }];
      [menuElements addObject:openNewTab];

      if (!isOffTheRecord) {
        // Open in Incognito Tab.
        UIAction* openIncognitoTab =
            [actionFactory actionToOpenInNewIncognitoTabWithURL:linkURL
                                                     completion:nil];
        [menuElements addObject:openIncognitoTab];
      }

      if (base::ios::IsMultipleScenesSupported()) {
        // Open in New Window.

        NSUserActivity* newWindowActivity = ActivityToLoadURL(
            WindowActivityContextMenuOrigin, linkURL, referrer, isOffTheRecord);
        UIAction* openNewWindow = [actionFactory
            actionToOpenInNewWindowWithActivity:newWindowActivity];

        [menuElements addObject:openNewWindow];
      }

      if (linkURL.SchemeIsHTTPOrHTTPS()) {
        NSString* innerText = params.text;
        if ([innerText length] > 0) {
          // Add to reading list.
          UIAction* addToReadingList =
              [actionFactory actionToAddToReadingListWithBlock:^{
                ContextMenuConfigurationProvider* strongSelf = weakSelf;
                if (!strongSelf)
                  return;

                id<BrowserCommands> handler = static_cast<id<BrowserCommands>>(
                    strongSelf.browser->GetCommandDispatcher());
                [handler addToReadingList:[[ReadingListAddCommand alloc]
                                              initWithURL:linkURL
                                                    title:innerText]];
              }];
          [menuElements addObject:addToReadingList];
        }
      }
    }

    // Copy Link.
    UIAction* copyLink = [actionFactory actionToCopyURL:linkURL];
    [menuElements addObject:copyLink];
  }

  if (isImage) {
    base::RecordAction(
        base::UserMetricsAction("MobileWebContextMenuImageImpression"));

    __weak UIViewController* weakBaseViewController = self.baseViewController;

    // Save Image.
    UIAction* saveImage = [actionFactory actionSaveImageWithBlock:^{
      if (!weakSelf || !weakBaseViewController)
        return;
      [weakSelf.imageSaver saveImageAtURL:imageURL
                                 referrer:referrer
                                 webState:weakSelf.currentWebState
                       baseViewController:weakBaseViewController];
    }];
    [menuElements addObject:saveImage];

    // Copy Image.
    UIAction* copyImage = [actionFactory actionCopyImageWithBlock:^{
      if (!weakSelf || !weakBaseViewController)
        return;
      [weakSelf.imageCopier copyImageAtURL:imageURL
                                  referrer:referrer
                                  webState:weakSelf.currentWebState
                        baseViewController:weakBaseViewController];
    }];
    [menuElements addObject:copyImage];

    // Open Image.
    UIAction* openImage = [actionFactory actionOpenImageWithURL:imageURL
                                                     completion:nil];
    [menuElements addObject:openImage];

    // Open Image in new tab.
    UrlLoadParams loadParams = UrlLoadParams::InNewTab(imageURL);
    loadParams.SetInBackground(YES);
    loadParams.web_params.referrer = referrer;
    loadParams.in_incognito = isOffTheRecord;
    loadParams.append_to = kCurrentTab;
    loadParams.origin_point = [params.view convertPoint:params.location
                                                 toView:nil];
    UIAction* openImageInNewTab =
        [actionFactory actionOpenImageInNewTabWithUrlLoadParams:loadParams
                                                     completion:nil];
    [menuElements addObject:openImageInNewTab];

    // Search the image using Lens if Lens is enabled and available. Otherwise
    // fall back to a standard search by image experience.
    TemplateURLService* service =
        ios::TemplateURLServiceFactory::GetForBrowserState(
            self.browser->GetBrowserState());
    __weak ContextMenuConfigurationProvider* weakSelf = self;

    const BOOL lensEnabled =
        ios::provider::IsLensSupported() &&
        base::FeatureList::IsEnabled(kUseLensToSearchForImage);
    const BOOL useLens =
        lensEnabled && search_engines::SupportsSearchImageWithLens(service);
    if (useLens) {
      UIAction* searchImageWithLensAction =
          [actionFactory actionToSearchImageUsingLensWithBlock:^{
            [weakSelf searchImageWithURL:imageURL
                               usingLens:YES
                                referrer:referrer];
          }];
      [menuElements addObject:searchImageWithLensAction];
      UMA_HISTOGRAM_ENUMERATION(kIOSLensSupportStatusHistogram,
                                LensSupportStatus::LensSearchSupported);
    } else if (lensEnabled) {
      UMA_HISTOGRAM_ENUMERATION(kIOSLensSupportStatusHistogram,
                                LensSupportStatus::NonGoogleSearchEngine);
    }

    if (!useLens && search_engines::SupportsSearchByImage(service)) {
      const TemplateURL* defaultURL = service->GetDefaultSearchProvider();
      NSString* title = l10n_util::GetNSStringF(
          IDS_IOS_CONTEXT_MENU_SEARCHWEBFORIMAGE, defaultURL->short_name());
      UIAction* searchByImage = [actionFactory
          actionSearchImageWithTitle:title
                               Block:^{
                                 [weakSelf searchImageWithURL:imageURL
                                                    usingLens:NO
                                                     referrer:referrer];
                               }];
      [menuElements addObject:searchByImage];
    }
  }

  // Insert any provided menu items. Do after Link and/or Image to allow
  // inserting at beginning or adding to end.
  ios::provider::AddContextMenuElements(
      menuElements, self.browser->GetBrowserState(), webState, params,
      self.baseViewController);

  if (menuElements.count == 0) {
    return nil;
  }

  NSString* menuTitle = nil;
  if (isLink || isImage) {
    menuTitle = GetContextMenuTitle(params);

    // Truncate context meny titles that originate from URLs, leaving text
    // titles untruncated.
    if (!IsImageTitle(params) &&
        menuTitle.length > kContextMenuMaxURLTitleLength + 1) {
      menuTitle = [[menuTitle substringToIndex:kContextMenuMaxURLTitleLength]
          stringByAppendingString:kContextMenuEllipsis];
    }
  }

  UIMenu* menu = [UIMenu menuWithTitle:menuTitle children:menuElements];

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        RecordMenuShown(menuScenario);
        return menu;
      };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

#pragma mark - Properties

- (web::WebState*)currentWebState {
  return self.browser ? self.browser->GetWebStateList()->GetActiveWebState()
                      : nullptr;
}

#pragma mark - Private

// Searches an image with the given |imageURL| and |referrer|, optionally using
// Lens.
- (void)searchImageWithURL:(GURL)imageURL
                 usingLens:(BOOL)usingLens
                  referrer:(web::Referrer)referrer {
  ImageFetchTabHelper* imageFetcher =
      ImageFetchTabHelper::FromWebState(self.currentWebState);
  DCHECK(imageFetcher);
  __weak ContextMenuConfigurationProvider* weakSelf = self;
  imageFetcher->GetImageData(imageURL, referrer, ^(NSData* data) {
    if (usingLens) {
      [weakSelf searchImageUsingLensWithData:data];
    } else {
      [weakSelf searchByImageData:data imageURL:imageURL];
    }
  });
}

// Starts a reverse image search based on |imageData| and |imageURL| in a new
// tab.
- (void)searchByImageData:(NSData*)imageData imageURL:(const GURL&)URL {
  web::NavigationManager::WebLoadParams webParams =
      ImageSearchParamGenerator::LoadParamsForImageData(
          imageData, URL,
          ios::TemplateURLServiceFactory::GetForBrowserState(
              self.browser->GetBrowserState()));

  UrlLoadParams params = UrlLoadParams::InNewTab(webParams);
  params.in_incognito = self.browser->GetBrowserState()->IsOffTheRecord();
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
}

// Searches an image with Lens using the given |imageData|.
- (void)searchImageUsingLensWithData:(NSData*)imageData {
  // TODO(crbug.com/1323783): This should be an id<LensCommands> and use
  // HandlerForProtocol().
  id<BrowserCommands> handler =
      static_cast<id<BrowserCommands>>(_browser->GetCommandDispatcher());
  UIImage* image = [UIImage imageWithData:imageData];
  SearchImageWithLensCommand* command =
      [[SearchImageWithLensCommand alloc] initWithImage:image];
  [handler searchImageWithLens:command];
}

@end
