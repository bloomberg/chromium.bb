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
#include "WebCryptoAlgorithm.h"
#include "WebCryptoKey.h"
#include "WebPrivatePtr.h"

// FIXME: Remove this once chromium side is updated.
#define WEBCRYPTO_HAS_KEY_ALGORITHM 1

namespace WebCore { class CryptoResult; }

#if INSIDE_BLINK
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace blink {

class WebArrayBuffer;
class WebString;

class WebCryptoResult {
public:
    WebCryptoResult(const WebCryptoResult& o)
    {
        assign(o);
    }

    ~WebCryptoResult()
    {
        reset();
    }

    WebCryptoResult& operator=(const WebCryptoResult& o)
    {
        assign(o);
        return *this;
    }

    BLINK_PLATFORM_EXPORT void completeWithError();

    // Note that WebString is NOT safe to pass across threads.
    //
    // Error details are intended to be displayed to developers for debugging.
    // They MUST NEVER reveal any secret information such as bytes of the key
    // or plain text. An appropriate error would be something like:
    //   "iv must be 16 bytes long".
    BLINK_PLATFORM_EXPORT void completeWithError(const WebString&);

    // Note that WebArrayBuffer is NOT safe to create from another thread.
    BLINK_PLATFORM_EXPORT void completeWithBuffer(const WebArrayBuffer&);
    // Makes a copy of the input data given as a pointer and byte length.
    BLINK_PLATFORM_EXPORT void completeWithBuffer(const void*, unsigned);
    BLINK_PLATFORM_EXPORT void completeWithBoolean(bool);
    BLINK_PLATFORM_EXPORT void completeWithKey(const WebCryptoKey&);
    BLINK_PLATFORM_EXPORT void completeWithKeyPair(const WebCryptoKey& publicKey, const WebCryptoKey& privateKey);

#if INSIDE_BLINK
    BLINK_PLATFORM_EXPORT explicit WebCryptoResult(const WTF::PassRefPtr<WebCore::CryptoResult>&);
#endif

private:
    BLINK_PLATFORM_EXPORT void reset();
    BLINK_PLATFORM_EXPORT void assign(const WebCryptoResult&);

    WebPrivatePtr<WebCore::CryptoResult> m_impl;
};

class WebCrypto {
public:
    // WebCrypto is the interface for starting one-shot cryptographic
    // operations.
    //
    // -----------------------
    // Completing the request
    // -----------------------
    //
    // Implementations signal completion by calling one of the methods on
    // "result". Only a single result/error should be set for the request.
    // Different operations expect different result types based on the
    // algorithm parameters; see the Web Crypto standard for details.
    //
    // The result can be set either synchronously while handling the request,
    // or asynchronously after the method has returned. When completing
    // asynchronously make a copy of the WebCryptoResult and call it from the
    // same thread that started the request.
    //
    // -----------------------
    // Threading
    // -----------------------
    //
    // The WebCrypto interface will only be called from the render's main
    // thread. All communication back to Blink must be on this same thread.
    // Notably:
    //
    //   * The WebCryptoResult is NOT threadsafe. It should only be used from
    //     the Blink main thread.
    //
    //   * WebCryptoKey and WebCryptoAlgorithm ARE threadsafe. They can be
    //     safely copied between threads and accessed. Copying is cheap because
    //     they are internally reference counted.
    //
    //   * WebArrayBuffer is NOT threadsafe. It should only be created from the
    //     Blink main thread. This means threaded implementations may have to
    //     make a copy of the output buffer.
    //
    // -----------------------
    // Inputs
    // -----------------------
    //
    //   * Data buffers are passed as (basePointer, byteLength) pairs.
    //     These buffers are only valid during the call itself. Asynchronous
    //     implementations wishing to access it after the function returns
    //     should make a copy.
    //
    //   * All WebCryptoKeys are guaranteeed to be !isNull().
    //
    //   * All WebCryptoAlgorithms are guaranteed to be !isNull()
    //     unless noted otherwise. Being "null" means that it was unspecified
    //     by the caller.
    //
    //   * Look to the Web Crypto spec for an explanation of the parameter. The
    //     method names here have a 1:1 correspondence with those of
    //     crypto.subtle, with the exception of "verify" which is here called
    //     "verifySignature".
    //
    // -----------------------
    // Guarantees on input validity
    // -----------------------
    //
    // Implementations MUST carefully sanitize algorithm inputs before using
    // them, as they come directly from the user. Few checks have been done on
    // algorithm parameters prior to passing to the embedder.
    //
    // Only the following checks can be assumed as having alread passed:
    //
    //  * The key is extractable when calling into exportKey/wrapKey.
    //  * The key usages permit the operation being requested.
    //  * The key's algorithm matches that of the requested operation.
    //
    virtual void encrypt(const WebCryptoAlgorithm&, const WebCryptoKey&, const unsigned char* data, unsigned dataSize, WebCryptoResult result) { result.completeWithError(); }
    virtual void decrypt(const WebCryptoAlgorithm&, const WebCryptoKey&, const unsigned char* data, unsigned dataSize, WebCryptoResult result) { result.completeWithError(); }
    virtual void sign(const WebCryptoAlgorithm&, const WebCryptoKey&, const unsigned char* data, unsigned dataSize, WebCryptoResult result) { result.completeWithError(); }
    virtual void verifySignature(const WebCryptoAlgorithm&, const WebCryptoKey&, const unsigned char* signature, unsigned signatureSize, const unsigned char* data, unsigned dataSize, WebCryptoResult result) { result.completeWithError(); }
    virtual void digest(const WebCryptoAlgorithm&, const unsigned char* data, unsigned dataSize, WebCryptoResult result) { result.completeWithError(); }
    virtual void generateKey(const WebCryptoAlgorithm&, bool extractable, WebCryptoKeyUsageMask, WebCryptoResult result) { result.completeWithError(); }
    // It is possible for the WebCryptoAlgorithm to be "isNull()"
    virtual void importKey(WebCryptoKeyFormat, const unsigned char* keyData, unsigned keyDataSize, const WebCryptoAlgorithm&, bool extractable, WebCryptoKeyUsageMask, WebCryptoResult result) { result.completeWithError(); }
    virtual void exportKey(WebCryptoKeyFormat, const WebCryptoKey&, WebCryptoResult result) { result.completeWithError(); }
    virtual void wrapKey(WebCryptoKeyFormat, const WebCryptoKey& key, const WebCryptoKey& wrappingKey, const WebCryptoAlgorithm&, WebCryptoResult result) { result.completeWithError(); }
    // It is possible that unwrappedKeyAlgorithm.isNull()
    virtual void unwrapKey(WebCryptoKeyFormat, const unsigned char* wrappedKey, unsigned wrappedKeySize, const WebCryptoKey&, const WebCryptoAlgorithm& unwrapAlgorithm, const WebCryptoAlgorithm& unwrappedKeyAlgorithm, bool extractable, WebCryptoKeyUsageMask, WebCryptoResult result) { result.completeWithError(); }

    // This is the one exception to the "Completing the request" guarantees
    // outlined above. digestSynchronous must provide the result into result
    // synchronously. It must return |true| on successful calculation of the
    // digest and |false| otherwise. This is useful for Blink internal crypto
    // and is not part of the WebCrypto standard.
    virtual bool digestSynchronous(const WebCryptoAlgorithmId algorithmId, const unsigned char* data, unsigned dataSize, WebArrayBuffer& result) { return false; }

protected:
    virtual ~WebCrypto() { }
};

} // namespace blink

#endif
