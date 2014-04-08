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

#ifndef CryptoResultImpl_h
#define CryptoResultImpl_h

#include "bindings/v8/NewScriptState.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "platform/CryptoResult.h"
#include "public/platform/WebCrypto.h"
#include "wtf/Assertions.h"
#include "wtf/Forward.h"
#include "wtf/Threading.h"

namespace WebCore {

class ScriptPromiseResolver;
class ScriptState;

// Wrapper around a Promise to notify completion of the crypto operation.
// Platform cannot know about Promises which are declared in bindings.
class CryptoResultImpl FINAL : public CryptoResult, public ContextLifecycleObserver {
public:
    ~CryptoResultImpl();

    static PassRefPtr<CryptoResultImpl> create();

    virtual void completeWithError() OVERRIDE;
    virtual void completeWithError(const blink::WebString&) OVERRIDE;
    virtual void completeWithBuffer(const blink::WebArrayBuffer&) OVERRIDE;
    virtual void completeWithBoolean(bool) OVERRIDE;
    virtual void completeWithKey(const blink::WebCryptoKey&) OVERRIDE;
    virtual void completeWithKeyPair(const blink::WebCryptoKey& publicKey, const blink::WebCryptoKey& privateKey) OVERRIDE;

    ScriptPromise promise() { return m_promiseResolver->promise(); }

private:
    CryptoResultImpl(ExecutionContext*);
    void finish();
    void CheckValidThread() const;

    // Override from ContextLifecycleObserver
    virtual void contextDestroyed() OVERRIDE;

    // Returns true if the ExecutionContext is still alive and running.
    bool canCompletePromise() const;

    void clearPromiseResolver();

    RefPtr<ScriptPromiseResolver> m_promiseResolver;
    RefPtr<NewScriptState> m_scriptState;

#if !ASSERT_DISABLED
    ThreadIdentifier m_owningThread;
    bool m_finished;
#endif
};

} // namespace WebCore

#endif
