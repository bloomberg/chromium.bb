// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher_delegate.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"

class GURL;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace password_manager {

class TestAffiliationFetcherFactory;

// Fetches authoritative information regarding which facets are affiliated with
// each other, that is, which facets belong to the same logical application.
// See affiliation_utils.h for a definition of what this means.
//
// An instance is good for exactly one fetch, and may be used from any thread
// that runs a message loop (i.e. not a worker pool thread).
class AffiliationFetcher {
 public:
  ~AffiliationFetcher();

  // Constructs a fetcher to retrieve affiliations for each facet in |facet_ids|
  // using the specified |url_loader_factory|, and will provide the results
  // to the |delegate| on the same thread that creates the instance.
  static AffiliationFetcher* Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::vector<FacetURI>& facet_uris,
      AffiliationFetcherDelegate* delegate);

  // Builds the URL for the Affiliation API's lookup method.
  static GURL BuildQueryURL();

  // Sets the |factory| to be used by Create() to construct AffiliationFetcher
  // instances. To be used only for testing.
  //
  // The caller must ensure that the |factory| outlives all potential Create()
  // calls. The caller may pass in NULL to resume using the default factory.
  static void SetFactoryForTesting(TestAffiliationFetcherFactory* factory);

  // Actually starts the request, and will call the delegate with the results on
  // the same thread when done. If |this| is destroyed before completion, the
  // in-flight request is cancelled, and the delegate will not be called.
  // Further details:
  //   * No cookies are sent/saved with the request.
  //   * In case of network/server errors, the request will not be retried.
  //   * Results are guaranteed to be always fresh and will never be cached.
  void StartRequest();

  const std::vector<FacetURI>& requested_facet_uris() const {
    return requested_facet_uris_;
  }

  AffiliationFetcherDelegate* delegate() const { return delegate_; }

 protected:
  AffiliationFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::vector<FacetURI>& facet_uris,
      AffiliationFetcherDelegate* delegate);

 private:
  // Prepares and returns the serialized protocol buffer message that will be
  // the payload of the POST request.
  std::string PreparePayload() const;

  // Parses and validates the response protocol buffer message for a list of
  // equivalence classes, stores them into |result| and returns true on success.
  // It is guaranteed that every one of the requested Facet URIs will be a
  // member of exactly one returned equivalence class.
  // Returns false if the response was gravely ill-formed or self-inconsistent.
  // Unknown kinds of facet URIs and new protocol buffer fields will be ignored.
  bool ParseResponse(const std::string& serialized_response,
                     AffiliationFetcherDelegate::Result* result) const;

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::vector<FacetURI> requested_facet_uris_;
  AffiliationFetcherDelegate* const delegate_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  DISALLOW_COPY_AND_ASSIGN(AffiliationFetcher);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_FETCHER_H_
