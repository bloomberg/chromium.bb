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

namespace WebKit {

class WebArrayBuffer;
class WebCryptoAlgorithm;
class WebCryptoKey;
class WebCryptoOperation;
class WebCryptoOperationResult;

class WebCrypto {
public:
    // FIXME: Deprecated, delete once chromium side is updated.
    virtual WebCryptoOperation* digest(const WebCryptoAlgorithm&) = 0;

    // The following methods begin an asynchronous multi-part cryptographic
    // operation.
    //
    // Let the WebCryptoOperationResult* be called "result".
    //
    // Implementations can either:
    //
    // * Synchronously fail initialization by calling:
    //   result->initializationFailed(errorDetails)
    //
    // OR
    //
    // * Create a new WebCryptoOperation* and return it by calling:
    //   result->initializationSucceeded(cryptoOperation)
    //
    // If initialization succeeds, then Blink will subsequently call
    // methods on cryptoOperation:
    //
    //   - cryptoOperation->process() to feed it data
    //   - cryptoOperation->finish() to indicate there is no more data
    //   - cryptoOperation->abort() to cancel.
    //
    // The embedder may call result->completeWithXXX() at any time while the
    // cryptoOperation is "in progress". Typically completion is set once
    // cryptoOperation->finish() is called, however it can also be called
    // during cryptoOperation->process() (for instance to set an error).
    //
    // The cryptoOperation pointer MUST remain valid while it is "in progress".
    // The cryptoOperation is said to be "in progress" from the time after
    // result->initializationSucceded() has been called, up until either:
    //
    //   - Blink calls cryptoOperation->abort()
    //   OR
    //   - Embedder calls result->completeWithXXX()
    //
    // Once the cryptoOperation is no longer "in progress" the "result" pointer
    // is no longer valid. At this time the embedder is responsible for freeing
    // the cryptoOperation.
    virtual void digest2(const WebCryptoAlgorithm&, WebCryptoOperationResult*) { }

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

// FIXME: Add error types.
class WebCryptoOperationResult {
public:
    // Only one of these should be called.
    virtual void initializationFailed() = 0;
    virtual void initializationSucceded(WebCryptoOperation*) = 0;

    // Only one of these should be called to set the result.
    virtual void completeWithError() = 0;
    virtual void completeWithArrayBuffer(const WebArrayBuffer&) = 0;

protected:
    virtual ~WebCryptoOperationResult() { }
};

} // namespace WebKit

#endif
