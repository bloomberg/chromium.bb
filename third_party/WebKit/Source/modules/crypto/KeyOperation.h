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

#ifndef KeyOperation_h
#define KeyOperation_h

#include "bindings/v8/ScriptObject.h"
#include "public/platform/WebCrypto.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"

namespace WebKit {
class WebCryptoKeyOperation;
class WebCryptoKey;
}

namespace WebCore {

class ExceptionState;
class ScriptPromiseResolver;

typedef int ExceptionCode;

class KeyOperation : public WebKit::WebCryptoKeyOperationResultPrivate, public ThreadSafeRefCounted<KeyOperation> {
public:
    static PassRefPtr<KeyOperation> create();

    ~KeyOperation();

    // Implementation of WebCryptoKeyOperationResultPrivate.
    virtual void initializationFailed() OVERRIDE;
    virtual void initializationSucceeded(WebKit::WebCryptoKeyOperation*) OVERRIDE;
    virtual void completeWithError() OVERRIDE;
    virtual void completeWithKey(const WebKit::WebCryptoKey&) OVERRIDE;
    virtual void ref() OVERRIDE;
    virtual void deref() OVERRIDE;

    ScriptObject returnValue(ExceptionState&);

private:
    enum State {
        Initializing,
        InProgress,
        Done,
    };

    KeyOperation();

    ScriptPromiseResolver* promiseResolver();

    State m_state;
    WebKit::WebCryptoKeyOperation* m_impl;
    ExceptionCode m_initializationError;
    RefPtr<ScriptPromiseResolver> m_promiseResolver;
};

} // namespace WebCore

#endif
