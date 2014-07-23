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
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink { class WebServiceWorkerResponse; }

namespace blink {

class Blob;
class ExceptionState;
class ResponseInit;

class Response FINAL : public RefCountedWillBeGarbageCollectedFinalized<Response>, public ScriptWrappable {
    DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(Response);
public:
    static PassRefPtrWillBeRawPtr<Response> create(Blob*, const Dictionary&, ExceptionState&);
    static PassRefPtrWillBeRawPtr<Response> create(const String&, const Dictionary&, ExceptionState&);
    static PassRefPtrWillBeRawPtr<Response> create(Blob*, const ResponseInit&, ExceptionState&);

    static PassRefPtrWillBeRawPtr<Response> create(PassRefPtrWillBeRawPtr<FetchResponseData>);

    String type() const;
    String url() const;
    unsigned short status() const;
    String statusText() const;
    PassRefPtrWillBeRawPtr<Headers> headers() const;

    PassRefPtrWillBeRawPtr<FetchBodyStream> body(ExecutionContext*);

    void populateWebServiceWorkerResponse(blink::WebServiceWorkerResponse&);

    void trace(Visitor*);

private:
    Response();
    explicit Response(PassRefPtrWillBeRawPtr<FetchResponseData>);

    RefPtrWillBeMember<FetchResponseData> m_response;
    RefPtrWillBeMember<Headers> m_headers;
    RefPtrWillBeMember<FetchBodyStream> m_fetchBodyStream;
};

} // namespace blink

#endif // Response_h
