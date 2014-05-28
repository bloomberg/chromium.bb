// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerResponse_h
#define WebServiceWorkerResponse_h

#include "WebCommon.h"
#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"

#if INSIDE_BLINK
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/text/StringHash.h"
#endif

// FIXME: Remove this after chromium code is cleaned up.
#define NEW_SERVICE_WORKER_RESPONSE_INTERFACE

namespace blink {

class WebServiceWorkerResponsePrivate;

// Represents a response to a fetch operation. ServiceWorker uses this to
// respond to a FetchEvent dispatched by the browser. The plan is for the Cache
// and fetch() API to also use it.
class BLINK_PLATFORM_EXPORT WebServiceWorkerResponse {
public:
    ~WebServiceWorkerResponse() { reset(); }
    WebServiceWorkerResponse();
    WebServiceWorkerResponse& operator=(const WebServiceWorkerResponse& other)
    {
        assign(other);
        return *this;
    }

    void reset();
    void assign(const WebServiceWorkerResponse&);

    void setStatus(unsigned short);
    unsigned short status() const;

    void setStatusText(const WebString&);
    WebString statusText() const;

    void setHeader(const WebString& key, const WebString& value);
    WebVector<WebString> getHeaderKeys() const;
    WebString getHeader(const WebString& key) const;

#if INSIDE_BLINK
    void setHeaders(const HashMap<String, String>&);
    const HashMap<String, String>& headers() const;
#endif

    // FIXME: Eventually this should have additional methods such as for blob.

private:
    WebPrivatePtr<WebServiceWorkerResponsePrivate> m_private;
};

} // namespace blink

#endif // WebServiceWorkerResponse_h
