// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/portal/portal.h"
#include "content/browser/portal/portal_navigation_throttle.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/portal/portal_created_observer.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {
namespace {

// TODO(jbroman): Perhaps this would be a useful utility generally.
GURL GetServerRedirectURL(const net::EmbeddedTestServer* server,
                          const std::string& hostname,
                          const GURL& destination) {
  return server->GetURL(
      hostname,
      "/server-redirect?" +
          net::EscapeQueryParamValue(destination.spec(), /*use_plus=*/false));
}

class PortalNavigationThrottleBrowserTest : public ContentBrowserTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kPortals},
        /*disabled_features=*/{blink::features::kPortalsCrossOrigin});
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/notreached",
        base::BindRepeating(
            [](const net::test_server::HttpRequest& r)
                -> std::unique_ptr<net::test_server::HttpResponse> {
              ADD_FAILURE() << "/notreached was requested";
              return nullptr;
            })));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* GetWebContents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }
  RenderFrameHostImpl* GetMainFrame() {
    return GetWebContents()->GetMainFrame();
  }

  Portal* InsertAndWaitForPortal(const GURL& url,
                                 bool expected_to_succeed = true) {
    TestNavigationObserver navigation_observer(url);
    navigation_observer.StartWatchingNewWebContents();
    PortalCreatedObserver portal_created_observer(GetMainFrame());
    EXPECT_TRUE(ExecJs(
        GetMainFrame(),
        base::StringPrintf("var portal = document.createElement('portal');\n"
                           "portal.src = '%s';\n"
                           "document.body.appendChild(portal);",
                           url.spec().c_str())));
    Portal* portal = portal_created_observer.WaitUntilPortalCreated();
    navigation_observer.StopWatchingNewWebContents();
    navigation_observer.Wait();
    EXPECT_EQ(navigation_observer.last_navigation_succeeded(),
              expected_to_succeed);
    return portal;
  }

  bool NavigatePortalViaSrcAttribute(Portal* portal,
                                     const GURL& url,
                                     int number_of_navigations) {
    TestNavigationObserver navigation_observer(portal->GetPortalContents(),
                                               number_of_navigations);
    EXPECT_TRUE(
        ExecJs(GetMainFrame(),
               base::StringPrintf(
                   "document.querySelector('body > portal').src = '%s';",
                   url.spec().c_str())));
    navigation_observer.WaitForNavigationFinished();
    return navigation_observer.last_navigation_succeeded();
  }

  bool NavigatePortalViaLocationHref(Portal* portal,
                                     const GURL& url,
                                     int number_of_navigations) {
    TestNavigationObserver navigation_observer(portal->GetPortalContents(),
                                               number_of_navigations);
    EXPECT_TRUE(ExecJs(
        portal->GetPortalContents(),
        base::StringPrintf("location.href = '%s';", url.spec().c_str())));
    navigation_observer.WaitForNavigationFinished();
    return navigation_observer.last_navigation_succeeded();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       SameOriginInitialNavigation) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("portal.test", "/title2.html"));
  EXPECT_NE(portal, nullptr);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       CrossOriginInitialNavigation) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("not.portal.test", "/title2.html"),
      /*expected_to_succeed=*/false);
  EXPECT_NE(portal, nullptr);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       SameOriginNavigation) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("portal.test", "/title2.html"));

  GURL destination_url =
      embedded_test_server()->GetURL("portal.test", "/title3.html");
  EXPECT_TRUE(NavigatePortalViaSrcAttribute(portal, destination_url, 1));
  EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(),
            destination_url);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       SameOriginNavigationTriggeredByPortal) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("portal.test", "/title2.html"));

  GURL destination_url =
      embedded_test_server()->GetURL("portal.test", "/title3.html");
  EXPECT_TRUE(NavigatePortalViaLocationHref(portal, destination_url, 1));
  EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(),
            destination_url);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       SameOriginNavigationWithServerRedirect) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("portal.test", "/title2.html"));

  GURL destination_url =
      embedded_test_server()->GetURL("portal.test", "/title3.html");
  GURL redirect_url = GetServerRedirectURL(embedded_test_server(),
                                           "portal.test", destination_url);
  EXPECT_TRUE(NavigatePortalViaSrcAttribute(portal, redirect_url, 1));
  EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(),
            destination_url);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       CrossOriginNavigation) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("portal.test", "/title2.html"));

  GURL destination_url =
      embedded_test_server()->GetURL("not.portal.test", "/notreached");
  EXPECT_FALSE(NavigatePortalViaSrcAttribute(portal, destination_url, 1));
  EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(),
            destination_url);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       CrossOriginNavigationTriggeredByPortal) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("portal.test", "/title2.html"));

  GURL destination_url =
      embedded_test_server()->GetURL("not.portal.test", "/notreached");
  EXPECT_FALSE(NavigatePortalViaLocationHref(portal, destination_url, 1));
  EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(),
            destination_url);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       CrossOriginNavigationViaRedirect) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("portal.test", "/title2.html"));

  GURL destination_url =
      embedded_test_server()->GetURL("not.portal.test", "/notreached");
  GURL redirect_url = GetServerRedirectURL(embedded_test_server(),
                                           "portal.test", destination_url);
  EXPECT_FALSE(NavigatePortalViaSrcAttribute(portal, redirect_url, 1));
  EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(),
            destination_url);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       CrossOriginRedirectLeadingBack) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("portal.test", "/title2.html"));

  GURL destination_url =
      embedded_test_server()->GetURL("portal.test", "/notreached");
  GURL redirect_url = GetServerRedirectURL(embedded_test_server(),
                                           "not.portal.test", destination_url);
  EXPECT_FALSE(NavigatePortalViaSrcAttribute(portal, redirect_url, 1));
  EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(), redirect_url);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       LogsConsoleWarning) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));

  auto* old_delegate = GetWebContents()->GetDelegate();
  ConsoleObserverDelegate console_delegate(GetWebContents(),
                                           "*portal*cross-origin*");
  GetWebContents()->SetDelegate(&console_delegate);

  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("not.portal.test", "/title2.html"),
      /*expected_to_succeed=*/false);
  EXPECT_NE(portal, nullptr);

  console_delegate.Wait();
  EXPECT_THAT(console_delegate.message(),
              ::testing::HasSubstr("http://not.portal.test"));
  GetWebContents()->SetDelegate(old_delegate);
}

}  // namespace
}  // namespace content
