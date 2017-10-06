// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimRequest_h
#define SimRequest_h

#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLResponse.h"

namespace blink {

class SimNetwork;
class WebURLLoaderClient;

// Simulates a single request for a resource from the server. Requires a
// SimNetwork to have been created first. Use the start(), write() and finish()
// methods to simulate the response from the server. Note that all started
// requests must be finished.
class SimRequest final {
 public:
  SimRequest(String url, String mime_type);
  ~SimRequest();

  // Starts the response from the server, this is as if the headers and 200 OK
  // reply had been received but no response body yet.
  void Start();

  // Write a chunk of the response body.
  void Write(const String& data);
  void Write(const Vector<char>& data);

  // Finish the response, this is as if the server closed the connection.
  void Finish();

  // Shorthand to complete a request (start/write/finish) sequence in order.
  void Complete(const String& data = String());
  void Complete(const Vector<char>& data);

  const String& Url() const { return url_; }
  const WebURLError& GetError() const { return error_; }
  const WebURLResponse& GetResponse() const { return response_; }

 private:
  friend class SimNetwork;

  void Reset();

  // Used by SimNetwork.
  void DidReceiveResponse(WebURLLoaderClient*, const WebURLResponse&);
  void DidFail(const WebURLError&);

  String url_;
  WebURLResponse response_;
  WebURLError error_;
  WebURLLoaderClient* client_;
  unsigned total_encoded_data_length_;
  bool is_ready_;
};

}  // namespace blink

#endif
