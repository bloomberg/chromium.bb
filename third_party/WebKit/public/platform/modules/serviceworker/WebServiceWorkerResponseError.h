// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerResponseError_h
#define WebServiceWorkerResponseError_h

namespace blink {

// This enum is used in UMA histograms, so don't change the order or remove
// entries.
enum WebServiceWorkerResponseError {
  kWebServiceWorkerResponseErrorUnknown = 0,
  kWebServiceWorkerResponseErrorPromiseRejected = 1,
  kWebServiceWorkerResponseErrorDefaultPrevented = 2,
  kWebServiceWorkerResponseErrorNoV8Instance = 3,
  kWebServiceWorkerResponseErrorResponseTypeError = 4,
  kWebServiceWorkerResponseErrorResponseTypeOpaque = 5,
  kWebServiceWorkerResponseErrorResponseTypeNotBasicOrDefault = 6,
  kWebServiceWorkerResponseErrorBodyUsed = 7,
  kWebServiceWorkerResponseErrorResponseTypeOpaqueForClientRequest = 8,
  kWebServiceWorkerResponseErrorResponseTypeOpaqueRedirect = 9,
  kWebServiceWorkerResponseErrorBodyLocked = 10,
  kWebServiceWorkerResponseErrorNoForeignFetchResponse = 11,
  kWebServiceWorkerResponseErrorForeignFetchHeadersWithoutOrigin = 12,
  kWebServiceWorkerResponseErrorForeignFetchMismatchedOrigin = 13,
  kWebServiceWorkerResponseErrorRedirectedResponseForNotFollowRequest = 14,
  kWebServiceWorkerResponseErrorDataPipeCreationFailed = 15,
  // Add a new type here, and update the following line and enums.xml.
  kWebServiceWorkerResponseErrorLast =
      kWebServiceWorkerResponseErrorDataPipeCreationFailed,
};

}  // namespace blink

#endif  // WebServiceWorkerResponseError_h
