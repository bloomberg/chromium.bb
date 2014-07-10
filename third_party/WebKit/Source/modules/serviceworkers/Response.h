// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Response_h
#define Response_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/serviceworkers/FetchResponseData.h"
#include "modules/serviceworkers/Headers.h"
#include "platform/blob/BlobData.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink { class WebServiceWorkerResponse; }

namespace WebCore {

class Blob;
class ExceptionState;
struct ResponseInit;

class Response FINAL : public RefCounted<Response>, public ScriptWrappable {
public:
    static PassRefPtr<Response> create(Blob*, const Dictionary&, ExceptionState&);
    static PassRefPtr<Response> create(Blob*, const ResponseInit&, ExceptionState&);

    static PassRefPtr<Response> create(PassRefPtr<FetchResponseData>);
    ~Response() { };

    String type() const;
    String url() const;
    unsigned short status() const;
    String statusText() const;
    PassRefPtr<Headers> headers() const;

    void populateWebServiceWorkerResponse(blink::WebServiceWorkerResponse&);

private:
    Response();
    Response(PassRefPtr<FetchResponseData>);

    RefPtr<FetchResponseData> m_response;
    RefPtr<Headers> m_headers;
    // FIXME: Support FetchBodyStream.
};

} // namespace WebCore

#endif // Response_h
