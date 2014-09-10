// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Response_h
#define Response_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/serviceworkers/FetchBodyStream.h"
#include "modules/serviceworkers/FetchResponseData.h"
#include "modules/serviceworkers/Headers.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Blob;
class ExceptionState;
class ResponseInit;
class WebServiceWorkerResponse;

class Response FINAL : public GarbageCollected<Response>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static Response* create(Blob*, const Dictionary&, ExceptionState&);
    static Response* create(const String&, const Dictionary&, ExceptionState&);
    static Response* create(const ArrayBuffer*, const Dictionary&, ExceptionState&);
    static Response* create(Blob*, const ResponseInit&, ExceptionState&);
    static Response* create(FetchResponseData*);
    static Response* create(const WebServiceWorkerResponse&);

    String type() const;
    String url() const;
    unsigned short status() const;
    String statusText() const;
    Headers* headers() const;

    FetchBodyStream* body(ExecutionContext*);

    void populateWebServiceWorkerResponse(WebServiceWorkerResponse&);

    void trace(Visitor*);

private:
    Response();
    explicit Response(FetchResponseData*);
    explicit Response(const WebServiceWorkerResponse&);

    Member<FetchResponseData> m_response;
    Member<Headers> m_headers;
    Member<FetchBodyStream> m_fetchBodyStream;
};

} // namespace blink

#endif // Response_h
