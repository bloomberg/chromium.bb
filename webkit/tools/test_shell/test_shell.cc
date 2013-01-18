// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#undef LOG

#include "webkit/tools/test_shell/test_shell.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/debug/debug_on_start_win.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/md5.h"
#include "base/message_loop.h"
#include "base/metrics/stats_table.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "googleurl/src/url_util.h"
#include "grit/webkit_strings.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_filter.h"
#include "skia/ext/bitmap_platform_device.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityObject.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDeviceOrientationClientMock.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationClientMock.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptController.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/tools/test_shell/notification_presenter.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_shell_devtools_agent.h"
#include "webkit/tools/test_shell/test_shell_devtools_client.h"
#include "webkit/tools/test_shell/test_shell_request_context.h"
#include "webkit/tools/test_shell/test_shell_switches.h"
#include "webkit/tools/test_shell/test_webview_delegate.h"
#include "webkit/user_agent/user_agent.h"
#include "webkit/user_agent/user_agent_util.h"

using WebKit::WebCanvas;
using WebKit::WebFrame;
using WebKit::WebNavigationPolicy;
using WebKit::WebRect;
using WebKit::WebScriptController;
using WebKit::WebSize;
using WebKit::WebURLRequest;
using WebKit::WebView;
using webkit_glue::WebPreferences;

namespace {

// Default timeout in ms for file page loads when in layout test mode.
const int kDefaultFileTestTimeoutMillisecs = 10 * 1000;

// Content area size for newly created windows.
const int kTestWindowWidth = 800;
const int kTestWindowHeight = 600;

// The W3C SVG layout tests use a different size than the other layout
// tests.
const int kSVGTestWindowWidth = 480;
const int kSVGTestWindowHeight = 360;

// URLRequestTestShellFileJob is used to serve the inspector
class URLRequestTestShellFileJob : public net::URLRequestFileJob {
 public:
  static net::URLRequestJob* InspectorFactory(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const std::string& scheme) {
    FilePath path;
    PathService::Get(base::DIR_EXE, &path);
    path = path.AppendASCII("resources");
    path = path.AppendASCII("inspector");
    path = path.AppendASCII(request->url().path().substr(1));
    return new URLRequestTestShellFileJob(request, network_delegate, path);
  }

 private:
  URLRequestTestShellFileJob(net::URLRequest* request,
                             net::NetworkDelegate* network_delegate,
                             const FilePath& path)
      : net::URLRequestFileJob(request, network_delegate, path) {
  }
  virtual ~URLRequestTestShellFileJob() { }

  DISALLOW_COPY_AND_ASSIGN(URLRequestTestShellFileJob);
};


}  // namespace

// Initialize static member variable
WindowList* TestShell::window_list_;
WebPreferences* TestShell::web_prefs_ = NULL;
bool TestShell::developer_extras_enabled_ = false;
bool TestShell::layout_test_mode_ = false;
bool TestShell::allow_external_pages_ = false;
int TestShell::file_test_timeout_ms_ = kDefaultFileTestTimeoutMillisecs;
bool TestShell::test_is_preparing_ = false;
bool TestShell::test_is_pending_ = false;
int TestShell::load_count_ = 1;
std::vector<std::string> TestShell::js_flags_;
bool TestShell::accelerated_2d_canvas_enabled_ = false;
bool TestShell::accelerated_compositing_enabled_ = false;

TestShell::TestParams::TestParams()
    : dump_tree(true),
      dump_pixels(false) {
}

TestShell::TestShell()
    : m_mainWnd(NULL),
      m_editWnd(NULL),
      m_webViewHost(NULL),
      m_popupHost(NULL),
      m_focusedWidgetHost(NULL),
#if defined(OS_WIN)
      default_edit_wnd_proc_(0),
#endif
      test_params_(NULL),
      is_modal_(false),
      is_loading_(false),
      allow_images_(true),
      allow_plugins_(true),
      allow_scripts_(true),
      dump_stats_table_on_exit_(false) {
    delegate_.reset(new TestWebViewDelegate(this));
    popup_delegate_.reset(new TestWebViewDelegate(this));
    navigation_controller_.reset(new TestNavigationController(this));
    notification_presenter_.reset(new TestNotificationPresenter(this));

    net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
    filter->AddHostnameHandler("test-shell-resource", "inspector",
                               &URLRequestTestShellFileJob::InspectorFactory);
    url_util::AddStandardScheme("test-shell-resource");
    webkit_glue::SetUserAgent(webkit_glue::BuildUserAgentFromProduct(
        "TestShell/0.0.0.0"), false);
}

TestShell::~TestShell() {
  delegate_->RevokeDragDrop();

  // DevTools frontend page is supposed to be navigated only once and
  // loading another URL in that Page is an error.
  if (!dev_tools_client_.get()) {
    // Navigate to an empty page to fire all the destruction logic for the
    // current page.
    LoadURL(GURL("about:blank"));
  }

  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->RemoveHostnameHandler("test-shell-resource", "inspector");

  // Call GC twice to clean up garbage.
  CallJSGC();
  CallJSGC();

  // Destroy the WebView before the TestWebViewDelegate.
  m_webViewHost.reset();

  CloseDevTools();

  PlatformCleanUp();

  base::StatsTable *table = base::StatsTable::current();
  if (dump_stats_table_on_exit_) {
    // Dump the stats table.
    printf("<stats>\n");
    if (table != NULL) {
      int counter_max = table->GetMaxCounters();
      for (int index = 0; index < counter_max; index++) {
        std::string name(table->GetRowName(index));
        if (name.length() > 0) {
          int value = table->GetRowValue(index);
          printf("%s:\t%d\n", name.c_str(), value);
        }
      }
    }
    printf("</stats>\n");
  }
}

void TestShell::UpdateNavigationControls() {
  int current_index = navigation_controller()->GetCurrentEntryIndex();
  int max_index = navigation_controller()->GetEntryCount() - 1;

  EnableUIControl(BACK_BUTTON, current_index > 0);
  EnableUIControl(FORWARD_BUTTON, current_index < max_index);
  EnableUIControl(STOP_BUTTON, is_loading_);
}

bool TestShell::CreateNewWindow(const GURL& starting_url,
                                TestShell** result) {
  TestShell* shell = new TestShell();
  bool rv = shell->Initialize(starting_url);
  if (rv) {
    if (result)
      *result = shell;
    TestShell::windowList()->push_back(shell->m_mainWnd);
  }
  return rv;
}

void TestShell::ShutdownTestShell() {
  PlatformShutdown();
  SimpleResourceLoaderBridge::Shutdown();
  delete window_list_;
  delete TestShell::web_prefs_;
}

// All fatal log messages (e.g. DCHECK failures) imply unit test failures
static void UnitTestAssertHandler(const std::string& str) {
  FAIL() << str;
}

// static
void TestShell::InitLogging(bool suppress_error_dialogs,
                            bool layout_test_mode,
                            bool enable_gp_fault_error_box) {
    if (suppress_error_dialogs)
        logging::SetLogAssertHandler(UnitTestAssertHandler);

#if defined(OS_WIN)
    if (!IsDebuggerPresent()) {
        UINT new_flags = SEM_FAILCRITICALERRORS |
                         SEM_NOOPENFILEERRORBOX;
        if (!enable_gp_fault_error_box)
            new_flags |= SEM_NOGPFAULTERRORBOX;

        // Preserve existing error mode, as discussed at
        // http://blogs.msdn.com/oldnewthing/archive/2004/07/27/198410.aspx
        UINT existing_flags = SetErrorMode(new_flags);
        SetErrorMode(existing_flags | new_flags);
    }
#endif

    // Only log to a file if we're running layout tests. This prevents debugging
    // output from disrupting whether or not we pass.
    logging::LoggingDestination destination =
        logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG;
    if (layout_test_mode)
      destination = logging::LOG_ONLY_TO_FILE;

    // We might have multiple test_shell processes going at once
    FilePath log_filename;
    PathService::Get(base::DIR_EXE, &log_filename);
    log_filename = log_filename.AppendASCII("test_shell.log");
    logging::InitLogging(
        log_filename.value().c_str(),
        destination,
        logging::LOCK_LOG_FILE,
        logging::DELETE_OLD_LOG_FILE,
        logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

    // we want process and thread IDs because we may have multiple processes
    logging::SetLogItems(true, true, false, true);

    // Turn on logging of notImplemented()s inside WebKit, but only if we're
    // not running layout tests (because otherwise they'd corrupt the test
    // output).
    if (!layout_test_mode)
      webkit_glue::EnableWebCoreLogChannels("NotYetImplemented");
}

// static
void TestShell::CleanupLogging() {
    logging::CloseLogFile();
}

// static
void TestShell::SetAllowScriptsToCloseWindows() {
  if (web_prefs_)
    web_prefs_->allow_scripts_to_close_windows = true;
}

// static
void TestShell::SetAccelerated2dCanvasEnabled(bool enabled) {
  accelerated_2d_canvas_enabled_ = enabled;
}

// static
void TestShell::SetAcceleratedCompositingEnabled(bool enabled) {
  accelerated_compositing_enabled_ = enabled;
}

// static
void TestShell::ResetWebPreferences() {
    DCHECK(web_prefs_);

    // Match the settings used by Mac DumpRenderTree, with the exception of
    // fonts.
    if (web_prefs_) {
        *web_prefs_ = WebPreferences();

#if defined(OS_MACOSX)
        web_prefs_->serif_font_family_map[WebPreferences::kCommonScript] =
            ASCIIToUTF16("Times");
        web_prefs_->cursive_font_family_map[WebPreferences::kCommonScript] =
            ASCIIToUTF16("Apple Chancery");
        web_prefs_->fantasy_font_family_map[WebPreferences::kCommonScript] =
            ASCIIToUTF16("Papyrus");
#else
        // NOTE: case matters here, this must be 'times new roman', else
        // some layout tests fail.
        web_prefs_->serif_font_family_map[WebPreferences::kCommonScript] =
            ASCIIToUTF16("times new roman");

        // These two fonts are picked from the intersection of
        // Win XP font list and Vista font list :
        //   http://www.microsoft.com/typography/fonts/winxp.htm
        //   http://blogs.msdn.com/michkap/archive/2006/04/04/567881.aspx
        // Some of them are installed only with CJK and complex script
        // support enabled on Windows XP and are out of consideration here.
        // (although we enabled both on our buildbots.)
        // They (especially Impact for fantasy) are not typical cursive
        // and fantasy fonts, but it should not matter for layout tests
        // as long as they're available.
        web_prefs_->cursive_font_family_map[WebPreferences::kCommonScript] =
            ASCIIToUTF16("Comic Sans MS");
        web_prefs_->fantasy_font_family_map[WebPreferences::kCommonScript] =
            ASCIIToUTF16("Impact");
#endif
        web_prefs_->standard_font_family_map[WebPreferences::kCommonScript] =
            web_prefs_->serif_font_family_map[WebPreferences::kCommonScript];
        web_prefs_->fixed_font_family_map[WebPreferences::kCommonScript] =
            ASCIIToUTF16("Courier");
        web_prefs_->sans_serif_font_family_map[WebPreferences::kCommonScript] =
            ASCIIToUTF16("Helvetica");

        web_prefs_->default_encoding = "ISO-8859-1";
        web_prefs_->default_font_size = 16;
        web_prefs_->default_fixed_font_size = 13;
        web_prefs_->minimum_font_size = 0;
        web_prefs_->minimum_logical_font_size = 9;
        web_prefs_->javascript_can_open_windows_automatically = true;
        web_prefs_->dom_paste_enabled = true;
        web_prefs_->developer_extras_enabled = !layout_test_mode_ ||
            developer_extras_enabled_;
        web_prefs_->site_specific_quirks_enabled = true;
        web_prefs_->shrinks_standalone_images_to_fit = false;
        web_prefs_->uses_universal_detector = false;
        web_prefs_->text_areas_are_resizable = false;
        web_prefs_->java_enabled = false;
        web_prefs_->allow_scripts_to_close_windows = false;
        web_prefs_->javascript_can_access_clipboard = true;
        web_prefs_->xss_auditor_enabled = false;
        // It's off by default for Chrome, but we don't want to
        // lose the coverage of dynamic font tests in webkit test.
        web_prefs_->remote_fonts_enabled = true;
        web_prefs_->local_storage_enabled = true;
        web_prefs_->application_cache_enabled = true;
        web_prefs_->databases_enabled = true;
        web_prefs_->allow_file_access_from_file_urls = true;
        // LayoutTests were written with Safari Mac in mind which does not allow
        // tabbing to links by default.
        web_prefs_->tabs_to_links = false;
        web_prefs_->accelerated_2d_canvas_enabled =
            accelerated_2d_canvas_enabled_;
        web_prefs_->accelerated_compositing_enabled =
            accelerated_compositing_enabled_;
        // Allow those layout tests running as local files, i.e. under
        // LayoutTests/http/tests/local, to access http server.
        if (layout_test_mode_)
          web_prefs_->allow_universal_access_from_file_urls = true;
        web_prefs_->visual_word_movement_enabled = false;
    }
}

// static
WebPreferences* TestShell::GetWebPreferences() {
  DCHECK(web_prefs_);
  return web_prefs_;
}

// static
bool TestShell::RemoveWindowFromList(gfx::NativeWindow window) {
  WindowList::iterator entry =
      std::find(TestShell::windowList()->begin(),
                TestShell::windowList()->end(),
                window);
  if (entry != TestShell::windowList()->end()) {
    TestShell::windowList()->erase(entry);
    return true;
  }

  return false;
}

void TestShell::TestTimedOut() {
  puts("#TEST_TIMED_OUT\n");
  TestFinished();
}

void TestShell::Show(WebNavigationPolicy policy) {
  delegate_->show(policy);
}

void TestShell::DumpBackForwardEntry(int index, string16* result) {
  int current_index = navigation_controller_->GetLastCommittedEntryIndex();

  std::string content_state =
      navigation_controller_->GetEntryAtIndex(index)->GetContentState();
  if (content_state.empty()) {
    content_state = webkit_glue::CreateHistoryStateForURL(
        navigation_controller_->GetEntryAtIndex(index)->GetURL());
  }

  result->append(
      webkit_glue::DumpHistoryState(content_state, 8, index == current_index));
}

void TestShell::DumpBackForwardList(string16* result) {
  result->append(ASCIIToUTF16(
                     "\n============== Back Forward List ==============\n"));

  for (int i = 0; i < navigation_controller_->GetEntryCount(); ++i)
    DumpBackForwardEntry(i, result);

  result->append(ASCIIToUTF16(
                     "===============================================\n"));
}

void TestShell::CallJSGC() {
  webView()->mainFrame()->collectGarbage();
}

WebView* TestShell::CreateWebView() {
  // If we're running layout tests, only open a new window if the test has
  // called testRunner.setCanOpenWindows()
  if (layout_test_mode_)
    return NULL;

  TestShell* new_win;
  if (!CreateNewWindow(GURL(), &new_win))
    return NULL;

  return new_win->webView();
}

void TestShell::ShowDevTools() {
  if (!devtools_shell_) {
    FilePath dir_exe;
    PathService::Get(base::DIR_EXE, &dir_exe);
    FilePath devtools_path =
        dir_exe.AppendASCII("resources/inspector/devtools.html");
    TestShell* devtools_shell;
    TestShell::CreateNewWindow(GURL(devtools_path.value()),
                               &devtools_shell);
    devtools_shell_ = devtools_shell->AsWeakPtr();
    devtools_shell_->CreateDevToolsClient(dev_tools_agent_.get());
  }
  DCHECK(devtools_shell_);
  devtools_shell_->Show(WebKit::WebNavigationPolicyNewWindow);
}

void TestShell::CloseDevTools() {
  if (devtools_shell_)
    devtools_shell_->DestroyWindow(devtools_shell_->mainWnd());
}

void TestShell::CreateDevToolsClient(TestShellDevToolsAgent *agent) {
  dev_tools_client_.reset(new TestShellDevToolsClient(agent,
                                                      webView()));
}

bool TestShell::IsSVGTestURL(const GURL& url) {
  return url.is_valid() && url.spec().find("W3C-SVG-1.1") != std::string::npos;
}

void TestShell::SizeToSVG() {
  SizeTo(kSVGTestWindowWidth, kSVGTestWindowHeight);
}

void TestShell::SizeToDefault() {
  SizeTo(kTestWindowWidth, kTestWindowHeight);
}

void TestShell::ResetTestController() {
  notification_presenter_->Reset();
  delegate_->Reset();
  if (geolocation_client_mock_.get())
    geolocation_client_mock_->resetMock();
}

void TestShell::LoadFile(const FilePath& file) {
  LoadURLForFrame(net::FilePathToFileURL(file), string16());
}

void TestShell::LoadURL(const GURL& url) {
  LoadURLForFrame(url, string16());
}

bool TestShell::Navigate(const TestNavigationEntry& entry, bool reload) {
  // Get the right target frame for the entry.
  WebFrame* frame = webView()->mainFrame();
  if (!entry.GetTargetFrame().empty())
      frame = webView()->findFrameByName(entry.GetTargetFrame());

  // TODO(mpcomplete): should we clear the target frame, or should
  // back/forward navigations maintain the target frame?

  // A navigation resulting from loading a javascript URL should not be
  // treated as a browser initiated event.  Instead, we want it to look as if
  // the page initiated any load resulting from JS execution.
  if (!entry.GetURL().SchemeIs("javascript")) {
    delegate_->set_pending_extra_data(
        new TestShellExtraData(entry.GetPageID()));
  }

  // If we are reloading, then WebKit will use the state of the current page.
  // Otherwise, we give it the state to navigate to.
  if (reload) {
    frame->reload(false);
  } else if (!entry.GetContentState().empty()) {
    DCHECK_NE(entry.GetPageID(), -1);
    frame->loadHistoryItem(
        webkit_glue::HistoryItemFromString(entry.GetContentState()));
  } else {
    DCHECK_EQ(entry.GetPageID(), -1);
    frame->loadRequest(WebURLRequest(entry.GetURL()));
  }

  // In case LoadRequest failed before DidCreateDataSource was called.
  delegate_->set_pending_extra_data(NULL);

  // Restore focus to the main frame prior to loading new request.
  // This makes sure that we don't have a focused iframe. Otherwise, that
  // iframe would keep focus when the SetFocus called immediately after
  // LoadRequest, thus making some tests fail (see http://b/issue?id=845337
  // for more details).
  webView()->setFocusedFrame(frame);
  SetFocus(webViewHost(), true);

  return true;
}

void TestShell::GoBackOrForward(int offset) {
    navigation_controller_->GoToOffset(offset);
}

void TestShell::DumpDocumentText() {
  FilePath file_path;
  if (!PromptForSaveFile(L"Dump document text", &file_path))
      return;

  const std::string data =
      UTF16ToUTF8(webkit_glue::DumpDocumentText(webView()->mainFrame()));
  file_util::WriteFile(file_path, data.c_str(), data.length());
}

void TestShell::DumpRenderTree() {
  FilePath file_path;
  if (!PromptForSaveFile(L"Dump render tree", &file_path))
    return;

  const std::string data =
      UTF16ToUTF8(webkit_glue::DumpRenderer(webView()->mainFrame()));
  file_util::WriteFile(file_path, data.c_str(), data.length());
}

string16 TestShell::GetDocumentText() {
  return webkit_glue::DumpDocumentText(webView()->mainFrame());
}

void TestShell::Reload() {
    navigation_controller_->Reload();
}

void TestShell::SetFocus(WebWidgetHost* host, bool enable) {
  if (!layout_test_mode_) {
    InteractiveSetFocus(host, enable);
  } else {
    // Simulate the effects of InteractiveSetFocus(), which includes calling
    // both setFocus() and setIsActive().
    if (enable) {
      if (m_focusedWidgetHost != host) {
        if (m_focusedWidgetHost)
          m_focusedWidgetHost->webwidget()->setFocus(false);
        webView()->setIsActive(enable);
        host->webwidget()->setFocus(enable);
        m_focusedWidgetHost = host;
      }
    } else {
      if (m_focusedWidgetHost == host) {
        host->webwidget()->setFocus(enable);
        webView()->setIsActive(enable);
        m_focusedWidgetHost = NULL;
      }
    }
  }
}

WebKit::WebDeviceOrientationClientMock*
TestShell::device_orientation_client_mock() {
  if (!device_orientation_client_mock_.get()) {
    device_orientation_client_mock_.reset(
        WebKit::WebDeviceOrientationClientMock::create());
  }
  return device_orientation_client_mock_.get();
}

WebKit::WebGeolocationClientMock* TestShell::geolocation_client_mock() {
  if (!geolocation_client_mock_.get()) {
    geolocation_client_mock_.reset(
        WebKit::WebGeolocationClientMock::create());
  }
  return geolocation_client_mock_.get();
}
