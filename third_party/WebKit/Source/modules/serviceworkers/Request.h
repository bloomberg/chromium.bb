// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Request_h
#define Request_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/serviceworkers/FetchRequestData.h"
#include "modules/serviceworkers/Headers.h"
#include "platform/weborigin/KURL.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink { class WebServiceWorkerRequest; }

namespace WebCore {

struct RequestInit;
class ResourceRequest;
struct ResourceLoaderOptions;
struct ThreadableLoaderOptions;

class Request FINAL : public RefCounted<Request>, public ScriptWrappable {
public:
    static PassRefPtr<Request> create(ExecutionContext*, const String&, ExceptionState&);
    static PassRefPtr<Request> create(ExecutionContext*, const String&, const Dictionary&, ExceptionState&);
    static PassRefPtr<Request> create(ExecutionContext*, Request*, ExceptionState&);
    static PassRefPtr<Request> create(ExecutionContext*, Request*, const Dictionary&, ExceptionState&);

    static PassRefPtr<Request> create(PassRefPtr<FetchRequestData>);

    static PassRefPtr<Request> create(const blink::WebServiceWorkerRequest&);

    ~Request() { };

    PassRefPtr<FetchRequestData> request() { return m_request; }

    String method() const;
    String url() const;
    PassRefPtr<Headers> headers() const { return m_headers; }
    // FIXME: Support body.
    String referrer() const;
    String mode() const;
    String credentials() const;

    PassOwnPtr<ResourceRequest> createResourceRequest() const;

private:
    explicit Request(PassRefPtr<FetchRequestData>);
    explicit Request(const blink::WebServiceWorkerRequest&);

    RefPtr<FetchRequestData> m_request;
    RefPtr<Headers> m_headers;
};

} // namespace WebCore

#endif // Request_h
