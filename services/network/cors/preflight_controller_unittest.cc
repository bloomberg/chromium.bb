// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/preflight_controller.h"

#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/cors/cors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network {

namespace cors {

namespace {

TEST(PreflightControllerCreatePreflightRequestTest, LexicographicalOrder) {
  ResourceRequest request;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("Orange", "Orange");
  request.headers.SetHeader("Apple", "Red");
  request.headers.SetHeader("Kiwifruit", "Green");
  request.headers.SetHeader("Content-Type", "application/octet-stream");
  request.headers.SetHeader("Strawberry", "Red");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequest(request);

  std::string header;
  EXPECT_TRUE(
      preflight->headers.GetHeader(net::HttpRequestHeaders::kOrigin, &header));
  EXPECT_EQ("null", header);

  EXPECT_TRUE(preflight->headers.GetHeader(
      cors::header_names::kAccessControlRequestHeaders, &header));
  EXPECT_EQ("apple,content-type,kiwifruit,orange,strawberry", header);
}

TEST(PreflightControllerCreatePreflightRequestTest, ExcludeSimpleHeaders) {
  ResourceRequest request;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("Accept", "everything");
  request.headers.SetHeader("Accept-Language", "everything");
  request.headers.SetHeader("Content-Language", "everything");
  request.headers.SetHeader("Save-Data", "on");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequest(request);

  // Do not emit empty-valued headers; an empty list of non-"CORS safelisted"
  // request headers should cause "Access-Control-Request-Headers:" to be
  // left out in the preflight request.
  std::string header;
  EXPECT_FALSE(preflight->headers.GetHeader(
      cors::header_names::kAccessControlRequestHeaders, &header));
}

TEST(PreflightControllerCreatePreflightRequestTest,
     ExcludeSimpleContentTypeHeader) {
  ResourceRequest request;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("Content-Type", "text/plain");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequest(request);

  // Empty list also; see comment in test above.
  std::string header;
  EXPECT_FALSE(preflight->headers.GetHeader(
      cors::header_names::kAccessControlRequestHeaders, &header));
}

TEST(PreflightControllerCreatePreflightRequestTest, IncludeNonSimpleHeader) {
  ResourceRequest request;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("X-Custom-Header", "foobar");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequest(request);

  std::string header;
  EXPECT_TRUE(preflight->headers.GetHeader(
      cors::header_names::kAccessControlRequestHeaders, &header));
  EXPECT_EQ("x-custom-header", header);
}

TEST(PreflightControllerCreatePreflightRequestTest,
     IncludeNonSimpleContentTypeHeader) {
  ResourceRequest request;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("Content-Type", "application/octet-stream");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequest(request);

  std::string header;
  EXPECT_TRUE(preflight->headers.GetHeader(
      cors::header_names::kAccessControlRequestHeaders, &header));
  EXPECT_EQ("content-type", header);
}

TEST(PreflightControllerCreatePreflightRequestTest, ExcludeForbiddenHeaders) {
  ResourceRequest request;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("referer", "https://www.google.com/");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequest(request);

  std::string header;
  EXPECT_FALSE(preflight->headers.GetHeader(
      cors::header_names::kAccessControlRequestHeaders, &header));
}

}  // namespace

}  // namespace cors

}  // namespace network
