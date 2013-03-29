// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_H_


#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/string_piece.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNavigationPolicy.h"
#include "ui/gfx/native_widget_types.h"
#include "webkit/tools/test_shell/webview_host.h"
#include "webkit/tools/test_shell/webwidget_host.h"

typedef std::list<gfx::NativeWindow> WindowList;

#if defined(OS_WIN)
namespace ui {
class ScopedOleInitializer;
}
#endif

namespace webkit_glue {
struct WebPreferences;
}

class GURL;
class TestNavigationEntry;
class TestNavigationController;
class TestNotificationPresenter;
class TestShellDevToolsAgent;
class TestShellDevToolsClient;
class TestWebViewDelegate;

namespace WebKit {
class WebDeviceOrientationClientMock;
class WebGeolocationClientMock;
}

class TestShell : public base::SupportsWeakPtr<TestShell>  {
public:
    struct TestParams {
      // Load the test defaults.
      TestParams();

      // The kind of output we want from this test.
      bool dump_tree;
      bool dump_pixels;

      // Filename we dump pixels to (when pixel testing is enabled).
      base::FilePath pixel_file_name;
      // The md5 hash of the bitmap dump (when pixel testing is enabled).
      std::string pixel_hash;
      // URL of the test.
      std::string test_url;
    };

    TestShell();
    virtual ~TestShell();

    // Initialization and clean up of logging.
    static void InitLogging(bool suppress_error_dialogs,
                            bool running_layout_tests,
                            bool enable_gp_fault_error_box);
    static void CleanupLogging();

    // Initialization and clean up of a static member variable.
    static void InitializeTestShell(bool layout_test_mode,
                                    bool allow_external_pages);
    static void ShutdownTestShell();

    static bool layout_test_mode() { return layout_test_mode_; }
    static bool allow_external_pages() { return allow_external_pages_; }

    // Called from the destructor to let each platform do any necessary
    // cleanup.
    void PlatformCleanUp();

    WebKit::WebView* webView() {
      return m_webViewHost.get() ? m_webViewHost->webview() : NULL;
    }
    WebViewHost* webViewHost() { return m_webViewHost.get(); }
    WebKit::WebWidget* popup() {
      return m_popupHost ? m_popupHost->webwidget() : NULL;
    }
    WebWidgetHost* popupHost() { return m_popupHost; }

    bool is_loading() const { return is_loading_; }
    void set_is_loading(bool is_loading) { is_loading_ = is_loading; }

    bool allow_images() const { return allow_images_; }
    void set_allow_images(bool allow) { allow_images_ = allow; }

    bool allow_plugins() const { return allow_plugins_; }
    void set_allow_plugins(bool allow) { allow_plugins_ = allow; }

    bool allow_scripts() const { return allow_scripts_; }
    void set_allow_scripts(bool allow) { allow_scripts_ = allow; }

    void UpdateNavigationControls();

    // A new TestShell window will be opened with devtools url.
    // DevTools window can be opened manually via menu or automatically when
    // inspector's layout test url is passed from command line or console.
    void ShowDevTools();
    void CloseDevTools();

    // Called to signal test completion.
    void TestFinished();

    // Called when a test hits the timeout, but does not cause a hang.  We can
    // avoid killing TestShell in this case and still dump the test results.
    void TestTimedOut();

    // Called to block the calling thread until TestFinished is called.
    void WaitTestFinished();

    void Show(WebKit::WebNavigationPolicy policy);

    // We use this to avoid relying on Windows focus during layout test mode.
    void SetFocus(WebWidgetHost* host, bool enable);

    TestWebViewDelegate* delegate() { return delegate_.get(); }
    TestWebViewDelegate* popup_delegate() { return popup_delegate_.get(); }
    TestNavigationController* navigation_controller() {
      return navigation_controller_.get();
    }
    TestNotificationPresenter* notification_presenter() {
      return notification_presenter_.get();
    }

    // Resets TestWebViewDelegate. Should be called before loading a page,
    // since some end-editing event notifications may arrive after the previous
    // page has finished dumping its text and therefore end up in the next
    // test's results if the messages are still enabled.
    void ResetTestController();

    bool AcceptsEditing() {
      return true;
    }

    void LoadFile(const base::FilePath& file);
    void LoadURL(const GURL& url);
    void LoadURLForFrame(const GURL& url, const base::string16& frame_name);
    void GoBackOrForward(int offset);
    void Reload();
    bool Navigate(const TestNavigationEntry& entry, bool reload);

    bool PromptForSaveFile(const wchar_t* prompt_title, base::FilePath* result);
    base::string16 GetDocumentText();
    void DumpDocumentText();
    void DumpRenderTree();

    gfx::NativeWindow mainWnd() const { return m_mainWnd; }
    gfx::NativeView webViewWnd() const { return m_webViewHost->view_handle(); }
    gfx::NativeEditView editWnd() const { return m_editWnd; }
    gfx::NativeView popupWnd() const { return m_popupHost->view_handle(); }

    static WindowList* windowList() { return window_list_; }

    // If shell is non-null, then *shell is assigned upon successful return
    static bool CreateNewWindow(const GURL& starting_url,
                                TestShell** shell = NULL);

    static void DestroyWindow(gfx::NativeWindow windowHandle);

    // Remove the given window from window_list_, return true if it was in the
    // list and was removed and false otherwise.
    static bool RemoveWindowFromList(gfx::NativeWindow window);

    // Implements CreateWebView for TestWebViewDelegate, which in turn
    // is called as a WebViewDelegate.
    WebKit::WebView* CreateWebView();
    WebKit::WebWidget* CreatePopupWidget();
    void ClosePopup();

#if defined(OS_WIN)
    static ATOM RegisterWindowClass();
#endif

    // Writes the back-forward list data for every open window into result.
    // Should call DumpBackForwardListOfWindow on each TestShell window.
    static void DumpAllBackForwardLists(base::string16* result);

    // Writes the single back-forward entry into result.
    void DumpBackForwardEntry(int index, base::string16* result);

    // Writes the back-forward list data for this test shell into result.
    void DumpBackForwardList(base::string16* result);

    static void ResetWebPreferences();

    static void SetAllowScriptsToCloseWindows();

    static void SetAccelerated2dCanvasEnabled(bool enabled);
    static void SetAcceleratedCompositingEnabled(bool enabled);

    static webkit_glue::WebPreferences* GetWebPreferences();

    // Some layout tests hardcode a file:///tmp/LayoutTests URL.  We get around
    // this by substituting "tmp" with the path to the LayoutTests parent dir.
    static std::string RewriteLocalUrl(const std::string& url);

    // Set the timeout for running a test.
    static void SetFileTestTimeout(int timeout_ms) {
      file_test_timeout_ms_ = timeout_ms;
    }

    // Get the timeout for running a test.
    static int GetLayoutTestTimeout() { return file_test_timeout_ms_; }

    // Get the timeout killing an unresponsive TestShell.
    // Make it a bit longer than the regular timeout to avoid killing the
    // TestShell process unless we really need to.
    static int GetLayoutTestTimeoutForWatchDog() {
      return (load_count_ * file_test_timeout_ms_) + 1000;
    }

    // Set the JavaScript flags to use. This is a vector as when multiple loads
    // are specified each load can have different flags passed.
    static void SetJavaScriptFlags(std::vector<std::string> js_flags) {
      js_flags_ = js_flags;
    }

    // Set the number of times to load each URL.
    static void SetMultipleLoad(int load_count) {
      load_count_ = load_count;
    }

    // Get the number of times to load each URL.
    static int GetLoadCount() { return load_count_; }

    // Get the JavaScript flags for a specific load
    static std::string GetJSFlagsForLoad(size_t load) {
      if (load >= js_flags_.size()) return "";
      return js_flags_[load];
    }

#if defined(OS_WIN)
    // Access to the finished event.  Used by the static WatchDog
    // thread.
    HANDLE finished_event() { return finished_event_; }
#endif

    // Have the shell print the StatsTable to stdout on teardown.
    void DumpStatsTableOnExit() { dump_stats_table_on_exit_ = true; }

    void CallJSGC();

    void set_is_modal(bool value) { is_modal_ = value; }
    bool is_modal() const { return is_modal_; }

    const TestParams* test_params() { return test_params_; }
    void set_test_params(const TestParams* test_params) {
      test_params_ = test_params;
    }

#if defined(OS_MACOSX)
    // handle cleaning up a shell given the associated window
    static void DestroyAssociatedShell(gfx::NativeWindow handle);
#endif

    // Show the "attach to me" dialog, for debugging test shell startup.
    static void ShowStartupDebuggingDialog();

    // This is called indirectly by the modules that need access resources.
    static base::StringPiece ResourceProvider(int key);

    TestShellDevToolsAgent* dev_tools_agent() {
      return dev_tools_agent_.get();
    }

    WebKit::WebDeviceOrientationClientMock* device_orientation_client_mock();

    WebKit::WebGeolocationClientMock* geolocation_client_mock();

protected:
    void CreateDevToolsClient(TestShellDevToolsAgent* agent);
    bool Initialize(const GURL& starting_url);
    bool IsSVGTestURL(const GURL& url);
    void SizeToSVG();
    void SizeToDefault();
    void SizeTo(int width, int height);
    void ResizeSubViews();

    // Set the focus in interactive mode (pass through to relevant system call).
    void InteractiveSetFocus(WebWidgetHost* host, bool enable);

    enum UIControl {
      BACK_BUTTON,
      FORWARD_BUTTON,
      STOP_BUTTON
    };

    void EnableUIControl(UIControl control, bool is_enabled);

#if defined(OS_WIN)
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK EditWndProc(HWND, UINT, WPARAM, LPARAM);
#endif

    static void PlatformShutdown();

    gfx::NativeWindow       m_mainWnd;
    gfx::NativeEditView     m_editWnd;
    scoped_ptr<WebViewHost> m_webViewHost;
    WebWidgetHost*          m_popupHost;
#if defined(OS_WIN)
    WNDPROC                 default_edit_wnd_proc_;
#endif

    // Primitive focus controller for layout test mode.
    WebWidgetHost*          m_focusedWidgetHost;

private:
    // A set of all our windows.
    static WindowList* window_list_;
#if defined(OS_MACOSX)
    typedef std::map<gfx::NativeWindow, TestShell *> WindowMap;
    static base::LazyInstance<WindowMap> window_map_;
#endif

#if defined(OS_WIN)
    static HINSTANCE instance_handle_;

    static ui::ScopedOleInitializer* ole_initializer_;
#endif

    // True if developer extras should be enabled.
    static bool developer_extras_enabled_;

    // True when the app is being run using the --layout-tests switch.
    static bool layout_test_mode_;

    // True when we wish to allow test shell to load external pages like
    // www.google.com even when in --layout-test mode (used for QA to
    // produce images of the rendered page)
    static bool allow_external_pages_;

    // Default timeout in ms for file page loads when in layout test mode.
    static int file_test_timeout_ms_;

    scoped_ptr<TestNavigationController> navigation_controller_;
    scoped_ptr<TestNotificationPresenter> notification_presenter_;

    scoped_ptr<TestWebViewDelegate> delegate_;
    scoped_ptr<TestWebViewDelegate> popup_delegate_;

    base::WeakPtr<TestShell> devtools_shell_;
    scoped_ptr<TestShellDevToolsAgent> dev_tools_agent_;
    scoped_ptr<TestShellDevToolsClient> dev_tools_client_;
    scoped_ptr<WebKit::WebDeviceOrientationClientMock>
        device_orientation_client_mock_;
    scoped_ptr<WebKit::WebGeolocationClientMock> geolocation_client_mock_;

    const TestParams* test_params_;

    // True while a test is preparing to run
    static bool test_is_preparing_;

    // True while a test is running
    static bool test_is_pending_;

    // Number of times to load each URL.
    static int load_count_;

    // JavaScript flags. Each element in the vector contains a set of flags as
    // a string (e.g. "--xxx --yyy"). Each flag set is used for separate loads
    // of each URL.
    static std::vector<std::string> js_flags_;

    // True if we're testing the accelerated canvas 2d path.
    static bool accelerated_2d_canvas_enabled_;

    // True if we're testing the accelerated compositing.
    static bool accelerated_compositing_enabled_;

    // True if driven from a nested message loop.
    bool is_modal_;

    // True if the page is loading.
    bool is_loading_;

    bool allow_images_;
    bool allow_plugins_;
    bool allow_scripts_;

    // The preferences for the test shell.
    static webkit_glue::WebPreferences* web_prefs_;

#if defined(OS_WIN)
    // Used by the watchdog to know when it's finished.
    HANDLE finished_event_;
#endif

    // Dump the stats table counters on exit.
    bool dump_stats_table_on_exit_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_H_
