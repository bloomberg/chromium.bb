// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include "include/cef_app.h"
#import "include/cef_application_mac.h"
#import "include/wrapper/cef_library_loader.h"
#include "tests/cefclient/browser/main_context_impl.h"
#include "tests/cefclient/browser/resource.h"
#include "tests/cefclient/browser/root_window.h"
#include "tests/cefclient/browser/test_runner.h"
#include "tests/shared/browser/client_app_browser.h"
#include "tests/shared/browser/main_message_loop_external_pump.h"
#include "tests/shared/browser/main_message_loop_std.h"
#include "tests/shared/common/client_switches.h"

namespace {

// Returns the top menu bar with the specified |tag|.
NSMenuItem* GetMenuBarMenuWithTag(NSInteger tag) {
  NSMenu* main_menu = [[NSApplication sharedApplication] mainMenu];
  NSInteger found_index = [main_menu indexOfItemWithTag:tag];
  if (found_index >= 0)
    return [main_menu itemAtIndex:found_index];
  return nil;
}

// Returns the item in |menu| that has the specified |action_selector|.
NSMenuItem* GetMenuItemWithAction(NSMenu* menu, SEL action_selector) {
  for (NSInteger i = 0; i < menu.numberOfItems; ++i) {
    NSMenuItem* item = [menu itemAtIndex:i];
    if (item.action == action_selector)
      return item;
  }
  return nil;
}

}  // namespace

// Receives notifications from the application. Will delete itself when done.
@interface ClientAppDelegate : NSObject <NSApplicationDelegate> {
 @private
  bool with_controls_;
  bool with_osr_;
}

- (id)initWithControls:(bool)with_controls andOsr:(bool)with_osr;
- (void)createApplication:(id)object;
- (void)tryToTerminateApplication:(NSApplication*)app;
- (void)testsItemSelected:(int)command_id;
- (IBAction)menuTestsGetText:(id)sender;
- (IBAction)menuTestsGetSource:(id)sender;
- (IBAction)menuTestsWindowNew:(id)sender;
- (IBAction)menuTestsWindowPopup:(id)sender;
- (IBAction)menuTestsRequest:(id)sender;
- (IBAction)menuTestsPluginInfo:(id)sender;
- (IBAction)menuTestsZoomIn:(id)sender;
- (IBAction)menuTestsZoomOut:(id)sender;
- (IBAction)menuTestsZoomReset:(id)sender;
- (IBAction)menuTestsSetFPS:(id)sender;
- (IBAction)menuTestsSetScaleFactor:(id)sender;
- (IBAction)menuTestsTracingBegin:(id)sender;
- (IBAction)menuTestsTracingEnd:(id)sender;
- (IBAction)menuTestsPrint:(id)sender;
- (IBAction)menuTestsPrintToPdf:(id)sender;
- (IBAction)menuTestsMuteAudio:(id)sender;
- (IBAction)menuTestsUnmuteAudio:(id)sender;
- (IBAction)menuTestsOtherTests:(id)sender;
- (void)enableAccessibility:(bool)bEnable;
@end

// Provide the CefAppProtocol implementation required by CEF.
@interface ClientApplication : NSApplication <CefAppProtocol> {
 @private
  BOOL handlingSendEvent_;
}
@end

@implementation ClientApplication

- (BOOL)isHandlingSendEvent {
  return handlingSendEvent_;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  handlingSendEvent_ = handlingSendEvent;
}

- (void)sendEvent:(NSEvent*)event {
  CefScopedSendingEvent sendingEventScoper;
  [super sendEvent:event];
}

// |-terminate:| is the entry point for orderly "quit" operations in Cocoa. This
// includes the application menu's quit menu item and keyboard equivalent, the
// application's dock icon menu's quit menu item, "quit" (not "force quit") in
// the Activity Monitor, and quits triggered by user logout and system restart
// and shutdown.
//
// The default |-terminate:| implementation ends the process by calling exit(),
// and thus never leaves the main run loop. This is unsuitable for Chromium
// since Chromium depends on leaving the main run loop to perform an orderly
// shutdown. We support the normal |-terminate:| interface by overriding the
// default implementation. Our implementation, which is very specific to the
// needs of Chromium, works by asking the application delegate to terminate
// using its |-tryToTerminateApplication:| method.
//
// |-tryToTerminateApplication:| differs from the standard
// |-applicationShouldTerminate:| in that no special event loop is run in the
// case that immediate termination is not possible (e.g., if dialog boxes
// allowing the user to cancel have to be shown). Instead, this method tries to
// close all browsers by calling CloseBrowser(false) via
// ClientHandler::CloseAllBrowsers. Calling CloseBrowser will result in a call
// to ClientHandler::DoClose and execution of |-performClose:| on the NSWindow.
// DoClose sets a flag that is used to differentiate between new close events
// (e.g., user clicked the window close button) and in-progress close events
// (e.g., user approved the close window dialog). The NSWindowDelegate
// |-windowShouldClose:| method checks this flag and either calls
// CloseBrowser(false) in the case of a new close event or destructs the
// NSWindow in the case of an in-progress close event.
// ClientHandler::OnBeforeClose will be called after the CEF NSView hosted in
// the NSWindow is dealloc'ed.
//
// After the final browser window has closed ClientHandler::OnBeforeClose will
// begin actual tear-down of the application by calling CefQuitMessageLoop.
// This ends the NSApplication event loop and execution then returns to the
// main() function for cleanup before application termination.
//
// The standard |-applicationShouldTerminate:| is not supported, and code paths
// leading to it must be redirected.
- (void)terminate:(id)sender {
  ClientAppDelegate* delegate = static_cast<ClientAppDelegate*>(
      [[NSApplication sharedApplication] delegate]);
  [delegate tryToTerminateApplication:self];
  // Return, don't exit. The application is responsible for exiting on its own.
}

// Detect dynamically if VoiceOver is running. Like Chromium, rely upon the
// undocumented accessibility attribute @"AXEnhancedUserInterface" which is set
// when VoiceOver is launched and unset when VoiceOver is closed.
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  if ([attribute isEqualToString:@"AXEnhancedUserInterface"]) {
    ClientAppDelegate* delegate = static_cast<ClientAppDelegate*>(
        [[NSApplication sharedApplication] delegate]);
    [delegate enableAccessibility:([value intValue] == 1)];
  }
  return [super accessibilitySetValue:value forAttribute:attribute];
}
@end

@implementation ClientAppDelegate

- (id)initWithControls:(bool)with_controls andOsr:(bool)with_osr {
  if (self = [super init]) {
    with_controls_ = with_controls;
    with_osr_ = with_osr;
  }
  return self;
}

// Create the application on the UI thread.
- (void)createApplication:(id)object {
  NSApplication* application = [NSApplication sharedApplication];

  // The top menu is configured using Interface Builder (IB). To modify the menu
  // start by loading MainMenu.xib in IB.
  //
  // To associate MainMenu.xib with ClientAppDelegate:
  // 1. Select "File's Owner" from the "Placeholders" section in the left side
  //    pane.
  // 2. Load the "Identity inspector" tab in the top-right side pane.
  // 3. In the "Custom Class" section set the "Class" value to
  //    "ClientAppDelegate".
  // 4. Pass an instance of ClientAppDelegate as the |owner| parameter to
  //    loadNibNamed:.
  //
  // To create a new top menu:
  // 1. Load the "Object library" tab in the bottom-right side pane.
  // 2. Drag a "Submenu Menu Item" widget from the Object library to the desired
  //    location in the menu bar shown in the center pane.
  // 3. Select the newly created top menu by left clicking on it.
  // 4. Load the "Attributes inspector" tab in the top-right side pane.
  // 5. Under the "Menu Item" section set the "Tag" value to a unique integer.
  //    This is necessary for the GetMenuBarMenuWithTag function to work
  //    properly.
  //
  // To create a new menu item in a top menu:
  // 1. Add a new receiver method in ClientAppDelegate (e.g. menuTestsDoStuff:).
  // 2. Load the "Object library" tab in the bottom-right side pane.
  // 3. Drag a "Menu Item" widget from the Object library to the desired
  //    location in the menu bar shown in the center pane.
  // 4. Double-click on the new menu item to set the label.
  // 5. Right click on the new menu item to show the "Get Source" dialog.
  // 6. In the "Sent Actions" section drag from the circle icon and drop on the
  //    new receiver method in the ClientAppDelegate source code file.
  //
  // Load the top menu from MainMenu.xib.
  [[NSBundle mainBundle] loadNibNamed:@"MainMenu"
                                owner:self
                      topLevelObjects:nil];

  // Set the delegate for application events.
  [application setDelegate:self];

  if (!with_osr_) {
    // Remove the OSR-related menu items when OSR is disabled.
    NSMenuItem* tests_menu = GetMenuBarMenuWithTag(8);
    if (tests_menu) {
      NSMenuItem* set_fps_item = GetMenuItemWithAction(
          tests_menu.submenu, @selector(menuTestsSetFPS:));
      if (set_fps_item)
        [tests_menu.submenu removeItem:set_fps_item];
      NSMenuItem* set_scale_factor_item = GetMenuItemWithAction(
          tests_menu.submenu, @selector(menuTestsSetScaleFactor:));
      if (set_scale_factor_item)
        [tests_menu.submenu removeItem:set_scale_factor_item];
    }
  }

  client::RootWindowConfig window_config;
  window_config.with_controls = with_controls_;
  window_config.with_osr = with_osr_;

  // Create the first window.
  client::MainContext::Get()->GetRootWindowManager()->CreateRootWindow(
      window_config);
}

- (void)tryToTerminateApplication:(NSApplication*)app {
  client::MainContext::Get()->GetRootWindowManager()->CloseAllWindows(false);
}

- (void)orderFrontStandardAboutPanel:(id)sender {
  [[NSApplication sharedApplication] orderFrontStandardAboutPanel:nil];
}

- (void)testsItemSelected:(int)command_id {
  // Retrieve the active RootWindow.
  NSWindow* key_window = [[NSApplication sharedApplication] keyWindow];
  if (!key_window)
    return;

  scoped_refptr<client::RootWindow> root_window =
      client::RootWindow::GetForNSWindow(key_window);

  CefRefPtr<CefBrowser> browser = root_window->GetBrowser();
  if (browser.get())
    client::test_runner::RunTest(browser, command_id);
}

- (IBAction)menuTestsGetText:(id)sender {
  [self testsItemSelected:ID_TESTS_GETTEXT];
}

- (IBAction)menuTestsGetSource:(id)sender {
  [self testsItemSelected:ID_TESTS_GETSOURCE];
}

- (IBAction)menuTestsWindowNew:(id)sender {
  [self testsItemSelected:ID_TESTS_WINDOW_NEW];
}

- (IBAction)menuTestsWindowPopup:(id)sender {
  [self testsItemSelected:ID_TESTS_WINDOW_POPUP];
}

- (IBAction)menuTestsRequest:(id)sender {
  [self testsItemSelected:ID_TESTS_REQUEST];
}

- (IBAction)menuTestsPluginInfo:(id)sender {
  [self testsItemSelected:ID_TESTS_PLUGIN_INFO];
}

- (IBAction)menuTestsZoomIn:(id)sender {
  [self testsItemSelected:ID_TESTS_ZOOM_IN];
}

- (IBAction)menuTestsZoomOut:(id)sender {
  [self testsItemSelected:ID_TESTS_ZOOM_OUT];
}

- (IBAction)menuTestsZoomReset:(id)sender {
  [self testsItemSelected:ID_TESTS_ZOOM_RESET];
}

- (IBAction)menuTestsSetFPS:(id)sender {
  [self testsItemSelected:ID_TESTS_OSR_FPS];
}

- (IBAction)menuTestsSetScaleFactor:(id)sender {
  [self testsItemSelected:ID_TESTS_OSR_DSF];
}

- (IBAction)menuTestsTracingBegin:(id)sender {
  [self testsItemSelected:ID_TESTS_TRACING_BEGIN];
}

- (IBAction)menuTestsTracingEnd:(id)sender {
  [self testsItemSelected:ID_TESTS_TRACING_END];
}

- (IBAction)menuTestsPrint:(id)sender {
  [self testsItemSelected:ID_TESTS_PRINT];
}

- (IBAction)menuTestsPrintToPdf:(id)sender {
  [self testsItemSelected:ID_TESTS_PRINT_TO_PDF];
}

- (IBAction)menuTestsMuteAudio:(id)sender {
  [self testsItemSelected:ID_TESTS_MUTE_AUDIO];
}

- (IBAction)menuTestsUnmuteAudio:(id)sender {
  [self testsItemSelected:ID_TESTS_UNMUTE_AUDIO];
}

- (IBAction)menuTestsOtherTests:(id)sender {
  [self testsItemSelected:ID_TESTS_OTHER_TESTS];
}

- (void)enableAccessibility:(bool)bEnable {
  // Retrieve the active RootWindow.
  NSWindow* key_window = [[NSApplication sharedApplication] keyWindow];
  if (!key_window)
    return;

  scoped_refptr<client::RootWindow> root_window =
      client::RootWindow::GetForNSWindow(key_window);

  CefRefPtr<CefBrowser> browser = root_window->GetBrowser();
  if (browser.get()) {
    browser->GetHost()->SetAccessibilityState(bEnable ? STATE_ENABLED
                                                      : STATE_DISABLED);
  }
}

- (NSApplicationTerminateReply)applicationShouldTerminate:
    (NSApplication*)sender {
  return NSTerminateNow;
}

@end

namespace client {
namespace {

int RunMain(int argc, char* argv[]) {
  // Load the CEF framework library at runtime instead of linking directly
  // as required by the macOS sandbox implementation.
  CefScopedLibraryLoader library_loader;
  if (!library_loader.LoadInMain())
    return 1;

  int result = -1;

  CefMainArgs main_args(argc, argv);

  @autoreleasepool {
    // Initialize the ClientApplication instance.
    [ClientApplication sharedApplication];

    // If there was an invocation to NSApp prior to this method, then the NSApp
    // will not be a ClientApplication, but will instead be an NSApplication.
    // This is undesirable and we must enforce that this doesn't happen.
    CHECK([NSApp isKindOfClass:[ClientApplication class]]);

    // Parse command-line arguments.
    CefRefPtr<CefCommandLine> command_line =
        CefCommandLine::CreateCommandLine();
    command_line->InitFromArgv(argc, argv);

    // Create a ClientApp of the correct type.
    CefRefPtr<CefApp> app;
    ClientApp::ProcessType process_type =
        ClientApp::GetProcessType(command_line);
    if (process_type == ClientApp::BrowserProcess)
      app = new ClientAppBrowser();

    // Create the main context object.
    scoped_ptr<MainContextImpl> context(
        new MainContextImpl(command_line, true));

    CefSettings settings;

// When generating projects with CMake the CEF_USE_SANDBOX value will be defined
// automatically. Pass -DUSE_SANDBOX=OFF to the CMake command-line to disable
// use of the sandbox.
#if !defined(CEF_USE_SANDBOX)
    settings.no_sandbox = true;
#endif

    // Populate the settings based on command line arguments.
    context->PopulateSettings(&settings);

    // Create the main message loop object.
    scoped_ptr<MainMessageLoop> message_loop;
    if (settings.external_message_pump)
      message_loop = MainMessageLoopExternalPump::Create();
    else
      message_loop.reset(new MainMessageLoopStd);

    // Initialize CEF.
    context->Initialize(main_args, settings, app, NULL);

    // Register scheme handlers.
    test_runner::RegisterSchemeHandlers();

    // Create the application delegate and window.
    ClientAppDelegate* delegate = [[ClientAppDelegate alloc]
        initWithControls:!command_line->HasSwitch(switches::kHideControls)
                  andOsr:settings.windowless_rendering_enabled ? true : false];
    [delegate performSelectorOnMainThread:@selector(createApplication:)
                               withObject:nil
                            waitUntilDone:NO];

    // Run the message loop. This will block until Quit() is called.
    result = message_loop->Run();

    // Shut down CEF.
    context->Shutdown();

    // Release objects in reverse order of creation.
#if !__has_feature(objc_arc)
    [delegate release];
#endif  // !__has_feature(objc_arc)
    delegate = nil;
    message_loop.reset();
    context.reset();
  }  // @autoreleasepool

  return result;
}

}  // namespace
}  // namespace client

// Entry point function for the browser process.
int main(int argc, char* argv[]) {
  return client::RunMain(argc, argv);
}
