// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/encryptedmedia/EncryptedMediaUtils.h"

namespace blink {

WebEncryptedMediaInitDataType EncryptedMediaUtils::convertToInitDataType(const String& initDataType)
{
    if (initDataType == "cenc")
        return WebEncryptedMediaInitDataType::Cenc;
    if (initDataType == "keyids")
        return WebEncryptedMediaInitDataType::Keyids;
    if (initDataType == "webm")
        return WebEncryptedMediaInitDataType::Webm;

    // |initDataType| is not restricted in the idl, so anything is possible.
    return WebEncryptedMediaInitDataType::Unknown;
}

String EncryptedMediaUtils::convertFromInitDataType(WebEncryptedMediaInitDataType initDataType)
{
    switch (initDataType) {
    case WebEncryptedMediaInitDataType::Cenc:
        return "cenc";
    case WebEncryptedMediaInitDataType::Keyids:
        return "keyids";
    case WebEncryptedMediaInitDataType::Webm:
        return "webm";
    case WebEncryptedMediaInitDataType::Unknown:
        return String();
    }

    ASSERT_NOT_REACHED();
    return String();
}

WebEncryptedMediaSessionType EncryptedMediaUtils::convertToSessionType(const String& sessionType)
{
    if (sessionType == "temporary")
        return WebEncryptedMediaSessionType::Temporary;
    if (sessionType == "persistent-license")
        return WebEncryptedMediaSessionType::PersistentLicense;
    if (sessionType == "persistent-release-message")
        return WebEncryptedMediaSessionType::PersistentReleaseMessage;

    ASSERT_NOT_REACHED();
    return WebEncryptedMediaSessionType::Unknown;
}

} // namespace blink
