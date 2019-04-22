// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace content {

// Test suite covering the interaction between browser bookmarks and
// `Sec-Fetch-*` headers that can't be covered by Web Platform Tests (yet).
// See https://mikewest.github.io/sec-metadata/#directly-user-initiated and
// https://github.com/web-platform-tests/wpt/issues/16019.
class SecFetchBrowserTest : public ContentBrowserTest {
 public:
  SecFetchBrowserTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_test_server_.AddDefaultHandlers(GetTestDataFilePath());
    https_test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(https_test_server_.Start());

    feature_list_.InitWithFeatures(
        {network::features::kFetchMetadata,
         network::features::kFetchMetadataDestination},
        {});
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  void NavigateForHeader(const std::string& header) {
    std::string url = "/echoheader?";
    ASSERT_TRUE(
        NavigateToURL(shell(), https_test_server_.GetURL(url + header)));

    NavigationEntry* entry =
        web_contents()->GetController().GetLastCommittedEntry();
    ASSERT_TRUE(PageTransitionCoreTypeIs(entry->GetTransitionType(),
                                         ui::PAGE_TRANSITION_TYPED));
  }

  GURL GetSecFetchUrl() {
    return https_test_server_.GetURL("/echoheader?sec-fetch-site");
  }

  std::string GetContent() {
    return EvalJs(shell(), "document.body.innerText").ExtractString();
  }

 private:
  net::EmbeddedTestServer https_test_server_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SecFetchBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SecFetchBrowserTest, TypedNavigation) {
  {
    // Sec-Fetch-Dest: document
    NavigateForHeader("Sec-Fetch-Dest");
    EXPECT_EQ("document", GetContent());
  }

  {
    // Sec-Fetch-Mode: navigate
    NavigateForHeader("Sec-Fetch-Mode");
    EXPECT_EQ("navigate", GetContent());
  }

  {
    // Sec-Fetch-Site: none
    NavigateForHeader("Sec-Fetch-Site");
    EXPECT_EQ("none", GetContent());
  }

  {
    // Sec-Fetch-User: ?1
    NavigateForHeader("Sec-Fetch-User");
    EXPECT_EQ("?1", GetContent());
  }
}

}  // namespace content
