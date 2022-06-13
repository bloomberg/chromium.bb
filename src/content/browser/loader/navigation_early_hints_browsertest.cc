// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/loader/navigation_early_hints_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/quic_simple_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/link_header.mojom.h"

namespace content {

using PreloadedResources = NavigationEarlyHintsManager::PreloadedResources;

namespace {

struct HeaderField {
  HeaderField(const std::string& name, const std::string& value)
      : name(name), value(value) {}

  std::string name;
  std::string value;
};

struct ResponseEntry {
  ResponseEntry(const std::string& path, net::HttpStatusCode status_code)
      : path(path) {
    headers[":path"] = path;
    headers[":status"] = base::StringPrintf("%d", status_code);
  }

  void AddEarlyHints(const std::vector<HeaderField>& header_fields) {
    spdy::Http2HeaderBlock hints_headers;
    for (const auto& header : header_fields)
      hints_headers.AppendValueOrAddHeader(header.name, header.value);
    early_hints.push_back(std::move(hints_headers));
  }

  std::string path;
  spdy::Http2HeaderBlock headers;
  std::string body;
  std::vector<spdy::Http2HeaderBlock> early_hints;
};

const char kPageWithHintedScriptPath[] = "/page_with_hinted_js.html";
const char kPageWithHintedScriptBody[] = "<script src=\"/hinted.js\"></script>";

const char kPageWithHintedCorsScriptPath[] = "/page_with_hinted_cors_js.html";
const char kPageWithHintedCorsScriptBody[] =
    "<script src=\"/hinted.js\" crossorigin></script>";

const char kPageWithIframePath[] = "/page_with_iframe.html";
const char kPageWithIframeBody[] =
    "<iframe src=\"page_with_hinted_js.html\"></iframe>";

const char kPageWithHintedModuleScriptPath[] =
    "/page_with_hinted_module_js.html";
const char kPageWithHintedModuleScriptBody[] =
    "<script src=\"/hinted.js\" type=\"module\"></script>";

const char kHintedScriptPath[] = "/hinted.js";
const char kHintedScriptBody[] = "document.title = 'Done';";

const char kHintedStylesheetPath[] = "/hinted.css";
const char kHintedStylesheetBody[] = "/*empty*/";

const char kEmptyPagePath[] = "/empty.html";
const char kEmptyPageBody[] = "<html></html>";

// Listens to sockets on an EmbeddedTestServer for preconnect tests. Created
// on the UI thread. EmbeddedTestServerConnectionListener methods are called
// from a different thread than the UI thread.
class PreconnectListener
    : public net::test_server::EmbeddedTestServerConnectionListener {
 public:
  PreconnectListener()
      : task_runner_(base::ThreadTaskRunnerHandle::Get()),
        weak_ptr_factory_(this) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }
  ~PreconnectListener() override = default;

  // net::test_server::EmbeddedTestServerConnectionListener implementation:
  std::unique_ptr<net::StreamSocket> AcceptedSocket(
      std::unique_ptr<net::StreamSocket> connection) override {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PreconnectListener::AcceptedSocketOnUIThread,
                                  weak_ptr_factory_.GetWeakPtr()));
    return connection;
  }
  void ReadFromSocket(const net::StreamSocket& connection, int rv) override {}

  size_t num_accepted_sockets() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return num_accepted_sockets_;
  }

 private:
  void AcceptedSocketOnUIThread() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    ++num_accepted_sockets_;
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  size_t num_accepted_sockets_ = 0;

  base::WeakPtrFactory<PreconnectListener> weak_ptr_factory_;
};

}  // namespace

// Most tests use EmbeddedTestServer but this uses QuicSimpleTestServer because
// Early Hints are only plumbed over HTTP/2 or HTTP/3 (QUIC).
class NavigationEarlyHintsTest : public ContentBrowserTest {
 public:
  NavigationEarlyHintsTest() = default;
  ~NavigationEarlyHintsTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ConfigureMockCertVerifier();
    host_resolver()->AddRule("*", "127.0.0.1");

    cross_origin_server_.RegisterRequestHandler(
        base::BindRepeating(&NavigationEarlyHintsTest::HandleCrossOriginRequest,
                            base::Unretained(this)));
    preconnect_listener_ = std::make_unique<PreconnectListener>();
    cross_origin_server().SetConnectionListener(preconnect_listener_.get());
    ASSERT_TRUE(cross_origin_server_.Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kOriginToForceQuicOn, "*");
    mock_cert_verifier_.SetUpCommandLine(command_line);
    feature_list_.InitWithFeatures(
        {features::kEarlyHintsPreloadForNavigation,
         net::features::kSplitCacheByNetworkIsolationKey},
        {});

    ASSERT_TRUE(net::QuicSimpleTestServer::Start());

    ContentBrowserTest::SetUpCommandLine(command_line);
  }

  void TearDown() override {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    net::QuicSimpleTestServer::Shutdown();
    ContentBrowserTest::TearDown();
  }

  net::test_server::EmbeddedTestServer& cross_origin_server() {
    return cross_origin_server_;
  }

 protected:
  base::test::ScopedFeatureList& feature_list() { return feature_list_; }

  PreconnectListener& preconnect_listener() { return *preconnect_listener_; }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void ConfigureMockCertVerifier() {
    auto test_cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "quic-chain.pem");
    net::CertVerifyResult verify_result;
    verify_result.verified_cert = test_cert;
    mock_cert_verifier_.mock_cert_verifier()->AddResultForCert(
        test_cert, verify_result, net::OK);
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
  }

  HeaderField CreatePreloadLinkForScript() {
    return HeaderField(
        "link",
        base::StringPrintf("<%s>; rel=preload; as=script", kHintedScriptPath));
  }

  HeaderField CreatePreloadLinkForCorsScript() {
    return HeaderField(
        "link", base::StringPrintf("<%s>; rel=preload; as=script; crossorigin",
                                   kHintedScriptPath));
  }

  HeaderField CreateModulePreloadLink() {
    return HeaderField("link", base::StringPrintf("<%s>; rel=modulepreload",
                                                  kHintedScriptPath));
  }

  HeaderField CreatePreloadLinkForStylesheet() {
    return HeaderField("link", base::StringPrintf("<%s>; rel=preload; as=style",
                                                  kHintedStylesheetPath));
  }

  void RegisterResponse(const ResponseEntry& entry) {
    net::QuicSimpleTestServer::AddResponseWithEarlyHints(
        entry.path, entry.headers, entry.body, entry.early_hints);
  }

  void RegisterHintedScriptResource() {
    ResponseEntry hinted_script_entry(kHintedScriptPath, net::HTTP_OK);
    hinted_script_entry.headers["content-type"] = "application/javascript";
    hinted_script_entry.headers["cache-control"] = "max-age=3600";
    hinted_script_entry.body = kHintedScriptBody;
    RegisterResponse(hinted_script_entry);
  }

  void RegisterHintedStylesheetResource() {
    ResponseEntry hinted_script_entry(kHintedStylesheetPath, net::HTTP_OK);
    hinted_script_entry.headers["content-type"] = "text/css";
    hinted_script_entry.body = kHintedStylesheetBody;
    RegisterResponse(hinted_script_entry);
  }

  ResponseEntry CreatePageEntryWithHintedScript(
      net::HttpStatusCode status_code) {
    RegisterHintedScriptResource();

    ResponseEntry entry(kPageWithHintedScriptPath, status_code);
    entry.body = kPageWithHintedScriptBody;
    HeaderField link_header = CreatePreloadLinkForScript();
    entry.AddEarlyHints({std::move(link_header)});

    return entry;
  }

  ResponseEntry CreateEmptyPageEntryWithHintedScript() {
    RegisterHintedScriptResource();

    ResponseEntry entry(kEmptyPagePath, net::HTTP_OK);
    entry.body = kEmptyPageBody;
    HeaderField link_header = CreatePreloadLinkForScript();
    entry.AddEarlyHints({std::move(link_header)});

    return entry;
  }

  ResponseEntry CreatePageEntryWithHintedCorsScript(
      net::HttpStatusCode status_code) {
    RegisterHintedScriptResource();

    ResponseEntry entry(kPageWithHintedCorsScriptPath, status_code);
    entry.body = kPageWithHintedCorsScriptBody;
    HeaderField link_header = CreatePreloadLinkForCorsScript();
    entry.AddEarlyHints({std::move(link_header)});

    return entry;
  }

  ResponseEntry CreatePageEntryWithHintedModuleScript(
      net::HttpStatusCode status_code) {
    RegisterHintedScriptResource();

    ResponseEntry entry(kPageWithHintedModuleScriptPath, status_code);
    entry.body = kPageWithHintedModuleScriptBody;
    HeaderField link_header = CreateModulePreloadLink();
    entry.AddEarlyHints({std::move(link_header)});

    return entry;
  }

  bool NavigateToURLAndWaitTitle(const GURL& url, const std::string& title) {
    std::u16string title16 = base::ASCIIToUTF16(title);
    TitleWatcher title_watcher(shell()->web_contents(), title16);
    if (!NavigateToURL(shell(), url))
      return false;
    return title16 == title_watcher.WaitAndGetTitle();
  }

  NavigationEarlyHintsManager* GetEarlyHintsManager() {
    RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetMainFrame());
    return rfh->early_hints_manager();
  }

  PreloadedResources WaitForPreloadedResources() {
    base::RunLoop loop;
    PreloadedResources result;
    if (!GetEarlyHintsManager())
      return result;

    GetEarlyHintsManager()->WaitForPreloadsFinishedForTesting(
        base::BindLambdaForTesting([&](PreloadedResources preloaded_resources) {
          result = preloaded_resources;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  enum class FetchResult {
    kFetched,
    kBlocked,
  };
  FetchResult FetchScriptOnDocument(ToRenderFrameHost target, GURL src) {
    EvalJsResult result = EvalJs(target, JsReplace(R"(
      new Promise(resolve => {
        const script = document.createElement("script");
        script.src = $1;
        script.onerror = () => resolve("blocked");
        script.onload = () => resolve("fetched");
        document.body.appendChild(script);
      });
    )",
                                                   src));
    return result.ExtractString() == "fetched" ? FetchResult::kFetched
                                               : FetchResult::kBlocked;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleCrossOriginRequest(
      const net::test_server::HttpRequest& request) {
    GURL relative_url = request.base_url.Resolve(request.relative_url);

    if (relative_url.path() == "/empty.html") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content("");
      return std::move(response);
    }

    if (relative_url.path() != "/hinted.js")
      return nullptr;

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("application/javascript");
    response->set_content("/*empty*/");

    std::string query = relative_url.query();
    if (query == "corp-cross-origin") {
      response->AddCustomHeader("Cross-Origin-Resource-Policy", "cross-origin");
    } else if (query == "corp-same-origin") {
      response->AddCustomHeader("Cross-Origin-Resource-Policy", "same-origin");
    }

    return std::move(response);
  }

  base::test::ScopedFeatureList feature_list_;

  ContentMockCertVerifier mock_cert_verifier_;

  // For tests that fetch resources from a cross origin server.
  net::EmbeddedTestServer cross_origin_server_;
  std::unique_ptr<PreconnectListener> preconnect_listener_;
};

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, Basic) {
  ResponseEntry entry = CreatePageEntryWithHintedScript(net::HTTP_OK);
  RegisterResponse(entry);

  EXPECT_TRUE(NavigateToURLAndWaitTitle(
      net::QuicSimpleTestServer::GetFileURL(kPageWithHintedScriptPath),
      "Done"));
  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_EQ(preloads.size(), 1UL);

  GURL preloaded_url = net::QuicSimpleTestServer::GetFileURL(kHintedScriptPath);
  auto it = preloads.find(preloaded_url);
  ASSERT_TRUE(it != preloads.end());
  ASSERT_FALSE(it->second.was_canceled);
  ASSERT_TRUE(it->second.error_code.has_value());
  EXPECT_EQ(it->second.error_code.value(), net::OK);
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, CorsAttribute) {
  ResponseEntry entry = CreatePageEntryWithHintedCorsScript(net::HTTP_OK);
  RegisterResponse(entry);

  EXPECT_TRUE(NavigateToURLAndWaitTitle(
      net::QuicSimpleTestServer::GetFileURL(kPageWithHintedCorsScriptPath),
      "Done"));
  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_EQ(preloads.size(), 1UL);

  GURL preloaded_url = net::QuicSimpleTestServer::GetFileURL(kHintedScriptPath);
  auto it = preloads.find(preloaded_url);
  ASSERT_TRUE(it != preloads.end());
  ASSERT_FALSE(it->second.was_canceled);
  ASSERT_TRUE(it->second.error_code.has_value());
  EXPECT_EQ(it->second.error_code.value(), net::OK);
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, ModulePreload) {
  ResponseEntry entry = CreatePageEntryWithHintedModuleScript(net::HTTP_OK);
  RegisterResponse(entry);

  EXPECT_TRUE(NavigateToURLAndWaitTitle(
      net::QuicSimpleTestServer::GetFileURL(kPageWithHintedModuleScriptPath),
      "Done"));
  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_EQ(preloads.size(), 1UL);

  GURL preloaded_url = net::QuicSimpleTestServer::GetFileURL(kHintedScriptPath);
  auto it = preloads.find(preloaded_url);
  ASSERT_TRUE(it != preloads.end());
  ASSERT_FALSE(it->second.was_canceled);
  ASSERT_TRUE(it->second.error_code.has_value());
  EXPECT_EQ(it->second.error_code.value(), net::OK);
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, DisallowPreloadFromIframe) {
  ResponseEntry page_entry(kPageWithIframePath, net::HTTP_OK);
  page_entry.body = kPageWithIframeBody;
  RegisterResponse(page_entry);

  ResponseEntry iframe_entry = CreatePageEntryWithHintedScript(net::HTTP_OK);
  RegisterResponse(iframe_entry);

  EXPECT_TRUE(NavigateToURL(
      shell(), net::QuicSimpleTestServer::GetFileURL(kPageWithIframePath)));

  // Find RenderFrameHost for the iframe.
  std::vector<RenderFrameHost*> all_frames =
      CollectAllRenderFrameHosts(shell()->web_contents());
  ASSERT_EQ(all_frames.size(), 2UL);
  ASSERT_EQ(all_frames[0], all_frames[1]->GetParent());
  RenderFrameHostImpl* iframe_host =
      static_cast<RenderFrameHostImpl*>(all_frames[1]);

  EXPECT_TRUE(WaitForLoadStop(WebContents::FromRenderFrameHost(iframe_host)));
  ASSERT_EQ(iframe_host->GetLastCommittedURL(),
            net::QuicSimpleTestServer::GetFileURL(kPageWithHintedScriptPath));

  // NavigationEarlyHintsManager should not be created for subframes. If it were
  // created it should have been created before navigation commit.
  EXPECT_EQ(iframe_host->early_hints_manager(), nullptr);
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, NavigationServerError) {
  ResponseEntry entry =
      CreatePageEntryWithHintedScript(net::HTTP_INTERNAL_SERVER_ERROR);
  entry.body = "Internal Server Error";
  RegisterResponse(entry);

  EXPECT_TRUE(NavigateToURL(shell(), net::QuicSimpleTestServer::GetFileURL(
                                         kPageWithHintedScriptPath)));
  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_TRUE(preloads.empty());
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, Redirect) {
  ResponseEntry entry = CreatePageEntryWithHintedScript(net::HTTP_FOUND);
  entry.headers["location"] = "/";
  entry.body = "";
  RegisterResponse(entry);

  EXPECT_TRUE(NavigateToURL(
      shell(), net::QuicSimpleTestServer::GetFileURL(kPageWithHintedScriptPath),
      net::QuicSimpleTestServer::GetFileURL("/")));
  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_TRUE(preloads.empty());
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, InvalidPreloadLink) {
  const std::string kPath = "/hinted.html";

  RegisterHintedScriptResource();

  ResponseEntry entry(kPath, net::HTTP_OK);
  entry.body = "body";
  entry.AddEarlyHints(
      {HeaderField("link", base::StringPrintf("<%s>; rel=preload; as=invalid",
                                              kHintedScriptPath))});
  RegisterResponse(entry);

  EXPECT_TRUE(
      NavigateToURL(shell(), net::QuicSimpleTestServer::GetFileURL(kPath)));
  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_TRUE(preloads.empty());
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, DuplicatePreloads) {
  RegisterHintedScriptResource();
  RegisterHintedStylesheetResource();

  ResponseEntry entry(kPageWithHintedScriptPath, net::HTTP_OK);
  entry.body = kPageWithHintedScriptBody;

  // Set two Early Hints responses which contain duplicate preload link headers.
  HeaderField script_link_header = CreatePreloadLinkForScript();
  HeaderField stylesheet_link_header = CreatePreloadLinkForStylesheet();
  entry.AddEarlyHints({script_link_header});
  entry.AddEarlyHints({script_link_header, stylesheet_link_header});
  RegisterResponse(entry);

  EXPECT_TRUE(NavigateToURLAndWaitTitle(
      net::QuicSimpleTestServer::GetFileURL(kPageWithHintedScriptPath),
      "Done"));
  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_EQ(preloads.size(), 2UL);

  GURL script_url = net::QuicSimpleTestServer::GetFileURL(kHintedScriptPath);
  GURL stylesheet_url =
      net::QuicSimpleTestServer::GetFileURL(kHintedStylesheetPath);
  EXPECT_TRUE(preloads.contains(script_url));
  EXPECT_TRUE(preloads.contains(stylesheet_url));
}

const char kPageWithCrossOriginScriptPage[] =
    "/page_with_cross_origin_script.html";

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, CORP_Pass) {
  // The server response's is a script with
  // `Cross-Origin-Resource-Policy: cross-origin`.
  const GURL kCrossOriginScriptUrl =
      cross_origin_server().GetURL("/hinted.js?corp-cross-origin");

  ResponseEntry page_entry(kPageWithCrossOriginScriptPage, net::HTTP_OK);
  HeaderField link_header = HeaderField(
      "link", base::StringPrintf("<%s>; rel=preload; as=script",
                                 kCrossOriginScriptUrl.spec().c_str()));
  page_entry.AddEarlyHints({std::move(link_header)});
  RegisterResponse(page_entry);

  EXPECT_TRUE(NavigateToURL(shell(), net::QuicSimpleTestServer::GetFileURL(
                                         kPageWithCrossOriginScriptPage)));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(FetchScriptOnDocument(shell(), kCrossOriginScriptUrl),
            FetchResult::kFetched);

  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_EQ(preloads.size(), 1UL);

  auto it = preloads.find(kCrossOriginScriptUrl);
  ASSERT_TRUE(it != preloads.end());
  ASSERT_FALSE(it->second.was_canceled);
  ASSERT_TRUE(it->second.error_code.has_value());
  EXPECT_EQ(it->second.error_code.value(), net::OK);
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, CORP_Blocked) {
  // The server response's is a script with
  // `Cross-Origin-Resource-Policy: same-origin`.
  const GURL kCrossOriginScriptUrl =
      cross_origin_server().GetURL("/hinted.js?corp-same-origin");

  ResponseEntry page_entry(kPageWithCrossOriginScriptPage, net::HTTP_OK);
  HeaderField link_header = HeaderField(
      "link", base::StringPrintf("<%s>; rel=preload; as=script",
                                 kCrossOriginScriptUrl.spec().c_str()));
  page_entry.AddEarlyHints({std::move(link_header)});
  RegisterResponse(page_entry);

  EXPECT_TRUE(NavigateToURL(shell(), net::QuicSimpleTestServer::GetFileURL(
                                         kPageWithCrossOriginScriptPage)));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // The script fetch should be blocked.
  EXPECT_EQ(FetchScriptOnDocument(shell(), kCrossOriginScriptUrl),
            FetchResult::kBlocked);
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, COEP_Pass) {
  // The server sends `Cross-Origin-Resource-Policy: cross-origin`.
  const GURL kCrossOriginScriptUrl =
      cross_origin_server().GetURL("/hinted.js?corp-cross-origin");
  ResponseEntry page_entry(kPageWithCrossOriginScriptPage, net::HTTP_OK);
  page_entry.headers["cross-origin-embedder-policy"] = "require-corp";
  HeaderField link_header = HeaderField(
      "link", base::StringPrintf("<%s>; rel=preload; as=script",
                                 kCrossOriginScriptUrl.spec().c_str()));
  HeaderField coep =
      HeaderField("cross-origin-embedder-policy", "require-corp");
  page_entry.AddEarlyHints({std::move(link_header), std::move(coep)});
  RegisterResponse(page_entry);

  EXPECT_TRUE(NavigateToURL(shell(), net::QuicSimpleTestServer::GetFileURL(
                                         kPageWithCrossOriginScriptPage)));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(FetchScriptOnDocument(shell(), kCrossOriginScriptUrl),
            FetchResult::kFetched);
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, COEP_Block) {
  // The server does not send `Cross-Origin-Resource-Policy` header.
  const GURL kCrossOriginScriptUrl = cross_origin_server().GetURL("/hinted.js");
  ResponseEntry page_entry(kPageWithCrossOriginScriptPage, net::HTTP_OK);
  page_entry.headers["cross-origin-embedder-policy"] = "require-corp";
  HeaderField link_header = HeaderField(
      "link", base::StringPrintf("<%s>; rel=preload; as=script",
                                 kCrossOriginScriptUrl.spec().c_str()));
  HeaderField coep =
      HeaderField("cross-origin-embedder-policy", "require-corp");
  page_entry.AddEarlyHints({std::move(link_header), std::move(coep)});
  RegisterResponse(page_entry);

  EXPECT_TRUE(NavigateToURL(shell(), net::QuicSimpleTestServer::GetFileURL(
                                         kPageWithCrossOriginScriptPage)));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(FetchScriptOnDocument(shell(), kCrossOriginScriptUrl),
            FetchResult::kBlocked);
}

// Test that network isolation key is set correctly for Early Hints preload.
IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, NetworkIsolationKey) {
  const GURL kHintedScriptUrl =
      net::QuicSimpleTestServer::GetFileURL(kHintedScriptPath);

  ResponseEntry entry = CreateEmptyPageEntryWithHintedScript();
  RegisterResponse(entry);

  absl::optional<bool> is_cached;
  URLLoaderInterceptor interceptor(
      base::BindLambdaForTesting(
          [&](URLLoaderInterceptor::RequestParams* params) { return false; }),
      base::BindLambdaForTesting(
          [&](const GURL& request_url,
              const network::URLLoaderCompletionStatus& status) {
            if (request_url != kHintedScriptUrl)
              return;
            is_cached = status.exists_in_cache;
          }),
      base::NullCallback());

  ASSERT_TRUE(NavigateToURL(
      shell(), net::QuicSimpleTestServer::GetFileURL(kEmptyPagePath)));

  // Make sure the hinted resource is preloaded.
  PreloadedResources preloads = WaitForPreloadedResources();
  auto it = preloads.find(kHintedScriptUrl);
  ASSERT_TRUE(it != preloads.end());
  ASSERT_FALSE(it->second.was_canceled);
  ASSERT_EQ(it->second.error_code.value(), net::OK);

  ASSERT_FALSE(is_cached.value());
  is_cached = absl::nullopt;

  // Fetch the hinted resource from the main frame. It should come from the
  // cache.
  FetchScriptOnDocument(shell(), kHintedScriptUrl);
  ASSERT_TRUE(is_cached.value());

  // Reset `is_cached` to make sure it is set true or false.
  is_cached = absl::nullopt;

  // Create an iframe with a different origin and fetch the hinted resource from
  // the iframe. It should not come from the cache.
  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHost* iframe =
      CreateSubframe(web_contents, /*frame_id=*/"",
                     cross_origin_server().GetURL("/empty.html"),
                     /*wait_for_navigation=*/true);
  FetchScriptOnDocument(iframe, kHintedScriptUrl);
  ASSERT_FALSE(is_cached.value());
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, SimplePreconnect) {
  const char kPageWithPreconnect[] = "/page_with_preconnect.html";
  const GURL kPreconnectUrl = cross_origin_server().GetURL("/");
  ResponseEntry page_entry(kPageWithPreconnect, net::HTTP_OK);
  HeaderField link_header =
      HeaderField("link", base::StringPrintf("<%s>; rel=preconnect",
                                             kPreconnectUrl.spec().c_str()));
  page_entry.AddEarlyHints({std::move(link_header)});
  RegisterResponse(page_entry);

  ASSERT_TRUE(NavigateToURL(
      shell(), net::QuicSimpleTestServer::GetFileURL(kPageWithPreconnect)));
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(preconnect_listener().num_accepted_sockets(), 1UL);
  EXPECT_TRUE(GetEarlyHintsManager()->WasResourceHintsReceived());
}

class NavigationEarlyHintsAddressSpaceTest : public NavigationEarlyHintsTest {
 public:
  NavigationEarlyHintsAddressSpaceTest() = default;
  ~NavigationEarlyHintsAddressSpaceTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    NavigationEarlyHintsTest::SetUpCommandLine(command_line);

    private_server_.AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(private_server_.Start());

    // Treat the main test server as public for IPAddressSpace tests.
    command_line->AppendSwitchASCII(
        network::switches::kIpAddressSpaceOverrides,
        base::StringPrintf("127.0.0.1:%d=public",
                           net::QuicSimpleTestServer::GetPort()));
  }

  net::test_server::EmbeddedTestServer& private_server() {
    return private_server_;
  }

 private:
  // For tests that trigger private network requests.
  net::EmbeddedTestServer private_server_;
};

// Tests that Early Hints preload is blocked when hints comes from the public
// network but a hinted resource is located in a private network.
IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsAddressSpaceTest,
                       PublicToPrivateRequestBlocked) {
  const GURL kPrivateResourceUrl = private_server().GetURL("/blank.jpg");
  ResponseEntry page_entry(kEmptyPagePath, net::HTTP_OK);
  HeaderField link_header = HeaderField(
      "link", base::StringPrintf("<%s>; rel=preload; as=image",
                                 kPrivateResourceUrl.spec().c_str()));
  page_entry.AddEarlyHints({std::move(link_header)});
  RegisterResponse(page_entry);

  EXPECT_TRUE(NavigateToURL(
      shell(), net::QuicSimpleTestServer::GetFileURL(kEmptyPagePath)));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_EQ(preloads.size(), 1UL);

  auto it = preloads.find(kPrivateResourceUrl);
  ASSERT_TRUE(it != preloads.end());
  ASSERT_FALSE(it->second.was_canceled);
  ASSERT_TRUE(it->second.error_code.has_value());
  EXPECT_EQ(it->second.error_code.value(), net::ERR_FAILED);
  EXPECT_EQ(it->second.cors_error_status->cors_error,
            network::mojom::CorsError::kInsecurePrivateNetwork);
}

enum class FieldTrialTestingMode {
  kDisabled,
  kForceDisabled,
  kEnabled,
};

std::string FieldTrialTestingModeToString(
    const testing::TestParamInfo<FieldTrialTestingMode>& info) {
  switch (info.param) {
    case FieldTrialTestingMode::kDisabled:
      return "FieldTrialDisabled";
    case FieldTrialTestingMode::kForceDisabled:
      return "FieldTrialForceDisabled";
    case FieldTrialTestingMode::kEnabled:
      return "FieldTrialEnabled";
  }
}

class NavigationEarlyHintsOriginTrialTest
    : public NavigationEarlyHintsTest,
      public testing::WithParamInterface<FieldTrialTestingMode> {
 public:
  NavigationEarlyHintsOriginTrialTest() = default;
  ~NavigationEarlyHintsOriginTrialTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    NavigationEarlyHintsTest::SetUpCommandLine(command_line);
    feature_list().Reset();
    switch (GetFieldTrialTestingMode()) {
      case FieldTrialTestingMode::kDisabled:
        feature_list().InitWithFeatures(
            {net::features::kSplitCacheByNetworkIsolationKey},
            {features::kEarlyHintsPreloadForNavigation});
        break;
      case FieldTrialTestingMode::kForceDisabled:
        feature_list().InitWithFeaturesAndParameters(
            /*enabled_features=*/{{features::kEarlyHintsPreloadForNavigation,
                                   {{"force_disable", "true"}}},
                                  {net::features::
                                       kSplitCacheByNetworkIsolationKey,
                                   {}}},
            /*disabled_features=*/{});
        break;
      case FieldTrialTestingMode::kEnabled:
        feature_list().InitWithFeatures(
            {net::features::kSplitCacheByNetworkIsolationKey,
             features::kEarlyHintsPreloadForNavigation},
            {});
        break;
    }
  }

  void SetUpOnMainThread() override {
    NavigationEarlyHintsTest::SetUpOnMainThread();
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindRepeating(
            &NavigationEarlyHintsOriginTrialTest::InterceptRequest,
            base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    NavigationEarlyHintsTest::TearDownOnMainThread();
  }

 protected:
  FieldTrialTestingMode GetFieldTrialTestingMode() { return GetParam(); }

  void SetOriginTrialTokens(const std::vector<std::string>& tokens) {
    origin_trial_tokens_ = tokens;
  }

  bool NavigateToTestPageAndCheckPreloadTriggered() {
    EXPECT_TRUE(NavigateToURLAndWaitTitle(GetUrl("/"), "Done"));
    PreloadedResources preloaded_resources = WaitForPreloadedResources();
    return preloaded_resources.size() > 0;
  }

 private:
  GURL GetUrl(const std::string& path) {
    return GURL("https://example.test/").Resolve(path);
  }

  // URLLoaderInterceptor callback
  bool InterceptRequest(URLLoaderInterceptor::RequestParams* params) {
    if (params->url_request.url.path() == "/") {
      // Send an Early Hints response.
      auto link_header = network::mojom::LinkHeader::New(
          GetUrl(kHintedScriptPath), network::mojom::LinkRelAttribute::kPreload,
          network::mojom::LinkAsAttribute::kScript,
          network::mojom::CrossOriginAttribute::kUnspecified,
          /*mime_type=*/absl::nullopt);
      auto hints = network::mojom::EarlyHints::New();
      hints->headers = network::mojom::ParsedHeaders::New();
      hints->headers->link_headers.push_back(std::move(link_header));
      hints->origin_trial_tokens = origin_trial_tokens_;
      params->client->OnReceiveEarlyHints(std::move(hints));

      // Send the final response.
      std::string headers =
          base::StrCat({"HTTP/1.1 200 OK\n", "Content-Type: text/html\n",
                        "Link: </hinted.js>; rel=preload; as=script\n", "\n"});
      URLLoaderInterceptor::WriteResponse(headers, kPageWithHintedScriptBody,
                                          params->client.get());
      return true;
    } else if (params->url_request.url.path() == "/hinted.js") {
      std::string headers = base::StrCat(
          {"HTTP/1.1 200 OK\n", "Content-Type: application/javascript\n",
           "Cache-Control: max-age=3600\n", "\n"});
      URLLoaderInterceptor::WriteResponse(headers, kHintedScriptBody,
                                          params->client.get());
      return true;
    }
    return false;
  }

  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  std::vector<std::string> origin_trial_tokens_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         NavigationEarlyHintsOriginTrialTest,
                         testing::Values(FieldTrialTestingMode::kDisabled,
                                         FieldTrialTestingMode::kForceDisabled,
                                         FieldTrialTestingMode::kEnabled),
                         FieldTrialTestingModeToString);

namespace {

// The following tokens expire at 2033-05-18.

// Generated by:
// tools/origin_trials/generate_token.py --version 3    \
//   --expire-timestamp=2000000000                      \
//   example.test EarlyHintsPreloadForNavigation
std::string kOriginTrialTokenValid =
    "A3chvgB5i9ttqhgstNf2W6c2fRL54WuepUiMS+sOoreEBoM+"
    "xi9hBdVuyhRqIOamM6OF7Fwalx3RkzvEUI+"
    "phwAAAABpeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR1cmUiO"
    "iAiRWFybHlIaW50c1ByZWxvYWRGb3JOYXZpZ2F0aW9uIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB"
    "9";

// Generated by:
// tools/origin_trials/generate_token.py --version 3    \
//   --expire-timestamp=2000000000                      \
//   other-origin.test EarlyHintsPreloadForNavigation
std::string kOriginTrialTokenOtherOrigin =
    "A0LXLzkFLYR6QrW+"
    "sbsAjOsqiyP2f4kiwJJEaATApr6WS4KAE51nk60Amw2kSutaOWYdVmNZWlWxAmIlmaasYgkAAA"
    "BueyJvcmlnaW4iOiAiaHR0cHM6Ly9vdGhlci1vcmlnaW4udGVzdDo0NDMiLCAiZmVhdHVyZSI6"
    "ICJFYXJseUhpbnRzUHJlbG9hZEZvck5hdmlnYXRpb24iLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH"
    "0=";

// Generated by:
// tools/origin_trials/generate_token.py --version 3    \
//   --expire-timestamp=2000000000                      \
//   example.test OtherFeature1
std::string kOriginTrialTokenOtherFeature1 =
    "A75D3a49+"
    "qBCTIdxmGZR41PbBLgYHCcxRNO5X03jCwQRYlk2nSqclhYpe6UuwXzcYrwKDRrSp1fW6eJFy3y"
    "29wsAAABYeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR1cmUiO"
    "iAiT3RoZXJGZWF0dXJlMSIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==";

// Generated by:
// tools/origin_trials/generate_token.py --version 3    \
//   --expire-timestamp=2000000000                      \
//   example.test OtherFeature2
std::string kOriginTrialTokenOtherFeature2 =
    "A0B34vdKZlnhkQ+70he3dtEN6E+"
    "ZLlyFjOjLoJaqOztOWJ07XtZZHVrnkvTMdz5UWBiPcRZ3zg1DHAAq+"
    "1ar5gYAAABYeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR1cmU"
    "iOiAiT3RoZXJGZWF0dXJlMiIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==";

}  // namespace

IN_PROC_BROWSER_TEST_P(NavigationEarlyHintsOriginTrialTest, ValidToken) {
  SetOriginTrialTokens({kOriginTrialTokenValid});
  bool triggered = NavigateToTestPageAndCheckPreloadTriggered();
  switch (GetFieldTrialTestingMode()) {
    case FieldTrialTestingMode::kDisabled:
      EXPECT_TRUE(triggered);
      break;
    case FieldTrialTestingMode::kForceDisabled:
      EXPECT_FALSE(triggered);
      break;
    case FieldTrialTestingMode::kEnabled:
      EXPECT_TRUE(triggered);
      break;
  }
}

IN_PROC_BROWSER_TEST_P(NavigationEarlyHintsOriginTrialTest, OriginIsDifferent) {
  SetOriginTrialTokens({kOriginTrialTokenOtherOrigin});
  bool triggered = NavigateToTestPageAndCheckPreloadTriggered();
  switch (GetFieldTrialTestingMode()) {
    case FieldTrialTestingMode::kDisabled:
      EXPECT_FALSE(triggered);
      break;
    case FieldTrialTestingMode::kForceDisabled:
      EXPECT_FALSE(triggered);
      break;
    case FieldTrialTestingMode::kEnabled:
      EXPECT_TRUE(triggered);
      break;
  }
}

IN_PROC_BROWSER_TEST_P(NavigationEarlyHintsOriginTrialTest,
                       FeatureIsDifferent) {
  SetOriginTrialTokens({kOriginTrialTokenOtherFeature1});
  bool triggered = NavigateToTestPageAndCheckPreloadTriggered();
  switch (GetFieldTrialTestingMode()) {
    case FieldTrialTestingMode::kDisabled:
      EXPECT_FALSE(triggered);
      break;
    case FieldTrialTestingMode::kForceDisabled:
      EXPECT_FALSE(triggered);
      break;
    case FieldTrialTestingMode::kEnabled:
      EXPECT_TRUE(triggered);
      break;
  }
}

IN_PROC_BROWSER_TEST_P(NavigationEarlyHintsOriginTrialTest,
                       MultipleTokens_ContainsValidToken) {
  SetOriginTrialTokens(
      {kOriginTrialTokenOtherFeature1, kOriginTrialTokenValid});
  bool triggered = NavigateToTestPageAndCheckPreloadTriggered();
  switch (GetFieldTrialTestingMode()) {
    case FieldTrialTestingMode::kDisabled:
      EXPECT_TRUE(triggered);
      break;
    case FieldTrialTestingMode::kForceDisabled:
      EXPECT_FALSE(triggered);
      break;
    case FieldTrialTestingMode::kEnabled:
      EXPECT_TRUE(triggered);
      break;
  }
}

IN_PROC_BROWSER_TEST_P(NavigationEarlyHintsOriginTrialTest,
                       MultipleTokens_NoValidToken) {
  SetOriginTrialTokens(
      {kOriginTrialTokenOtherFeature1, kOriginTrialTokenOtherFeature2});
  bool triggered = NavigateToTestPageAndCheckPreloadTriggered();
  switch (GetFieldTrialTestingMode()) {
    case FieldTrialTestingMode::kDisabled:
      EXPECT_FALSE(triggered);
      break;
    case FieldTrialTestingMode::kForceDisabled:
      EXPECT_FALSE(triggered);
      break;
    case FieldTrialTestingMode::kEnabled:
      EXPECT_TRUE(triggered);
      break;
  }
}

}  // namespace content
