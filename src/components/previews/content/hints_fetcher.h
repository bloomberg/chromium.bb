// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_HINTS_FETCHER_H_
#define COMPONENTS_PREVIEWS_CONTENT_HINTS_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/previews/core/previews_experiments.h"
#include "url/gurl.h"

namespace previews {

class HintCache;

// A class to handle OnePlatform client requests for optimization hints from
// the remote Optimization Guide Service.
//
// When Chrome starts up, if the client's OnePlatform hints have not been
// updated in over day, this class will be triggered to fetch new hints from the
// remote Optimization Guide Service. Owner must ensure that |hint_cache|
// remains alive for the lifetime of |HintsFetcher|.
class HintsFetcher {
 public:
  explicit HintsFetcher(HintCache* hint_cache);
  ~HintsFetcher();

  // Handles any configuration or checking needed and then
  // initiates the fetch of OnePlatform Hints.
  void FetchHintsForHosts(const std::vector<std::string>& hosts);

 private:
  // Creates the GetHintsResponse proto and executes the SimpleURLLoader request
  // to the remote Optimization Guide Service with the |get_hints_request_|.
  void GetRemoteHints(const std::vector<std::string>& hosts);

  // Handles the response from the remote Optimization Guide Service.
  // |response| is the response body, |status| is the
  // |net::Error| of the response, and response_code is the HTTP
  // response code (if available).
  void HandleResponse(const std::string& response,
                      int status,
                      int response_code);

  // URL loader completion callback.
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // Parses the hints component of |get_hints_response| and applies it to
  // |hints_|. Returns true if |get_hints_response| was successfully
  // parsed and applied.
  bool ParseGetHintsResponseAndApplyHints(
      const optimization_guide::proto::GetHintsResponse& get_hints_response);

  // Used to hold the GetHintsRequest being constructed and sent as a remote
  // request.
  std::unique_ptr<optimization_guide::proto::GetHintsRequest>
      get_hints_request_;

  // The URL for the remote Optimization Guide Service.
  const GURL oneplatform_service_url_;

  // The caller must ensure that the |hints_| outlives this instance.
  HintCache* hint_cache_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(HintsFetcher);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_HINTS_FETCHER_H_
