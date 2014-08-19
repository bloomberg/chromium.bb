// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Request_h
#define Request_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/serviceworkers/FetchRequestData.h"
#include "modules/serviceworkers/Headers.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class RequestInit;
struct ResourceLoaderOptions;
class ResourceRequest;
struct ThreadableLoaderOptions;
class WebServiceWorkerRequest;

class Request FINAL : public RefCountedWillBeGarbageCollected<Request>, public ScriptWrappable {
    DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(Request);
public:
    static PassRefPtrWillBeRawPtr<Request> create(ExecutionContext*, const String&, ExceptionState&);
    static PassRefPtrWillBeRawPtr<Request> create(ExecutionContext*, const String&, const Dictionary&, ExceptionState&);
    static PassRefPtrWillBeRawPtr<Request> create(ExecutionContext*, Request*, ExceptionState&);
    static PassRefPtrWillBeRawPtr<Request> create(ExecutionContext*, Request*, const Dictionary&, ExceptionState&);

    static PassRefPtrWillBeRawPtr<Request> create(PassRefPtrWillBeRawPtr<FetchRequestData>);

    static PassRefPtrWillBeRawPtr<Request> create(const WebServiceWorkerRequest&);

    PassRefPtrWillBeRawPtr<FetchRequestData> request() { return m_request; }

    String method() const;
    String url() const;
    PassRefPtrWillBeRawPtr<Headers> headers() const { return m_headers; }
    // FIXME: Support body.
    String referrer() const;
    String mode() const;
    String credentials() const;

    void trace(Visitor*);

private:
    explicit Request(PassRefPtrWillBeRawPtr<FetchRequestData>);
    explicit Request(const WebServiceWorkerRequest&);

    RefPtrWillBeMember<FetchRequestData> m_request;
    RefPtrWillBeMember<Headers> m_headers;
};

} // namespace blink

#endif // Request_h
