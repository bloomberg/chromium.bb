// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Response_h
#define Response_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/serviceworkers/Body.h"
#include "modules/serviceworkers/FetchResponseData.h"
#include "modules/serviceworkers/Headers.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"

namespace blink {

class Blob;
class ExceptionState;
class ResponseInit;
class WebServiceWorkerResponse;

class Response final : public Body {
    DEFINE_WRAPPERTYPEINFO();
public:
    virtual ~Response() { }
    static Response* create(ExecutionContext*, Blob*, const Dictionary&, ExceptionState&);
    static Response* create(ExecutionContext*, const String&, const Dictionary&, ExceptionState&);
    static Response* create(ExecutionContext*, const ArrayBuffer*, const Dictionary&, ExceptionState&);
    static Response* create(ExecutionContext*, const ArrayBufferView*, const Dictionary&, ExceptionState&);
    static Response* create(ExecutionContext*, Blob*, const ResponseInit&, ExceptionState&);
    static Response* create(ExecutionContext*, FetchResponseData*);
    static Response* create(ExecutionContext*, const WebServiceWorkerResponse&);
    // The 'FetchResponseData' object is shared between responses, as it is
    // immutable to the user after Response creation. Headers are copied.
    static Response* create(const Response&);

    const FetchResponseData* response() const { return m_response; }

    String type() const;
    String url() const;
    unsigned short status() const;
    String statusText() const;
    Headers* headers() const;

    Response* clone() const;

    void populateWebServiceWorkerResponse(WebServiceWorkerResponse&);

    virtual void trace(Visitor*) override;

private:
    explicit Response(const Response&);
    explicit Response(ExecutionContext*);
    Response(ExecutionContext*, FetchResponseData*);

    virtual PassRefPtr<BlobDataHandle> blobDataHandle() override;

    const Member<FetchResponseData> m_response;
    const Member<Headers> m_headers;
};

} // namespace blink

#endif // Response_h
