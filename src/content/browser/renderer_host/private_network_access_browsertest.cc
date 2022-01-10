// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/private_network_access_util.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/resource_load_observer.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

// These domains are mapped to the IP addresses above using the
// `--host-resolver-rules` command-line switch. The exact values come from the
// embedded HTTPS server, which has certificates for these domains
constexpr char kLocalHost[] = "a.test";
constexpr char kPrivateHost[] = "b.test";
constexpr char kPublicHost[] = "c.test";

// Path to a default response served by all servers in this test.
constexpr char kDefaultPath[] = "/defaultresponse";

// Path to a response with the `treat-as-public-address` CSP directive.
constexpr char kTreatAsPublicAddressPath[] =
    "/set-header?Content-Security-Policy: treat-as-public-address";

// Path to a response with a wide-open CORS header. This can be fetched
// cross-origin without triggering CORS violations.
constexpr char kCorsPath[] = "/set-header?Access-Control-Allow-Origin: *";

// Path to a response that passes Private Network Access checks.
constexpr char kPnaPath[] =
    "/set-header?"
    "Access-Control-Allow-Origin: *&"
    "Access-Control-Allow-Private-Network: true";

// Path to a cacheable response.
constexpr char kCacheablePath[] = "/cachetime";

// Returns a snippet of Javascript that fetch()es the given URL.
//
// The snippet evaluates to a boolean promise which resolves to true iff the
// fetch was successful. The promise never rejects, as doing so makes it hard
// to assert failure.
std::string FetchSubresourceScript(const GURL& url) {
  return JsReplace(
      R"(fetch($1).then(
           response => response.ok,
           error => {
             console.log('Error fetching ' + $1, error);
             return false;
           });
      )",
      url);
}

// A |ContentBrowserClient| implementation that allows modifying the return
// value of |ShouldAllowInsecurePrivateNetworkRequests()| at will.
class PolicyTestContentBrowserClient : public TestContentBrowserClient {
 public:
  PolicyTestContentBrowserClient() = default;

  PolicyTestContentBrowserClient(const PolicyTestContentBrowserClient&) =
      delete;
  PolicyTestContentBrowserClient& operator=(
      const PolicyTestContentBrowserClient&) = delete;

  ~PolicyTestContentBrowserClient() override = default;

  // Adds an origin to the allowlist.
  void SetAllowInsecurePrivateNetworkRequestsFrom(const url::Origin& origin) {
    allowlisted_origins_.insert(origin);
  }

  bool ShouldAllowInsecurePrivateNetworkRequests(
      content::BrowserContext* browser_context,
      const url::Origin& origin) override {
    return allowlisted_origins_.find(origin) != allowlisted_origins_.end();
  }

 private:
  std::set<url::Origin> allowlisted_origins_;
};

// RAII wrapper for |SetContentBrowserClientForTesting()|.
class ContentBrowserClientRegistration {
 public:
  explicit ContentBrowserClientRegistration(ContentBrowserClient* client)
      : old_client_(SetBrowserClientForTesting(client)) {}

  ~ContentBrowserClientRegistration() {
    SetBrowserClientForTesting(old_client_);
  }

 private:
  const raw_ptr<ContentBrowserClient> old_client_;
};

// A `net::EmbeddedTestServer` that only starts on demand and pretends to be
// in a particular IP address space.
//
// NOTE(titouan): The IP address space overrides CLI switch is copied to utility
// processes when said processes are started. If we start a lazy server after
// the network process has started, any updates we make to our own CLI switches
// will not propagate to the network process, yielding inconsistent results.
// These tests currently do not rely on this behavior - the IP address space of
// documents is calculated in the browser process, and all network resources
// fetched in this test are `local`. If we want to fix this, we can get rid of
// the fancy lazy-initialization and simply start all servers at test fixture
// construction time, then set up the CLI switch in `SetUpCommandLine()`.
class LazyServer {
 public:
  LazyServer(net::EmbeddedTestServer::Type type,
             network::mojom::IPAddressSpace ip_address_space,
             base::FilePath test_data_path)
      : type_(type),
        ip_address_space_(ip_address_space),
        test_data_path_(std::move(test_data_path)) {}

  net::EmbeddedTestServer& Get() {
    if (!server_) {
      Initialize();
    }
    return *server_;
  }

 private:
  void Initialize() {
    server_ = std::make_unique<net::EmbeddedTestServer>(type_);

    // Use a certificate valid for multiple domains, which we can use to
    // distinguish `local`, `private` and `public` address spaces.
    server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);

    server_->AddDefaultHandlers(test_data_path_);
    EXPECT_TRUE(server_->Start());

    AddCommandLineOverride();
  }

  // Sets up the command line in order for this server to be considered a part
  // of `ip_address_space_`, irrespective of the actual IP it binds to.
  void AddCommandLineOverride() const {
    DCHECK(server_);

    base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
    std::string switch_str = command_line.GetSwitchValueASCII(
        network::switches::kIpAddressSpaceOverrides);

    // If `switch_str` was empty, we prepend an empty value by unconditionally
    // adding a comma before the new entry. This empty value is ignored by the
    // switch parsing logic.
    base::StrAppend(&switch_str,
                    {
                        ",",
                        server_->host_port_pair().ToString(),
                        "=",
                        IPAddressSpaceToSwitchValue(ip_address_space_),
                    });

    command_line.AppendSwitchASCII(network::switches::kIpAddressSpaceOverrides,
                                   switch_str);
  }

  static base::StringPiece IPAddressSpaceToSwitchValue(
      network::mojom::IPAddressSpace space) {
    switch (space) {
      case network::mojom::IPAddressSpace::kLocal:
        return "local";
      case network::mojom::IPAddressSpace::kPrivate:
        return "private";
      case network::mojom::IPAddressSpace::kPublic:
        return "public";
      default:
        ADD_FAILURE() << "Unhandled address space " << space;
        return "";
    }
  }

  net::EmbeddedTestServer::Type type_;
  network::mojom::IPAddressSpace ip_address_space_;
  base::FilePath test_data_path_;
  std::unique_ptr<net::EmbeddedTestServer> server_;
};

}  // namespace

// This being an integration/browser test, we concentrate on a few behaviors
// relevant to Private Network Access:
//
//  - testing the values of important properties on top-level documents:
//    - address space
//    - secure context bit
//    - private network request policy
//  - testing the inheritance semantics of these properties
//  - testing the correct handling of the CSP: treat-as-public-address directive
//  - testing that insecure private network requests are blocked when the right
//    feature flag is enabled
//  - and a few other odds and ends
//
// We use the `--ip-address-space-overrides` command-line switch to test against
// `private` and `public` address spaces, even though all responses are actually
// served from localhost. Combined with host resolver rules, this lets us define
// three different domains that map to the different address spaces:
//
//  - `a.test` is `local`
//  - `b.test` is `private`
//  - `c.test` is `public`
//
// We also have unit tests that test all possible combinations of source and
// destination IP address spaces in services/network/url_loader_unittest.cc.
class PrivateNetworkAccessBrowserTestBase : public ContentBrowserTest {
 public:
  RenderFrameHostImpl* root_frame_host() {
    return static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetMainFrame());
  }

 protected:
  // Allows subclasses to construct instances with different features enabled.
  explicit PrivateNetworkAccessBrowserTestBase(
      const std::vector<base::Feature>& enabled_features,
      const std::vector<base::Feature>& disabled_features)
      : insecure_local_server_(net::EmbeddedTestServer::TYPE_HTTP,
                               network::mojom::IPAddressSpace::kLocal,
                               GetTestDataFilePath()),
        insecure_private_server_(net::EmbeddedTestServer::TYPE_HTTP,
                                 network::mojom::IPAddressSpace::kPrivate,
                                 GetTestDataFilePath()),
        insecure_public_server_(net::EmbeddedTestServer::TYPE_HTTP,
                                network::mojom::IPAddressSpace::kPublic,
                                GetTestDataFilePath()),
        secure_local_server_(net::EmbeddedTestServer::TYPE_HTTPS,
                             network::mojom::IPAddressSpace::kLocal,
                             GetTestDataFilePath()),
        secure_private_server_(net::EmbeddedTestServer::TYPE_HTTPS,
                               network::mojom::IPAddressSpace::kPrivate,
                               GetTestDataFilePath()),
        secure_public_server_(net::EmbeddedTestServer::TYPE_HTTPS,
                              network::mojom::IPAddressSpace::kPublic,
                              GetTestDataFilePath()) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // Rules must be added on the main thread, otherwise `AddRule()` segfaults.
    host_resolver()->AddRule(kLocalHost, "127.0.0.1");
    host_resolver()->AddRule(kPrivateHost, "127.0.0.1");
    host_resolver()->AddRule(kPublicHost, "127.0.0.1");
  }

  GURL InsecureLocalURL(const std::string& path) {
    return insecure_local_server_.Get().GetURL(kLocalHost, path);
  }

  GURL InsecurePrivateURL(const std::string& path) {
    return insecure_private_server_.Get().GetURL(kPrivateHost, path);
  }

  GURL InsecurePublicURL(const std::string& path) {
    return insecure_public_server_.Get().GetURL(kPublicHost, path);
  }

  GURL SecureLocalURL(const std::string& path) {
    return secure_local_server_.Get().GetURL(kLocalHost, path);
  }

  GURL SecurePrivateURL(const std::string& path) {
    return secure_private_server_.Get().GetURL(kPrivateHost, path);
  }

  GURL SecurePublicURL(const std::string& path) {
    return secure_public_server_.Get().GetURL(kPublicHost, path);
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  // All servers are started on demand. Most tests require the use of one or
  // two servers, never six at the same time.
  LazyServer insecure_local_server_;
  LazyServer insecure_private_server_;
  LazyServer insecure_public_server_;
  LazyServer secure_local_server_;
  LazyServer secure_private_server_;
  LazyServer secure_public_server_;
};

// Test with insecure private network subresource requests from the `public`
// address space blocked.
class PrivateNetworkAccessBrowserTest
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTest()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kWarnAboutSecurePrivateNetworkRequests,
            },
            {}) {}
};

// Test with insecure private network subresource requests blocked, including
// from the `private` address space.
class PrivateNetworkAccessBrowserTestBlockFromPrivate
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTestBlockFromPrivate()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kBlockInsecurePrivateNetworkRequestsFromPrivate,
                // This feature should be superseded by the one above.
                features::kPrivateNetworkAccessRespectPreflightResults,
                features::kWarnAboutSecurePrivateNetworkRequests,
            },
            {}) {}
};

// Test with insecure private network subresource requests blocked, including
// from the `private` address space.
class PrivateNetworkAccessBrowserTestBlockFromUnknown
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTestBlockFromUnknown()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kBlockInsecurePrivateNetworkRequestsFromUnknown,
                // This feature should be superseded by the one above.
                features::kPrivateNetworkAccessRespectPreflightResults,
                features::kWarnAboutSecurePrivateNetworkRequests,
            },
            {}) {}
};

// Test with insecure private network requests blocked, including navigations.
class PrivateNetworkAccessBrowserTestBlockNavigations
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTestBlockNavigations()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kBlockInsecurePrivateNetworkRequestsFromPrivate,
                features::kWarnAboutSecurePrivateNetworkRequests,
                features::kBlockInsecurePrivateNetworkRequestsForNavigations,
            },
            {}) {}
};

// Test with the feature to send preflights (unenforced) enabled, and insecure
// private network subresource requests blocked.
class PrivateNetworkAccessBrowserTestNoPreflights
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTestNoPreflights()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
            },
            {
                features::kPrivateNetworkAccessSendPreflights,
            }) {}
};

// Test with the feature to send preflights (enforced) enabled, and insecure
// private network subresource requests blocked.
class PrivateNetworkAccessBrowserTestRespectPreflightResults
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTestRespectPreflightResults()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kPrivateNetworkAccessRespectPreflightResults,
            },
            {}) {}
};

// Test with insecure private network requests allowed.
class PrivateNetworkAccessBrowserTestNoBlocking
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTestNoBlocking()
      : PrivateNetworkAccessBrowserTestBase(
            {},
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kBlockInsecurePrivateNetworkRequestsFromPrivate,
                features::kWarnAboutSecurePrivateNetworkRequests,
                features::kBlockInsecurePrivateNetworkRequestsForNavigations,
            }) {}
};

// ===========================
// CLIENT SECURITY STATE TESTS
// ===========================
//
// These tests verify the contents of `ClientSecurityState` for top-level
// documents in various different circumstances.

// This test verifies the contents of the ClientSecurityState for the initial
// empty document in a new main frame created by the browser.
//
// Note: the renderer-created main frame case is exercised by the
// OpeneeInherits* tests below.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForInitialEmptyDoc) {
  // Start a navigation. This forces the RenderFrameHost to initialize its
  // RenderFrame. The navigation is then cancelled by a HTTP 204 code.
  // We're left with a RenderFrameHost containing the default
  // ClientSecurityState values.
  //
  // Serve the response from a secure public server, to confirm that none of
  // the connection's properties are reflected in the committed document, which
  // is not a secure context and belongs to the `local` address space.
  EXPECT_TRUE(
      NavigateToURLAndExpectNoCommit(shell(), SecurePublicURL("/nocontent")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::CrossOriginEmbedderPolicyValue::kNone,
            security_state->cross_origin_embedder_policy.value);
  EXPECT_EQ(network::mojom::PrivateNetworkRequestPolicy::kBlock,
            security_state->private_network_request_policy);

  // Browser-created empty main frames are trusted to access the local network,
  // if they execute code injected via DevTools, WebView APIs or extensions.
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

// This test verifies the contents of the ClientSecurityState for `about:blank`
// in a new main frame created by the browser.
//
// Note: the renderer-created main frame case is exercised by the Openee
// inheritance tests below.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForAboutBlank) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForDataURL) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kUnknown,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForFileURL) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "empty.html")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForInsecureLocalAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForInsecurePrivateAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePrivateURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPrivate,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForInsecurePublicAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForSecureLocalAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForSecurePrivateAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePrivateURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPrivate,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForSecurePublicAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForTreatAsPublicAddress) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLocalURL(kTreatAsPublicAddressPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForCachedSecureLocalDocument) {
  // Navigate to the cacheable document in order to cache it, then navigate
  // away.
  const GURL url = SecureLocalURL(kCacheablePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  // Navigate to the cached document.
  //
  // NOTE: We do not use `NavigateToURL()`, nor `window.location.reload()`, as
  // both of those seem to bypass the cache.
  ResourceLoadObserver observer(shell());
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.location.href = $1;", url)));
  observer.WaitForResourceCompletion(url);

  blink::mojom::ResourceLoadInfoPtr* info = observer.GetResource(url);
  ASSERT_TRUE(info);
  ASSERT_TRUE(*info);
  EXPECT_TRUE((*info)->was_cached);

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForCachedInsecurePublicDocument) {
  // Navigate to the cacheable document in order to cache it, then navigate
  // away.
  const GURL url = InsecurePublicURL(kCacheablePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  // Navigate to the cached document.
  //
  // NOTE: We do not use `NavigateToURL()`, nor `window.location.reload()`, as
  // both of those seem to bypass the cache.
  ResourceLoadObserver observer(shell());
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.location.href = $1;", url)));
  observer.WaitForResourceCompletion(url);

  blink::mojom::ResourceLoadInfoPtr* info = observer.GetResource(url);
  ASSERT_TRUE(info);
  ASSERT_TRUE(*info);
  EXPECT_TRUE((*info)->was_cached);

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

// This test verifies that the chrome:// scheme is considered local for the
// purpose of Private Network Access.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForSpecialSchemeChromeURL) {
  // Not all chrome:// hosts are available in content/ but ukm is one of them.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("chrome://ukm")));
  EXPECT_TRUE(
      root_frame_host()->GetLastCommittedURL().SchemeIs(kChromeUIScheme));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

// The view-source:// scheme should only ever appear in the display URL. It
// shouldn't affect the IPAddressSpace computation. This test verifies that we
// end up with the response IPAddressSpace.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForSpecialSchemeViewSourcePublic) {
  const GURL url = SecurePublicURL(kDefaultPath);
  EXPECT_TRUE(NavigateToURL(shell(), GURL("view-source:" + url.spec())));

  EXPECT_FALSE(
      root_frame_host()->GetLastCommittedURL().SchemeIs(kViewSourceScheme));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

// Variation of above test with a private address.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForSpecialSchemeViewSourcePrivate) {
  const GURL url = SecurePrivateURL(kDefaultPath);
  EXPECT_TRUE(NavigateToURL(shell(), GURL("view-source:" + url.spec())));

  EXPECT_FALSE(
      root_frame_host()->GetLastCommittedURL().SchemeIs(kViewSourceScheme));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPrivate,
            security_state->ip_address_space);
}

// The chrome-error:// scheme should only ever appear in origins. It shouldn't
// affect the IPAddressSpace computation. This test verifies that we end up with
// the response IPAddressSpace. Error pages should not be considered secure
// contexts however.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForSpecialSchemeChromeErrorPublic) {
  EXPECT_FALSE(NavigateToURL(shell(), SecurePublicURL("/empty404.html")));

  EXPECT_FALSE(
      root_frame_host()->GetLastCommittedURL().SchemeIs(kChromeErrorScheme));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

// Variation of above test with a private address.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForSpecialSchemeChromeErrorPrivate) {
  EXPECT_FALSE(NavigateToURL(shell(), SecurePrivateURL("/empty404.html")));

  EXPECT_FALSE(
      root_frame_host()->GetLastCommittedURL().SchemeIs(kChromeErrorScheme));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPrivate,
            security_state->ip_address_space);
}

// ========================
// INHERITANCE TEST HELPERS
// ========================

namespace {

// Helper for CreateBlobURL() and CreateFilesystemURL().
// ASSERT_* macros can only be used in functions returning void.
void AssertResultIsString(const EvalJsResult& result) {
  // We could skip this assert, but it helps in case of error.
  ASSERT_EQ("", result.error);
  // We could use result.value.is_string(), but this logs the actual type in
  // case of mismatch.
  ASSERT_EQ(base::Value::Type::STRING, result.value.type()) << result.value;
}

// Creates a blob containing dummy HTML, then returns its URL.
// Executes javascript to do so in |frame_host|, which must not be nullptr.
GURL CreateBlobURL(RenderFrameHostImpl* frame_host) {
  EvalJsResult result = EvalJs(frame_host, R"(
    const blob = new Blob(["foo"], {type: "text/html"});
    URL.createObjectURL(blob)
  )");

  AssertResultIsString(result);
  return GURL(result.ExtractString());
}

// Writes some dummy HTML to a file, then returns its `filesystem:` URL.
// Executes javascript to do so in |frame_host|, which must not be nullptr.
GURL CreateFilesystemURL(RenderFrameHostImpl* frame_host) {
  EvalJsResult result = EvalJs(frame_host, R"(
    // It seems anonymous async functions are not available yet, so we cannot
    // use an immediately-invoked function expression.
    async function run() {
      const fs = await new Promise((resolve, reject) => {
        window.webkitRequestFileSystem(window.TEMPORARY, 1024, resolve, reject);
      });
      const file = await new Promise((resolve, reject) => {
        fs.root.getFile('hello.html', {create: true}, resolve, reject);
      });
      const writer = await new Promise((resolve, reject) => {
        file.createWriter(resolve, reject);
      });
      await new Promise((resolve) => {
        writer.onwriteend = resolve;
        writer.write(new Blob(["foo"], {type: "text/html"}));
      });
      return file.toURL();
    }
    run()
  )");

  AssertResultIsString(result);
  return GURL(result.ExtractString());
}

// Helper for AddChildWithScript().
// ASSERT_* macros can only be used in functions returning void.
void AssertChildCountEquals(RenderFrameHostImpl* parent, size_t count) {
  ASSERT_EQ(parent->child_count(), count);
}

// Executes |script| to add a new child iframe to the given |parent| document.
//
// |parent| must not be nullptr.
// |script| must return true / resolve to true upon success.
//
// Returns a pointer to the child frame host.
RenderFrameHostImpl* AddChildWithScript(RenderFrameHostImpl* parent,
                                        const std::string& script) {
  size_t initial_child_count = parent->child_count();

  EvalJsResult result = EvalJs(parent, script);
  EXPECT_EQ(true, result);  // For the error message.

  AssertChildCountEquals(parent, initial_child_count + 1);
  return parent->child_at(initial_child_count)->current_frame_host();
}

// Adds a child iframe sourced from |url| to the given |parent| document.
//
// |parent| must not be nullptr.
RenderFrameHostImpl* AddChildFromURL(RenderFrameHostImpl* parent,
                                     const GURL& url) {
  std::string script_template = R"(
    new Promise((resolve) => {
      const iframe = document.createElement("iframe");
      iframe.src = $1;
      iframe.onload = _ => { resolve(true); };
      document.body.appendChild(iframe);
    })
  )";
  return AddChildWithScript(parent, JsReplace(script_template, url));
}

RenderFrameHostImpl* AddChildFromAboutBlank(RenderFrameHostImpl* parent) {
  return AddChildFromURL(parent, GURL("about:blank"));
}

RenderFrameHostImpl* AddChildInitialEmptyDoc(RenderFrameHostImpl* parent) {
  return AddChildWithScript(parent, R"(
    const iframe = document.createElement("iframe");
    iframe.src = "/nocontent";  // Returns 204 NO CONTENT, thus no doc commits.
    document.body.appendChild(iframe);
    true  // Do not wait for iframe.onload, which never fires.
  )");
}

RenderFrameHostImpl* AddChildFromSrcdoc(RenderFrameHostImpl* parent) {
  return AddChildWithScript(parent, R"(
    new Promise((resolve) => {
      const iframe = document.createElement("iframe");
      iframe.srcdoc = "foo";
      iframe.onload = _ => { resolve(true); };
      document.body.appendChild(iframe);
    })
  )");
}

RenderFrameHostImpl* AddChildFromDataURL(RenderFrameHostImpl* parent) {
  return AddChildFromURL(parent, GURL("data:text/html,foo"));
}

RenderFrameHostImpl* AddChildFromJavascriptURL(RenderFrameHostImpl* parent) {
  return AddChildFromURL(parent, GURL("javascript:'foo'"));
}

RenderFrameHostImpl* AddChildFromBlob(RenderFrameHostImpl* parent) {
  GURL blob_url = CreateBlobURL(parent);
  return AddChildFromURL(parent, blob_url);
}

RenderFrameHostImpl* AddChildFromFilesystem(RenderFrameHostImpl* parent) {
  GURL fs_url = CreateFilesystemURL(parent);
  return AddChildFromURL(parent, fs_url);
}

RenderFrameHostImpl* AddSandboxedChildFromURL(RenderFrameHostImpl* parent,
                                              const GURL& url) {
  std::string script_template = R"(
    new Promise((resolve) => {
      const iframe = document.createElement("iframe");
      iframe.src = $1;
      iframe.sandbox = "";
      iframe.onload = _ => { resolve(true); };
      document.body.appendChild(iframe);
    })
  )";
  return AddChildWithScript(parent, JsReplace(script_template, url));
}

RenderFrameHostImpl* AddSandboxedChildFromAboutBlank(
    RenderFrameHostImpl* parent) {
  return AddSandboxedChildFromURL(parent, GURL("about:blank"));
}

RenderFrameHostImpl* AddSandboxedChildInitialEmptyDoc(
    RenderFrameHostImpl* parent) {
  return AddChildWithScript(parent, R"(
    const iframe = document.createElement("iframe");
    iframe.src = "/nocontent";  // Returns 204 NO CONTENT, thus no doc commits.
    iframe.sandbox = "";
    document.body.appendChild(iframe);
    true  // Do not wait for iframe.onload, which never fires.
  )");
}

RenderFrameHostImpl* AddSandboxedChildFromSrcdoc(RenderFrameHostImpl* parent) {
  return AddChildWithScript(parent, R"(
    new Promise((resolve) => {
      const iframe = document.createElement("iframe");
      iframe.srcdoc = "foo";
      iframe.sandbox = "";
      iframe.onload = _ => { resolve(true); };
      document.body.appendChild(iframe);
    })
  )");
}

RenderFrameHostImpl* AddSandboxedChildFromDataURL(RenderFrameHostImpl* parent) {
  return AddSandboxedChildFromURL(parent, GURL("data:text/html,foo"));
}

RenderFrameHostImpl* AddSandboxedChildFromBlob(RenderFrameHostImpl* parent) {
  GURL blob_url = CreateBlobURL(parent);
  return AddSandboxedChildFromURL(parent, blob_url);
}

RenderFrameHostImpl* AddSandboxedChildFromFilesystem(
    RenderFrameHostImpl* parent) {
  GURL fs_url = CreateFilesystemURL(parent);
  return AddSandboxedChildFromURL(parent, fs_url);
}

// Returns the main frame RenderFrameHostImpl in the given |shell|.
//
// |shell| must not be nullptr.
//
// Helper for OpenWindow*().
RenderFrameHostImpl* GetMainFrameHostImpl(Shell* shell) {
  return static_cast<RenderFrameHostImpl*>(
      shell->web_contents()->GetMainFrame());
}

// Opens a new window from within |parent|, pointed at the given |url|.
// Waits until the openee window has navigated to |url|, then returns a pointer
// to its main frame RenderFrameHostImpl.
//
// |parent| must not be nullptr.
RenderFrameHostImpl* OpenWindowFromURL(RenderFrameHostImpl* parent,
                                       const GURL& url) {
  return GetMainFrameHostImpl(OpenPopup(parent, url, "_blank"));
}

RenderFrameHostImpl* OpenWindowFromAboutBlank(RenderFrameHostImpl* parent) {
  return OpenWindowFromURL(parent, GURL("about:blank"));
}

// Same as above, but with the "noopener" window feature.
RenderFrameHostImpl* OpenWindowFromAboutBlankNoOpener(
    RenderFrameHostImpl* parent) {
  // Setting the "noopener" window feature makes `window.open()` return `null`.
  constexpr bool kNoExpectReturnFromWindowOpen = false;

  return GetMainFrameHostImpl(OpenPopup(parent, GURL("about:blank"), "_blank",
                                        "noopener",
                                        kNoExpectReturnFromWindowOpen));
}

RenderFrameHostImpl* OpenWindowFromURLExpectNoCommit(
    RenderFrameHostImpl* parent,
    const GURL& url,
    base::StringPiece features = "") {
  ShellAddedObserver observer;

  base::StringPiece script_template = R"(
    window.open($1, "_blank", $2);
  )";
  EXPECT_TRUE(ExecJs(parent, JsReplace(script_template, url, features)));

  return GetMainFrameHostImpl(observer.GetShell());
}

RenderFrameHostImpl* OpenWindowInitialEmptyDoc(RenderFrameHostImpl* parent) {
  // Note: We do not use OpenWindowFromURL() because we do not want to wait for
  // a navigation - none will commit.
  return OpenWindowFromURLExpectNoCommit(parent, GURL("/nocontent"));
}

// Same as above, but with the "noopener" window feature.
RenderFrameHostImpl* OpenWindowInitialEmptyDocNoOpener(
    RenderFrameHostImpl* parent) {
  // Note: We do not use OpenWindowFromURL() because we do not want to wait for
  // a navigation - none will commit.
  return OpenWindowFromURLExpectNoCommit(parent, GURL("/nocontent"),
                                         "noopener");
}

GURL JavascriptURL(base::StringPiece script) {
  return GURL(base::StrCat({"javascript:", script}));
}

RenderFrameHostImpl* OpenWindowFromJavascriptURL(
    RenderFrameHostImpl* parent,
    base::StringPiece script = "'foo'") {
  // Note: We do not use OpenWindowFromURL() because we do not want to wait for
  // a navigation, since the `javascript:` URL will not commit (`about:blank`
  // will).
  return OpenWindowFromURLExpectNoCommit(parent, JavascriptURL(script));
}

// Same as above, but with the "noopener" window feature.
RenderFrameHostImpl* OpenWindowFromJavascriptURLNoOpener(
    RenderFrameHostImpl* parent,
    base::StringPiece script) {
  // Note: We do not use OpenWindowFromURL() because we do not want to wait for
  // a navigation - none will commit.
  return OpenWindowFromURLExpectNoCommit(parent, JavascriptURL(script),
                                         "noopener");
}

RenderFrameHostImpl* OpenWindowFromBlob(RenderFrameHostImpl* parent) {
  GURL blob_url = CreateBlobURL(parent);
  return OpenWindowFromURL(parent, blob_url);
}

}  // namespace

// ===============================
// ADDRESS SPACE INHERITANCE TESTS
// ===============================
//
// These tests verify that `ClientSecurityState.ip_address_space` is correctly
// inherited by child iframes and openee documents for a variety of URLs with
// local schemes.

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForAboutBlankFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForAboutBlankFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForAboutBlankFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForAboutBlankFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

// This test verifies that a newly-opened window targeting `about:blank`
// inherits its address space from the opener. In this case, the opener's
// address space is `public`.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForAboutBlankFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

// This test verifies that a newly-opened window targeting `about:blank`
// inherits its address space from the opener. In this case, the opener's
// address space is `local`.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForAboutBlankFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

// This test verifies that a newly-opened window targeting `about:blank`,
// opened with the "noopener" feature, has its address space set to `local`
// regardless of the address space of the opener.
//
// Compare and contrast against the above tests without "noopener".
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeNoOpenerAddressSpaceForAboutBlankIsLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window =
      OpenWindowFromAboutBlankNoOpener(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForInitialEmptyDocFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForInitialEmptyDocFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForInitialEmptyDocFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForInitialEmptyDocFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

// This test verifies that a newly-opened window containing the initial empty
// document inherits its address space from the opener. In this case, the
// opener's address space is `public`.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForInitialEmptyDocFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

// This test verifies that a newly-opened window containing the initial empty
// document inherits its address space from the opener. In this case, the
// opener's address space is `local`.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForInitialEmptyDocFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

// This test verifies that a newly-opened window containing the initial empty
// document, opened with the "noopener" feature, has its address space set to
// `local` regardless of the address space of the opener.
//
// Compare and contrast against the above tests without "noopener".
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeNoOpenerAddressSpaceForInitialEmptyDocIsLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window =
      OpenWindowInitialEmptyDocNoOpener(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForAboutSrcdocFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForAboutSrcdocFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForAboutSrcdocFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForAboutSrcdocFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForDataURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForDataURLFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForDataURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       SandboxedIframeInheritsAddressSpaceForDataURLFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForJavascriptURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddChildFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForJavascriptURLFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddChildFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForJavascriptURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForJavascriptURLFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURL(
      root_frame_host(), "var injectedCodeWasExecuted = true");
  ASSERT_NE(nullptr, window);

  // The Javascript in the URL got executed in the new window.
  EXPECT_EQ(true, EvalJs(window, "injectedCodeWasExecuted"));

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeNoOpenerAddressSpaceForJavascriptURLIsLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURLNoOpener(
      root_frame_host(), "var injectedCodeWasExecuted = true");
  ASSERT_NE(nullptr, window);

  // The Javascript in the URL was not executed in the new window. This ensures
  // it is safe to classify the new window as `local` without allowing the
  // opener to execute arbitrary JS in the `local` address space.
  EXPECT_EQ("undefined", EvalJs(window, "typeof injectedCodeWasExecuted"));

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForBlobURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForBlobURLFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForBlobURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       SandboxedIframeInheritsAddressSpaceForBlobURLFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForBlobURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromBlob(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForBlobURLFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromBlob(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForFilesystemURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromFilesystem(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForFilesystemURLFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromFilesystem(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForFilesystemURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromFilesystem(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForFilesystemURLFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromFilesystem(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

// ================================
// SECURE CONTEXT INHERITANCE TESTS
// ================================
//
// These tests verify that `ClientSecurityState.is_web_secure_context` is
// correctly inherited by child iframes and openee documents for a variety of
// URLs with local schemes.

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForAboutBlankFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForAboutBlankFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForAboutBlankFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForAboutBlankFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsSecureContextForAboutBlankFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsSecureContextForAboutBlankFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    IframeInheritsSecureContextForInitialEmptyDocFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    IframeInheritsSecureContextForInitialEmptyDocFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForInitialEmptyDocFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForInitialEmptyDocFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    OpeneeInheritsSecureContextForInitialEmptyDocFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    OpeneeInheritsSecureContextForInitialEmptyDocFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForAboutSrcdocFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForAboutSrcdocFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForAboutSrcdocFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForAboutSrcdocFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForDataURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForDataURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForDataURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForDataURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForJavascriptURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddChildFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    IframeInheritsSecureContextForJavascriptURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddChildFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    OpeneeInheritsSecureContextForJavascriptURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsSecureContextForJavascriptURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForBlobURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForBlobURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForBlobURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForBlobURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsSecureContextForBlobURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromBlob(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsSecureContextForBlobURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromBlob(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForFilesystemURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromFilesystem(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    IframeInheritsSecureContextForFilesystemURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromFilesystem(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForFilesystemURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromFilesystem(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForFilesystemURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromFilesystem(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

// ====================================
// PRIVATE NETWORK REQUEST POLICY TESTS
// ====================================
//
// These tests verify the correct setting of
// `ClientSecurityState.private_network_request_policy` in various situations.

// This test verifies that with the blocking feature disabled, the private
// network request policy used by RenderFrameHostImpl is to warn about requests
// from non-secure contexts.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoBlocking,
                       PrivateNetworkPolicyIsPreflightWarnByDefault) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kPreflightWarn);
}

// This test verifies that with the blocking feature disabled, the private
// network request policy used by RenderFrameHostImpl is to allow requests from
// secure contexts.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoBlocking,
                       PrivateNetworkPolicyIsAllowByDefaultForSecureContexts) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kPreflightWarn);
}

// This test verifies that by default, the private network request policy used
// by RenderFrameHostImpl for requests is set to block requests from non-secure
// contexts in the `public` address space.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       PrivateNetworkPolicyIsBlockByDefaultForInsecurePublic) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that by default, the private network request policy used
// by RenderFrameHostImpl for requests is set to allow requests from non-secure
// contexts in the `private` address space.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    PrivateNetworkPolicyIsPreflightWarnByDefaultForInsecurePrivate) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePrivateURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kPreflightWarn);
}

// This test verifies that when the right feature is enabled, the private
// network request policy used by RenderFrameHostImpl for requests is set to
// block requests from non-secure contexts in the private address space.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestBlockFromPrivate,
                       PrivateNetworkPolicyIsBlockForInsecurePrivate) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePrivateURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that by default, the private network request policy used
// by RenderFrameHostImpl for requests is set to allow requests from non-secure
// contexts in the `unknown` address space.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       PrivateNetworkPolicyIsAllowByDefaultForInsecureUnknown) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

// This test verifies that when the right feature is enabled, the private
// network request policy used by RenderFrameHostImpl for requests is set to
// block requests from non-secure contexts in the `unknown` address space.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestBlockFromUnknown,
                       PrivateNetworkPolicyIsBlockForInsecureUnknown) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that by default, the private network request policy used
// by RenderFrameHostImpl for requests is set to allow requests from secure
// contexts.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoPreflights,
                       PrivateNetworkPolicyIsAllowByDefaultForSecureContexts) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

// This test verifies that when sending preflights is enabled, the private
// network request policy for secure contexts is `kPreflightWarn`.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       PrivateNetworkPolicyIsPreflightWarnForSecureContexts) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kPreflightWarn);
}

// This test verifies that when sending preflights is enabled, the private
// network request policy for non-secure contexts in the `kPrivate` address
// space is `kPreflightWarn`.
// This checks that as long as the "block from insecure private" feature flag
// is not enabled, we will send preflights for these requests.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       PrivateNetworkPolicyIsPreflightWarnForInsecurePrivate) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePrivateURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kPreflightWarn);
}

// This test verifies that blocking insecure private network requests from the
// `kPublic` address space takes precedence over sending preflight requests.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       PrivateNetworkPolicyIsBlockForInsecurePublic) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that when enforcing preflights is enabled, the private
// network request policy for secure contexts is `kPreflightBlock`.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       PrivateNetworkPolicyIsPreflightBlockForSecureContexts) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kPreflightBlock);
}

// This test verifies that when enforcing preflights is enabled, the private
// network request policy for non-secure contexts in the `kPrivate` address
// space is `kPreflightBlock`.
// This checks that as long as the "block from insecure private" feature flag
// is not enabled, we will enforce preflights for these requests.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       PrivateNetworkPolicyIsPreflightBlockForInsecurePrivate) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePrivateURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kPreflightBlock);
}

// This test verifies that blocking insecure private network requests from the
// `kPublic` address space takes precedence over enforcing preflight requests.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       PrivateNetworkPolicyIsBlockForInsecurePublic) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that child frames with distinct origins from their parent
// do not inherit their private network request policy, which is based on the
// origin of the child document instead.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       PrivateNetworkRequestPolicyCalculatedPerOrigin) {
  GURL url = InsecurePublicURL(kDefaultPath);

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  // Register the client before we navigate, so that the navigation commits the
  // correct PrivateNetworkRequestPolicy.
  ContentBrowserClientRegistration registration(&client);

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* child_frame =
      AddChildFromURL(root_frame_host(), InsecureLocalURL(kDefaultPath));

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that the initial empty document, which inherits its origin
// from the document creator, also inherits its private network request policy.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    PrivateNetworkRequestPolicyInheritedWithOriginForInitialEmptyDoc) {
  GURL url = InsecurePublicURL(kDefaultPath);

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  // Register the client before we navigate, so that the navigation commits the
  // correct PrivateNetworkRequestPolicy.
  ContentBrowserClientRegistration registration(&client);

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

// This test verifies that `about:blank` iframes, which inherit their origin
// from the navigation initiator, also inherit their private network request
// policy.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    PrivateNetworkRequestPolicyInheritedWithOriginForAboutBlank) {
  GURL url = InsecurePublicURL(kDefaultPath);

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  // Register the client before we navigate, so that the navigation commits the
  // correct PrivateNetworkRequestPolicy.
  ContentBrowserClientRegistration registration(&client);

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

// This test verifies that `data:` iframes, which commit an opaque origin
// derived from the navigation initiator's origin, do not inherit their private
// network request policy.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    PrivateNetworkRequestPolicyNotInheritedWithOriginForDataURL) {
  GURL url = InsecurePublicURL(kDefaultPath);

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  // Register the client before we navigate, so that the navigation commits the
  // correct PrivateNetworkRequestPolicy.
  ContentBrowserClientRegistration registration(&client);

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// ==================================================
// SECURE CONTEXT RESTRICTION DEPRECATION TRIAL TESTS
// ==================================================
//
// These tests verify the correct behavior of `private_network_request_policy`
// in the face of the `PrivateNetworkAccessNonSecureContextsAllowed` deprecation
// trial.

// Test with insecure private network requests blocked, excluding navigations.
class PrivateNetworkAccessDeprecationTrialDisabledBrowserTest
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessDeprecationTrialDisabledBrowserTest()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
            },
            {
                features::kBlockInsecurePrivateNetworkRequestsDeprecationTrial,
            }) {}
};

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessDeprecationTrialDisabledBrowserTest,
                       OriginEnabledDoesNothing) {
  DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(NavigateToURL(shell(), interceptor.EnabledUrl()));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       DeprecationTrialOriginEnabled) {
  DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(NavigateToURL(shell(), interceptor.EnabledUrl()));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       DeprecationTrialOriginDisabled) {
  DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(NavigateToURL(shell(), interceptor.DisabledUrl()));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// =======================
// SUBRESOURCE FETCH TESTS
// =======================
//
// These tests verify the behavior of the browser when fetching subresources
// across IP address spaces. When the right features are enabled, private
// network requests are blocked.

// This test mimics the tests below, with all blocking features disabled. It
// verifies that by default requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoBlocking,
                       PrivateNetworkRequestIsNotBlockedByDefault) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  // Check that the page can load a local resource.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are disabled, requests:
//  - from a secure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromSecureTreatAsPublicToLocalIsNotBlocked) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLocalURL(kTreatAsPublicAddressPath)));

  // Check that the page can load a local resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are disabled, requests:
//  - from a secure page served from a public IP address
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoPreflights,
                       FromSecurePublicToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  // Check that the page can load a local resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are sent but not enforced, requests:
//  - from a secure page served from a public IP address
//  - to a local IP address
//  - for which the target server does not respond OK to the preflight request
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromSecurePublicToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  // Check that the page can load a local resource.
  //
  // We load the resource from a secure origin to avoid running afoul of mixed
  // content restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are sent and enforced, requests:
//  - from a secure page served from a public IP address
//  - to a local IP address
//  - when the target server does not respond OK to the preflight request
// are blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       FromSecurePublicToLocalIsBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  // We load the resource from a secure origin to avoid running afoul of mixed
  // content restrictions.
  EXPECT_EQ(false, EvalJs(root_frame_host(),
                          FetchSubresourceScript(SecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are disabled, requests:
//  - from a secure page served from a private IP address
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoPreflights,
                       FromSecurePrivateToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePrivateURL(kDefaultPath)));

  // Check that the page can load a local resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are sent but not enforced, requests:
//  - from a secure page served from a private IP address
//  - to a local IP address
//  - for which the target server does not respond OK to the preflight request
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromSecurePrivateToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePrivateURL(kDefaultPath)));

  // Check that the page can load a local resource.
  //
  // We load it from a secure origin to avoid running afoul of mixed content
  // restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are sent and enforced, requests:
//  - from a secure page served from a private IP address
//  - to a local IP address
//  - for which the target server does not respond OK to the preflight request
// are blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       FromSecurePrivateToLocalIsBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePrivateURL(kDefaultPath)));

  // We load the resource from a secure origin to avoid running afoul of mixed
  // content restrictions.
  EXPECT_EQ(false, EvalJs(root_frame_host(),
                          FetchSubresourceScript(SecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are disabled, requests:
//  - from a secure page served from a local IP address
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoPreflights,
                       FromSecureLocalToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  // Check that the page can load a local resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are sent but not enforced, requests:
//  - from a secure page served from a local IP address
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromSecureLocalToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  // Check that the page can load a local resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are sent and enforced, requests:
//  - from a secure page served from a local IP address
//  - to a local IP address
//  - for which the target server does not respond OK to the preflight request
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       FromSecureLocalToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  // Check that the page can load a local resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are sent but not enforced, requests:
//  - from a secure page served from a local IP address
//  - to a local IP address
//  - for which the target server responds OK to the preflight request
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromSecurePublicToLocalPreflightOK) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  // Check that the page can load a local resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kPnaPath))));
}

// This test verifies that when preflights are sent and enforced, requests:
//  - from a secure page served from a local IP address
//  - to a local IP address
//  - for which the target server responds OK to the preflight request
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       FromSecurePublicToLocalPreflightOK) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  // Check that the page can load a local resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kPnaPath))));
}

// This test verifies that when the right feature is enabled but the content
// browser client overrides it, requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    FromInsecureTreatAsPublicToLocalWithPolicySetToAllowIsNotBlocked) {
  GURL url = InsecureLocalURL(kTreatAsPublicAddressPath);

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  // Register the client before we navigate, so that the navigation commits the
  // correct PrivateNetworkRequestPolicy.
  ContentBrowserClientRegistration registration(&client);

  EXPECT_TRUE(NavigateToURL(shell(), url));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);

  // Check that the page can load a local resource.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// This test verifies that when the right feature is enabled, requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromInsecureTreatAsPublicToLocalIsBlocked) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLocalURL(kTreatAsPublicAddressPath)));

  // Check that the page cannot load a local resource.
  EXPECT_EQ(false, EvalJs(root_frame_host(),
                          FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// This test verifies that when the right feature is enabled, requests:
//  - from an insecure page served by a public IP address
//  - to local IP addresses
//  are blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromInsecurePublicToLocalIsBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  // Check that the page cannot load a local resource.
  EXPECT_EQ(false, EvalJs(root_frame_host(),
                          FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// This test verifies that when the right feature is disabled, requests:
//  - from an insecure page served by a private IP address
//  - to local IP addresses
//  are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromInsecurePrivateToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePrivateURL(kDefaultPath)));

  // Check that the page can load a local resource.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// This test verifies that when the right feature is enabled, requests:
//  - from an insecure page served by a private IP address
//  - to local IP addresses
//  are blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestBlockFromPrivate,
                       FromInsecurePrivateToLocalIsBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePrivateURL(kDefaultPath)));

  // Check that the page cannot load a local resource.
  EXPECT_EQ(false, EvalJs(root_frame_host(),
                          FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// This test verifies that when the right feature is enabled, requests:
//  - from an insecure page served by a local IP address
//  - to local IP addresses
//  are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromInsecureLocalToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  // Check that the page can load a local resource.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// This test verifies that when the right feature is enabled, requests:
//  - from a secure page with the "treat-as-public-address" CSP directive
//  - embedded in an insecure page served from a local IP address
//  - to local IP addresses
//  are blocked.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    FromSecurePublicEmbeddedInInsecureLocalToLocalIsBlocked) {
  // First navigate to an insecure page served by a local IP address.
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  // Then embed a secure public iframe.
  std::string script = JsReplace(
      R"(
        const iframe = document.createElement("iframe");
        iframe.src = $1;
        document.body.appendChild(iframe);
      )",
      SecurePublicURL(kDefaultPath));
  EXPECT_TRUE(ExecJs(root_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ASSERT_EQ(1ul, root_frame_host()->child_count());
  RenderFrameHostImpl* child_frame =
      root_frame_host()->child_at(0)->current_frame_host();

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  // Even though the iframe document was loaded from a secure connection, the
  // context is deemed insecure because it was embedded by an insecure context.
  EXPECT_FALSE(security_state->is_web_secure_context);

  // The address space of the document, however, is not influenced by the
  // parent's address space.
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);

  // Check that the iframe cannot load a local resource.
  EXPECT_EQ(false, EvalJs(child_frame,
                          FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// This test verifies that even when the right feature is enabled, requests:
//  - from a non-secure context in the `public` IP address space
//  - to a subresource cached from a `local` IP address
//  are not blocked.
//
// TODO(https://crbug.com/1124340): Decide whether this is bad and either change
// this test or delete this todo.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromInsecurePublicToCachedLocalIsBlocked) {
  GURL cached_url = SecureLocalURL(kCacheablePath);

  // Cache the resource first, by fetching it from a document in the same IP
  // address space.
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));
  EXPECT_EQ(true,
            EvalJs(root_frame_host(), FetchSubresourceScript(cached_url)));

  // Now navigate to a document in the `public` address space belonging to the
  // same site as the previous document (this will use the same cache key).
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLocalURL(kTreatAsPublicAddressPath)));

  ResourceLoadObserver observer(shell());

  // Check that the page can still load the subresource. This fetch would fail
  // were the subresource not cached.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(), FetchSubresourceScript(cached_url)));

  observer.WaitForResourceCompletion(cached_url);

  // And that the resource was loaded from the cache.
  blink::mojom::ResourceLoadInfoPtr* info = observer.GetResource(cached_url);
  ASSERT_TRUE(info);
  ASSERT_TRUE(*info);
  EXPECT_TRUE((*info)->was_cached);
}

// This test verifies that even with the blocking feature disabled, an insecure
// page in the `local` address space cannot fetch a `file:` URL.
//
// This is relevant to Private Network Access, since `file:` URLs are considered
// to be in the `local` IP address space.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoBlocking,
                       InsecurePageCannotRequestFile) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  // Check that the page cannot load a `file:` URL.
  EXPECT_EQ(false, EvalJs(root_frame_host(), FetchSubresourceScript(GetTestUrl(
                                                 "", "empty.html"))));
}

// This test verifies that even with the blocking feature disabled, a secure
// page in the `local` address space cannot fetch a `file:` URL.
//
// This is relevant to Private Network Access, since `file:` URLs are considered
// to be in the `local` IP address space.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoBlocking,
                       SecurePageCannotRequestFile) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  // Check that the page cannot load a `file:` URL.
  EXPECT_EQ(false, EvalJs(root_frame_host(), FetchSubresourceScript(GetTestUrl(
                                                 "", "empty.html"))));
}

// ======================
// NAVIGATION FETCH TESTS
// ======================
//
// These tests verify the behavior of the browser when navigating across IP
// address spaces.
//
// Iframe navigations are effectively treated as subresource fetches of the
// parent document: they are handled by checking the resource's address space
// against the parent document's address space. This is incorrect, as the
// initiator of the navigation is not always the parent document.
//
// TODO(https://crbug.com/1170335): Revisit this when the initiator's address
// space is used instead.
//
// Top-level navigations are never blocked.
//
// TODO(https://crbug.com/1129326): Revisit this when top-level navigations are
// subject to Private Network Access checks.

// This test verifies that when the right feature is enabled, iframe requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestBlockNavigations,
                       IframeFromInsecureTreatAsPublicToLocalIsBlocked) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLocalURL(kTreatAsPublicAddressPath)));

  GURL url = InsecureLocalURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  EXPECT_TRUE(ExecJs(root_frame_host(), R"(
    const iframe = document.createElement("iframe");
    iframe.src = "/empty.html";
    document.body.appendChild(iframe);
  )"));

  child_navigation_manager.WaitForNavigationFinished();

  // Check that the child iframe failed to fetch.
  EXPECT_FALSE(child_navigation_manager.was_successful());

  ASSERT_EQ(1ul, root_frame_host()->child_count());
  RenderFrameHostImpl* child_frame =
      root_frame_host()->child_at(0)->current_frame_host();
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            EvalJs(child_frame, "document.location.href"));

  // The frame committed an error page but retains the original URL so that
  // reloading the page does the right thing. The committed origin on the other
  // hand is opaque, which it would not be if the navigation had succeeded.
  EXPECT_EQ(url, child_frame->GetLastCommittedURL());
  EXPECT_TRUE(child_frame->GetLastCommittedOrigin().opaque());
}

// This test mimics the one above, only it is executed without enabling the
// BlockInsecurePrivateNetworkRequestsForNavigations feature. It asserts that
// the navigation is not blocked in this case.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeFromInsecureTreatAsPublicToLocalIsNotBlocked) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLocalURL(kTreatAsPublicAddressPath)));

  GURL url = InsecureLocalURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  EXPECT_TRUE(ExecJs(root_frame_host(), R"(
    const iframe = document.createElement("iframe");
    iframe.src = "/empty.html";
    document.body.appendChild(iframe);
  )"));

  child_navigation_manager.WaitForNavigationFinished();

  // Check that the child iframe navigated successfully.
  EXPECT_TRUE(child_navigation_manager.was_successful());

  ASSERT_EQ(1ul, root_frame_host()->child_count());
  RenderFrameHostImpl* child_frame =
      root_frame_host()->child_at(0)->current_frame_host();
  EXPECT_EQ(url, EvalJs(child_frame, "document.location.href"));
}

// Similar to IframeFromInsecureTreatAsPublicToLocalIsBlocked, but in
// report-only mode. As a result "treat-as-public-address" must be ignored.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       CspReportOnlyTreatAsPublicAddressIgnored) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      InsecureLocalURL("/set-header?Content-Security-Policy-Report-Only: "
                       "treat-as-public-address")));

  GURL url = InsecureLocalURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  EXPECT_TRUE(ExecJs(root_frame_host(), R"(
    const iframe = document.createElement("iframe");
    iframe.src = "/empty.html";
    document.body.appendChild(iframe);
  )"));

  child_navigation_manager.WaitForNavigationFinished();

  // Check that the child iframe was not blocked.
  EXPECT_TRUE(child_navigation_manager.was_successful());

  ASSERT_EQ(1ul, root_frame_host()->child_count());
  RenderFrameHostImpl* child_frame =
      root_frame_host()->child_at(0)->current_frame_host();
  EXPECT_EQ(url, EvalJs(child_frame, "document.location.href"));
  EXPECT_EQ(url, child_frame->GetLastCommittedURL());
  EXPECT_FALSE(child_frame->GetLastCommittedOrigin().opaque());
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestBlockNavigations,
    FormSubmissionFromInsecurePublictoLocalIsNotBlockedInMainFrame) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  GURL url = InsecureLocalURL(kDefaultPath);
  TestNavigationManager navigation_manager(shell()->web_contents(), url);

  base::StringPiece script_template = R"(
    const form = document.createElement("form");
    form.action = $1;
    form.method = "post";
    document.body.appendChild(form);
    form.submit();
  )";

  EXPECT_TRUE(ExecJs(root_frame_host(), JsReplace(script_template, url)));

  navigation_manager.WaitForNavigationFinished();

  // Check that the child iframe was not blocked.
  EXPECT_TRUE(navigation_manager.was_successful());

  EXPECT_EQ(url, EvalJs(root_frame_host(), "document.location.href"));
  EXPECT_EQ(url, root_frame_host()->GetLastCommittedURL());
  EXPECT_FALSE(root_frame_host()->GetLastCommittedOrigin().opaque());
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestBlockNavigations,
    FormSubmissionFromInsecurePublictoLocalIsBlockedInChildFrame) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  GURL url = InsecureLocalURL(kDefaultPath);
  TestNavigationManager navigation_manager(shell()->web_contents(), url);

  base::StringPiece script_template = R"(
    const iframe = document.createElement("iframe");
    document.body.appendChild(iframe);

    const childDoc = iframe.contentDocument;
    const form = childDoc.createElement("form");
    form.action = $1;
    form.method = "post";
    childDoc.body.appendChild(form);
    form.submit();
  )";

  EXPECT_TRUE(ExecJs(root_frame_host(), JsReplace(script_template, url)));

  navigation_manager.WaitForNavigationFinished();

  // Check that the child iframe was blocked.
  EXPECT_FALSE(navigation_manager.was_successful());

  ASSERT_EQ(1ul, root_frame_host()->child_count());
  RenderFrameHostImpl* child_frame =
      root_frame_host()->child_at(0)->current_frame_host();

  // Failed navigation.
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            EvalJs(child_frame, "document.location.href"));

  // The URL is the form target URL, to allow for reloading.
  // The origin is opaque though, a symptom of the failed navigation.
  EXPECT_EQ(url, child_frame->GetLastCommittedURL());
  EXPECT_TRUE(child_frame->GetLastCommittedOrigin().opaque());
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestBlockNavigations,
    FormSubmissionGetFromInsecurePublictoLocalIsBlockedInChildFrame) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  GURL target_url = InsecureLocalURL(kDefaultPath);

  // The page navigates to `url` followed by an empty query: '?'.
  GURL expected_url = GURL(target_url.spec() + "?");
  TestNavigationManager navigation_manager(shell()->web_contents(),
                                           expected_url);

  base::StringPiece script_template = R"(
    const iframe = document.createElement("iframe");
    document.body.appendChild(iframe);

    const childDoc = iframe.contentDocument;
    const form = childDoc.createElement("form");
    form.action = $1;
    form.method = "get";
    childDoc.body.appendChild(form);
    form.submit();
  )";

  EXPECT_TRUE(
      ExecJs(root_frame_host(), JsReplace(script_template, target_url)));

  navigation_manager.WaitForNavigationFinished();

  // Check that the child iframe was blocked.
  EXPECT_FALSE(navigation_manager.was_successful());

  ASSERT_EQ(1ul, root_frame_host()->child_count());
  RenderFrameHostImpl* child_frame =
      root_frame_host()->child_at(0)->current_frame_host();

  // Failed navigation.
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            EvalJs(child_frame, "document.location.href"));

  // The URL is the form target URL, to allow for reloading.
  // The origin is opaque though, a symptom of the failed navigation.
  EXPECT_EQ(expected_url, child_frame->GetLastCommittedURL());
  EXPECT_TRUE(child_frame->GetLastCommittedOrigin().opaque());
}

}  // namespace content
