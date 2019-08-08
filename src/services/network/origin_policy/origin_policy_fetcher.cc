// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/network/origin_policy/origin_policy_fetcher.h"

#include <utility>

#include "base/strings/strcat.h"
#include "net/base/load_flags.h"
#include "net/http/http_util.h"
#include "services/network/origin_policy/origin_policy_constants.h"
#include "services/network/origin_policy/origin_policy_manager.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {

OriginPolicyFetcher::OriginPolicyFetcher(
    OriginPolicyManager* owner_policy_manager,
    const std::string& policy_version,
    const std::string& report_to,
    const url::Origin& origin,
    mojom::URLLoaderFactory* factory,
    mojom::OriginPolicyManager::RetrieveOriginPolicyCallback callback)
    : owner_policy_manager_(owner_policy_manager),
      report_to_(report_to),
      fetch_url_(GetPolicyURL(policy_version, origin)),
      callback_(std::move(callback)),
      must_redirect_(false) {
  DCHECK(callback_);
  DCHECK(!policy_version.empty());
  FetchPolicy(factory);
}

OriginPolicyFetcher::OriginPolicyFetcher(
    OriginPolicyManager* owner_policy_manager,
    const std::string& report_to,
    const url::Origin& origin,
    mojom::URLLoaderFactory* factory,
    mojom::OriginPolicyManager::RetrieveOriginPolicyCallback callback)
    : owner_policy_manager_(owner_policy_manager),
      report_to_(report_to),
      fetch_url_(GetDefaultPolicyURL(origin)),
      callback_(std::move(callback)),
      must_redirect_(true) {
  DCHECK(callback_);
  FetchPolicy(factory);
}

OriginPolicyFetcher::~OriginPolicyFetcher() {}

// static
GURL OriginPolicyFetcher::GetPolicyURL(const std::string& version,
                                       const url::Origin& origin) {
  // TODO(vogelheim): update this check when the origin policy spec is updated
  // to clearly specify the proper sanitization of a version.
  if (!net::HttpUtil::IsToken(version))
    return GURL();

  return GURL(
      base::StrCat({origin.Serialize(), kOriginPolicyWellKnown, "/", version}));
}

// static
GURL OriginPolicyFetcher::GetDefaultPolicyURL(const url::Origin& origin) {
  return GURL(base::StrCat({origin.Serialize(), kOriginPolicyWellKnown}));
}

bool OriginPolicyFetcher::IsValidRedirectForTesting(
    const net::RedirectInfo& redirect_info) const {
  return IsValidRedirect(redirect_info);
}

void OriginPolicyFetcher::OnPolicyHasArrived(
    std::unique_ptr<std::string> policy_content) {
  // Fail hard if the policy could not be loaded.
  if (!policy_content || must_redirect_) {
    Report(mojom::OriginPolicyState::kCannotLoadPolicy);
    WorkDone(nullptr, mojom::OriginPolicyState::kCannotLoadPolicy);
  } else {
    WorkDone(std::move(policy_content), mojom::OriginPolicyState::kLoaded);
  }
}

void OriginPolicyFetcher::OnPolicyRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers) {
  if (IsValidRedirect(redirect_info)) {
    must_redirect_ = false;
    // TODO(andypaicu): should we callback with the original url or the new url?
    fetch_url_ = redirect_info.new_url;
    return;
  }

  // Fail hard if the policy response follows an invalid redirect.
  Report(mojom::OriginPolicyState::kInvalidRedirect);

  WorkDone(nullptr, mojom::OriginPolicyState::kInvalidRedirect);
}

void OriginPolicyFetcher::FetchPolicy(mojom::URLLoaderFactory* factory) {
  // Create the traffic annotation
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("origin_policy_loader", R"(
        semantics {
          sender: "Origin Policy URL Loader Throttle"
          description:
            "Fetches the Origin Policy with a given version from an origin."
          trigger:
            "In case the Origin Policy with a given version does not "
            "exist in the cache, it is fetched from the origin at a "
            "well-known location."
          data:
            "None, the URL itself contains the origin and Origin Policy "
            "version."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings. Server "
            "opt-in or out of this mechanism."
          policy_exception_justification:
            "Not implemented, considered not useful."})");

  FetchCallback done = base::BindOnce(&OriginPolicyFetcher::OnPolicyHasArrived,
                                      base::Unretained(this));
  RedirectCallback redirect = base::BindRepeating(
      &OriginPolicyFetcher::OnPolicyRedirect, base::Unretained(this));

  // Create and configure the SimpleURLLoader for the policy.
  std::unique_ptr<ResourceRequest> policy_request =
      std::make_unique<ResourceRequest>();
  policy_request->url = fetch_url_;
  policy_request->request_initiator = url::Origin::Create(fetch_url_);
  policy_request->allow_credentials = false;

  url_loader_ =
      SimpleURLLoader::Create(std::move(policy_request), traffic_annotation);
  url_loader_->SetOnRedirectCallback(std::move(redirect));

  // Start the download, and pass the callback for when we're finished.
  url_loader_->DownloadToString(factory, std::move(done),
                                kOriginPolicyMaxPolicySize);
}

void OriginPolicyFetcher::WorkDone(std::unique_ptr<std::string> policy_content,
                                   mojom::OriginPolicyState state) {
  if (callback_) {
    auto result = mojom::OriginPolicy::New();
    result->state = state;
    if (policy_content) {
      result->contents = mojom::OriginPolicyContents::New();
      result->contents->raw_policy = *policy_content;
    }
    result->policy_url = fetch_url_;

    std::move(callback_).Run(std::move(result));
  }

  // Do not add code after this call as it will destroy this object.
  owner_policy_manager_->FetcherDone(this);
}

bool OriginPolicyFetcher::IsValidRedirect(
    const net::RedirectInfo& redirect_info) const {
  if (!must_redirect_)
    return false;
  if (!redirect_info.new_url.is_valid())
    return false;

  // If the url is correctly built, the filename portion of the URL is the
  // policy version.
  std::string new_version = redirect_info.new_url.ExtractFileName();
  if (new_version.empty())
    return false;

  // The specified redirect url must be correctly built according to origin
  // policy url rules.
  return redirect_info.new_url ==
         GetPolicyURL(new_version, url::Origin::Create(fetch_url_));
}

// TODO(andypaicu): use report_to implement this function
void OriginPolicyFetcher::Report(mojom::OriginPolicyState error_state) {}

}  // namespace network
