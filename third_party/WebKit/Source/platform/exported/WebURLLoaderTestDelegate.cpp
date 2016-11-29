// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebURLLoaderTestDelegate.h"

#include "public/platform/WebURLError.h"
#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLLoaderClient.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

WebURLLoaderTestDelegate::WebURLLoaderTestDelegate() {}

WebURLLoaderTestDelegate::~WebURLLoaderTestDelegate() {}

void WebURLLoaderTestDelegate::didReceiveResponse(
    WebURLLoaderClient* originalClient,
    const WebURLResponse& response) {
  originalClient->didReceiveResponse(response);
}

void WebURLLoaderTestDelegate::didReceiveData(
    WebURLLoaderClient* originalClient,
    const char* data,
    int dataLength,
    int encodedDataLength) {
  originalClient->didReceiveData(data, dataLength, encodedDataLength);
}

void WebURLLoaderTestDelegate::didFail(WebURLLoaderClient* originalClient,
                                       const WebURLError& error,
                                       int64_t totalEncodedDataLength,
                                       int64_t totalEncodedBodyLength) {
  originalClient->didFail(error, totalEncodedDataLength,
                          totalEncodedBodyLength);
}

void WebURLLoaderTestDelegate::didFinishLoading(
    WebURLLoaderClient* originalClient,
    double finishTime,
    int64_t totalEncodedDataLength,
    int64_t totalEncodedBodyLength) {
  originalClient->didFinishLoading(finishTime, totalEncodedDataLength,
                                   totalEncodedBodyLength);
}

}  // namespace blink
