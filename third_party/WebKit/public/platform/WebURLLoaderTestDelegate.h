// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebURLLoaderTestDelegate_h
#define WebURLLoaderTestDelegate_h

#include "public/platform/WebCommon.h"

namespace blink {

class WebURLResponse;
class WebURLLoaderClient;
struct WebURLError;

// Use with WebURLLoaderMockFactory::SetLoaderDelegate to intercept calls to a
// WebURLLoaderClient for controlling network responses in a test. Default
// implementations of all methods just call the original method on the
// WebURLLoaderClient.
class BLINK_PLATFORM_EXPORT WebURLLoaderTestDelegate {
 public:
  WebURLLoaderTestDelegate();
  virtual ~WebURLLoaderTestDelegate();

  virtual void DidReceiveResponse(WebURLLoaderClient* original_client,
                                  const WebURLResponse&);
  virtual void DidReceiveData(WebURLLoaderClient* original_client,
                              const char* data,
                              int data_length);
  virtual void DidFail(WebURLLoaderClient* original_client,
                       const WebURLError&,
                       int64_t total_encoded_data_length,
                       int64_t total_encoded_body_length,
                       int64_t total_decoded_body_length);
  virtual void DidFinishLoading(WebURLLoaderClient* original_client,
                                double finish_time,
                                int64_t total_encoded_data_length,
                                int64_t total_encoded_body_length,
                                int64_t total_decoded_body_length);
};

}  // namespace blink

#endif
