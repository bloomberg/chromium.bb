// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SAFE_SEARCH_URL_REPORTER_H_
#define CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SAFE_SEARCH_URL_REPORTER_H_

#include <list>
#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/gurl.h"

class GURL;
class Profile;

namespace identity {
class IdentityManager;
struct AccessTokenInfo;
}  // namespace identity

namespace network {
class SharedURLLoaderFactory;
}

class SafeSearchURLReporter {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;

  SafeSearchURLReporter(
      identity::IdentityManager* identity_manager,
      const std::string& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~SafeSearchURLReporter();

  static std::unique_ptr<SafeSearchURLReporter> CreateWithProfile(
      Profile* profile);

  void ReportUrl(const GURL& url, SuccessCallback callback);

 private:
  struct Report;
  using ReportList = std::list<std::unique_ptr<Report>>;

  void OnAccessTokenFetchComplete(Report* report,
                                  GoogleServiceAuthError error,
                                  identity::AccessTokenInfo token_info);

  void OnSimpleLoaderComplete(ReportList::iterator it,
                              std::unique_ptr<std::string> response_body);

  // Requests an access token, which is the first thing we need. This is where
  // we restart when the returned access token has expired.
  void StartFetching(Report* Report);

  void DispatchResult(ReportList::iterator it, bool success);

  identity::IdentityManager* identity_manager_;
  std::string account_id_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  ReportList reports_;

  DISALLOW_COPY_AND_ASSIGN(SafeSearchURLReporter);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SAFE_SEARCH_URL_REPORTER_H_
