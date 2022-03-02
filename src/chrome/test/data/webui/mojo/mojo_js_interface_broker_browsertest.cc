// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_forward.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/data/grit/webui_test_resources.h"
#include "chrome/test/data/webui/mojo/foobar.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/untrusted_web_ui_controller.h"
#include "url/gurl.h"

namespace {

static constexpr char kFooURL[] = "chrome://foo/";
static constexpr char kBarURL[] = "chrome-untrusted://bar/";
static constexpr char kBuzURL[] = "chrome-untrusted://buz/";

// A (trusted) WebUIController provides Foo Mojo API.
class FooUI : public content::WebUIController, public ::test::mojom::Foo {
 public:
  explicit FooUI(content::WebUI* web_ui)
      : content::WebUIController(web_ui), foo_receiver_(this) {
    content::WebUIDataSource* data_source =
        content::WebUIDataSource::Create("foo");
    data_source->SetDefaultResource(IDR_MOJO_JS_INTERFACE_BROKER_TEST_FOO_HTML);
    data_source->AddResourcePath("foobar.mojom-lite.js",
                                 IDR_FOOBAR_MOJO_LITE_JS);

    // Allow Foo to embed chrome-untrusted://bar.
    data_source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::ChildSrc,
        "child-src chrome-untrusted://bar/;");
    // Allow inline test helper scripts to execute with a special nonce.
    data_source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::ScriptSrc,
        "script-src 'self' chrome://resources/ 'nonce-test';");
    content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                  data_source);

    // Allow requesting chrome-untrusted://bar in iframe.
    web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
  }

  void BindInterface(mojo::PendingReceiver<::test::mojom::Foo> receiver) {
    foo_receiver_.reset();
    foo_receiver_.Bind(std::move(receiver));
  }

  // ::test::mojom::Foo:
  void GetFoo(GetFooCallback callback) override {
    std::move(callback).Run("foo");
  }

  WEB_UI_CONTROLLER_TYPE_DECL();

 private:
  mojo::Receiver<::test::mojom::Foo> foo_receiver_;
};

WEB_UI_CONTROLLER_TYPE_IMPL(FooUI)

// A untrusted WebUIController that provides Bar Mojo API.
class BarUI : public ui::UntrustedWebUIController, public ::test::mojom::Bar {
 public:
  explicit BarUI(content::WebUI* web_ui)
      : ui::UntrustedWebUIController(web_ui), bar_receiver_(this) {
    content::WebUIDataSource* data_source =
        content::WebUIDataSource::Create(kBarURL);
    data_source->SetDefaultResource(IDR_MOJO_JS_INTERFACE_BROKER_TEST_BAR_HTML);

    // Allow Foo to embed this UI.
    data_source->AddFrameAncestor(GURL(kFooURL));

    data_source->AddResourcePath("foobar.mojom-lite.js",
                                 IDR_FOOBAR_MOJO_LITE_JS);
    // If requested path is "error", trigger an error page.
    data_source->SetRequestFilter(
        base::BindRepeating(
            [](const std::string& path) { return path == "error"; }),
        base::BindRepeating(
            [](const std::string& id,
               content::WebUIDataSource::GotDataCallback callback) {
              std::move(callback).Run(nullptr);
            }));
    content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                  data_source);
  }

  void BindInterface(mojo::PendingReceiver<::test::mojom::Bar> receiver) {
    bar_receiver_.reset();
    bar_receiver_.Bind(std::move(receiver));
  }

  // ::test::mojom::Bar:
  void GetBar(GetBarCallback callback) override {
    std::move(callback).Run("bar");
  }

  WEB_UI_CONTROLLER_TYPE_DECL();

 private:
  mojo::Receiver<::test::mojom::Bar> bar_receiver_;
};

WEB_UI_CONTROLLER_TYPE_IMPL(BarUI)

// A untrusted WebUIController that isn't registered in the registry.
class BuzUI : public ui::UntrustedWebUIController {
 public:
  explicit BuzUI(content::WebUI* web_ui)
      : ui::UntrustedWebUIController(web_ui) {
    content::WebUIDataSource* data_source =
        content::WebUIDataSource::Create(kBuzURL);
    data_source->SetDefaultResource(IDR_MOJO_JS_INTERFACE_BROKER_TEST_BUZ_HTML);
    content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                  data_source);
  }
};

// WebUIControllerFactory that serves our TestWebUIController.
class TestWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  TestWebUIControllerFactory() = default;

  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override {
    if (url::IsSameOriginWith(url, GURL(kFooURL)))
      return std::make_unique<FooUI>(web_ui);
    if (url::IsSameOriginWith(url, GURL(kBarURL)))
      return std::make_unique<BarUI>(web_ui);
    if (url::IsSameOriginWith(url, GURL(kBuzURL)))
      return std::make_unique<BuzUI>(web_ui);

    return nullptr;
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override {
    if (url::IsSameOriginWith(url, GURL(kFooURL)))
      return &kFooURL;
    if (url::IsSameOriginWith(url, GURL(kBarURL)))
      return &kBarURL;
    if (url::IsSameOriginWith(url, GURL(kBuzURL)))
      return &kBuzURL;

    return content::WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override {
    return GetWebUIType(browser_context, url) != content::WebUI::kNoWebUI;
  }
};

}  // namespace

// MojoJSInterfaceBrokerBrowserTest tests the plumbing between WebUI
// infrastructure and WebUIBrowserInterfaceBrokerRegistry is correct.
//
// 1. The correct broker is instantiated based on what's stored in the registry.
// 2. The instantiated broker can bind the registered interfaces, and refuses
//    to bind other interfaces.
// 3. Request to bind unexpected interfaces shuts down the renderer.
// 4. WebUIs that doesn't have a registered interface broker don't
//    automatically get MojoJS bindings enabled.
//
// TODO(https://crbug.com/1157718): This test fixture and test suites should
// migrate to EvalJs / ExecJs after they work with WebUI CSP.
class MojoJSInterfaceBrokerBrowserTest : public InProcessBrowserTest {
 public:
  MojoJSInterfaceBrokerBrowserTest() {
    factory_ = std::make_unique<TestWebUIControllerFactory>();
    content::WebUIControllerFactory::RegisterFactory(factory_.get());
  }

  void SetUpOnMainThread() override {
    base::FilePath pak_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &pak_path));
    pak_path = pak_path.AppendASCII("browser_tests.pak");
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_path, ui::kScaleFactorNone);

    content::SetBrowserClientForTesting(&test_content_browser_client_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Turn on MojoJS test so we can test MojoJS interceptors.
    // This does not imply MojoJS is enabled.
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "MojoJSTest");
  }

  // Evaluate |statement| in frame (defaults to main frame), and returns its
  // result. For convenience, |script| should evaluate to a string, or a promise
  // that resolves to a string.
  std::string EvalStatement(const std::string& statement,
                            content::RenderFrameHost* frame = nullptr) {
    auto* eval_frame = frame ? frame
                             : browser()
                                   ->tab_strip_model()
                                   ->GetActiveWebContents()
                                   ->GetMainFrame();
    // We can't use EvalJs with a different world_id to get around CSP
    // restrictions, because Mojo is only exposed to the global world
    // (ISOLATED_WORLD_ID_GLOBAL). So we use |ExecuteScriptAndExtractString| to
    // work with CSP and in the right world_id.
    std::string result;
    std::string wrapped_script =
        base::StrCat({"Promise.resolve(", statement, ").then(",
                      "  resolved => domAutomationController.send(resolved),"
                      "  error => domAutomationController.send('JS Error: ' + "
                      "    error.message)",
                      ");"});
    EXPECT_TRUE(
        ExecuteScriptAndExtractString(eval_frame, wrapped_script, &result));
    return result;
  }

  // Returns whether |frame| (defaults to the main frame) has Mojo bindings
  // exposed.
  bool FrameHasMojo(content::RenderFrameHost* frame = nullptr) {
    auto* eval_frame = frame ? frame
                             : browser()
                                   ->tab_strip_model()
                                   ->GetActiveWebContents()
                                   ->GetMainFrame();
    // We can't use EvalJs with a different world_id to get around CSP
    // restrictions, because Mojo is only exposed to the global world
    // (ISOLATED_WORLD_ID_GLOBAL). So we use |ExecuteScriptAndExtractString| to
    // work with CSP and in the right world_id.
    bool result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        eval_frame, "domAutomationController.send(!!window.Mojo)", &result));
    return result;
  }

 private:
  class TestContentBrowserClient : public ChromeContentBrowserClient {
   public:
    TestContentBrowserClient() = default;
    TestContentBrowserClient(const TestContentBrowserClient&) = delete;
    TestContentBrowserClient& operator=(const TestContentBrowserClient&) =
        delete;
    ~TestContentBrowserClient() override = default;

    void RegisterWebUIInterfaceBrokers(
        content::WebUIBrowserInterfaceBrokerRegistry& registry) override {
      registry.ForWebUI<FooUI>().Add<::test::mojom::Foo>();
      registry.ForWebUI<BarUI>().Add<::test::mojom::Bar>();
    }
  };

  std::unique_ptr<TestWebUIControllerFactory> factory_;
  TestContentBrowserClient test_content_browser_client_;
};

// Try to get Foo Mojo interface on a top-level frame.
IN_PROC_BROWSER_TEST_F(MojoJSInterfaceBrokerBrowserTest, FooWorks) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(NavigateToURL(web_contents, GURL(kFooURL)));

  EXPECT_EQ("foo", EvalStatement("(async () => {"
                                 "  let fooRemote = test.mojom.Foo.getRemote();"
                                 "  let resp = await fooRemote.getFoo();"
                                 "  return resp.value;"
                                 "})()"));

  auto* broker1 =
      web_contents->GetWebUI()->GetController()->broker_for_testing();
  // Refresh to trigger a RenderFrame reuse.
  content::TestNavigationObserver observer(web_contents, 1);
  EXPECT_TRUE(content::ExecuteScript(web_contents, "location.reload()"));
  observer.Wait();

  // Verify a new broker is created, and Foo still works.
  auto* broker2 =
      web_contents->GetWebUI()->GetController()->broker_for_testing();
  EXPECT_NE(broker1, broker2);
  EXPECT_EQ("foo", EvalStatement("(async () => {"
                                 "  let fooRemote = test.mojom.Foo.getRemote();"
                                 "  let resp = await fooRemote.getFoo();"
                                 "  return resp.value;"
                                 "})()"));

  // Perform a same-document navigation, verify the current broker persists, and
  // Foo still works.
  ASSERT_TRUE(NavigateToURL(web_contents, GURL(kFooURL).Resolve("#fragment")));
  auto* broker3 =
      web_contents->GetWebUI()->GetController()->broker_for_testing();
  EXPECT_EQ(broker2, broker3);
  EXPECT_EQ("foo", EvalStatement("(async () => {"
                                 "  let fooRemote = test.mojom.Foo.getRemote();"
                                 "  let resp = await fooRemote.getFoo();"
                                 "  return resp.value;"
                                 "})()"));
}

// Attempts to get interfaces registered for a different WebUI shuts down the
// renderer.
IN_PROC_BROWSER_TEST_F(MojoJSInterfaceBrokerBrowserTest,
                       InterfaceRequestViolation) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(NavigateToURL(web_contents, GURL(kFooURL)));

  content::ScopedAllowRendererCrashes allow;
  content::RenderProcessHostWatcher watcher(
      web_contents, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // Attempt to get a remote for a Bar interface (registered for BarUI).
  //
  // Don't use EvalJs here because it don't work with WebUI's default CSP, and
  // we only expose Mojo to the global world (ISOLATED_WORLD_ID_GLOBAL) thus the
  // `world_id = 1` work-around doesn't work.
  //
  // EXPECT_FALSE because this the following should cause renderer to shutdown
  // before it can reply with the result.
  std::string result;
  EXPECT_FALSE(content::ExecuteScriptAndExtractString(
      web_contents,
      "(async () => {"
      "  let barRemote = test.mojom.Bar.getRemote();"
      "  let resp = await barRemote.getBar();"
      "  domAutomationController.send(resp.value);"
      "})()",
      &result));
  EXPECT_EQ("", result);
  watcher.Wait();
  EXPECT_FALSE(watcher.did_exit_normally());
  EXPECT_TRUE(web_contents->IsCrashed());
}

// Broker works for BarUI iframe in FooUI.
IN_PROC_BROWSER_TEST_F(MojoJSInterfaceBrokerBrowserTest, IframeBarWorks) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(NavigateToURL(web_contents, GURL(kFooURL)));

  // Foo page gets Foo Mojo API.
  EXPECT_EQ("foo", EvalStatement("(async () => {"
                                 "  let fooRemote = test.mojom.Foo.getRemote();"
                                 "  let resp = await fooRemote.getFoo();"
                                 "  return resp.value;"
                                 "})()"));

  // Wait until Bar is loaded.
  EXPECT_EQ("loaded", EvalStatement("window.barLoadedPromise"));

  // Bar page gets Bar Mojo API.
  content::RenderFrameHost* bar_frame =
      ChildFrameAt(web_contents->GetMainFrame(), 0);
  ASSERT_EQ(GURL(kBarURL), bar_frame->GetLastCommittedURL());

  EXPECT_EQ("bar", EvalStatement("(async () => {"
                                 "  let barRemote = test.mojom.Bar.getRemote();"
                                 "  let resp = await barRemote.getBar();"
                                 "  return resp.value;"
                                 "})()",
                                 bar_frame));

  // Reload Bar iframe, Bar interface should still work.
  content::TestNavigationObserver observer(web_contents, 1);
  EXPECT_TRUE(content::ExecuteScript(bar_frame, "location.reload()"));
  observer.Wait();

  EXPECT_EQ("bar", EvalStatement("(async () => {"
                                 "  let barRemote = test.mojom.Bar.getRemote();"
                                 "  let resp = await barRemote.getBar();"
                                 "  return resp.value;"
                                 "})()",
                                 bar_frame));
}

// WebUI that doesn't register its interfaces don't get MojoJS enabled.
IN_PROC_BROWSER_TEST_F(MojoJSInterfaceBrokerBrowserTest,
                       DontExposeMojoJSByDefault) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, GURL(kBuzURL)));
  EXPECT_FALSE(FrameHasMojo());
}

// Tests the following sequence won't leak MojoJS bindings.
// 1. Successful navigation to WebUI
// 2. Failed navigation to WebUI
// 2. Failed navigation to a website
IN_PROC_BROWSER_TEST_F(MojoJSInterfaceBrokerBrowserTest, FailedNavigation) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(NavigateToURL(web_contents, GURL(kBarURL)));
  EXPECT_TRUE(FrameHasMojo());

  // Error page of a WebUI with registered interface broker shouldn't have Mojo.
  GURL webui_error_url = GURL(kBarURL).Resolve("/error");
  EXPECT_FALSE(NavigateToURL(web_contents, webui_error_url));
  EXPECT_FALSE(FrameHasMojo());

  // HTTP error page shouldn't have Mojo.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL http_error_url(
      embedded_test_server()->GetURL("example.com", "/noexistent"));
  EXPECT_FALSE(NavigateToURL(web_contents, http_error_url));
  EXPECT_FALSE(FrameHasMojo());
}

// Try to get Foo Mojo interface on a top-level frame.
IN_PROC_BROWSER_TEST_F(MojoJSInterfaceBrokerBrowserTest, MojoInterceptorWorks) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(NavigateToURL(web_contents, GURL(kFooURL)));

  EXPECT_EQ(
      "success",
      EvalStatement(
          "(async function() {"
          "  window.intercepted = false;"
          "  window.interceptor = new "
          "MojoInterfaceInterceptor('test.mojom.Foo', 'context_js');"
          "  interceptor.oninterfacerequest = _ => window.intercepted = true;"
          "  return 'success';"
          "})()"));

  // Start interceptor, and verify it intercepts the request.
  EXPECT_EQ("success",
            EvalStatement("(async function() {"
                          "  window.interceptor.start();"
                          "  window.intercepted = false;"
                          ""
                          "  const r = Mojo.createMessagePipe();"
                          "  Mojo.bindInterface('test.mojom.Foo', r.handle1);"
                          "  if (window.intercepted) {"
                          "    return \"Interface isn't intercepted\";"
                          "  }"
                          ""
                          "  return 'success';"
                          "})()"));

  // Stop interceptor. Verify the interface method calls are handled in the
  // browser.
  EXPECT_EQ("foo", EvalStatement("(async () => {"
                                 "window.interceptor.stop();"
                                 "  let fooRemote = test.mojom.Foo.getRemote();"
                                 "  let resp = await fooRemote.getFoo();"
                                 "  return resp.value;"
                                 "})()"));
}
