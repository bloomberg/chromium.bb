// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "bindings/core/v8/ExceptionState.h"
#include "modules/serviceworkers/Response.h"
#include "modules/serviceworkers/FetchResponseData.h"
#include "public/platform/WebServiceWorkerResponse.h"
#include <gtest/gtest.h>

namespace blink {
namespace {

TEST(ServiceWorkerResponseTest, FromFetchResponseData)
{
    const KURL url(ParsedURLString, "http://www.response.com");

    RefPtrWillBeRawPtr<FetchResponseData> fetchResponseData = FetchResponseData::create();
    fetchResponseData->setURL(url);

    RefPtrWillBeRawPtr<Response> response = Response::create(fetchResponseData);
    ASSERT(response);
    EXPECT_EQ(url, response->url());
}

TEST(ServiceWorkerResponseTest, FromWebServiceWorkerResponse)
{
    const KURL url(ParsedURLString, "http://www.webresponse.com/");
    const unsigned short status = 200;
    const String statusText = "the best status text";
    struct { const char* key; const char* value; } headers[] = { {"X-Foo", "bar"}, {"X-Quux", "foop"}, {0, 0} };

    WebServiceWorkerResponse webResponse;
    webResponse.setURL(url);
    webResponse.setStatus(status);
    webResponse.setStatusText(statusText);
    for (int i = 0; headers[i].key; ++i)
        webResponse.setHeader(WebString::fromUTF8(headers[i].key), WebString::fromUTF8(headers[i].value));

    RefPtrWillBeRawPtr<Response> response = Response::create(webResponse);
    ASSERT(response);
    EXPECT_EQ(url, response->url());
    EXPECT_EQ(status, response->status());
    EXPECT_EQ(statusText, response->statusText());

    RefPtrWillBeRawPtr<Headers> responseHeaders = response->headers();

    WTF::HashMap<String, String> headersMap;
    for (int i = 0; headers[i].key; ++i)
        headersMap.add(headers[i].key, headers[i].value);
    EXPECT_EQ(headersMap.size(), responseHeaders->size());
    for (WTF::HashMap<String, String>::iterator iter = headersMap.begin(); iter != headersMap.end(); ++iter) {
        TrackExceptionState exceptionState;
        EXPECT_EQ(iter->value, responseHeaders->get(iter->key, exceptionState));
        EXPECT_FALSE(exceptionState.hadException());
    }
}

} // namespace
} // namespace blink
