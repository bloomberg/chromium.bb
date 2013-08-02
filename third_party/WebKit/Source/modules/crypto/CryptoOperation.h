/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CryptoOperation_h
#define CryptoOperation_h

#include "bindings/v8/ScriptObject.h"
#include "bindings/v8/ScriptWrappable.h"
#include "modules/crypto/Algorithm.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"

namespace WebCore {

class ScriptPromiseResolver;
class ExceptionState;

typedef int ExceptionCode;

// CryptoOperation vs CryptoOperationImpl:
//
// A CryptoOperation corresponds with the request from JavaScript to start a
// multi-part cryptographic operation. This is forwarded to the Platform layer,
// which creates a WebCryptoOperation. When the WebCryptoOperation eventually
// completes, it resolves a Promise.
//
// To avoid a reference cycle between WebCryptoOperation and CryptoOperation,
// WebCryptoOperation's result handle holds a reference to
// CryptoOperationImpl rather than CryptoOperation. This prevents extending the
// lifetime of CryptoOperation beyond JavaScript garbage collection, which is
// important since:
//
//   * When JavaScript garbage collects CryptoOperation and finish() has NOT
//     been called on it, the Platform operation can no longer complete and
//     should therefore be aborted.
//   * The WebCryptoOperation may outlive CryptoOperation if finish() was
//     called, as the result is delivered to a separate Promise (this is
//     different than what the current version of the spec says).

class CryptoOperationImpl : public WebKit::WebCryptoOperationResultPrivate, public ThreadSafeRefCounted<CryptoOperationImpl> {
public:
    static PassRefPtr<CryptoOperationImpl> create() { return adoptRef(new CryptoOperationImpl); }

    bool throwInitializationError(ExceptionState&);

    // The CryptoOperation which started the request is getting destroyed.
    void detach();

    void process(const void* bytes, size_t);

    ScriptObject finish();
    ScriptObject abort();

    // WebCryptoOperationResultPrivate implementation.
    virtual void initializationFailed() OVERRIDE;
    virtual void initializationSucceeded(WebKit::WebCryptoOperation*) OVERRIDE;
    virtual void completeWithError() OVERRIDE;
    virtual void completeWithArrayBuffer(const WebKit::WebArrayBuffer&) OVERRIDE;
    virtual void completeWithBoolean(bool) OVERRIDE;
    virtual void ref() OVERRIDE;
    virtual void deref() OVERRIDE;

private:
    enum State {
        // Constructing the WebCryptoOperationImpl.
        Initializing,

        // Accepting calls to process().
        Processing,

        // finish() has been called, but the Promise has not been resolved yet.
        Finishing,

        // The operation either:
        //  - completed successfully
        //  - failed
        //  - was aborted
        Done,
    };

    CryptoOperationImpl();

    ScriptPromiseResolver* promiseResolver();

    State m_state;

    WebKit::WebCryptoOperation* m_impl;
    RefPtr<ScriptPromiseResolver> m_promiseResolver;
    ExceptionCode m_initializationError;
};

class CryptoOperation : public ScriptWrappable, public RefCounted<CryptoOperation> {
public:
    ~CryptoOperation();
    static PassRefPtr<CryptoOperation> create(const WebKit::WebCryptoAlgorithm&, CryptoOperationImpl*);

    CryptoOperation* process(ArrayBuffer* data);
    CryptoOperation* process(ArrayBufferView* data);

    ScriptObject finish();
    ScriptObject abort();

    Algorithm* algorithm();

    CryptoOperationImpl* impl() { return m_impl.get(); }

private:
    explicit CryptoOperation(const WebKit::WebCryptoAlgorithm&, CryptoOperationImpl*);

    WebKit::WebCryptoAlgorithm m_algorithm;
    RefPtr<Algorithm> m_algorithmNode;

    RefPtr<CryptoOperationImpl> m_impl;
};

} // namespace WebCore

#endif
