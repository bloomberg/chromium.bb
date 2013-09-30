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
#include "core/platform/graphics/ContentDecryptionModule.h"

#include "platform/NotImplemented.h"
#include "core/platform/graphics/ContentDecryptionModuleSession.h"
#include "public/platform/Platform.h"

namespace WebCore {

bool ContentDecryptionModule::supportsKeySystem(const String& keySystem)
{
    // FIXME: Chromium should handle this, possibly using
    // MIMETypeRegistry::isSupportedEncryptedMediaMIMEType().
    notImplemented();
    return keySystem == "org.w3.clearkey";
}

PassOwnPtr<ContentDecryptionModule> ContentDecryptionModule::create(const String& keySystem)
{
    ASSERT(!keySystem.isEmpty());
    OwnPtr<WebKit::WebContentDecryptionModule> cdm = adoptPtr(WebKit::Platform::current()->createContentDecryptionModule(keySystem));
    if (!cdm)
        return nullptr;
    return adoptPtr(new ContentDecryptionModule(cdm.release()));
}

ContentDecryptionModule::ContentDecryptionModule(PassOwnPtr<WebKit::WebContentDecryptionModule> cdm)
    : m_cdm(cdm)
{
    ASSERT(m_cdm);
}

ContentDecryptionModule::~ContentDecryptionModule()
{
}

bool ContentDecryptionModule::supportsMIMEType(const String& mimeType)
{
    // FIXME: Chromium should handle this, possibly using
    // MIMETypeRegistry::isSupportedEncryptedMediaMIMEType().
    notImplemented();
    return mimeType == "video/webm";
}

PassOwnPtr<ContentDecryptionModuleSession> ContentDecryptionModule::createSession(ContentDecryptionModuleSessionClient* client)
{
    return adoptPtr(new ContentDecryptionModuleSession(m_cdm.get(), client));
}

} // namespace WebCore
