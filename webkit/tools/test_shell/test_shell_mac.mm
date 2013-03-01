// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
#import <objc/objc-runtime.h>
#include <sys/stat.h>

#include "webkit/tools/test_shell/test_shell.h"

#include "base/base_paths.h"
#include "base/basictypes.h"
#include "base/debug/debugger.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "grit/webkit_resources.h"
#include "net/base/mime_util.h"
#include "skia/ext/bitmap_platform_device.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/resource/data_pack.h"
#include "ui/gfx/size.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/tools/test_shell/mac/test_shell_webview.h"
#include "webkit/tools/test_shell/resource.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_shell_webkit_init.h"
#include "webkit/tools/test_shell/test_webview_delegate.h"

#include "third_party/skia/include/core/SkBitmap.h"

#import "mac/DumpRenderTreePasteboard.h"

using WebKit::WebWidget;

#define MAX_LOADSTRING 100

// Sizes for URL bar layout
#define BUTTON_HEIGHT 22
#define BUTTON_WIDTH 72
#define BUTTON_MARGIN 8
#define URLBAR_HEIGHT  32

// Global Variables:

// Content area size for newly created windows.
const int kTestWindowWidth = 800;
const int kTestWindowHeight = 600;

// The W3C SVG layout tests use a different size than the other layout tests
const int kSVGTestWindowWidth = 480;
const int kSVGTestWindowHeight = 360;

// Hide the window offscreen when in layout test mode.  Mac OS X limits
// window positions to +/- 16000.
const int kTestWindowXLocation = -14000;
const int kTestWindowYLocation = -14000;

// Data pack resource. This is a pointer to the mmapped resources file.
static ui::DataPack* g_resource_data_pack = NULL;

// Define static member variables
base::LazyInstance <std::map<gfx::NativeWindow, TestShell *> >
    TestShell::window_map_ = LAZY_INSTANCE_INITIALIZER;

// Helper method for getting the path to the test shell resources directory.
base::FilePath GetResourcesFilePath() {
  base::FilePath path;
  // We need to know if we're bundled or not to know which path to use.
  if (base::mac::AmIBundled()) {
    PathService::Get(base::DIR_EXE, &path);
    path = path.Append(base::FilePath::kParentDirectory);
    return path.AppendASCII("Resources");
  } else {
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.AppendASCII("webkit");
    path = path.AppendASCII("tools");
    path = path.AppendASCII("test_shell");
    return path.AppendASCII("resources");
  }
}

// Receives notification that the window is closing so that it can start the
// tear-down process. Is responsible for deleting itself when done.
@interface WindowDelegate : NSObject<NSWindowDelegate> {
 @private
  TestShellWebView* m_webView;
}
- (id)initWithWebView:(TestShellWebView*)view;
@end

@implementation WindowDelegate

- (id)initWithWebView:(TestShellWebView*)view {
  if ((self = [super init])) {
    m_webView = view;
  }
  return self;
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  [m_webView setIsActive:YES];
}

- (void)windowDidResignKey:(NSNotification*)notification {
  [m_webView setIsActive:NO];
}

// Called when the window is about to close. Perform the self-destruction
// sequence by getting rid of the shell and removing it and the window from
// the various global lists. Instead of doing it here, however, we fire off
// a delayed call to |-cleanup:| to allow everything to get off the stack
// before we go deleting objects. By returning YES, we allow the window to be
// removed from the screen.
- (BOOL)windowShouldClose:(id)window {
  m_webView = nil;

  // Try to make the window go away, but it may not when running layout
  // tests due to the quirkyness of autorelease pools and having no main loop.
  [window autorelease];

  // clean ourselves up and do the work after clearing the stack of anything
  // that might have the shell on it.
  [self performSelectorOnMainThread:@selector(cleanup:)
                         withObject:window
                      waitUntilDone:NO];

  return YES;
}

// does the work of removing the window from our various bookkeeping lists
// and gets rid of the shell.
- (void)cleanup:(id)window {
  TestShell::RemoveWindowFromList(window);
  TestShell::DestroyAssociatedShell(window);

  [self release];
}

@end

// Mac-specific stuff to do when the dtor is called. Nothing to do in our
// case.
void TestShell::PlatformCleanUp() {
}

void TestShell::EnableUIControl(UIControl control, bool is_enabled) {
  // TODO(darin): Implement me.
}

// static
void TestShell::DestroyAssociatedShell(gfx::NativeWindow handle) {
  WindowMap::iterator it = window_map_.Get().find(handle);
  if (it != window_map_.Get().end()) {
    // Break the view's association with its shell before deleting the shell.
    TestShellWebView* web_view =
      static_cast<TestShellWebView*>(it->second->m_webViewHost->view_handle());
    if ([web_view isKindOfClass:[TestShellWebView class]]) {
      [web_view setShell:NULL];
    }

    delete it->second;
    window_map_.Get().erase(it);
  } else {
    LOG(ERROR) << "Failed to find shell for window during destroy";
  }
}

// static
void TestShell::PlatformShutdown() {
  // for each window in the window list, release it and destroy its shell
  for (WindowList::iterator it = TestShell::windowList()->begin();
       it != TestShell::windowList()->end();
       ++it) {
    DestroyAssociatedShell(*it);
    [*it release];
  }
  // assert if we have anything left over, that would be bad.
  DCHECK(window_map_.Get().empty());

  // Dump the pasteboards we built up.
  [DumpRenderTreePasteboard releaseLocalPasteboards];
}

// static
void TestShell::InitializeTestShell(bool layout_test_mode,
                                    bool allow_external_pages) {
  // This should move to a per-process platform-specific initialization function
  // when one exists.

  window_list_ = new WindowList;
  layout_test_mode_ = layout_test_mode;
  allow_external_pages_ = allow_external_pages;

  web_prefs_ = new webkit_glue::WebPreferences;

  // mmap the data pack which holds strings used by WebCore. This is only
  // a fatal error if we're bundled, which means we might be running layout
  // tests. This is a harmless failure for test_shell_tests.
  g_resource_data_pack = new ui::DataPack(ui::SCALE_FACTOR_100P);
  NSString *resource_path =
      [base::mac::FrameworkBundle() pathForResource:@"test_shell"
                                             ofType:@"pak"];
  base::FilePath resources_pak_path([resource_path fileSystemRepresentation]);
  if (!g_resource_data_pack->LoadFromPath(resources_pak_path)) {
    LOG(FATAL) << "failed to load test_shell.pak";
  }

  ResetWebPreferences();

  // Load the Ahem font, which is used by layout tests.
  NSString* ahem_path = [[base::mac::FrameworkBundle() resourcePath]
      stringByAppendingPathComponent:@"AHEM____.TTF"];
  NSURL* ahem_path_url = [NSURL fileURLWithPath:ahem_path];
  CFErrorRef error;
  if (!CTFontManagerRegisterFontsForURL((CFURLRef)ahem_path_url,
                                        kCTFontManagerScopeProcess, &error)) {
    DLOG(FATAL) << "CTFontManagerRegisterFontsForURL "
                << [ahem_path fileSystemRepresentation]
                << [[(NSError*)error description] UTF8String];
  }

  // Add <app bundle's parent dir>/plugins to the plugin path so we can load
  // test plugins.
  base::FilePath plugins_dir;
  PathService::Get(base::DIR_EXE, &plugins_dir);
  if (base::mac::AmIBundled()) {
    plugins_dir = plugins_dir.AppendASCII("../../../plugins");
  } else {
    plugins_dir = plugins_dir.AppendASCII("plugins");
  }
  webkit::npapi::PluginList::Singleton()->AddExtraPluginDir(plugins_dir);
}

NSButton* MakeTestButton(NSRect* rect, NSString* title, NSView* parent) {
  NSButton* button = [[[NSButton alloc] initWithFrame:*rect] autorelease];
  [button setTitle:title];
  [button setBezelStyle:NSSmallSquareBezelStyle];
  [button setAutoresizingMask:(NSViewMaxXMargin | NSViewMinYMargin)];
  [parent addSubview:button];
  rect->origin.x += BUTTON_WIDTH;
  return button;
}

bool TestShell::Initialize(const GURL& starting_url) {
  // Perform application initialization:
  // send message to app controller?  need to work this out

  // TODO(awalker): this is a straight recreation of windows test_shell.cc's
  // window creation code--we should really pull this from the nib and grab
  // references to the already-created subviews that way.
  NSRect screen_rect = [[NSScreen mainScreen] visibleFrame];
  NSRect window_rect = { {0, screen_rect.size.height - kTestWindowHeight},
                         {kTestWindowWidth, kTestWindowHeight} };
  m_mainWnd = [[NSWindow alloc]
                  initWithContentRect:window_rect
                            styleMask:(NSTitledWindowMask |
                                       NSClosableWindowMask |
                                       NSMiniaturizableWindowMask |
                                       NSResizableWindowMask )
                              backing:NSBackingStoreBuffered
                                defer:NO];
  [m_mainWnd setTitle:@"TestShell"];

  // Add to our map
  window_map_.Get()[m_mainWnd] = this;

  // Rely on the window delegate to clean us up rather than immediately
  // releasing when the window gets closed. We use the delegate to do
  // everything from the autorelease pool so the shell isn't on the stack
  // during cleanup (ie, a window close from javascript).
  [m_mainWnd setReleasedWhenClosed:NO];

  // Create a webview. Note that |web_view| takes ownership of this shell so we
  // will get cleaned up when it gets destroyed.
  m_webViewHost.reset(
      WebViewHost::Create([m_mainWnd contentView],
                          delegate_.get(),
                          0,
                          *TestShell::web_prefs_));
  delegate_->RegisterDragDrop();
  TestShellWebView* web_view =
      static_cast<TestShellWebView*>(m_webViewHost->view_handle());
  [web_view setShell:this];

  // Create a window delegate to watch for when it's asked to go away. It will
  // clean itself up so we don't need to hold a reference.
  [m_mainWnd setDelegate:[[WindowDelegate alloc] initWithWebView:web_view]];

  // create buttons
  NSRect button_rect = [[m_mainWnd contentView] bounds];
  button_rect.origin.y = window_rect.size.height - URLBAR_HEIGHT +
      (URLBAR_HEIGHT - BUTTON_HEIGHT) / 2;
  button_rect.size.height = BUTTON_HEIGHT;
  button_rect.origin.x += BUTTON_MARGIN;
  button_rect.size.width = BUTTON_WIDTH;

  NSView* content = [m_mainWnd contentView];

  NSButton* button = MakeTestButton(&button_rect, @"Back", content);
  [button setTarget:web_view];
  [button setAction:@selector(goBack:)];

  button = MakeTestButton(&button_rect, @"Forward", content);
  [button setTarget:web_view];
  [button setAction:@selector(goForward:)];

  // reload button
  button = MakeTestButton(&button_rect, @"Reload", content);
  [button setTarget:web_view];
  [button setAction:@selector(reload:)];

  // stop button
  button = MakeTestButton(&button_rect, @"Stop", content);
  [button setTarget:web_view];
  [button setAction:@selector(stopLoading:)];

  // text field for URL
  button_rect.origin.x += BUTTON_MARGIN;
  button_rect.size.width = [[m_mainWnd contentView] bounds].size.width -
      button_rect.origin.x - BUTTON_MARGIN;
  m_editWnd = [[NSTextField alloc] initWithFrame:button_rect];
  [[m_mainWnd contentView] addSubview:m_editWnd];
  [m_editWnd setAutoresizingMask:(NSViewWidthSizable | NSViewMinYMargin)];
  [m_editWnd setTarget:web_view];
  [m_editWnd setAction:@selector(takeURLStringValueFrom:)];
  [[m_editWnd cell] setWraps:NO];
  [[m_editWnd cell] setScrollable:YES];

  // show the window
  [m_mainWnd makeKeyAndOrderFront: nil];

  // Load our initial content.
  if (starting_url.is_valid())
    LoadURL(starting_url);

  if (IsSVGTestURL(starting_url)) {
    SizeTo(kSVGTestWindowWidth, kSVGTestWindowHeight);
  } else {
    SizeToDefault();
  }

  return true;
}

void TestShell::TestFinished() {
  if (!test_is_pending_)
    return;  // reached when running under test_shell_tests

  test_is_pending_ = false;
  MessageLoop::current()->Quit();
}

// A class to be the target/selector of the "watchdog" thread that ensures
// pages timeout if they take too long and tells the test harness via stdout.
@interface WatchDogTarget : NSObject {
 @private
  NSTimeInterval timeout_;
}
// |timeout| is in seconds
- (id)initWithTimeout:(NSTimeInterval)timeout;
// serves as the "run" method of a NSThread.
- (void)run:(id)sender;
@end

@implementation WatchDogTarget

- (id)initWithTimeout:(NSTimeInterval)timeout {
  if ((self = [super init])) {
    timeout_ = timeout;
  }
  return self;
}

- (void)run:(id)ignore {
  base::mac::ScopedNSAutoreleasePool scoped_pool;

  // Check for debugger, just bail if so. We don't want the timeouts hitting
  // when we're trying to track down an issue.
  if (base::debug::BeingDebugged())
    return;

  NSThread* currentThread = [NSThread currentThread];

  // Wait to be cancelled. If we are that means the test finished. If it hasn't,
  // then we need to tell the layout script we timed out and start again.
  NSDate* limitDate = [NSDate dateWithTimeIntervalSinceNow:timeout_];
  while ([(NSDate*)[NSDate date] compare:limitDate] == NSOrderedAscending &&
         ![currentThread isCancelled]) {
    // sleep for a small increment then check again
    NSDate* incrementDate = [NSDate dateWithTimeIntervalSinceNow:1.0];
    [NSThread sleepUntilDate:incrementDate];
  }
  if (![currentThread isCancelled]) {
    // Print a warning to be caught by the layout-test script.
    // Note: the layout test driver may or may not recognize
    // this as a timeout.
    puts("#TEST_TIMED_OUT\n");
    puts("#EOF\n");
    fflush(stdout);
    abort();
  }
}

@end

void TestShell::WaitTestFinished() {
  DCHECK(!test_is_pending_) << "cannot be used recursively";

  test_is_pending_ = true;

  // Create a watchdog thread which just sets a timer and
  // kills the process if it times out.  This catches really
  // bad hangs where the shell isn't coming back to the
  // message loop.  If the watchdog is what catches a
  // timeout, it can't do anything except terminate the test
  // shell, which is unfortunate.
  // Windows multiplies by 2.5, but that causes us to run for far, far too
  // long. We use the passed value and let the scripts flag override
  // the value as needed.
  NSTimeInterval timeout_seconds = GetLayoutTestTimeoutForWatchDog() / 1000;
  WatchDogTarget* watchdog = [[[WatchDogTarget alloc]
                                initWithTimeout:timeout_seconds] autorelease];
  NSThread* thread = [[NSThread alloc] initWithTarget:watchdog
                                             selector:@selector(run:)
                                               object:nil];
  [thread start];

  // TestFinished() will post a quit message to break this loop when the page
  // finishes loading.
  while (test_is_pending_)
    MessageLoop::current()->Run();

  // Tell the watchdog that we're finished. No point waiting to re-join, it'll
  // die on its own.
  [thread cancel];
  [thread release];
}

void TestShell::InteractiveSetFocus(WebWidgetHost* host, bool enable) {
  if (enable) {
    [[host->view_handle() window] makeKeyAndOrderFront:nil];
  } else {
    // There is no way to resign key window status in Cocoa.  Fake it by
    // ordering the window out (transferring key status to another window) and
    // then ordering the window back in, but without making it key.
    [[host->view_handle() window] orderOut:nil];
    [[host->view_handle() window] orderFront:nil];
  }
}

// static
void TestShell::DestroyWindow(gfx::NativeWindow windowHandle) {
  // This code is like -cleanup: on our window delegate.  This call needs to be
  // able to force down a window for tests, so it closes down the window making
  // sure it cleans up the window delegate and the test shells list of windows
  // and map of windows to shells.

  TestShell::RemoveWindowFromList(windowHandle);
  TestShell::DestroyAssociatedShell(windowHandle);

  id windowDelegate = [windowHandle delegate];
  DCHECK(windowDelegate);
  [windowHandle setDelegate:nil];
  [windowDelegate release];

  [windowHandle close];
  [windowHandle autorelease];
}

WebWidget* TestShell::CreatePopupWidget() {
  DCHECK(!m_popupHost);
  m_popupHost = WebWidgetHost::Create(webViewWnd(), popup_delegate_.get());

  return m_popupHost->webwidget();
}

void TestShell::ClosePopup() {
  // PostMessage(popupWnd(), WM_CLOSE, 0, 0);
  m_popupHost = NULL;
}

void TestShell::SizeTo(int width, int height) {
  // WebViewHost::Create() sets the HTML content rect to start 32 pixels below
  // the top of the window to account for the "toolbar". We need to match that
  // here otherwise the HTML content area will be too short.
  NSRect r = [m_mainWnd contentRectForFrameRect:[m_mainWnd frame]];
  r.size.width = width;
  r.size.height = height + URLBAR_HEIGHT;
  [m_mainWnd setFrame:[m_mainWnd frameRectForContentRect:r] display:YES];
}

void TestShell::ResizeSubViews() {
  // handled by Cocoa for us
}

/* static */ void TestShell::DumpAllBackForwardLists(string16* result) {
  result->clear();
  for (WindowList::iterator iter = TestShell::windowList()->begin();
       iter != TestShell::windowList()->end(); iter++) {
    NSWindow* window = *iter;
    WindowMap::iterator it = window_map_.Get().find(window);
    if (it != window_map_.Get().end())
      it->second->DumpBackForwardList(result);
    else
      LOG(ERROR) << "Failed to find shell for window during dump";
  }
}

void TestShell::LoadURLForFrame(const GURL& url,
                                const string16& frame_name) {
  if (!url.is_valid())
    return;

  if (IsSVGTestURL(url)) {
    SizeTo(kSVGTestWindowWidth, kSVGTestWindowHeight);
  } else {
    // only resize back to the default when running tests
    if (layout_test_mode())
      SizeToDefault();
  }

  navigation_controller_->LoadEntry(
      new TestNavigationEntry(-1, url, frame_name));
}

bool TestShell::PromptForSaveFile(const wchar_t* prompt_title,
                                  base::FilePath* result)
{
  NSSavePanel* save_panel = [NSSavePanel savePanel];

  /* set up new attributes */
  [save_panel setAllowedFileTypes:@[@"txt"]];
  [save_panel setMessage:
      [NSString stringWithUTF8String:WideToUTF8(prompt_title).c_str()]];

  /* display the NSSavePanel */
  [save_panel setDirectoryURL:[NSURL fileURLWithPath:NSHomeDirectory()]];
  [save_panel setNameFieldStringValue:@""];
  if ([save_panel runModal] == NSFileHandlingPanelOKButton) {
    *result = base::FilePath([[[save_panel URL] path] fileSystemRepresentation]);
    return true;
  }
  return false;
}

// static
std::string TestShell::RewriteLocalUrl(const std::string& url) {
  // Convert file:///tmp/LayoutTests urls to the actual location on disk.
  const char kPrefix[] = "file:///tmp/LayoutTests/";
  const int kPrefixLen = arraysize(kPrefix) - 1;

  std::string new_url(url);
  if (url.compare(0, kPrefixLen, kPrefix, kPrefixLen) == 0) {
    base::FilePath replace_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &replace_path);
    replace_path = replace_path.Append(
        "third_party/WebKit/LayoutTests/");
    new_url = std::string("file://") + replace_path.value() +
        url.substr(kPrefixLen);
  }

  return new_url;
}

// static
void TestShell::ShowStartupDebuggingDialog() {
  NSAlert* alert = [[[NSAlert alloc] init] autorelease];
  alert.messageText = @"Attach to me?";
  alert.informativeText = @"This would probably be a good time to attach your "
      "debugger.";
  [alert addButtonWithTitle:@"OK"];

  [alert runModal];
}

base::StringPiece TestShell::ResourceProvider(int key) {
  base::StringPiece res;
  g_resource_data_pack->GetStringPiece(key, &res);
  return res;
}

//-----------------------------------------------------------------------------

string16 TestShellWebKitInit::GetLocalizedString(int message_id) {
  base::StringPiece res;
  if (!g_resource_data_pack->GetStringPiece(message_id, &res)) {
    LOG(FATAL) << "failed to load webkit string with id " << message_id;
  }

  // Data packs hold strings as either UTF8 or UTF16.
  string16 msg;
  switch (g_resource_data_pack->GetTextEncodingType()) {
  case ui::DataPack::UTF8:
    msg = UTF8ToUTF16(res);
    break;
  case ui::DataPack::UTF16:
    msg = string16(reinterpret_cast<const char16*>(res.data()),
                   res.length() / 2);
    break;
  case ui::DataPack::BINARY:
    NOTREACHED();
    break;
  }

  return msg;
}

base::StringPiece TestShellWebKitInit::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  switch (resource_id) {
  case IDR_BROKENIMAGE: {
    // Use webkit's broken image icon (16x16)
    static std::string broken_image_data;
    if (broken_image_data.empty()) {
      base::FilePath path = GetResourcesFilePath();
      // In order to match WebKit's colors for the missing image, we have to
      // use a PNG. The GIF doesn't have the color range needed to correctly
      // match the TIFF they use in Safari.
      path = path.AppendASCII("missingImage.png");
      bool success = file_util::ReadFileToString(path, &broken_image_data);
      if (!success) {
        LOG(FATAL) << "Failed reading: " << path.value();
      }
    }
    return broken_image_data;
  }
  case IDR_TEXTAREA_RESIZER: {
    // Use webkit's text area resizer image.
    static std::string resize_corner_data;
    if (resize_corner_data.empty()) {
      base::FilePath path = GetResourcesFilePath();
      path = path.AppendASCII("textAreaResizeCorner.png");
      bool success = file_util::ReadFileToString(path, &resize_corner_data);
      if (!success) {
        LOG(FATAL) << "Failed reading: " << path.value();
      }
    }
    return resize_corner_data;
  }

  case IDR_SEARCH_CANCEL:
  case IDR_SEARCH_CANCEL_PRESSED:
  case IDR_SEARCH_MAGNIFIER:
  case IDR_SEARCH_MAGNIFIER_RESULTS:
  case IDR_INPUT_SPEECH:
  case IDR_INPUT_SPEECH_RECORDING:
  case IDR_INPUT_SPEECH_WAITING:
    // TODO(flackr): Pass scale_factor to ResourceProvider.
    return TestShell::ResourceProvider(resource_id);

  default:
    break;
  }

  return base::StringPiece();
}

namespace webkit_glue {

bool DownloadUrl(const std::string& url, NSWindow* caller_window) {
  return false;
}

void DidLoadPlugin(const std::string& filename) {
}

void DidUnloadPlugin(const std::string& filename) {
}

}  // namespace webkit_glue
