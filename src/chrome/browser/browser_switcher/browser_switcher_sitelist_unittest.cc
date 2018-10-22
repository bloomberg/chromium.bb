// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"

#include "base/values.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace browser_switcher {

namespace {

base::Value StringArrayToValue(const std::vector<const char*>& strings) {
  std::vector<base::Value> values(strings.size());
  std::transform(strings.begin(), strings.end(), values.begin(),
                 [](const char* s) { return base::Value(s); });
  return base::Value(values);
}

}  // namespace

class BrowserSwitcherSitelistTest : public testing::Test {
 public:
  void Initialize(const std::vector<const char*>& url_list,
                  const std::vector<const char*>& url_greylist) {
    prefs_.registry()->RegisterListPref(prefs::kUrlList);
    prefs_.registry()->RegisterListPref(prefs::kUrlGreylist);
    prefs_.Set(prefs::kUrlList, StringArrayToValue(url_list));
    prefs_.Set(prefs::kUrlGreylist, StringArrayToValue(url_greylist));
    sitelist_ = std::make_unique<BrowserSwitcherSitelist>(&prefs_);
  }

  BrowserSwitcherSitelist* sitelist() { return sitelist_.get(); }

 private:
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<BrowserSwitcherSitelist> sitelist_;
};

TEST_F(BrowserSwitcherSitelistTest, ShouldRedirectWildcard) {
  // A "*" by itself means everything matches.
  Initialize({"*"}, {});
  EXPECT_TRUE(sitelist()->ShouldRedirect(GURL("http://example.com/")));
  EXPECT_TRUE(sitelist()->ShouldRedirect(GURL("https://example.com/foobar/")));
  EXPECT_TRUE(sitelist()->ShouldRedirect(GURL("http://example.com/foobar/")));
  EXPECT_TRUE(sitelist()->ShouldRedirect(GURL("http://google.com/")));
}

TEST_F(BrowserSwitcherSitelistTest, ShouldRedirectHost) {
  // A string without slashes means compare the URL's host (case-insensitive).
  Initialize({"example.com"}, {});
  EXPECT_TRUE(sitelist()->ShouldRedirect(GURL("http://example.com/")));
  EXPECT_TRUE(sitelist()->ShouldRedirect(GURL("https://example.com/")));
  EXPECT_TRUE(
      sitelist()->ShouldRedirect(GURL("http://subdomain.example.com/")));
  EXPECT_TRUE(sitelist()->ShouldRedirect(GURL("http://example.com/foobar/")));
  EXPECT_FALSE(sitelist()->ShouldRedirect(GURL("http://google.com/")));
  EXPECT_FALSE(sitelist()->ShouldRedirect(GURL("http://example.ca/")));

  // For backwards compatibility, this should also match, even if it's not the
  // same host.
  EXPECT_TRUE(sitelist()->ShouldRedirect(GURL("https://notexample.com/")));
}

TEST_F(BrowserSwitcherSitelistTest, ShouldRedirectHostNotLowerCase) {
  // Host is not in lowercase form, but we compare ignoring case.
  Initialize({"eXaMpLe.CoM"}, {});
  EXPECT_TRUE(sitelist()->ShouldRedirect(GURL("http://example.com/")));
}

TEST_F(BrowserSwitcherSitelistTest, ShouldRedirectWrongScheme) {
  Initialize({"example.com"}, {});
  // Scheme is not one of 'http', 'https' or 'file'.
  EXPECT_FALSE(sitelist()->ShouldRedirect(GURL("ftp://example.com/")));
}

TEST_F(BrowserSwitcherSitelistTest, ShouldRedirectPrefix) {
  // A string with slashes means check if it's a prefix (case-sensitive).
  Initialize({"http://example.com/foobar"}, {});
  EXPECT_TRUE(sitelist()->ShouldRedirect(GURL("http://example.com/foobar")));
  EXPECT_TRUE(
      sitelist()->ShouldRedirect(GURL("http://example.com/foobar/subroute/")));
  EXPECT_TRUE(
      sitelist()->ShouldRedirect(GURL("http://example.com/foobar#fragment")));
  EXPECT_TRUE(sitelist()->ShouldRedirect(
      GURL("http://example.com/foobar?query=param")));
  EXPECT_FALSE(sitelist()->ShouldRedirect(GURL("http://example.com/")));
  EXPECT_FALSE(sitelist()->ShouldRedirect(GURL("https://example.com/foobar")));
  EXPECT_FALSE(sitelist()->ShouldRedirect(GURL("HTTP://EXAMPLE.COM/FOOBAR")));
  EXPECT_FALSE(
      sitelist()->ShouldRedirect(GURL("http://subdomain.example.com/")));
  EXPECT_FALSE(sitelist()->ShouldRedirect(GURL("http://google.com/")));
}

TEST_F(BrowserSwitcherSitelistTest, ShouldRedirectPrefixNotLowerCase) {
  // The scheme and host are case-insensitive, but the rest is case-sensitive.
  Initialize({"HTTP://EXAMPLE.COM/SUBROUTE"}, {});
  EXPECT_TRUE(sitelist()->ShouldRedirect(GURL("http://example.com/SUBROUTE")));
  EXPECT_FALSE(sitelist()->ShouldRedirect(GURL("http://example.com/subroute")));
}

TEST_F(BrowserSwitcherSitelistTest, ShouldRedirectInvertedMatch) {
  // The most specific (i.e., longest string) rule should have priority.
  Initialize({"!subdomain.example.com", "example.com"}, {});
  EXPECT_TRUE(sitelist()->ShouldRedirect(GURL("http://example.com/")));
  EXPECT_FALSE(
      sitelist()->ShouldRedirect(GURL("http://subdomain.example.com/")));
}

TEST_F(BrowserSwitcherSitelistTest, ShouldRedirectGreylist) {
  // The most specific (i.e., longest string) rule should have priority.
  Initialize({"example.com"}, {"http://example.com/login/"});
  EXPECT_TRUE(sitelist()->ShouldRedirect(GURL("http://example.com/")));
  EXPECT_FALSE(sitelist()->ShouldRedirect(GURL("http://example.com/login/")));
}

TEST_F(BrowserSwitcherSitelistTest, ShouldRedirectGreylistWildcard) {
  Initialize({"*"}, {"*"});
  // If both are wildcards, prefer the greylist.
  EXPECT_FALSE(sitelist()->ShouldRedirect(GURL("http://example.com/")));
}

}  // namespace browser_switcher
