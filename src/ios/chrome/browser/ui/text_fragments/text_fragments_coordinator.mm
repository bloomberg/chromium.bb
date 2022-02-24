// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/text_fragments/text_fragments_coordinator.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "components/shared_highlighting/ios/shared_highlighting_constants.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/share_highlight_command.h"
#import "ios/chrome/browser/ui/text_fragments/text_fragments_mediator.h"
#import "ios/chrome/browser/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_dependency_installer_bridge.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/text_fragments/text_fragments_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TextFragmentsCoordinator () <DependencyInstalling,
                                        TextFragmentsDelegate,
                                        CRWWebStateObserver>

@property(nonatomic, strong, readonly) TextFragmentsMediator* mediator;

@property(nonatomic, strong) ActionSheetCoordinator* actionSheet;

@end

@implementation TextFragmentsCoordinator {
  // Bridge which observes WebStateList and alerts this coordinator when this
  // needs to register the Mediator with a new WebState.
  std::unique_ptr<WebStateDependencyInstallerBridge> _dependencyInstallerBridge;

  // Used to observe the active WebState
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<ActiveWebStateObservationForwarder> _forwarder;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  if (self = [super initWithBaseViewController:baseViewController
                                       browser:browser]) {
    _mediator = [[TextFragmentsMediator alloc] initWithConsumer:self];
    _dependencyInstallerBridge =
        std::make_unique<WebStateDependencyInstallerBridge>(
            self, browser->GetWebStateList());
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _forwarder = std::make_unique<ActiveWebStateObservationForwarder>(
        browser->GetWebStateList(), _webStateObserverBridge.get());
  }
  return self;
}

#pragma mark - TextFragmentsDelegate methods

- (void)userTappedTextFragmentInWebState:(web::WebState*)webState {
}

- (void)userTappedTextFragmentInWebState:(web::WebState*)webState
                              withSender:(CGRect)rect
                                withText:(NSString*)text {
  self.actionSheet = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:[self baseViewController]
                         browser:[self browser]
                           title:l10n_util::GetNSString(
                                     IDS_IOS_SHARED_HIGHLIGHT_MENU_TITLE)
                         message:nil
                            rect:rect
                            view:[self.baseViewController view]];

  __weak TextFragmentsCoordinator* weakSelf = self;
  [self.actionSheet
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_SHARED_HIGHLIGHT_REMOVE)
                action:^{
                  [weakSelf.mediator removeTextFragmentsInWebState:webState];
                }
                 style:UIAlertActionStyleDestructive];
  [self.actionSheet
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_SHARED_HIGHLIGHT_RESHARE)
                action:^{
                  id<ActivityServiceCommands> handler = HandlerForProtocol(
                      weakSelf.browser->GetCommandDispatcher(),
                      ActivityServiceCommands);

                  auto* webState =
                      weakSelf.browser->GetWebStateList()->GetActiveWebState();

                  ShareHighlightCommand* command =
                      [[ShareHighlightCommand alloc]
                           initWithURL:webState->GetLastCommittedURL()
                                 title:base::SysUTF16ToNSString(
                                           webState->GetTitle())
                          selectedText:text
                            sourceView:webState->GetView()
                            sourceRect:rect];

                  [handler shareHighlight:command];
                }
                 style:UIAlertActionStyleDefault];
  [self.actionSheet
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_SHARED_HIGHLIGHT_LEARN_MORE)
                action:^{
                  id<ApplicationCommands> handler = HandlerForProtocol(
                      weakSelf.browser->GetCommandDispatcher(),
                      ApplicationCommands);
                  [handler openURLInNewTab:[OpenNewTabCommand
                                               commandWithURLFromChrome:
                                                   GURL(shared_highlighting::
                                                            kLearnMoreUrl)]];
                }
                 style:UIAlertActionStyleDefault];
  [self.actionSheet start];
}

#pragma mark - DependencyInstalling methods

- (void)installDependencyForWebState:(web::WebState*)webState {
  [self.mediator registerWithWebState:webState];
}

#pragma mark - ChromeCoordinator methods

- (void)stop {
  if ([self.actionSheet isVisible]) {
    [self.actionSheet stop];
  }
  // Reset this observer manually. We want this to go out of scope now, ensuring
  // it detaches before |browser| and its WebStateList get destroyed.
  _dependencyInstallerBridge.reset();
}

#pragma mark - CRWWebStateObserver methods

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigationContext {
  if ([self.actionSheet isVisible]) {
    [self.actionSheet stop];
  }
}

@end
