// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_view_controller.h"

#import <MobileCoreServices/MobileCoreServices.h>

#import "ios/web_view/shell/shell_auth_service.h"
#import "ios/web_view/shell/shell_autofill_delegate.h"
#import "ios/web_view/shell/shell_translation_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Externed accessibility identifier.
NSString* const kWebViewShellBackButtonAccessibilityLabel = @"Back";
NSString* const kWebViewShellForwardButtonAccessibilityLabel = @"Forward";
NSString* const kWebViewShellAddressFieldAccessibilityLabel = @"Address field";
NSString* const kWebViewShellJavaScriptDialogTextFieldAccessibiltyIdentifier =
    @"WebViewShellJavaScriptDialogTextFieldAccessibiltyIdentifier";

@interface ShellViewController ()<CWVDownloadTaskDelegate,
                                  CWVNavigationDelegate,
                                  CWVUIDelegate,
                                  CWVScriptCommandHandler,
                                  CWVSyncControllerDelegate,
                                  UITextFieldDelegate>
// Header containing navigation buttons and |field|.
@property(nonatomic, strong) UIView* headerBackgroundView;
// Header containing navigation buttons and |field|.
@property(nonatomic, strong) UIView* headerContentView;
// Button to navigate backwards.
@property(nonatomic, strong) UIButton* backButton;
// Button to navigate forwards.
@property(nonatomic, strong) UIButton* forwardButton;
// Button that either refresh the page or stops the page load.
@property(nonatomic, strong) UIButton* reloadOrStopButton;
// Button that shows the menu
@property(nonatomic, strong) UIButton* menuButton;
// Text field used for navigating to URLs.
@property(nonatomic, strong) UITextField* field;
// Container for |webView|.
@property(nonatomic, strong) UIView* contentView;
// Handles the autofill of the content displayed in |webView|.
@property(nonatomic, strong) ShellAutofillDelegate* autofillDelegate;
// Handles the translation of the content displayed in |webView|.
@property(nonatomic, strong) ShellTranslationDelegate* translationDelegate;
// The on-going download task if any.
@property(nonatomic, strong, nullable) CWVDownloadTask* downloadTask;
// The path to a local file which the download task is writing to.
@property(nonatomic, strong, nullable) NSString* downloadFilePath;
// A controller to show a "Share" menu for the downloaded file.
@property(nonatomic, strong, nullable)
    UIDocumentInteractionController* documentInteractionController;
@property(nonatomic, strong) ShellAuthService* authService;

- (void)back;
- (void)forward;
- (void)reloadOrStop;
// Disconnects and release the |webView|.
- (void)removeWebView;
// Resets translate settings back to default.
- (void)resetTranslateSettings;
@end

@implementation ShellViewController

@synthesize autofillDelegate = _autofillDelegate;
@synthesize backButton = _backButton;
@synthesize contentView = _contentView;
@synthesize field = _field;
@synthesize forwardButton = _forwardButton;
@synthesize reloadOrStopButton = _reloadOrStopButton;
@synthesize menuButton = _menuButton;
@synthesize headerBackgroundView = _headerBackgroundView;
@synthesize headerContentView = _headerContentView;
@synthesize webView = _webView;
@synthesize translationDelegate = _translationDelegate;
@synthesize downloadTask = _downloadTask;
@synthesize downloadFilePath = _downloadFilePath;
@synthesize documentInteractionController = _documentInteractionController;
@synthesize authService = _authService;

- (void)viewDidLoad {
  [super viewDidLoad];

  // View creation.
  self.headerBackgroundView = [[UIView alloc] init];
  self.headerContentView = [[UIView alloc] init];
  self.contentView = [[UIView alloc] init];
  self.backButton = [[UIButton alloc] init];
  self.forwardButton = [[UIButton alloc] init];
  self.reloadOrStopButton = [[UIButton alloc] init];
  self.menuButton = [[UIButton alloc] init];
  self.field = [[UITextField alloc] init];

  // View hierarchy.
  [self.view addSubview:_headerBackgroundView];
  [self.view addSubview:_contentView];
  [_headerBackgroundView addSubview:_headerContentView];
  [_headerContentView addSubview:_backButton];
  [_headerContentView addSubview:_forwardButton];
  [_headerContentView addSubview:_reloadOrStopButton];
  [_headerContentView addSubview:_menuButton];
  [_headerContentView addSubview:_field];

  // Additional view setup.
  _headerBackgroundView.backgroundColor = [UIColor colorWithRed:66.0 / 255.0
                                                          green:133.0 / 255.0
                                                           blue:244.0 / 255.0
                                                          alpha:1.0];

  [_backButton setImage:[UIImage imageNamed:@"ic_back"]
               forState:UIControlStateNormal];
  _backButton.tintColor = [UIColor whiteColor];
  [_backButton addTarget:self
                  action:@selector(back)
        forControlEvents:UIControlEventTouchUpInside];
  [_backButton setAccessibilityLabel:kWebViewShellBackButtonAccessibilityLabel];

  [_forwardButton setImage:[UIImage imageNamed:@"ic_forward"]
                  forState:UIControlStateNormal];
  _forwardButton.tintColor = [UIColor whiteColor];
  [_forwardButton addTarget:self
                     action:@selector(forward)
           forControlEvents:UIControlEventTouchUpInside];
  [_forwardButton
      setAccessibilityLabel:kWebViewShellForwardButtonAccessibilityLabel];

  _reloadOrStopButton.tintColor = [UIColor whiteColor];
  [_reloadOrStopButton addTarget:self
                          action:@selector(reloadOrStop)
                forControlEvents:UIControlEventTouchUpInside];

  _menuButton.tintColor = [UIColor whiteColor];
  [_menuButton setImage:[UIImage imageNamed:@"ic_menu"]
               forState:UIControlStateNormal];
  [_menuButton addTarget:self
                  action:@selector(showMenu)
        forControlEvents:UIControlEventTouchUpInside];

  _field.placeholder = @"Search or type URL";
  _field.backgroundColor = [UIColor whiteColor];
  _field.tintColor = _headerBackgroundView.backgroundColor;
  [_field setContentHuggingPriority:UILayoutPriorityDefaultLow - 1
                            forAxis:UILayoutConstraintAxisHorizontal];
  _field.delegate = self;
  _field.layer.cornerRadius = 2.0;
  _field.keyboardType = UIKeyboardTypeURL;
  _field.autocapitalizationType = UITextAutocapitalizationTypeNone;
  _field.clearButtonMode = UITextFieldViewModeWhileEditing;
  _field.autocorrectionType = UITextAutocorrectionTypeNo;
  UIView* spacerView = [[UIView alloc] init];
  spacerView.frame = CGRectMake(0, 0, 8, 8);
  _field.leftViewMode = UITextFieldViewModeAlways;
  _field.leftView = spacerView;

  // Constraints.
  _headerBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_headerBackgroundView.topAnchor
        constraintEqualToAnchor:self.view.topAnchor],
    [_headerBackgroundView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_headerBackgroundView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_headerBackgroundView.bottomAnchor
        constraintEqualToAnchor:_headerContentView.bottomAnchor],
  ]];

  _headerContentView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_headerContentView.topAnchor
        constraintEqualToAnchor:_headerBackgroundView.safeAreaLayoutGuide
                                    .topAnchor],
    [_headerContentView.leadingAnchor
        constraintEqualToAnchor:_headerBackgroundView.safeAreaLayoutGuide
                                    .leadingAnchor],
    [_headerContentView.trailingAnchor
        constraintEqualToAnchor:_headerBackgroundView.safeAreaLayoutGuide
                                    .trailingAnchor],
    [_headerContentView.heightAnchor constraintEqualToConstant:56.0],
  ]];

  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_contentView.topAnchor
        constraintEqualToAnchor:_headerBackgroundView.bottomAnchor],
    [_contentView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_contentView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_contentView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
  ]];

  _backButton.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_backButton.leadingAnchor
        constraintEqualToAnchor:_headerContentView.safeAreaLayoutGuide
                                    .leadingAnchor
                       constant:16.0],
    [_backButton.centerYAnchor
        constraintEqualToAnchor:_headerContentView.centerYAnchor],
  ]];

  _forwardButton.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_forwardButton.leadingAnchor
        constraintEqualToAnchor:_backButton.trailingAnchor
                       constant:16.0],
    [_forwardButton.centerYAnchor
        constraintEqualToAnchor:_headerContentView.centerYAnchor],
  ]];

  _reloadOrStopButton.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_reloadOrStopButton.leadingAnchor
        constraintEqualToAnchor:_forwardButton.trailingAnchor
                       constant:16.0],
    [_reloadOrStopButton.centerYAnchor
        constraintEqualToAnchor:_headerContentView.centerYAnchor],
  ]];
  _menuButton.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_menuButton.leadingAnchor
        constraintEqualToAnchor:_reloadOrStopButton.trailingAnchor
                       constant:16.0],
    [_menuButton.centerYAnchor
        constraintEqualToAnchor:_headerContentView.centerYAnchor],
  ]];

  _field.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_field.leadingAnchor constraintEqualToAnchor:_menuButton.trailingAnchor
                                         constant:16.0],
    [_field.centerYAnchor
        constraintEqualToAnchor:_headerContentView.centerYAnchor],
    [_field.trailingAnchor
        constraintEqualToAnchor:_headerContentView.safeAreaLayoutGuide
                                    .trailingAnchor
                       constant:-16.0],
    [_field.heightAnchor constraintEqualToConstant:32.0],
  ]];

  [CWVWebView setUserAgentProduct:@"Dummy/1.0"];

  CWVWebViewConfiguration* configuration =
      [CWVWebViewConfiguration defaultConfiguration];
  configuration.syncController.delegate = self;
  [self createWebViewWithConfiguration:configuration];

  _authService = [[ShellAuthService alloc] init];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:@"canGoBack"]) {
    _backButton.enabled = [_webView canGoBack];
  } else if ([keyPath isEqualToString:@"canGoForward"]) {
    _forwardButton.enabled = [_webView canGoForward];
  } else if ([keyPath isEqualToString:@"loading"]) {
    NSString* imageName = _webView.loading ? @"ic_stop" : @"ic_reload";
    [_reloadOrStopButton setImage:[UIImage imageNamed:imageName]
                         forState:UIControlStateNormal];
  }
}

- (void)back {
  if ([_webView canGoBack]) {
    [_webView goBack];
  }
}

- (void)forward {
  if ([_webView canGoForward]) {
    [_webView goForward];
  }
}

- (void)reloadOrStop {
  if (_webView.loading) {
    [_webView stopLoading];
  } else {
    [_webView reload];
  }
}

- (void)showMenu {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:nil
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                         style:UIAlertActionStyleCancel
                                       handler:nil]];

  __weak ShellViewController* weakSelf = self;

  // Toggles the incognito mode.
  NSString* incognitoActionTitle = _webView.configuration.persistent
                                       ? @"Enter incognito"
                                       : @"Exit incognito";
  [alertController
      addAction:[UIAlertAction actionWithTitle:incognitoActionTitle
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf toggleIncognito];
                                       }]];

  // Removes the web view from the view hierarchy, releases it, and recreates
  // the web view with the same configuration. This is for testing deallocation
  // and sharing configuration.
  [alertController
      addAction:[UIAlertAction
                    actionWithTitle:@"Recreate web view"
                              style:UIAlertActionStyleDefault
                            handler:^(UIAlertAction* action) {
                              CWVWebViewConfiguration* configuration =
                                  weakSelf.webView.configuration;
                              [weakSelf removeWebView];
                              [weakSelf
                                  createWebViewWithConfiguration:configuration];
                            }]];

  // Resets all translation settings to default values.
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Reset translate settings"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf resetTranslateSettings];
                                       }]];

  for (CWVIdentity* identity in [_authService identities]) {
    NSString* title =
        [NSString stringWithFormat:@"Start sync with %@", identity.email];
    [alertController
        addAction:[UIAlertAction
                      actionWithTitle:title
                                style:UIAlertActionStyleDefault
                              handler:^(UIAlertAction* action) {
                                CWVSyncController* syncController =
                                    weakSelf.webView.configuration
                                        .syncController;
                                [syncController
                                    startSyncWithIdentity:identity
                                               dataSource:_authService];
                              }]];
  }

  [alertController
      addAction:[UIAlertAction
                    actionWithTitle:@"Stop sync"
                              style:UIAlertActionStyleDefault
                            handler:^(UIAlertAction* action) {
                              [weakSelf.webView.configuration
                                      .syncController stopSyncAndClearIdentity];
                            }]];

  NSString* sandboxTitle = [CWVFlags sharedInstance].usesSyncAndWalletSandbox
                               ? @"Use production sync/wallet"
                               : @"Use sandbox sync/wallet";
  [alertController
      addAction:[UIAlertAction actionWithTitle:sandboxTitle
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [CWVFlags sharedInstance]
                                             .usesSyncAndWalletSandbox ^= YES;
                                       }]];

  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Cancel download"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf.downloadTask cancel];
                                       }]];

  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)resetTranslateSettings {
  CWVWebViewConfiguration* configuration =
      [CWVWebViewConfiguration defaultConfiguration];
  [configuration.preferences resetTranslationSettings];
}

- (void)toggleIncognito {
  BOOL wasPersistent = _webView.configuration.persistent;
  [self removeWebView];
  CWVWebViewConfiguration* newConfiguration =
      wasPersistent ? [CWVWebViewConfiguration incognitoConfiguration]
                    : [CWVWebViewConfiguration defaultConfiguration];
  [self createWebViewWithConfiguration:newConfiguration];
}

- (void)createWebViewWithConfiguration:(CWVWebViewConfiguration*)configuration {
  self.webView = [[CWVWebView alloc] initWithFrame:[_contentView bounds]
                                     configuration:configuration];
  [_contentView addSubview:_webView];

  // Gives a restoration identifier so that state restoration works.
  _webView.restorationIdentifier = @"webView";

  // Configure delegates.
  _webView.navigationDelegate = self;
  _webView.UIDelegate = self;
  _translationDelegate = [[ShellTranslationDelegate alloc] init];
  _webView.translationController.delegate = _translationDelegate;
  _autofillDelegate = [[ShellAutofillDelegate alloc] init];
  _webView.autofillController.delegate = _autofillDelegate;

  // Constraints.
  _webView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_webView.topAnchor
        constraintEqualToAnchor:_contentView.safeAreaLayoutGuide.topAnchor],
    [_webView.leadingAnchor
        constraintEqualToAnchor:_contentView.safeAreaLayoutGuide.leadingAnchor],
    [_webView.trailingAnchor
        constraintEqualToAnchor:_contentView.safeAreaLayoutGuide
                                    .trailingAnchor],
    [_webView.bottomAnchor
        constraintEqualToAnchor:_contentView.safeAreaLayoutGuide.bottomAnchor],
  ]];

  [_webView addObserver:self
             forKeyPath:@"canGoBack"
                options:NSKeyValueObservingOptionNew |
                        NSKeyValueObservingOptionInitial
                context:nil];
  [_webView addObserver:self
             forKeyPath:@"canGoForward"
                options:NSKeyValueObservingOptionNew |
                        NSKeyValueObservingOptionInitial
                context:nil];
  [_webView addObserver:self
             forKeyPath:@"loading"
                options:NSKeyValueObservingOptionNew |
                        NSKeyValueObservingOptionInitial
                context:nil];

  [_webView addScriptCommandHandler:self commandPrefix:@"test"];
}

- (void)removeWebView {
  [_webView removeFromSuperview];
  [_webView removeObserver:self forKeyPath:@"canGoBack"];
  [_webView removeObserver:self forKeyPath:@"canGoForward"];
  [_webView removeObserver:self forKeyPath:@"loading"];
  [_webView removeScriptCommandHandlerForCommandPrefix:@"test"];

  _webView = nil;
}

- (void)dealloc {
  [_webView removeObserver:self forKeyPath:@"canGoBack"];
  [_webView removeObserver:self forKeyPath:@"canGoForward"];
  [_webView removeObserver:self forKeyPath:@"loading"];
  [_webView removeScriptCommandHandlerForCommandPrefix:@"test"];
}

- (BOOL)textFieldShouldReturn:(UITextField*)field {
  NSString* enteredText = field.text;
  if (![enteredText hasPrefix:@"http"]) {
    enteredText =
        [enteredText stringByAddingPercentEncodingWithAllowedCharacters:
                         [NSCharacterSet URLQueryAllowedCharacterSet]];
    enteredText = [NSString
        stringWithFormat:@"https://www.google.com/search?q=%@", enteredText];
  }
  NSURLRequest* request =
      [NSURLRequest requestWithURL:[NSURL URLWithString:enteredText]];
  [_webView loadRequest:request];
  [field resignFirstResponder];
  [self updateToolbar];
  return YES;
}

- (void)updateToolbar {
  // Do not update the URL if the text field is currently being edited.
  if ([_field isFirstResponder]) {
    return;
  }

  [_field setText:[[_webView visibleURL] absoluteString]];
}

#pragma mark CWVUIDelegate methods

- (CWVWebView*)webView:(CWVWebView*)webView
    createWebViewWithConfiguration:(CWVWebViewConfiguration*)configuration
               forNavigationAction:(CWVNavigationAction*)action {
  NSLog(@"Create new CWVWebView for %@. User initiated? %@", action.request.URL,
        action.userInitiated ? @"Yes" : @"No");
  return nil;
}

- (void)webViewDidClose:(CWVWebView*)webView {
  NSLog(@"webViewDidClose");
}

- (void)webView:(CWVWebView*)webView
    runContextMenuWithTitle:(NSString*)menuTitle
             forHTMLElement:(CWVHTMLElement*)element
                     inView:(UIView*)view
        userGestureLocation:(CGPoint)location {
  if (!element.hyperlink) {
    return;
  }

  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:menuTitle
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  alert.popoverPresentationController.sourceView = view;
  alert.popoverPresentationController.sourceRect =
      CGRectMake(location.x, location.y, 1.0, 1.0);

  void (^copyHandler)(UIAlertAction*) = ^(UIAlertAction* action) {
    NSDictionary* item = @{
      (NSString*)(kUTTypeURL) : element.hyperlink,
      (NSString*)(kUTTypeUTF8PlainText) : [[element.hyperlink absoluteString]
          dataUsingEncoding:NSUTF8StringEncoding],
    };
    [[UIPasteboard generalPasteboard] setItems:@[ item ]];
  };
  [alert addAction:[UIAlertAction actionWithTitle:@"Copy Link"
                                            style:UIAlertActionStyleDefault
                                          handler:copyHandler]];

  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];

  [self presentViewController:alert animated:YES completion:nil];
}

- (void)webView:(CWVWebView*)webView
    runJavaScriptAlertPanelWithMessage:(NSString*)message
                               pageURL:(NSURL*)URL
                     completionHandler:(void (^)(void))handler {
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:nil
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  [alert addAction:[UIAlertAction actionWithTitle:@"Ok"
                                            style:UIAlertActionStyleDefault
                                          handler:^(UIAlertAction* action) {
                                            handler();
                                          }]];

  [self presentViewController:alert animated:YES completion:nil];
}

- (void)webView:(CWVWebView*)webView
    runJavaScriptConfirmPanelWithMessage:(NSString*)message
                                 pageURL:(NSURL*)URL
                       completionHandler:(void (^)(BOOL))handler {
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:nil
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  [alert addAction:[UIAlertAction actionWithTitle:@"Ok"
                                            style:UIAlertActionStyleDefault
                                          handler:^(UIAlertAction* action) {
                                            handler(YES);
                                          }]];
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:^(UIAlertAction* action) {
                                            handler(NO);
                                          }]];

  [self presentViewController:alert animated:YES completion:nil];
}

- (void)webView:(CWVWebView*)webView
    runJavaScriptTextInputPanelWithPrompt:(NSString*)prompt
                              defaultText:(NSString*)defaultText
                                  pageURL:(NSURL*)URL
                        completionHandler:(void (^)(NSString*))handler {
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:nil
                                          message:prompt
                                   preferredStyle:UIAlertControllerStyleAlert];

  [alert addTextFieldWithConfigurationHandler:^(UITextField* textField) {
    textField.text = defaultText;
    textField.accessibilityIdentifier =
        kWebViewShellJavaScriptDialogTextFieldAccessibiltyIdentifier;
  }];

  __weak UIAlertController* weakAlert = alert;
  [alert addAction:[UIAlertAction
                       actionWithTitle:@"Ok"
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction* action) {
                                 NSString* textInput =
                                     weakAlert.textFields.firstObject.text;
                                 handler(textInput);
                               }]];
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:^(UIAlertAction* action) {
                                            handler(nil);
                                          }]];

  [self presentViewController:alert animated:YES completion:nil];
}

- (void)webView:(CWVWebView*)webView
    didLoadFavicons:(NSArray<CWVFavicon*>*)favIcons {
  NSLog(@"%@", NSStringFromSelector(_cmd));
}

#pragma mark CWVNavigationDelegate methods

- (BOOL)webView:(CWVWebView*)webView
    shouldStartLoadWithRequest:(NSURLRequest*)request
                navigationType:(CWVNavigationType)navigationType {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  return YES;
}

- (BOOL)webView:(CWVWebView*)webView
    shouldContinueLoadWithResponse:(NSURLResponse*)response
                      forMainFrame:(BOOL)forMainFrame {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  return YES;
}

- (void)webViewDidStartProvisionalNavigation:(CWVWebView*)webView {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  [self updateToolbar];
}

- (void)webViewDidCommitNavigation:(CWVWebView*)webView {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  [self updateToolbar];
}

- (void)webViewDidFinishNavigation:(CWVWebView*)webView {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  // TODO(crbug.com/679895): Add some visual indication that the page load has
  // finished.
  [self updateToolbar];
}

- (void)webView:(CWVWebView*)webView
    didFailNavigationWithError:(NSError*)error {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  [self updateToolbar];
}

- (void)webViewWebContentProcessDidTerminate:(CWVWebView*)webView {
  NSLog(@"%@", NSStringFromSelector(_cmd));
}

- (BOOL)webView:(CWVWebView*)webView
    shouldPreviewElement:(CWVPreviewElementInfo*)elementInfo {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  return YES;
}

- (UIViewController*)webView:(CWVWebView*)webView
    previewingViewControllerForElement:(CWVPreviewElementInfo*)elementInfo {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  return nil;
}

- (void)webView:(CWVWebView*)webView
    commitPreviewingViewController:(UIViewController*)previewingViewController {
  NSLog(@"%@", NSStringFromSelector(_cmd));
}

- (void)webView:(CWVWebView*)webView
    didFailNavigationWithSSLError:(NSError*)error
                      overridable:(BOOL)overridable
                  decisionHandler:
                      (void (^)(CWVSSLErrorDecision))decisionHandler {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  decisionHandler(CWVSSLErrorDecisionDoNothing);
}

- (void)webView:(CWVWebView*)webView
    didRequestDownloadWithTask:(CWVDownloadTask*)task {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  self.downloadTask = task;
  NSString* documentDirectoryPath = NSSearchPathForDirectoriesInDomains(
      NSDocumentDirectory, NSUserDomainMask, YES)[0];
  self.downloadFilePath = [documentDirectoryPath
      stringByAppendingPathComponent:task.suggestedFileName];
  task.delegate = self;
  [task startDownloadToLocalFileAtPath:self.downloadFilePath];
}

#pragma mark CWVScriptCommandHandler

- (BOOL)webView:(CWVWebView*)webView
    handleScriptCommand:(nonnull CWVScriptCommand*)command
          fromMainFrame:(BOOL)fromMainFrame {
  NSLog(@"%@ command.content=%@", NSStringFromSelector(_cmd), command.content);
  return YES;
}

#pragma mark CWVDownloadTaskDelegate

- (void)downloadTask:(CWVDownloadTask*)downloadTask
    didFinishWithError:(nullable NSError*)error {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  if (!error) {
    NSURL* url = [NSURL fileURLWithPath:self.downloadFilePath];
    self.documentInteractionController =
        [UIDocumentInteractionController interactionControllerWithURL:url];
    [self.documentInteractionController presentOptionsMenuFromRect:CGRectZero
                                                            inView:self.view
                                                          animated:YES];
  }
  self.downloadTask = nil;
  self.downloadFilePath = nil;
}

- (void)downloadTaskProgressDidChange:(CWVDownloadTask*)downloadTask {
  NSLog(@"%@", NSStringFromSelector(_cmd));
}

#pragma mark CWVSyncControllerDelegate

- (void)syncControllerDidStartSync:(CWVSyncController*)syncController {
  NSLog(@"%@", NSStringFromSelector(_cmd));
}

- (void)syncController:(CWVSyncController*)syncController
      didFailWithError:(NSError*)error {
  NSLog(@"%@:%@", NSStringFromSelector(_cmd), error);
}

- (void)syncController:(CWVSyncController*)syncController
    didStopSyncWithReason:(CWVStopSyncReason)reason {
  NSLog(@"%@:%ld", NSStringFromSelector(_cmd), (long)reason);
}

@end
