// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchResponseData.h"

#include "core/typed_arrays/DOMArrayBuffer.h"
#include "modules/fetch/FetchHeaderList.h"
#include "platform/blob/BlobData.h"
#include "platform/wtf/Vector.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class FetchResponseDataTest : public ::testing::Test {
 public:
  FetchResponseData* CreateInternalResponse() {
    FetchResponseData* internal_response = FetchResponseData::Create();
    internal_response->SetStatus(200);
    Vector<KURL> url_list;
    url_list.push_back(KURL(kParsedURLString, "http://www.example.com"));
    internal_response->SetURLList(url_list);
    internal_response->HeaderList()->Append("set-cookie", "foo");
    internal_response->HeaderList()->Append("bar", "bar");
    internal_response->HeaderList()->Append("cache-control", "no-cache");
    return internal_response;
  }

  void CheckHeaders(const WebServiceWorkerResponse& web_response) {
    EXPECT_STREQ("foo", web_response.GetHeader("set-cookie").Utf8().c_str());
    EXPECT_STREQ("bar", web_response.GetHeader("bar").Utf8().c_str());
    EXPECT_STREQ("no-cache",
                 web_response.GetHeader("cache-control").Utf8().c_str());
  }
};

TEST_F(FetchResponseDataTest, HeaderList) {
  FetchResponseData* response_data = CreateInternalResponse();

  Vector<String> set_cookie_values;
  response_data->HeaderList()->GetAll("set-cookie", set_cookie_values);
  ASSERT_EQ(1U, set_cookie_values.size());
  EXPECT_EQ("foo", set_cookie_values[0]);

  Vector<String> bar_values;
  response_data->HeaderList()->GetAll("bar", bar_values);
  ASSERT_EQ(1U, bar_values.size());
  EXPECT_EQ("bar", bar_values[0]);

  Vector<String> cache_control_values;
  response_data->HeaderList()->GetAll("cache-control", cache_control_values);
  ASSERT_EQ(1U, cache_control_values.size());
  EXPECT_EQ("no-cache", cache_control_values[0]);
}

TEST_F(FetchResponseDataTest, ToWebServiceWorkerDefaultType) {
  WebServiceWorkerResponse web_response;
  FetchResponseData* internal_response = CreateInternalResponse();

  internal_response->PopulateWebServiceWorkerResponse(web_response);
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            web_response.ResponseType());
  CheckHeaders(web_response);
}

TEST_F(FetchResponseDataTest, BasicFilter) {
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* basic_response_data =
      internal_response->CreateBasicFilteredResponse();

  EXPECT_EQ(internal_response, basic_response_data->InternalResponse());

  EXPECT_FALSE(basic_response_data->HeaderList()->Has("set-cookie"));

  Vector<String> bar_values;
  basic_response_data->HeaderList()->GetAll("bar", bar_values);
  ASSERT_EQ(1U, bar_values.size());
  EXPECT_EQ("bar", bar_values[0]);

  Vector<String> cache_control_values;
  basic_response_data->HeaderList()->GetAll("cache-control",
                                            cache_control_values);
  ASSERT_EQ(1U, cache_control_values.size());
  EXPECT_EQ("no-cache", cache_control_values[0]);
}

TEST_F(FetchResponseDataTest, ToWebServiceWorkerBasicType) {
  WebServiceWorkerResponse web_response;
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* basic_response_data =
      internal_response->CreateBasicFilteredResponse();

  basic_response_data->PopulateWebServiceWorkerResponse(web_response);
  EXPECT_EQ(network::mojom::FetchResponseType::kBasic,
            web_response.ResponseType());
  CheckHeaders(web_response);
}

TEST_F(FetchResponseDataTest, CORSFilter) {
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* cors_response_data =
      internal_response->CreateCORSFilteredResponse();

  EXPECT_EQ(internal_response, cors_response_data->InternalResponse());

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("set-cookie"));

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("bar"));

  Vector<String> cache_control_values;
  cors_response_data->HeaderList()->GetAll("cache-control",
                                           cache_control_values);
  ASSERT_EQ(1U, cache_control_values.size());
  EXPECT_EQ("no-cache", cache_control_values[0]);
}

TEST_F(FetchResponseDataTest,
       CORSFilterOnResponseWithAccessControlExposeHeaders) {
  FetchResponseData* internal_response = CreateInternalResponse();
  internal_response->HeaderList()->Append("access-control-expose-headers",
                                          "set-cookie, bar");

  FetchResponseData* cors_response_data =
      internal_response->CreateCORSFilteredResponse();

  EXPECT_EQ(internal_response, cors_response_data->InternalResponse());

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("set-cookie"));

  Vector<String> bar_values;
  cors_response_data->HeaderList()->GetAll("bar", bar_values);
  ASSERT_EQ(1U, bar_values.size());
  EXPECT_EQ("bar", bar_values[0]);
}

TEST_F(FetchResponseDataTest, CORSFilterWithEmptyHeaderSet) {
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* cors_response_data =
      internal_response->CreateCORSFilteredResponse(WebHTTPHeaderSet());

  EXPECT_EQ(internal_response, cors_response_data->InternalResponse());

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("set-cookie"));

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("bar"));

  Vector<String> cache_control_values;
  cors_response_data->HeaderList()->GetAll("cache-control",
                                           cache_control_values);
  ASSERT_EQ(1U, cache_control_values.size());
  EXPECT_EQ("no-cache", cache_control_values[0]);
}

TEST_F(FetchResponseDataTest,
       CORSFilterWithEmptyHeaderSetOnResponseWithAccessControlExposeHeaders) {
  FetchResponseData* internal_response = CreateInternalResponse();
  internal_response->HeaderList()->Append("access-control-expose-headers",
                                          "set-cookie, bar");

  FetchResponseData* cors_response_data =
      internal_response->CreateCORSFilteredResponse(WebHTTPHeaderSet());

  EXPECT_EQ(internal_response, cors_response_data->InternalResponse());

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("set-cookie"));

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("bar"));

  Vector<String> cache_control_values;
  cors_response_data->HeaderList()->GetAll("cache-control",
                                           cache_control_values);
  ASSERT_EQ(1U, cache_control_values.size());
  EXPECT_EQ("no-cache", cache_control_values[0]);
}

TEST_F(FetchResponseDataTest, CORSFilterWithExplicitHeaderSet) {
  FetchResponseData* internal_response = CreateInternalResponse();
  WebHTTPHeaderSet exposed_headers;
  exposed_headers.insert("set-cookie");
  exposed_headers.insert("bar");

  FetchResponseData* cors_response_data =
      internal_response->CreateCORSFilteredResponse(exposed_headers);

  EXPECT_EQ(internal_response, cors_response_data->InternalResponse());

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("set-cookie"));

  Vector<String> bar_values;
  cors_response_data->HeaderList()->GetAll("bar", bar_values);
  ASSERT_EQ(1U, bar_values.size());
  EXPECT_EQ("bar", bar_values[0]);
}

TEST_F(FetchResponseDataTest, ToWebServiceWorkerCORSType) {
  WebServiceWorkerResponse web_response;
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* cors_response_data =
      internal_response->CreateCORSFilteredResponse();

  cors_response_data->PopulateWebServiceWorkerResponse(web_response);
  EXPECT_EQ(network::mojom::FetchResponseType::kCORS,
            web_response.ResponseType());
  CheckHeaders(web_response);
}

TEST_F(FetchResponseDataTest, OpaqueFilter) {
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* opaque_response_data =
      internal_response->CreateOpaqueFilteredResponse();

  EXPECT_EQ(internal_response, opaque_response_data->InternalResponse());

  EXPECT_FALSE(opaque_response_data->HeaderList()->Has("set-cookie"));
  EXPECT_FALSE(opaque_response_data->HeaderList()->Has("bar"));
  EXPECT_FALSE(opaque_response_data->HeaderList()->Has("cache-control"));
}

TEST_F(FetchResponseDataTest, OpaqueRedirectFilter) {
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* opaque_response_data =
      internal_response->CreateOpaqueRedirectFilteredResponse();

  EXPECT_EQ(internal_response, opaque_response_data->InternalResponse());

  EXPECT_EQ(opaque_response_data->HeaderList()->size(), 0u);
  EXPECT_EQ(*opaque_response_data->Url(), *internal_response->Url());
}

TEST_F(FetchResponseDataTest,
       OpaqueFilterOnResponseWithAccessControlExposeHeaders) {
  FetchResponseData* internal_response = CreateInternalResponse();
  internal_response->HeaderList()->Append("access-control-expose-headers",
                                          "set-cookie, bar");

  FetchResponseData* opaque_response_data =
      internal_response->CreateOpaqueFilteredResponse();

  EXPECT_EQ(internal_response, opaque_response_data->InternalResponse());

  EXPECT_FALSE(opaque_response_data->HeaderList()->Has("set-cookie"));
  EXPECT_FALSE(opaque_response_data->HeaderList()->Has("bar"));
  EXPECT_FALSE(opaque_response_data->HeaderList()->Has("cache-control"));
}

TEST_F(FetchResponseDataTest, ToWebServiceWorkerOpaqueType) {
  WebServiceWorkerResponse web_response;
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* opaque_response_data =
      internal_response->CreateOpaqueFilteredResponse();

  opaque_response_data->PopulateWebServiceWorkerResponse(web_response);
  EXPECT_EQ(network::mojom::FetchResponseType::kOpaque,
            web_response.ResponseType());
  CheckHeaders(web_response);
}

TEST_F(FetchResponseDataTest, ToWebServiceWorkerOpaqueRedirectType) {
  WebServiceWorkerResponse web_response;
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* opaque_redirect_response_data =
      internal_response->CreateOpaqueRedirectFilteredResponse();

  opaque_redirect_response_data->PopulateWebServiceWorkerResponse(web_response);
  EXPECT_EQ(network::mojom::FetchResponseType::kOpaqueRedirect,
            web_response.ResponseType());
  CheckHeaders(web_response);
}

}  // namespace blink
