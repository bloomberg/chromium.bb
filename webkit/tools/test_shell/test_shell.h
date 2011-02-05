/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_H_

#pragma once

#include <string>
#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/scoped_temp_dir.h"
#if defined(OS_MACOSX)
#include "base/lazy_instance.h"
#endif
#include "base/ref_counted.h"
#include "base/weak_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNavigationPolicy.h"
#include "ui/gfx/native_widget_types.h"
#include "webkit/tools/test_shell/layout_test_controller.h"
#include "webkit/tools/test_shell/webview_host.h"
#include "webkit/tools/test_shell/webwidget_host.h"

typedef std::list<gfx::NativeWindow> WindowList;

struct WebPreferences;
class GURL;
class TestNavigationEntry;
class TestNavigationController;
class TestNotificationPresenter;
class TestShellDevToolsAgent;
class TestShellDevToolsClient;
class TestWebViewDelegate;

namespace base {
class StringPiece;
}

namespace WebKit {
class WebDeviceOrientationClientMock;
class WebGeolocationClientMock;
class WebSpeechInputControllerMock;
class WebSpeechInputListener;
}

class TestShell : public base::SupportsWeakPtr<TestShell>  {
public:
    struct TestParams {
      // Load the test defaults.
      TestParams() : dump_tree(true), dump_pixels(false) {
      }

      // The kind of output we want from this test.
      bool dump_tree;
      bool dump_pixels;

      // Filename we dump pixels to (when pixel testing is enabled).
      FilePath pixel_file_name;
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

    // Called by the LayoutTestController to signal test completion.
    void TestFinished();

    // Called by LayoutTestController when a test hits the timeout, but does not
    // cause a hang. We can avoid killing TestShell in this case and still dump
    // the test results.
    void TestTimedOut();

    // Called to block the calling thread until TestFinished is called.
    void WaitTestFinished();

    void Show(WebKit::WebNavigationPolicy policy);

    // We use this to avoid relying on Windows focus during layout test mode.
    void SetFocus(WebWidgetHost* host, bool enable);

    LayoutTestController* layout_test_controller() {
      return layout_test_controller_.get();
    }
    TestWebViewDelegate* delegate() { return delegate_.get(); }
    TestWebViewDelegate* popup_delegate() { return popup_delegate_.get(); }
    TestNavigationController* navigation_controller() {
      return navigation_controller_.get();
    }
    TestNotificationPresenter* notification_presenter() {
      return notification_presenter_.get();
    }

    // Resets the LayoutTestController and EventSendingController.  Should be
    // called before loading a page, since some end-editing event notifications
    // may arrive after the previous page has finished dumping its text and
    // therefore end up in the next test's results if the messages are still
    // enabled.
    void ResetTestController();

    // Passes options from LayoutTestController through to the delegate (or
    // any other caller).
    bool ShouldDumpEditingCallbacks() {
      return layout_test_mode_ &&
             layout_test_controller_->ShouldDumpEditingCallbacks();
    }
    bool ShouldDumpFrameLoadCallbacks() {
      return layout_test_mode_ && (test_is_preparing_ || test_is_pending_) &&
             layout_test_controller_->ShouldDumpFrameLoadCallbacks();
    }
    bool ShouldDumpResourceLoadCallbacks() {
      return layout_test_mode_ && (test_is_preparing_ || test_is_pending_) &&
             layout_test_controller_->ShouldDumpResourceLoadCallbacks();
    }
    bool ShouldDumpResourceResponseMIMETypes() {
      return layout_test_mode_ && (test_is_preparing_ || test_is_pending_) &&
             layout_test_controller_->ShouldDumpResourceResponseMIMETypes();
    }
    bool ShouldDumpTitleChanges() {
      return layout_test_mode_ &&
             layout_test_controller_->ShouldDumpTitleChanges();
    }
    bool AcceptsEditing() {
      return layout_test_controller_->AcceptsEditing();
    }

    void LoadFile(const FilePath& file);
    void LoadURL(const GURL& url);
    void LoadURLForFrame(const GURL& url, const std::wstring& frame_name);
    void GoBackOrForward(int offset);
    void Reload();
    bool Navigate(const TestNavigationEntry& entry, bool reload);

    bool PromptForSaveFile(const wchar_t* prompt_title, FilePath* result);
    string16 GetDocumentText();
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

    // Called by the WebView delegate WindowObjectCleared() method, this
    // binds the layout_test_controller_ and other C++ controller classes to
    // window JavaScript objects so they can be accessed by layout tests.
    virtual void BindJSObjectsToWindow(WebKit::WebFrame* frame);

    // Runs a layout test.  Loads a single file (specified in params.test_url)
    // into the first available window, then dumps the requested text
    // representation to stdout. Returns false if the test cannot be run
    // because no windows are open.
    static bool RunFileTest(const TestParams& params);

    // Writes the back-forward list data for every open window into result.
    // Should call DumpBackForwardListOfWindow on each TestShell window.
    static void DumpAllBackForwardLists(string16* result);

    // Writes the single back-forward entry into result.
    void DumpBackForwardEntry(int index, string16* result);

    // Writes the back-forward list data for this test shell into result.
    void DumpBackForwardList(string16* result);

    // Dumps the output from given test as text and/or image depending on
    // the flags set.
    static void Dump(TestShell* shell);

    // Writes the image captured from the given web frame to the given file.
    // The returned string is the ASCII-ized MD5 sum of the image.
    static std::string DumpImage(skia::PlatformCanvas* canvas,
                                 const FilePath& path,
                                 const std::string& pixel_hash);

    static void ResetWebPreferences();

    static void SetAllowScriptsToCloseWindows();

    static void SetAccelerated2dCanvasEnabled(bool enabled);
    static void SetAcceleratedCompositingEnabled(bool enabled);

    WebPreferences* GetWebPreferences() { return web_prefs_; }

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

    // Set whether to dump when the loaded page has finished processing. This is
    // used with multiple load testing where normally we only want to have the
    // output from the last load.
    static void SetDumpWhenFinished(bool dump_when_finished) {
      dump_when_finished_ = dump_when_finished;
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

    WebKit::WebSpeechInputControllerMock* CreateSpeechInputControllerMock(
        WebKit::WebSpeechInputListener* listener);
    WebKit::WebSpeechInputControllerMock* speech_input_controller_mock();

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

protected:
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

    scoped_ptr<LayoutTestController> layout_test_controller_;
    scoped_ptr<TestNavigationController> navigation_controller_;
    scoped_ptr<TestNotificationPresenter> notification_presenter_;

    scoped_ptr<TestWebViewDelegate> delegate_;
    scoped_ptr<TestWebViewDelegate> popup_delegate_;

    base::WeakPtr<TestShell> devtools_shell_;
    scoped_ptr<TestShellDevToolsAgent> dev_tools_agent_;
    scoped_ptr<TestShellDevToolsClient> dev_tools_client_;
    scoped_ptr<WebKit::WebDeviceOrientationClientMock>
        device_orientation_client_mock_;
    scoped_ptr<WebKit::WebSpeechInputControllerMock>
        speech_input_controller_mock_;
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

    // True if a test shell dump should be made when the test is finished.
    static bool dump_when_finished_;

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
    static WebPreferences* web_prefs_;

#if defined(OS_WIN)
    // Used by the watchdog to know when it's finished.
    HANDLE finished_event_;
#endif

    // Dump the stats table counters on exit.
    bool dump_stats_table_on_exit_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_H_
