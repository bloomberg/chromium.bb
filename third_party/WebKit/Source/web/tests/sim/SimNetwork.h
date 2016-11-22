// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimNetwork_h
#define SimNetwork_h

#include "public/platform/WebURLLoaderTestDelegate.h"
#include "wtf/HashMap.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

class SimRequest;
class WebURLLoader;
class WebURLLoaderClient;
class WebURLResponse;

// Simulates a network with precise flow control so you can make requests
// return, write data, and finish in a specific order in a unit test. One of
// these must be created before using the SimRequest to issue requests.
class SimNetwork final : public WebURLLoaderTestDelegate {
 public:
  SimNetwork();
  ~SimNetwork();

 private:
  friend class SimRequest;

  static SimNetwork& current();

  void servePendingRequests();
  void addRequest(SimRequest&);
  void removeRequest(SimRequest&);

  // WebURLLoaderTestDelegate
  void didReceiveResponse(WebURLLoaderClient*,
                          WebURLLoader*,
                          const WebURLResponse&) override;
  void didReceiveData(WebURLLoaderClient*,
                      WebURLLoader*,
                      const char* data,
                      int dataLength,
                      int encodedDataLength) override;
  void didFail(WebURLLoaderClient*,
               WebURLLoader*,
               const WebURLError&,
               int64_t totalEncodedDataLength) override;
  void didFinishLoading(WebURLLoaderClient*,
                        WebURLLoader*,
                        double finishTime,
                        int64_t totalEncodedDataLength) override;

  SimRequest* m_currentRequest;
  HashMap<String, SimRequest*> m_requests;
};

}  // namespace blink

#endif
