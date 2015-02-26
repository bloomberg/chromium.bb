/*
 * Copyright (C) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "public/platform/WebContentDecryptionModuleSession.h"
#include "public/platform/WebString.h"

namespace blink {

WebContentDecryptionModuleSession::~WebContentDecryptionModuleSession()
{
}

// FIXME: remove once Chromium overrides this method.
void WebContentDecryptionModuleSession::initializeNewSession(WebEncryptedMediaInitDataType initDataType, const unsigned char* initData, size_t initDataLength, WebEncryptedMediaSessionType sessionType, WebContentDecryptionModuleResult result)
{
    WebString initDataTypeString;
    switch (initDataType) {
    case WebEncryptedMediaInitDataType::Cenc:
        initDataTypeString = "cenc";
        break;
    case WebEncryptedMediaInitDataType::Keyids:
        initDataTypeString = "keyids";
        break;
    case WebEncryptedMediaInitDataType::Webm:
        initDataTypeString = "webm";
        break;
    case WebEncryptedMediaInitDataType::Unknown:
        ASSERT_NOT_REACHED();
        initDataTypeString = WebString();
        break;
    }

    WebString sessionTypeString;
    switch (sessionType) {
    case WebEncryptedMediaSessionType::Temporary:
        sessionTypeString = "temporary";
        break;
    case WebEncryptedMediaSessionType::PersistentLicense:
        sessionTypeString = "persistent-license";
        break;
    case WebEncryptedMediaSessionType::PersistentReleaseMessage:
        sessionTypeString = "persistent-release-message";
        break;
    case WebEncryptedMediaSessionType::Unknown:
        sessionTypeString = WebString();
        break;
    }

    initializeNewSession(initDataTypeString, initData, initDataLength, sessionTypeString, result);
}

WebContentDecryptionModuleSession::Client::~Client()
{
}

} // namespace blink
