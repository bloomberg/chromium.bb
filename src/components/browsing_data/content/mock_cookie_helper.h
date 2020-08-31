// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_MOCK_COOKIE_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_MOCK_COOKIE_HELPER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "net/cookies/canonical_cookie.h"

namespace content {
class BrowserContext;
}

namespace browsing_data {

// Mock for CookieHelper.
class MockCookieHelper : public CookieHelper {
 public:
  explicit MockCookieHelper(content::BrowserContext* browser_context);

  // CookieHelper methods.
  void StartFetching(FetchCallback callback) override;
  void DeleteCookie(const net::CanonicalCookie& cookie) override;

  // Adds some cookie samples.
  void AddCookieSamples(const GURL& url, const std::string& cookie_line);

  // Notifies the callback.
  void Notify();

  // Marks all cookies as existing.
  void Reset();

  // Returns true if all cookies since the last Reset() invocation were
  // deleted.
  bool AllDeleted();

 private:
  ~MockCookieHelper() override;

  FetchCallback callback_;

  net::CookieList cookie_list_;

  // Stores which cookies exist.
  std::map<const std::string, bool> cookies_;

  DISALLOW_COPY_AND_ASSIGN(MockCookieHelper);
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_MOCK_COOKIE_HELPER_H_
