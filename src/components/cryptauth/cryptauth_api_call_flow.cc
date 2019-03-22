// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/cryptauth_api_call_flow.h"

#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace cryptauth {

namespace {

const char kPost[] = "POST";

NetworkRequestError GetErrorForHttpResponseCode(int response_code) {
  if (response_code == 400)
    return NetworkRequestError::kBadRequest;

  if (response_code == 403)
    return NetworkRequestError::kAuthenticationError;

  if (response_code == 404)
    return NetworkRequestError::kEndpointNotFound;

  if (response_code >= 500 && response_code < 600)
    return NetworkRequestError::kInternalServerError;

  return NetworkRequestError::kUnknown;
}

}  // namespace

CryptAuthApiCallFlow::CryptAuthApiCallFlow() {
}

CryptAuthApiCallFlow::~CryptAuthApiCallFlow() {
}

void CryptAuthApiCallFlow::Start(
    const GURL& request_url,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token,
    const std::string& serialized_request,
    const ResultCallback& result_callback,
    const ErrorCallback& error_callback) {
  request_url_ = request_url;
  serialized_request_ = serialized_request;
  result_callback_ = result_callback;
  error_callback_ = error_callback;
  OAuth2ApiCallFlow::Start(std::move(url_loader_factory), access_token);
}

GURL CryptAuthApiCallFlow::CreateApiCallUrl() {
  return request_url_;
}

std::string CryptAuthApiCallFlow::CreateApiCallBody() {
  return serialized_request_;
}

std::string CryptAuthApiCallFlow::CreateApiCallBodyContentType() {
  return "application/x-protobuf";
}

std::string CryptAuthApiCallFlow::GetRequestTypeForBody(
    const std::string& body) {
  return kPost;
}

void CryptAuthApiCallFlow::ProcessApiCallSuccess(
    const network::ResourceResponseHead* head,
    std::unique_ptr<std::string> body) {
  if (!body) {
    error_callback_.Run(NetworkRequestError::kResponseMalformed);
    return;
  }
  result_callback_.Run(std::move(*body));
}

void CryptAuthApiCallFlow::ProcessApiCallFailure(
    int net_error,
    const network::ResourceResponseHead* head,
    std::unique_ptr<std::string> body) {
  base::Optional<NetworkRequestError> error;
  std::string error_message;
  if (net_error == net::OK) {
    int response_code = -1;
    if (head && head->headers)
      response_code = head->headers->response_code();
    error = GetErrorForHttpResponseCode(response_code);
  } else {
    error = NetworkRequestError::kOffline;
  }

  PA_LOG(ERROR) << "API call failed, error code: "
                << (error ? *error : NetworkRequestError::kUnknown);
  if (body)
    PA_LOG(VERBOSE) << "API failure response body:\n" << *body;

  error_callback_.Run(*error);
}

net::PartialNetworkTrafficAnnotationTag
CryptAuthApiCallFlow::GetNetworkTrafficAnnotationTag() {
  DCHECK(partial_network_annotation_ != nullptr);
  return *partial_network_annotation_.get();
}

}  // namespace cryptauth
