// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerResponse_h
#define WebServiceWorkerResponse_h

#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"

namespace blink {

// Represents a response to a fetch operation. ServiceWorker uses this to
// respond to a FetchEvent dispatched by the browser. The plan is for the Cache
// and fetch() API to also use it.
class BLINK_PLATFORM_EXPORT WebServiceWorkerResponse {
public:
    void setStatusCode(unsigned short);
    unsigned short statusCode() const;

    void setStatusText(const WebString&);
    WebString statusText() const;

    void setMethod(const WebString&);
    WebString method() const;

    // FIXME: Eventually this should have additional methods such as for headers and blob.

private:
    unsigned short m_statusCode;
    WebString m_statusText;
    WebString m_method;
};

} // namespace blink

#endif // WebServiceWorkerResponse_h
