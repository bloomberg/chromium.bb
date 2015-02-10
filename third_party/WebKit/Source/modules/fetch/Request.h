// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Request_h
#define Request_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/modules/v8/UnionTypesModules.h"
#include "modules/fetch/Body.h"
#include "modules/fetch/FetchRequestData.h"
#include "modules/fetch/Headers.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class BodyStreamBuffer;
class RequestInit;
class WebServiceWorkerRequest;

typedef RequestOrUSVString RequestInfo;

class Request final : public Body {
    DEFINE_WRAPPERTYPEINFO();
public:
    ~Request() override { }

    // From Request.idl:
    static Request* create(ExecutionContext*, const RequestInfo&, const Dictionary&, ExceptionState&);

    static Request* create(ExecutionContext*, const String&, ExceptionState&);
    static Request* create(ExecutionContext*, const String&, const Dictionary&, ExceptionState&);
    static Request* create(ExecutionContext*, Request*, ExceptionState&);
    static Request* create(ExecutionContext*, Request*, const Dictionary&, ExceptionState&);
    static Request* create(ExecutionContext*, FetchRequestData*);
    static Request* create(ExecutionContext*, const WebServiceWorkerRequest&);

    const FetchRequestData* request() { return m_request; }

    // From Request.idl:
    String method() const;
    KURL url() const;
    Headers* headers() const { return m_headers; }
    String referrer() const;
    String mode() const;
    String credentials() const;

    // From Request.idl:
    Request* clone(ExceptionState&) const;

    void populateWebServiceWorkerRequest(WebServiceWorkerRequest&) const;

    void setBodyBlobHandle(PassRefPtr<BlobDataHandle>);
    bool hasBody() const { return m_request->blobDataHandle(); }

    DECLARE_VIRTUAL_TRACE();

private:
    // The 'FetchRequestData' object is shared between requests, as it is
    // immutable to the user after Request creation. Headers are copied.
    explicit Request(const Request&);

    Request(ExecutionContext*, FetchRequestData*);
    Request(ExecutionContext*, const WebServiceWorkerRequest&);

    static Request* createRequestWithRequestOrString(ExecutionContext*, Request*, const String&, const RequestInit&, ExceptionState&);
    void clearHeaderList();

    PassRefPtr<BlobDataHandle> blobDataHandle() const override;
    BodyStreamBuffer* buffer() const override;
    String contentTypeForBuffer() const override;

    const Member<FetchRequestData> m_request;
    const Member<Headers> m_headers;
};

} // namespace blink

#endif // Request_h
