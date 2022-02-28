// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/data/web_ui_test.test-mojom.h"
#include "content/test/data/web_ui_test_types.test-mojom.h"
#include "content/test/grit/web_ui_mojo_test_resources.h"
#include "content/test/grit/web_ui_mojo_test_resources_map.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"

namespace content {
namespace {

const char kMojoWebUiHost[] = "mojo-web-ui";
const char kDummyWebUiHost[] = "dummy-web-ui";

class WebUIMojoTestCacheImpl : public mojom::WebUIMojoTestCache {
 public:
  explicit WebUIMojoTestCacheImpl(
      mojo::PendingReceiver<mojom::WebUIMojoTestCache> receiver)
      : receiver_(this, std::move(receiver)) {}

  ~WebUIMojoTestCacheImpl() override = default;

  // mojom::WebUIMojoTestCache overrides:
  void Put(const GURL& url, const std::string& contents) override {
    cache_[url] = contents;
  }

  void GetAll(GetAllCallback callback) override {
    std::vector<mojom::CacheItemPtr> items;
    for (const auto& entry : cache_)
      items.push_back(mojom::CacheItem::New(entry.first, entry.second));
    std::move(callback).Run(std::move(items));
  }

 private:
  mojo::Receiver<mojom::WebUIMojoTestCache> receiver_;
  std::map<GURL, std::string> cache_;
};

// WebUIController that sets up mojo bindings.
class TestWebUIController : public WebUIController {
 public:
  explicit TestWebUIController(WebUI* web_ui,
                               int bindings = BINDINGS_POLICY_MOJO_WEB_UI)
      : WebUIController(web_ui) {
    const base::span<const webui::ResourcePath> kMojoWebUiResources =
        base::make_span(kWebUiMojoTestResources, kWebUiMojoTestResourcesSize);

    web_ui->SetBindings(bindings);
    {
      WebUIDataSource* data_source = WebUIDataSource::Create(kMojoWebUiHost);
      data_source->OverrideContentSecurityPolicy(
          network::mojom::CSPDirectiveName::ScriptSrc,
          "script-src chrome://resources 'self' 'unsafe-eval';");
      data_source->DisableTrustedTypesCSP();
      data_source->AddResourcePaths(kMojoWebUiResources);
      data_source->AddResourcePath("", IDR_WEB_UI_MOJO_HTML);
      WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                           data_source);
    }
    {
      WebUIDataSource* data_source = WebUIDataSource::Create(kDummyWebUiHost);
      data_source->SetRequestFilter(
          base::BindRepeating([](const std::string& path) { return true; }),
          base::BindRepeating([](const std::string& id,
                                 WebUIDataSource::GotDataCallback callback) {
            std::move(callback).Run(new base::RefCountedString);
          }));
      WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                           data_source);
    }
  }

  TestWebUIController(const TestWebUIController&) = delete;
  TestWebUIController& operator=(const TestWebUIController&) = delete;

 protected:
  std::unique_ptr<WebUIMojoTestCacheImpl> cache_;
};

// TestWebUIController that can bind a WebUIMojoTestCache interface when
// requested by the page.
class CacheTestWebUIController : public TestWebUIController {
 public:
  explicit CacheTestWebUIController(WebUI* web_ui)
      : TestWebUIController(web_ui) {}
  ~CacheTestWebUIController() override = default;

  void CreateHandler(
      mojo::PendingReceiver<mojom::WebUIMojoTestCache> receiver) {
    cache_ = std::make_unique<WebUIMojoTestCacheImpl>(std::move(receiver));
  }
};

// WebUIControllerFactory that creates TestWebUIController.
class TestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  TestWebUIControllerFactory()
      : registered_controllers_(
            {{"cache", base::BindRepeating(
                           &TestWebUIControllerFactory::CreateCacheController,
                           base::Unretained(this))},
             {"hybrid", base::BindRepeating(
                            &TestWebUIControllerFactory::CreateHybridController,
                            base::Unretained(this))},
             {"webui_bindings",
              base::BindRepeating(
                  &TestWebUIControllerFactory::CreateWebUIController,
                  base::Unretained(this))}}) {}

  TestWebUIControllerFactory(const TestWebUIControllerFactory&) = delete;
  TestWebUIControllerFactory& operator=(const TestWebUIControllerFactory&) =
      delete;

  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) override {
    if (!web_ui_enabled_ || !url.SchemeIs(kChromeUIScheme))
      return nullptr;

    auto it = registered_controllers_.find(url.query());
    if (it != registered_controllers_.end())
      return it->second.Run(web_ui);

    return std::make_unique<TestWebUIController>(web_ui);
  }

  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) override {
    if (!web_ui_enabled_ || !url.SchemeIs(kChromeUIScheme))
      return WebUI::kNoWebUI;

    return reinterpret_cast<WebUI::TypeID>(1);
  }

  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) override {
    return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
  }

  void set_web_ui_enabled(bool enabled) { web_ui_enabled_ = enabled; }

 private:
  std::unique_ptr<WebUIController> CreateCacheController(WebUI* web_ui) {
    return std::make_unique<CacheTestWebUIController>(web_ui);
  }

  std::unique_ptr<WebUIController> CreateHybridController(WebUI* web_ui) {
    return std::make_unique<TestWebUIController>(
        web_ui, BINDINGS_POLICY_WEB_UI | BINDINGS_POLICY_MOJO_WEB_UI);
  }

  std::unique_ptr<WebUIController> CreateWebUIController(WebUI* web_ui) {
    return std::make_unique<TestWebUIController>(web_ui,
                                                 BINDINGS_POLICY_WEB_UI);
  }

  bool web_ui_enabled_ = true;
  const base::flat_map<
      std::string,
      base::RepeatingCallback<std::unique_ptr<WebUIController>(WebUI*)>>
      registered_controllers_;
};

// Base for unit tests that need a ContentBrowserClient.
class TestWebUIContentBrowserClient : public ContentBrowserClient {
 public:
  TestWebUIContentBrowserClient() {}
  TestWebUIContentBrowserClient(const TestWebUIContentBrowserClient&) = delete;
  TestWebUIContentBrowserClient& operator=(
      const TestWebUIContentBrowserClient&) = delete;
  ~TestWebUIContentBrowserClient() override {}

  void RegisterBrowserInterfaceBindersForFrame(
      RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override {
    map->Add<mojom::WebUIMojoTestCache>(base::BindRepeating(
        &TestWebUIContentBrowserClient::BindTestCache, base::Unretained(this)));
  }
  void BindTestCache(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::WebUIMojoTestCache> receiver) {
    auto* contents = WebContents::FromRenderFrameHost(render_frame_host);
    static_cast<CacheTestWebUIController*>(
        contents->GetWebUI()->GetController())
        ->CreateHandler(std::move(receiver));
  }
};

class WebUIMojoTest : public ContentBrowserTest {
 public:
  WebUIMojoTest() = default;

  WebUIMojoTest(const WebUIMojoTest&) = delete;
  WebUIMojoTest& operator=(const WebUIMojoTest&) = delete;

  TestWebUIControllerFactory* factory() { return &factory_; }

  void NavigateWithNewWebUI(const std::string& path) {
    // Load a dummy WebUI URL first so that a new WebUI is set up when we load
    // the URL we're actually interested in.
    EXPECT_TRUE(NavigateToURL(shell(), GetWebUIURL(kDummyWebUiHost)));
    EXPECT_TRUE(NavigateToURL(
        shell(), GetWebUIURL(kMojoWebUiHost + std::string("/") + path)));
  }

  // Run |script| and return a boolean result.
  bool RunBoolFunction(const std::string& script) {
    return EvalJs(shell()->web_contents(), script).ExtractBool();
  }

 protected:
  void SetUpOnMainThread() override {
    original_client_ = SetBrowserClientForTesting(&client_);
  }

  void TearDownOnMainThread() override {
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
  }

 private:
  TestWebUIControllerFactory factory_;
  content::ScopedWebUIControllerFactoryRegistration factory_registration_{
      &factory_};
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
  TestWebUIContentBrowserClient client_;
};

// Loads a WebUI page that contains Mojo JS bindings and verifies a message
// round-trip between the page and the browser.
IN_PROC_BROWSER_TEST_F(WebUIMojoTest, EndToEndCommunication) {
  GURL kTestUrl(GetWebUIURL(std::string(kMojoWebUiHost) + "/?cache"));
  const std::string kTestScript = "runTest();";
  EXPECT_TRUE(NavigateToURL(shell(), kTestUrl));
  EXPECT_EQ(true, EvalJs(shell()->web_contents(), kTestScript,
                         EXECUTE_SCRIPT_USE_MANUAL_REPLY));

  // Check that a second shell works correctly.
  Shell* other_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(other_shell, kTestUrl));
  EXPECT_EQ(true, EvalJs(other_shell->web_contents(), kTestScript,
                         EXECUTE_SCRIPT_USE_MANUAL_REPLY));

  // We expect two independent chrome://foo tabs/shells to use a separate
  // process.
  EXPECT_NE(shell()->web_contents()->GetMainFrame()->GetProcess(),
            other_shell->web_contents()->GetMainFrame()->GetProcess());

  // Close the second shell and wait until its process exits.
  RenderProcessHostWatcher process_watcher(
      other_shell->web_contents()->GetMainFrame()->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  other_shell->Close();
  process_watcher.Wait();

  // Check that a third shell works correctly, even if we force it to share a
  // process with the first shell, by forcing an artificially low process
  // limit.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  other_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(other_shell, kTestUrl));
  EXPECT_EQ(shell()->web_contents()->GetMainFrame()->GetProcess(),
            other_shell->web_contents()->GetMainFrame()->GetProcess());
  EXPECT_EQ(true, EvalJs(other_shell->web_contents(), kTestScript,
                         EXECUTE_SCRIPT_USE_MANUAL_REPLY));
}

// Disabled due to flakiness: crbug.com/860385.
#if defined(OS_ANDROID)
#define MAYBE_NativeMojoAvailable DISABLED_NativeMojoAvailable
#else
#define MAYBE_NativeMojoAvailable NativeMojoAvailable
#endif
IN_PROC_BROWSER_TEST_F(WebUIMojoTest, MAYBE_NativeMojoAvailable) {
  // Mojo bindings should be enabled.
  NavigateWithNewWebUI("web_ui_mojo_native.html");
  EXPECT_TRUE(RunBoolFunction("isNativeMojoAvailable()"));

  // Now navigate again with normal WebUI bindings and ensure chrome.send is
  // available.
  NavigateWithNewWebUI("web_ui_mojo_native.html?webui_bindings");
  EXPECT_FALSE(RunBoolFunction("isNativeMojoAvailable()"));

  // Now navigate again both WebUI and Mojo bindings and ensure chrome.send is
  // available.
  NavigateWithNewWebUI("web_ui_mojo_native.html?hybrid");
  EXPECT_TRUE(RunBoolFunction("isNativeMojoAvailable()"));

  // Now navigate again with WebUI disabled and ensure the native bindings are
  // not available.
  factory()->set_web_ui_enabled(false);
  NavigateWithNewWebUI("web_ui_mojo_native.html?hybrid");
  EXPECT_FALSE(RunBoolFunction("isNativeMojoAvailable()"));
}

// Disabled due to flakiness: crbug.com/860385.
#if defined(OS_ANDROID)
#define MAYBE_ChromeSendAvailable DISABLED_ChromeSendAvailable
#else
#define MAYBE_ChromeSendAvailable ChromeSendAvailable
#endif
IN_PROC_BROWSER_TEST_F(WebUIMojoTest, MAYBE_ChromeSendAvailable) {
  // chrome.send is not available on mojo-only WebUIs.
  NavigateWithNewWebUI("web_ui_mojo_native.html");
  EXPECT_FALSE(RunBoolFunction("isChromeSendAvailable()"));

  // Now navigate again with normal WebUI bindings and ensure chrome.send is
  // available.
  NavigateWithNewWebUI("web_ui_mojo_native.html?webui_bindings");
  EXPECT_TRUE(RunBoolFunction("isChromeSendAvailable()"));

  // Now navigate again both WebUI and Mojo bindings and ensure chrome.send is
  // available.
  NavigateWithNewWebUI("web_ui_mojo_native.html?hybrid");
  EXPECT_TRUE(RunBoolFunction("isChromeSendAvailable()"));

  // Now navigate again with WebUI disabled and ensure that chrome.send is
  // not available.
  factory()->set_web_ui_enabled(false);
  NavigateWithNewWebUI("web_ui_mojo_native.html?hybrid");
  EXPECT_FALSE(RunBoolFunction("isChromeSendAvailable()"));
}

IN_PROC_BROWSER_TEST_F(WebUIMojoTest, ChromeSendAvailable_AfterCrash) {
  GURL test_url(GetWebUIURL(std::string(kMojoWebUiHost) +
                            "/web_ui_mojo_native.html?webui_bindings"));

  // Navigate with normal WebUI bindings and ensure chrome.send is available.
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_TRUE(EvalJs(shell(), "isChromeSendAvailable()").ExtractBool());

  WebUIImpl* web_ui = static_cast<WebUIImpl*>(
      shell()->web_contents()->GetMainFrame()->GetWebUI());

  // Simulate a crash on the page.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(shell());
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  shell()->web_contents()->GetController().LoadURL(
      GURL(blink::kChromeUICrashURL), content::Referrer(),
      ui::PAGE_TRANSITION_TYPED, std::string());
  crash_observer.Wait();
  EXPECT_FALSE(web_ui->GetRemoteForTest().is_bound());

  // Now navigate again both WebUI and Mojo bindings and ensure chrome.send is
  // available.
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_TRUE(EvalJs(shell(), "isChromeSendAvailable()").ExtractBool());
  // The RenderFrameHost has been replaced after the crash, so get web_ui again.
  web_ui = static_cast<WebUIImpl*>(
      shell()->web_contents()->GetMainFrame()->GetWebUI());
  EXPECT_TRUE(web_ui->GetRemoteForTest().is_bound());
}

}  // namespace
}  // namespace content
