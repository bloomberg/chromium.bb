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

#include "config.h"
#include "public/platform/WebCrypto.h"

namespace WebKit {

// FIXME: Assert that these methods are called from the right thread.

void WebCryptoOperationResult::initializationFailed()
{
    m_impl->initializationFailed();
    reset();
}

void WebCryptoOperationResult::initializationSucceeded(WebCryptoOperation* op)
{
    m_impl->initializationSucceeded(op);
    reset();
}

void WebCryptoOperationResult::completeWithError()
{
    m_impl->completeWithError();
    reset();
}

void WebCryptoOperationResult::completeWithArrayBuffer(const WebArrayBuffer& buffer)
{
    m_impl->completeWithArrayBuffer(buffer);
    reset();
}

void WebCryptoOperationResult::completeWithBoolean(bool b)
{
    m_impl->completeWithBoolean(b);
    reset();
}

void WebCryptoOperationResult::reset()
{
    m_impl.reset();
}

void WebCryptoOperationResult::assign(const WebCryptoOperationResult& o)
{
    m_impl = o.m_impl;
}

void WebCryptoOperationResult::assign(WebCryptoOperationResultPrivate* impl)
{
    m_impl = impl;
}

void WebCryptoKeyOperationResult::initializationFailed()
{
    m_impl->initializationFailed();
    reset();
}

void WebCryptoKeyOperationResult::initializationSucceeded(WebCryptoKeyOperation* op)
{
    m_impl->initializationSucceeded(op);
    reset();
}

void WebCryptoKeyOperationResult::completeWithError()
{
    m_impl->completeWithError();
    reset();
}

void WebCryptoKeyOperationResult::completeWithKey(const WebCryptoKey& key)
{
    m_impl->completeWithKey(key);
    reset();
}

void WebCryptoKeyOperationResult::reset()
{
    m_impl.reset();
}

void WebCryptoKeyOperationResult::assign(const WebCryptoKeyOperationResult& o)
{
    m_impl = o.m_impl;
}

void WebCryptoKeyOperationResult::assign(WebCryptoKeyOperationResultPrivate* impl)
{
    m_impl = impl;
}

} // namespace WebKit
