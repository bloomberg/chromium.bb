// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CLIENT_NEARBY_SHARE_API_CALL_FLOW_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_CLIENT_NEARBY_SHARE_API_CALL_FLOW_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_api_call_flow.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// NearbyShareApiCallFlowImpl is a wrapper around OAuth2ApiCallFlow
// that provides convenience methods StartGetRequest, StartPostRequest,
// and StartPatchRequest.
// We assume the following:
//   * A POST or PATCH request's body is the serialized request proto,
//   * A GET request encodes the request proto as query parameters and has no
//     body,
//   * The response body is the serialized response proto.
class NearbyShareApiCallFlowImpl : public NearbyShareApiCallFlow,
                                   public OAuth2ApiCallFlow {
 public:
  NearbyShareApiCallFlowImpl();
  ~NearbyShareApiCallFlowImpl() override;

  NearbyShareApiCallFlowImpl(const NearbyShareApiCallFlowImpl&) = delete;
  NearbyShareApiCallFlowImpl& operator=(const NearbyShareApiCallFlowImpl&) =
      delete;

  // NearbyShareApiCallFlow
  void StartPostRequest(
      const GURL& request_url,
      const std::string& serialized_request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) override;
  void StartPatchRequest(
      const GURL& request_url,
      const std::string& serialized_request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) override;
  void StartGetRequest(
      const GURL& request_url,
      const QueryParameters& request_as_query_parameters,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) override;
  void SetPartialNetworkTrafficAnnotation(
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      override;

 protected:
  // Reduce the visibility of OAuth2ApiCallFlow::Start() to avoid exposing
  // overloaded methods.
  using OAuth2ApiCallFlow::Start;

  // google_apis::OAuth2ApiCallFlow:
  GURL CreateApiCallUrl() override;
  net::HttpRequestHeaders CreateApiCallHeaders() override;
  std::string CreateApiCallBody() override;
  std::string CreateApiCallBodyContentType() override;
  std::string GetRequestTypeForBody(const std::string& body) override;
  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  void ProcessApiCallFailure(int net_error,
                             const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  net::PartialNetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTag()
      override;

 private:
  // The URL of the endpoint serving the request.
  GURL request_url_;

  // The HTTP method to use.
  std::string request_http_method_;

  // Serialized request message proto that will be sent in the request body.
  // Null if request is GET.
  absl::optional<std::string> serialized_request_;

  // The request message proto represented as key-value pairs that will be sent
  // as query parameters in the API GET request. Note: A key can have multiple
  // values. Null if HTTP method is not GET.
  absl::optional<QueryParameters> request_as_query_parameters_;

  // Callback invoked with the serialized response message proto when the flow
  // completes successfully.
  ResultCallback result_callback_;

  // Callback invoked with an error message when the flow fails.
  ErrorCallback error_callback_;

  std::unique_ptr<net::PartialNetworkTrafficAnnotationTag>
      partial_network_annotation_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CLIENT_NEARBY_SHARE_API_CALL_FLOW_IMPL_H_
