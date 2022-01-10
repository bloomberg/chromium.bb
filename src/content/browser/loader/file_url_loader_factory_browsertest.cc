// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

// This must be before Windows headers
#include "base/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/public/test/browser_test.h"

#if defined(OS_WIN)
#include <objbase.h>
#include <shlobj.h>
#include <windows.h>
#include <wrl/client.h>
#endif

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/gtest_util.h"
#include "url/gurl.h"

namespace content {
namespace {

const char16_t kSuccessTitle[] = u"Title Of Awesomeness";
const char16_t kErrorTitle[] = u"Error";

base::FilePath TestFilePath() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  return GetTestFilePath("", "title2.html");
}

base::FilePath AbsoluteFilePath(const base::FilePath& file_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  return base::MakeAbsoluteFilePath(file_path);
}

class TestFileAccessContentBrowserClient : public TestContentBrowserClient {
 public:
  struct FileAccessAllowedArgs {
    base::FilePath path;
    base::FilePath absolute_path;
    base::FilePath profile_path;
  };

  TestFileAccessContentBrowserClient() {
    old_content_browser_client_ = SetBrowserClientForTesting(this);
  }

  void set_blocked_path(const base::FilePath& blocked_path) {
    blocked_path_ = AbsoluteFilePath(blocked_path);
  }

  TestFileAccessContentBrowserClient(
      const TestFileAccessContentBrowserClient&) = delete;
  TestFileAccessContentBrowserClient& operator=(
      const TestFileAccessContentBrowserClient&) = delete;

  ~TestFileAccessContentBrowserClient() override {
    EXPECT_EQ(this, SetBrowserClientForTesting(old_content_browser_client_));
  }

  bool IsFileAccessAllowed(const base::FilePath& path,
                           const base::FilePath& absolute_path,
                           const base::FilePath& profile_path) override {
    access_allowed_args_.push_back(
        FileAccessAllowedArgs{path, absolute_path, profile_path});
    return blocked_path_ != absolute_path;
  }

  // Returns a vector of arguments passed to each invocation of
  // IsFileAccessAllowed().
  const std::vector<FileAccessAllowedArgs>& access_allowed_args() const {
    return access_allowed_args_;
  }

  void ClearAccessAllowedArgs() { access_allowed_args_.clear(); }

 private:
  raw_ptr<ContentBrowserClient> old_content_browser_client_;

  base::FilePath blocked_path_;

  std::vector<FileAccessAllowedArgs> access_allowed_args_;
};

// This class contains integration tests for file URLs.
class FileURLLoaderFactoryBrowserTest : public ContentBrowserTest {
 public:
  FileURLLoaderFactoryBrowserTest() = default;

  // ContentBrowserTest implementation:
  void SetUpOnMainThread() override {
    base::FilePath test_data_path;
    EXPECT_TRUE(base::PathService::Get(DIR_TEST_DATA, &test_data_path));
    embedded_test_server()->ServeFilesFromDirectory(test_data_path);
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  base::FilePath ProfilePath() const {
    return shell()
        ->web_contents()
        ->GetSiteInstance()
        ->GetBrowserContext()
        ->GetPath();
  }

  GURL RedirectToFileURL() const {
    return embedded_test_server()->GetURL(
        "/server-redirect?" + net::FilePathToFileURL(TestFilePath()).spec());
  }
};

IN_PROC_BROWSER_TEST_F(FileURLLoaderFactoryBrowserTest, Basic) {
  TestFileAccessContentBrowserClient test_browser_client;
  EXPECT_TRUE(NavigateToURL(shell(), net::FilePathToFileURL(TestFilePath())));
  EXPECT_EQ(kSuccessTitle, shell()->web_contents()->GetTitle());

  ASSERT_EQ(1u, test_browser_client.access_allowed_args().size());
  EXPECT_EQ(TestFilePath(), test_browser_client.access_allowed_args()[0].path);
  EXPECT_EQ(AbsoluteFilePath(TestFilePath()),
            test_browser_client.access_allowed_args()[0].absolute_path);
  EXPECT_EQ(ProfilePath(),
            test_browser_client.access_allowed_args()[0].profile_path);
}

IN_PROC_BROWSER_TEST_F(FileURLLoaderFactoryBrowserTest, FileAccessNotAllowed) {
  TestFileAccessContentBrowserClient test_browser_client;
  test_browser_client.set_blocked_path(TestFilePath());

  TestNavigationObserver navigation_observer(shell()->web_contents());
  EXPECT_FALSE(NavigateToURL(shell(), net::FilePathToFileURL(TestFilePath())));
  EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  EXPECT_THAT(navigation_observer.last_net_error_code(),
              net::test::IsError(net::ERR_ACCESS_DENIED));
  EXPECT_EQ(net::FilePathToFileURL(TestFilePath()),
            shell()->web_contents()->GetURL());
  EXPECT_EQ(kErrorTitle, shell()->web_contents()->GetTitle());

  ASSERT_EQ(1u, test_browser_client.access_allowed_args().size());
  EXPECT_EQ(TestFilePath(), test_browser_client.access_allowed_args()[0].path);
  EXPECT_EQ(AbsoluteFilePath(TestFilePath()),
            test_browser_client.access_allowed_args()[0].absolute_path);
  EXPECT_EQ(ProfilePath(),
            test_browser_client.access_allowed_args()[0].profile_path);
}

#if defined(OS_POSIX)

// Test symbolic links on POSIX platforms. These act like the contents of
// the symbolic link are the same as the contents of the file it links to.
IN_PROC_BROWSER_TEST_F(FileURLLoaderFactoryBrowserTest, SymlinksToFiles) {
  TestFileAccessContentBrowserClient test_browser_client;

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Get an absolute path since |temp_dir| can contain a symbolic link.
  base::FilePath absolute_temp_dir = AbsoluteFilePath(temp_dir.GetPath());

  // MIME sniffing should use the symbolic link's destination path, so despite
  // not having a file extension of "html", this should be rendered as HTML.
  base::FilePath sym_link = absolute_temp_dir.AppendASCII("link.bin");
  ASSERT_TRUE(
      base::CreateSymbolicLink(AbsoluteFilePath(TestFilePath()), sym_link));

  EXPECT_TRUE(NavigateToURL(shell(), net::FilePathToFileURL(sym_link)));
  EXPECT_EQ(kSuccessTitle, shell()->web_contents()->GetTitle());

  ASSERT_EQ(1u, test_browser_client.access_allowed_args().size());
  EXPECT_EQ(sym_link, test_browser_client.access_allowed_args()[0].path);
  EXPECT_EQ(AbsoluteFilePath(sym_link),
            test_browser_client.access_allowed_args()[0].absolute_path);
  EXPECT_EQ(ProfilePath(),
            test_browser_client.access_allowed_args()[0].profile_path);

  // Test the case where access to the destination URL is blocked. Note that
  // this is the same as blocking the symbolic link URL - the
  // IsFileAccessAllowed() is passed both the symbolic link path and the
  // absolute path, so rejecting on looks just like rejecting the other.

  test_browser_client.ClearAccessAllowedArgs();
  test_browser_client.set_blocked_path(TestFilePath());

  TestNavigationObserver navigation_observer3(shell()->web_contents());
  EXPECT_FALSE(NavigateToURL(shell(), net::FilePathToFileURL(sym_link)));
  EXPECT_FALSE(navigation_observer3.last_navigation_succeeded());
  EXPECT_THAT(navigation_observer3.last_net_error_code(),
              net::test::IsError(net::ERR_ACCESS_DENIED));
  EXPECT_EQ(net::FilePathToFileURL(sym_link),
            shell()->web_contents()->GetURL());
  EXPECT_EQ(kErrorTitle, shell()->web_contents()->GetTitle());

  ASSERT_EQ(1u, test_browser_client.access_allowed_args().size());
  EXPECT_EQ(sym_link, test_browser_client.access_allowed_args()[0].path);
  EXPECT_EQ(AbsoluteFilePath(sym_link),
            test_browser_client.access_allowed_args()[0].absolute_path);
  EXPECT_EQ(ProfilePath(),
            test_browser_client.access_allowed_args()[0].profile_path);
}

#elif defined(OS_WIN)

// Test shortcuts on Windows. These are treated as redirects.
IN_PROC_BROWSER_TEST_F(FileURLLoaderFactoryBrowserTest, ResolveShortcutTest) {
  TestFileAccessContentBrowserClient test_browser_client;

  // Create an empty temp directory, to be sure there's no file in it.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath lnk_path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("foo.lnk"));

  base::FilePath test = TestFilePath();

  // Create a shortcut for the test.
  {
    Microsoft::WRL::ComPtr<IShellLink> shell;
    ASSERT_TRUE(SUCCEEDED(::CoCreateInstance(
        CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shell))));
    Microsoft::WRL::ComPtr<IPersistFile> persist;
    ASSERT_TRUE(SUCCEEDED(shell.As<IPersistFile>(&persist)));
    EXPECT_TRUE(SUCCEEDED(shell->SetPath(TestFilePath().value().c_str())));
    EXPECT_TRUE(SUCCEEDED(shell->SetDescription(L"ResolveShortcutTest")));
    std::wstring lnk_string = lnk_path.value();
    EXPECT_TRUE(SUCCEEDED(persist->Save(lnk_string.c_str(), TRUE)));
  }

  EXPECT_TRUE(NavigateToURL(
      shell(), net::FilePathToFileURL(lnk_path),
      net::FilePathToFileURL(TestFilePath()) /* expect_commit_url */));
  EXPECT_EQ(kSuccessTitle, shell()->web_contents()->GetTitle());

  ASSERT_EQ(2u, test_browser_client.access_allowed_args().size());
  EXPECT_EQ(lnk_path, test_browser_client.access_allowed_args()[0].path);
  EXPECT_EQ(AbsoluteFilePath(lnk_path),
            test_browser_client.access_allowed_args()[0].absolute_path);
  EXPECT_EQ(ProfilePath(),
            test_browser_client.access_allowed_args()[0].profile_path);

  EXPECT_EQ(TestFilePath(), test_browser_client.access_allowed_args()[1].path);
  EXPECT_EQ(AbsoluteFilePath(TestFilePath()),
            test_browser_client.access_allowed_args()[1].absolute_path);
  EXPECT_EQ(ProfilePath(),
            test_browser_client.access_allowed_args()[1].profile_path);

  // Test the case where access to the shortcut URL is blocked. Should display
  // an error page at the shortcut's file URL.

  test_browser_client.ClearAccessAllowedArgs();
  test_browser_client.set_blocked_path(lnk_path);

  TestNavigationObserver navigation_observer2(shell()->web_contents());
  EXPECT_FALSE(NavigateToURL(shell(), net::FilePathToFileURL(lnk_path)));
  EXPECT_FALSE(navigation_observer2.last_navigation_succeeded());
  EXPECT_THAT(navigation_observer2.last_net_error_code(),
              net::test::IsError(net::ERR_ACCESS_DENIED));
  EXPECT_EQ(net::FilePathToFileURL(lnk_path),
            shell()->web_contents()->GetURL());
  EXPECT_EQ(kErrorTitle, shell()->web_contents()->GetTitle());

  ASSERT_EQ(1u, test_browser_client.access_allowed_args().size());
  EXPECT_EQ(lnk_path, test_browser_client.access_allowed_args()[0].path);
  EXPECT_EQ(AbsoluteFilePath(lnk_path),
            test_browser_client.access_allowed_args()[0].absolute_path);
  EXPECT_EQ(ProfilePath(),
            test_browser_client.access_allowed_args()[0].profile_path);

  // Test the case where access to the destination URL is blocked. The redirect
  // is followed, so this should end up at the shortcut destination, but
  // displaying an error.

  test_browser_client.ClearAccessAllowedArgs();
  test_browser_client.set_blocked_path(TestFilePath());

  TestNavigationObserver navigation_observer3(shell()->web_contents());
  EXPECT_FALSE(NavigateToURL(shell(), net::FilePathToFileURL(lnk_path)));
  EXPECT_FALSE(navigation_observer3.last_navigation_succeeded());
  EXPECT_THAT(navigation_observer3.last_net_error_code(),
              net::test::IsError(net::ERR_ACCESS_DENIED));
  EXPECT_EQ(net::FilePathToFileURL(TestFilePath()),
            shell()->web_contents()->GetURL());
  EXPECT_EQ(kErrorTitle, shell()->web_contents()->GetTitle());

  ASSERT_EQ(2u, test_browser_client.access_allowed_args().size());
  EXPECT_EQ(lnk_path, test_browser_client.access_allowed_args()[0].path);
  EXPECT_EQ(AbsoluteFilePath(lnk_path),
            test_browser_client.access_allowed_args()[0].absolute_path);
  EXPECT_EQ(ProfilePath(),
            test_browser_client.access_allowed_args()[0].profile_path);

  EXPECT_EQ(TestFilePath(), test_browser_client.access_allowed_args()[1].path);
  EXPECT_EQ(AbsoluteFilePath(TestFilePath()),
            test_browser_client.access_allowed_args()[1].absolute_path);
  EXPECT_EQ(ProfilePath(),
            test_browser_client.access_allowed_args()[1].profile_path);
}

#endif  // defined(OS_WIN)

IN_PROC_BROWSER_TEST_F(FileURLLoaderFactoryBrowserTest,
                       RedirectToFileUrlMainFrame) {
  TestFileAccessContentBrowserClient test_browser_client;

  TestNavigationObserver navigation_observer(shell()->web_contents());
  EXPECT_FALSE(NavigateToURL(shell(), RedirectToFileURL()));
  EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  EXPECT_THAT(navigation_observer.last_net_error_code(),
              net::test::IsError(net::ERR_UNSAFE_REDIRECT));
  // The redirect should not have been followed. This is important so that a
  // reload will show the same error.
  EXPECT_EQ(RedirectToFileURL(), shell()->web_contents()->GetURL());
  EXPECT_EQ(kErrorTitle, shell()->web_contents()->GetTitle());
  // There should never have been a request for the file URL.
  EXPECT_TRUE(test_browser_client.access_allowed_args().empty());

  // Reloading returns the same error.
  TestNavigationObserver navigation_observer2(shell()->web_contents());
  shell()->Reload();
  navigation_observer2.Wait();
  EXPECT_FALSE(navigation_observer2.last_navigation_succeeded());
  EXPECT_THAT(navigation_observer2.last_net_error_code(),
              net::test::IsError(net::ERR_UNSAFE_REDIRECT));
  EXPECT_EQ(RedirectToFileURL(), shell()->web_contents()->GetURL());
  EXPECT_EQ(kErrorTitle, shell()->web_contents()->GetTitle());
  // There should never have been a request for the file URL.
  EXPECT_TRUE(test_browser_client.access_allowed_args().empty());
}

IN_PROC_BROWSER_TEST_F(FileURLLoaderFactoryBrowserTest,
                       RedirectToFileUrlFetch) {
  TestFileAccessContentBrowserClient test_browser_client;

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));
  std::string fetch_redirect_to_file = base::StringPrintf(
      "(async () => {"
      "  try {"
      "    var resp = (await fetch('%s'));"
      "    return 'ok';"
      "  } catch (error) {"
      "    return 'error';"
      "  }"
      "})();",
      RedirectToFileURL().spec().c_str());
  // Unfortunately, fetch doesn't provide a way to unambiguously know if the
  // request failed due to the redirect being unsafe.
  EXPECT_EQ("error", EvalJs(shell()->web_contents()->GetMainFrame(),
                            fetch_redirect_to_file));
  // There should never have been a request for the file URL.
  EXPECT_TRUE(test_browser_client.access_allowed_args().empty());
}

IN_PROC_BROWSER_TEST_F(FileURLLoaderFactoryBrowserTest,
                       RedirectToFileUrlSubFrame) {
  TestFileAccessContentBrowserClient test_browser_client;

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/page_with_iframe.html")));
  LOG(WARNING) << embedded_test_server()->GetURL("/page_with_iframe.html");

  TestNavigationObserver navigation_observer(shell()->web_contents());
  EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                                  RedirectToFileURL()));
  navigation_observer.Wait();
  EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  EXPECT_THAT(navigation_observer.last_net_error_code(),
              net::test::IsError(net::ERR_UNSAFE_REDIRECT));
  // There should never have been a request for the file URL.
  EXPECT_TRUE(test_browser_client.access_allowed_args().empty());
}

// The test below opens a |popup| in a way that doesn't trigger a
// RenderFrameImpl::CommitNavigation.  This means that the popup will depend on
// inheriting URLLoaderFactoryBundle from the opener.
//
// Before triggering any subresource fetches in the popup, the test closes the
// opener window.  If the URLLoaderFactoryBundle hasn't been inherited at this
// point yet, then we might run into trouble later.  Another way how we might
// get into trouble is if FileURLLoaderFactory's clone doesn't survive closing
// of the opener.
//
// Finally, the test triggers a subresource fetch in the popup.  We expect that
// this fetch should use the right URLLoaderFactoryBundle - one that contains
// a functional FileURLLoaderFactory.
//
// This is a regression test for https://crbug.com/1106995
IN_PROC_BROWSER_TEST_F(FileURLLoaderFactoryBrowserTest,
                       LifetimeOfClonedFactory) {
  GURL opener_url(GetTestUrl("", "title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), opener_url));

  // Open a |popup| in a way that doesn't trigger RFI::CommitNavigation.
  WebContentsAddedObserver popup_observer;
  ASSERT_TRUE(ExecJs(shell(), "window.open('', '_blank')"));
  WebContents* popup = popup_observer.GetWebContents();

  // Close the opener and make sure that the |popup| actually sees that the
  // |opener| has closed.
  //
  // This test step means that we cannot rely on the |opener| being alive when
  // the first subresource fetch happens in the |popup| (in the next test step
  // below).
  EXPECT_EQ(true, EvalJs(popup, "!!window.opener"));
  shell()->Close();
  const char kScriptToWaitForNoOpener[] = R"(
      new Promise(function (resolve, reject) {
          function CheckAndWaitMoreIfNeeded() {
            if (!window.opener)
              resolve('OPENER GONE');

            setTimeout(CheckAndWaitMoreIfNeeded, 50);
          }

          CheckAndWaitMoreIfNeeded();
      });
  )";
  EXPECT_EQ("OPENER GONE", EvalJs(popup, kScriptToWaitForNoOpener));

  // Trigger a subresource fetch in the popup.  This is the main test step - it
  // verifies that at this point the popup has access to a functional
  // FileURLLoaderFactory.
  const char kScriptTemplateToTriggerSubresourceFetch[] = R"(
      new Promise(function (resolve, reject) {
          var img = document.createElement('img');
          img.src = $1;
          img.onload = _ => resolve('OK');
          img.onerror = e => resolve('ERR: ' + e);
      });
  )";
  GURL img_url = GetTestUrl("site_isolation", "png-corp.png");
  EXPECT_EQ("OK",
            EvalJs(popup, JsReplace(kScriptTemplateToTriggerSubresourceFetch,
                                    img_url)));
}

class FileURLLoaderFactoryDisabledSecurityBrowserTest
    : public FileURLLoaderFactoryBrowserTest {
 public:
  FileURLLoaderFactoryDisabledSecurityBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableWebSecurity);
    FileURLLoaderFactoryBrowserTest::SetUpCommandLine(command_line);
  }
};

// Test whether sandboxed file: frames still can access file: subresources.
IN_PROC_BROWSER_TEST_F(FileURLLoaderFactoryDisabledSecurityBrowserTest,
                       SubresourcesInSandboxedFileFrame) {
  GURL main_url(GetTestUrl("", "title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Append a sandboxed file: subframe.
  {
    GURL sandboxed_url(GetTestUrl("", "title2.html"));
    TestNavigationObserver nav_observer(shell()->web_contents());
    const char kScriptTemplateToAddSubframe[] = R"(
        f = document.createElement('iframe');
        f.sandbox = 'allow-scripts';
        f.src = $1;
        document.body.appendChild(f);
    )";
    ASSERT_TRUE(ExecJs(shell(),
                       JsReplace(kScriptTemplateToAddSubframe, sandboxed_url)));
    nav_observer.Wait();
  }

  // Trigger a subresource fetch in the subframe.  This is the main test step -
  // it verifies that at this point the subframe has access to a functional
  // FileURLLoaderFactory.
  //
  // Access to a file: (local-scheme) subresource is normally forbidden by
  // blink::SecurityOrigin::CanDisplay if the requesting origin is opaque.
  // Example error message:
  //     Not allowed to load local resource:
  //     file:///.../src/content/test/data/site_isolation/png-corp.png,
  //     source: file:///...src/content/test/data/title2.html
  //
  // OTOH, this restriction can be relaxed by 1) the --disable-web-security
  // switch (this is what this test suite is doing) or 2) the following Android
  // WebView API: android.webkit.WebSettings.setAllowFileAccess.  This is why
  // the test asserts below that the access is successful.
  RenderFrameHost* main_frame = shell()->web_contents()->GetMainFrame();
  RenderFrameHost* child_frame = ChildFrameAt(main_frame, 0);
  const char kScriptTemplateToTriggerSubresourceFetch[] = R"(
      new Promise(function (resolve, reject) {
          var img = document.createElement('img');
          img.src = $1;
          img.onload = _ => resolve('OK');
          img.onerror = e => resolve('ERR: ' + e);
      });
  )";
  GURL img_url = GetTestUrl("site_isolation", "png-corp.png");
  EXPECT_EQ("OK", EvalJs(child_frame,
                         JsReplace(kScriptTemplateToTriggerSubresourceFetch,
                                   img_url)));
}

}  // namespace
}  // namespace content
