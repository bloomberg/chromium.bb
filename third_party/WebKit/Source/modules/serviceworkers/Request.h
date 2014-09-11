// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Request_h
#define Request_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/serviceworkers/Body.h"
#include "modules/serviceworkers/FetchRequestData.h"
#include "modules/serviceworkers/Headers.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class RequestInit;
struct ResourceLoaderOptions;
class ResourceRequest;
struct ThreadableLoaderOptions;
class WebServiceWorkerRequest;

class Request FINAL : public Body {
    DEFINE_WRAPPERTYPEINFO();
public:
    virtual ~Request() { }
    static Request* create(ExecutionContext*, const String&, ExceptionState&);
    static Request* create(ExecutionContext*, const String&, const Dictionary&, ExceptionState&);
    static Request* create(ExecutionContext*, Request*, ExceptionState&);
    static Request* create(ExecutionContext*, Request*, const Dictionary&, ExceptionState&);
    static Request* create(ExecutionContext*, FetchRequestData*);
    static Request* create(ExecutionContext*, const WebServiceWorkerRequest&);

    FetchRequestData* request() { return m_request; }

    String method() const;
    String url() const;
    Headers* headers() const { return m_headers; }
    String referrer() const;
    String mode() const;
    String credentials() const;

    void populateWebServiceWorkerRequest(WebServiceWorkerRequest&);

    void setBodyBlobHandle(PassRefPtr<BlobDataHandle>);

    virtual void trace(Visitor*)  OVERRIDE;

private:
    Request(ExecutionContext*, FetchRequestData*);
    Request(ExecutionContext*, const WebServiceWorkerRequest&);

    virtual PassRefPtr<BlobDataHandle> blobDataHandle() OVERRIDE;

    Member<FetchRequestData> m_request;
    Member<Headers> m_headers;
};

} // namespace blink

#endif // Request_h
