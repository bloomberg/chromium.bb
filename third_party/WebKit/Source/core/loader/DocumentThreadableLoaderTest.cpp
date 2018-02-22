// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/DocumentThreadableLoader.h"

#include "platform/exported/WrappedResourceResponse.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "public/platform/WebCORS.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(DocumentThreadableLoaderCreatePreflightRequestTest, LexicographicalOrder) {
  WebURLRequest request;
  request.AddHTTPHeaderField("Orange", "Orange");
  request.AddHTTPHeaderField("Apple", "Red");
  request.AddHTTPHeaderField("Kiwifruit", "Green");
  request.AddHTTPHeaderField("Content-Type", "application/octet-stream");
  request.AddHTTPHeaderField("Strawberry", "Red");

  WebURLRequest preflight =
      DocumentThreadableLoader::CreateAccessControlPreflightRequestForTesting(
          request);

  EXPECT_EQ("apple,content-type,kiwifruit,orange,strawberry",
            preflight.HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(DocumentThreadableLoaderCreatePreflightRequestTest, ExcludeSimpleHeaders) {
  WebURLRequest request;
  request.AddHTTPHeaderField("Accept", "everything");
  request.AddHTTPHeaderField("Accept-Language", "everything");
  request.AddHTTPHeaderField("Content-Language", "everything");
  request.AddHTTPHeaderField("Save-Data", "on");

  WebURLRequest preflight =
      DocumentThreadableLoader::CreateAccessControlPreflightRequestForTesting(
          request);

  // Do not emit empty-valued headers; an empty list of non-"CORS safelisted"
  // request headers should cause "Access-Control-Request-Headers:" to be
  // left out in the preflight request.
  EXPECT_EQ(WebString(g_null_atom),
            preflight.HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(DocumentThreadableLoaderCreatePreflightRequestTest,
     ExcludeSimpleContentTypeHeader) {
  WebURLRequest request;
  request.AddHTTPHeaderField("Content-Type", "text/plain");

  WebURLRequest preflight =
      DocumentThreadableLoader::CreateAccessControlPreflightRequestForTesting(
          request);

  // Empty list also; see comment in test above.
  EXPECT_EQ(WebString(g_null_atom),
            preflight.HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(DocumentThreadableLoaderCreatePreflightRequestTest,
     IncludeNonSimpleHeader) {
  WebURLRequest request;
  request.AddHTTPHeaderField("X-Custom-Header", "foobar");

  WebURLRequest preflight =
      DocumentThreadableLoader::CreateAccessControlPreflightRequestForTesting(
          request);

  EXPECT_EQ("x-custom-header",
            preflight.HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(DocumentThreadableLoaderCreatePreflightRequestTest,
     IncludeNonSimpleContentTypeHeader) {
  WebURLRequest request;
  request.AddHTTPHeaderField("Content-Type", "application/octet-stream");

  WebURLRequest preflight =
      DocumentThreadableLoader::CreateAccessControlPreflightRequestForTesting(
          request);

  EXPECT_EQ("content-type",
            preflight.HttpHeaderField("Access-Control-Request-Headers"));
}

}  // namespace

}  // namespace blink
