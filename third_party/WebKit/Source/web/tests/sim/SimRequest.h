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

class SimRequest {
public:
    SimRequest(String url, String mimeType);

    void didReceiveResponse(WebURLLoaderClient*, WebURLLoader*, const WebURLResponse&);
    void didFail(const WebURLError&);

    void start();
    void write(const String& data);
    void finish();

    bool isReady() const { return m_isReady; }
    const String& url() const { return m_url; }
    const WebURLError& error() const { return m_error; }
    const WebURLResponse& response() const { return m_response; }

    void reset();

private:
    String m_url;
    WebURLLoader* m_loader;
    WebURLResponse m_response;
    WebURLError m_error;
    WebURLLoaderClient* m_client;
    unsigned m_totalEncodedDataLength;
    bool m_isReady;
};

} // namespace blink

#endif
