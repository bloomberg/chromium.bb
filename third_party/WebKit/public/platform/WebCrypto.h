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

#ifndef WebCrypto_h
#define WebCrypto_h

#include "WebCommon.h"
#include "WebCryptoKey.h"
#include "WebPrivatePtr.h"

namespace WebKit {

class WebArrayBuffer;
class WebCryptoKeyOperation;
class WebCryptoKeyOperationResult;
class WebCryptoOperation;
class WebCryptoOperationResult;

class WebCrypto {
public:
    // FIXME: Deprecated, delete once chromium side is updated.
    virtual WebCryptoOperation* digest(const WebCryptoAlgorithm&) { WEBKIT_ASSERT_NOT_REACHED(); return 0; }

    // The following methods begin an asynchronous multi-part cryptographic
    // operation.
    //
    // Let the WebCryptoOperationResult& be called "result".
    //
    // Before returning, implementations can either:
    //
    // * Synchronously fail initialization by calling:
    //   result.initializationFailed(errorDetails)
    //
    // OR
    //
    // * Create a new WebCryptoOperation* and return it by calling:
    //   result.initializationSucceeded(cryptoOperation)
    //
    // If initialization succeeds, then Blink will subsequently call
    // methods on cryptoOperation:
    //
    //   - cryptoOperation->process() to feed it data
    //   - cryptoOperation->finish() to indicate there is no more data
    //   - cryptoOperation->abort() to cancel.
    //
    // The embedder may call result.completeWithXXX() at any time while the
    // cryptoOperation is "in progress" (can copy the initial "result" object).
    // Typically completion is set once cryptoOperation->finish() is called,
    // however it can also be called during cryptoOperation->process() (for
    // instance to set an error).
    //
    // The cryptoOperation pointer MUST remain valid while it is "in progress".
    // The cryptoOperation is said to be "in progress" from the time after
    // result->initializationSucceded() has been called, up until either:
    //
    //   - Blink calls cryptoOperation->abort()
    //   OR
    //   - Embedder calls result.completeWithXXX()
    //
    // Once the cryptoOperation is no longer "in progress" the embedder is
    // responsible for freeing the cryptoOperation.
    virtual void encrypt(const WebCryptoAlgorithm&, const WebCryptoKey&, WebCryptoOperationResult&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void decrypt(const WebCryptoAlgorithm&, const WebCryptoKey&, WebCryptoOperationResult&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void sign(const WebCryptoAlgorithm&, const WebCryptoKey&, WebCryptoOperationResult&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void verifySignature(const WebCryptoAlgorithm&, const WebCryptoKey&, const unsigned char* signature, size_t, WebCryptoOperationResult&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void digest(const WebCryptoAlgorithm&, WebCryptoOperationResult&) { WEBKIT_ASSERT_NOT_REACHED(); }

    // The following methods begin an asynchronous single-part key operation.
    //
    // Let the WebCryptoKeyOperationResult& be called "result".
    //
    // Before returning, implementations can either:
    //
    // (a) Synchronously fail initialization by calling:
    //     result.initializationFailed(errorDetails)
    //     (this results in throwing a Javascript exception)
    //
    // (b) Synchronously complete the request (success or error) by calling:
    //     result.completeWithXXX()
    //
    // (c) Defer completion by calling
    //     result.initializationSucceeded(keyOperation)
    //
    // If asynchronous completion (c) was chosen, then the embedder can notify
    // completion after returning by calling result.completeWithXXX() (can make
    // a copy of "result").
    //
    // The keyOperation pointer MUST remain valid while it is "in progress".
    // The keyOperation is said to be "in progress" from the time after
    // result->initializationSucceded() has been called, up until either:
    //
    //   - Blink calls keyOperation->abort()
    //   OR
    //   - Embedder calls result.completeWithXXX()
    //
    // Once the keyOperation is no longer "in progress" the embedder is
    // responsible for freeing the cryptoOperation.
    virtual void generateKey(const WebCryptoAlgorithm&, bool extractable, WebCryptoKeyUsageMask, WebCryptoKeyOperationResult&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void importKey(WebCryptoKeyFormat, const unsigned char* keyData, size_t keyDataSize, const WebCryptoAlgorithm&, bool extractable, WebCryptoKeyUsageMask, WebCryptoKeyOperationResult&) { WEBKIT_ASSERT_NOT_REACHED(); }

protected:
    virtual ~WebCrypto() { }
};

class WebCryptoOperation {
public:
    // Feeds data (bytes, size) to the operation.
    //   - |bytes| may be 0 if |size| is 0
    //   - |bytes| is valid only until process() returns
    //   - process() will not be called after abort() or finish()
    virtual void process(const unsigned char*, size_t) = 0;

    // Cancels the in-progress operation.
    //   * Implementations should delete |this| after aborting.
    virtual void abort() = 0;

    // Indicates that there is no more data to receive.
    virtual void finish() = 0;

protected:
    virtual ~WebCryptoOperation() { }
};

// Can be implemented by embedder for unit-testing.
class WebCryptoOperationResultPrivate {
public:
    virtual ~WebCryptoOperationResultPrivate() { }

    virtual void initializationFailed() = 0;
    virtual void initializationSucceeded(WebCryptoOperation*) = 0;
    virtual void completeWithError() = 0;
    virtual void completeWithArrayBuffer(const WebArrayBuffer&) = 0;
    virtual void completeWithBoolean(bool) = 0;

    virtual void ref() = 0;
    virtual void deref() = 0;
};

// FIXME: Add error types.
class WebCryptoOperationResult {
public:
    explicit WebCryptoOperationResult(WebCryptoOperationResultPrivate* impl)
    {
        assign(impl);
    }

    ~WebCryptoOperationResult() { reset(); }

    WebCryptoOperationResult(const WebCryptoOperationResult& o)
    {
        assign(o);
    }

    WebCryptoOperationResult& operator=(const WebCryptoOperationResult& o)
    {
        assign(o);
        return *this;
    }

    WEBKIT_EXPORT void initializationFailed();
    WEBKIT_EXPORT void initializationSucceeded(WebCryptoOperation*);
    WEBKIT_EXPORT void completeWithError();
    WEBKIT_EXPORT void completeWithArrayBuffer(const WebArrayBuffer&);
    WEBKIT_EXPORT void completeWithBoolean(bool);

private:
    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT void assign(const WebCryptoOperationResult&);
    WEBKIT_EXPORT void assign(WebCryptoOperationResultPrivate*);

    WebPrivatePtr<WebCryptoOperationResultPrivate> m_impl;
};

class WebCryptoKeyOperation {
public:
    // Cancels the in-progress operation.
    //   * Implementations should delete |this| after aborting.
    virtual void abort() = 0;

protected:
    virtual ~WebCryptoKeyOperation() { }
};

// Can be implemented by embedder for unit-testing.
class WebCryptoKeyOperationResultPrivate {
public:
    virtual ~WebCryptoKeyOperationResultPrivate() { }

    virtual void initializationFailed() = 0;
    virtual void initializationSucceeded(WebCryptoKeyOperation*) = 0;
    virtual void completeWithError() = 0;
    virtual void completeWithKey(const WebCryptoKey&) = 0;

    virtual void ref() = 0;
    virtual void deref() = 0;
};

class WebCryptoKeyOperationResult {
public:
    explicit WebCryptoKeyOperationResult(WebCryptoKeyOperationResultPrivate* impl)
    {
        assign(impl);
    }

    ~WebCryptoKeyOperationResult() { reset(); }

    WebCryptoKeyOperationResult(const WebCryptoKeyOperationResult& o)
    {
        assign(o);
    }

    WebCryptoKeyOperationResult& operator=(const WebCryptoKeyOperationResult& o)
    {
        assign(o);
        return *this;
    }

    WEBKIT_EXPORT void initializationFailed();
    WEBKIT_EXPORT void initializationSucceeded(WebCryptoKeyOperation*);

    WEBKIT_EXPORT void completeWithError();
    WEBKIT_EXPORT void completeWithKey(const WebCryptoKey&);

private:
    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT void assign(const WebCryptoKeyOperationResult&);
    WEBKIT_EXPORT void assign(WebCryptoKeyOperationResultPrivate*);

    WebPrivatePtr<WebCryptoKeyOperationResultPrivate> m_impl;
};

} // namespace WebKit

#endif
