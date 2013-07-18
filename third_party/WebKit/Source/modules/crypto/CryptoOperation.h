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
#include "wtf/RefCounted.h"

namespace WebCore {

class ScriptPromiseResolver;

typedef int ExceptionCode;

class CryptoOperation : public ScriptWrappable, public RefCounted<CryptoOperation> {
public:
    ~CryptoOperation();
    static PassRefPtr<CryptoOperation> create(const WebKit::WebCryptoAlgorithm&, WebKit::WebCryptoOperation*);

    CryptoOperation* process(ArrayBuffer* data);
    CryptoOperation* process(ArrayBufferView* data);

    ScriptObject finish();
    ScriptObject abort();

    Algorithm* algorithm();

private:
    class Result;
    friend class Result;

    enum State {
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

    CryptoOperation(const WebKit::WebCryptoAlgorithm&, WebKit::WebCryptoOperation*);

    void process(const unsigned char*, size_t);

    // Aborts and clears m_impl. If the operation has already comleted then
    // returns false.
    bool abortImpl();

    ScriptPromiseResolver* promiseResolver();

    WebKit::WebCryptoAlgorithm m_algorithm;
    WebKit::WebCryptoOperation* m_impl;
    RefPtr<Algorithm> m_algorithmNode;
    State m_state;

    RefPtr<ScriptPromiseResolver> m_promiseResolver;

    OwnPtr<Result> m_result;
};

} // namespace WebCore

#endif
