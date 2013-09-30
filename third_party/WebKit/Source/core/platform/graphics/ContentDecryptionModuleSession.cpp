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
#include "core/platform/graphics/ContentDecryptionModuleSession.h"

#include "platform/NotImplemented.h"
#include "public/platform/Platform.h"
#include "public/platform/WebContentDecryptionModule.h"
#include "public/platform/WebURL.h"
#include "weborigin/KURL.h"
#include "wtf/Uint8Array.h"

namespace WebCore {

ContentDecryptionModuleSession::ContentDecryptionModuleSession(WebKit::WebContentDecryptionModule* contentDecryptionModule, ContentDecryptionModuleSessionClient* client)
    : m_client(client)
{
    m_session = adoptPtr(contentDecryptionModule->createSession(this));
    ASSERT(m_session);
}

ContentDecryptionModuleSession::~ContentDecryptionModuleSession()
{
}

String ContentDecryptionModuleSession::sessionId() const
{
    return m_session->sessionId();
}

void ContentDecryptionModuleSession::generateKeyRequest(const String& mimeType, const Uint8Array& initData)
{
    m_session->generateKeyRequest(mimeType, initData.data(), initData.length());
}

void ContentDecryptionModuleSession::update(const Uint8Array& key)
{
    m_session->update(key.data(), key.length());
}

void ContentDecryptionModuleSession::close()
{
    m_session->close();
}

void ContentDecryptionModuleSession::keyAdded()
{
    m_client->keyAdded();
}

void ContentDecryptionModuleSession::keyError(MediaKeyErrorCode errorCode, unsigned long systemCode)
{
    m_client->keyError(static_cast<ContentDecryptionModuleSessionClient::MediaKeyErrorCode>(errorCode), systemCode);
}

void ContentDecryptionModuleSession::keyMessage(const unsigned char* message, size_t messageLength, const WebKit::WebURL& destinationURL)
{
    m_client->keyMessage(message, messageLength, destinationURL);
}

} // namespace WebCore
