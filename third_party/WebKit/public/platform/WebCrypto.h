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
    // The following methods start a new asynchronous multi-part cryptographic
    // operation.
    //
    //   - Returns 0 on failure
    //   - The returned pointer must remain valid until the operation has
    //     completed. After this, the embedder is responsible for freeing it.

    virtual WebCryptoOperation* digest(const WebCryptoAlgorithm&) = 0;

protected:
    virtual ~WebCrypto() { }
};

// WebCryptoOperation represents a multi-part cryptographic operation. The
// methods on this interface will be called in this order:
//
//   (1) 0 or more calls to process()
//   (2) 0 or 1 calls to finish()
//   (3) 0 or 1 calls to abort()
//
// Deletion of the WebCryptoOperation is the responsibility of the embedder.
// However it MUST remain alive until either:
//   (a) Blink has called this->abort()
//   (b) The embedder has called result->setXXX()
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

    // Completes the operation and writes the result to the
    // WebCryptoOperationResult* (henceforth called |result|).
    //
    // |result| can be set either synchronously or asynchronously. |result|
    // will remain alive until the operation completes OR this->abort() is
    // called. |result| SHOULD NOT be used after this->abort() has been called.
    //
    //   * Implementations should delete |this| after setting |result|.
    virtual void finish(WebCryptoOperationResult*) = 0;

protected:
    virtual ~WebCryptoOperation() { }
};

// WebCryptoOperationResult is a handle for either synchronous or asynchronous
// completion of WebCryptoOperation::finish().
//
// The result can be either an error or a value.
//
// Only one of the setXXX() methods should be called, corresponding to the
// expected type of result for the operation. For instance digest() outputs an
// ArrayBuffer whereas verify() outputs a boolean.
//
// Note on re-entrancy: After completing the result (i.e. calling one of the
// setXXX() methods) the embedder must be ready to service other requests. In
// other words, it should release any locks that would prevent other
// WebCryptoOperations from being created or used.
class WebCryptoOperationResult {
public:
    virtual void setArrayBuffer(const WebArrayBuffer&) = 0;

protected:
    virtual ~WebCryptoOperationResult() { }
};

} // namespace WebKit

#endif
