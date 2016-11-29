// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimRequest_h
#define SimRequest_h

#include "public/platform/WebURLError.h"
#include "public/platform/WebURLResponse.h"
#include "wtf/text/WTFString.h"

namespace blink {

class SimNetwork;
class WebURLLoader;
class WebURLLoaderClient;

// Simulates a single request for a resource from the server. Requires a
// SimNetwork to have been created first. Use the start(), write() and finish()
// methods to simulate the response from the server. Note that all started
// requests must be finished.
class SimRequest final {
 public:
  SimRequest(String url, String mimeType);
  ~SimRequest();

  // Starts the response from the server, this is as if the headers and 200 OK
  // reply had been received but no response body yet.
  void start();

  // Write a chunk of the response body.
  void write(const String& data);

  // Finish the response, this is as if the server closed the connection.
  void finish();

  // Shorthand to complete a request (start/write/finish) sequence in order.
  void complete(const String& data = String());

  const String& url() const { return m_url; }
  const WebURLError& error() const { return m_error; }
  const WebURLResponse& response() const { return m_response; }

 private:
  friend class SimNetwork;

  void reset();

  // Used by SimNetwork.
  void didReceiveResponse(WebURLLoaderClient*,
                          const WebURLResponse&);
  void didFail(const WebURLError&);

  String m_url;
  WebURLLoader* m_loader;
  WebURLResponse m_response;
  WebURLError m_error;
  WebURLLoaderClient* m_client;
  unsigned m_totalEncodedDataLength;
  bool m_isReady;
};

}  // namespace blink

#endif
