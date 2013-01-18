// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_test.h"

using WebKit::WebFrame;
using WebKit::WebScriptSource;
using WebKit::WebString;

#if defined(OS_WIN)
#define TEST_PLUGIN_NAME "npapi_test_plugin.dll"
#elif defined(OS_MACOSX)
#define TEST_PLUGIN_NAME "npapi_test_plugin.plugin"
#elif defined(OS_POSIX)
#define TEST_PLUGIN_NAME "libnpapi_test_plugin.so"
#endif

namespace {

const char kPluginsDir[] = "plugins";

}

// Ignore these until 64-bit plugin build is fixed. http://crbug.com/18337
#if !defined(ARCH_CPU_64_BITS)
// Provides functionality for creating plugin tests.
class PluginTest : public TestShellTest {
 public:
  PluginTest() {
    FilePath executable_directory;
    PathService::Get(base::DIR_EXE, &executable_directory);
    plugin_src_ = executable_directory.AppendASCII(TEST_PLUGIN_NAME);
    CHECK(file_util::PathExists(plugin_src_));

    plugin_file_path_ = executable_directory.AppendASCII(kPluginsDir);
    file_util::CreateDirectory(plugin_file_path_);

    plugin_file_path_ = plugin_file_path_.AppendASCII(TEST_PLUGIN_NAME);
  }

  void CopyTestPlugin() {
    // On Linux, we need to delete before copying because if the plugin is a
    // hard link, the copy will fail.
    DeleteTestPlugin();
    ASSERT_TRUE(file_util::CopyDirectory(plugin_src_, plugin_file_path_, true));
  }

  void DeleteTestPlugin() {
    file_util::Delete(plugin_file_path_, true);
  }

  virtual void SetUp() {
    CopyTestPlugin();
    TestShellTest::SetUp();
  }

  FilePath plugin_src_;
  FilePath plugin_file_path_;
};

// Tests navigator.plugins.refresh() works.
TEST_F(PluginTest, Refresh) {
  std::string html = "\
      <div id='result'>Test running....</div>\
      <script>\
      function check() {\
      var l = navigator.plugins.length;\
      var result = document.getElementById('result');\
      for(var i = 0; i < l; i++) {\
        if (navigator.plugins[i].filename == '" TEST_PLUGIN_NAME "') {\
          result.innerHTML = 'DONE';\
          break;\
        }\
      }\
      \
      if (result.innerHTML != 'DONE')\
        result.innerHTML = 'FAIL';\
      }\
      </script>\
      ";

  WebScriptSource call_check(
      WebString::fromUTF8("check();"));
  WebScriptSource refresh(
      WebString::fromUTF8("navigator.plugins.refresh(false)"));

  // Remove any leftover from previous tests if they exist.  We must also
  // refresh WebKit's plugin cache since it might have had an entry for the
  // test plugin from a previous test.
  DeleteTestPlugin();
  ASSERT_FALSE(file_util::PathExists(plugin_file_path_));
  test_shell_->webView()->mainFrame()->executeScript(refresh);

  test_shell_->webView()->mainFrame()->loadHTMLString(
      html, GURL("about:blank"));
  test_shell_->WaitTestFinished();

  std::string text;
  test_shell_->webView()->mainFrame()->executeScript(call_check);
  text = test_shell_->webView()->mainFrame()->contentAsText(10000).utf8();
  ASSERT_EQ(text, "FAIL");

  CopyTestPlugin();

  test_shell_->webView()->mainFrame()->executeScript(refresh);
  test_shell_->webView()->mainFrame()->executeScript(call_check);
  text = test_shell_->webView()->mainFrame()->contentAsText(10000).utf8();
  ASSERT_EQ(text, "DONE");
}

// Tests that if a frame is deleted as a result of calling NPP_HandleEvent, we
// don't crash.
TEST_F(PluginTest, DeleteFrameDuringEvent) {
  FilePath test_html = data_dir_;
  test_html = test_html.AppendASCII(kPluginsDir);
  test_html = test_html.AppendASCII("delete_frame.html");
  test_shell_->LoadFile(test_html);
  test_shell_->WaitTestFinished();

  WebKit::WebMouseEvent input;
  input.button = WebKit::WebMouseEvent::ButtonLeft;
  input.x = 50;
  input.y = 50;
  input.type = WebKit::WebInputEvent::MouseUp;
  test_shell_->webView()->handleInputEvent(input);

  // No crash means we passed.
}

// Tests that a forced reload of the plugin will not crash.
TEST_F(PluginTest, ForceReload) {
  FilePath test_html = data_dir_;
  test_html = test_html.AppendASCII(kPluginsDir);
  test_html = test_html.AppendASCII("force_reload.html");
  test_shell_->LoadFile(test_html);
  test_shell_->WaitTestFinished();

  // No crash means we passed.
}

#if defined(OS_WIN)
BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lparam) {
  HWND* plugin_hwnd = reinterpret_cast<HWND*>(lparam);
  if (*plugin_hwnd) {
    // More than one child window found, unexpected.
    plugin_hwnd = NULL;
    return FALSE;
  }
  *plugin_hwnd = hwnd;
  return TRUE;
}

// Tests that hiding/showing the parent frame hides/shows the plugin.
// Fails for WIN. http://crbug.com/111601
#if defined(OS_WIN)
#define MAYBE_PluginVisibilty DISABLED_PluginVisibilty
#else
#define MAYBE_PluginVisibilty PluginVisibilty
#endif

TEST_F(PluginTest, MAYBE_PluginVisibilty) {
  FilePath test_html = data_dir_;
  test_html = test_html.AppendASCII(kPluginsDir);
  test_html = test_html.AppendASCII("plugin_visibility.html");
  test_shell_->LoadFile(test_html);
  test_shell_->WaitTestFinished();

  WebFrame* main_frame = test_shell_->webView()->mainFrame();
  HWND frame_hwnd = test_shell_->webViewWnd();
  HWND plugin_hwnd = NULL;
  EnumChildWindows(frame_hwnd, EnumChildProc,
      reinterpret_cast<LPARAM>(&plugin_hwnd));
  ASSERT_TRUE(plugin_hwnd != NULL);
  ASSERT_FALSE(IsWindowVisible(plugin_hwnd));

  main_frame->executeScript(WebString::fromUTF8("showPlugin(true)"));
  ASSERT_TRUE(IsWindowVisible(plugin_hwnd));

  main_frame->executeScript(WebString::fromUTF8("showFrame(false)"));
  ASSERT_FALSE(IsWindowVisible(plugin_hwnd));

  main_frame->executeScript(WebString::fromUTF8("showFrame(true)"));
  ASSERT_TRUE(IsWindowVisible(plugin_hwnd));
}
#endif
#endif //!ARCH_CPU_64_BITS
