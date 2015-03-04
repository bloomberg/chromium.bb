// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebMediaKeySystemConfiguration.h"

namespace blink {

WebVector<WebEncryptedMediaInitDataType> WebMediaKeySystemConfiguration::getInitDataTypes() const
{
    WebVector<WebEncryptedMediaInitDataType> initDataTypesList(initDataTypes.size());
    for (size_t i = 0; i < initDataTypes.size(); ++i) {
        if (initDataTypes[i] == "cenc") {
            initDataTypesList[i] = WebEncryptedMediaInitDataType::Cenc;
        } else if (initDataTypes[i] == "keyids") {
            initDataTypesList[i] = WebEncryptedMediaInitDataType::Keyids;
        } else if (initDataTypes[i] == "webm") {
            initDataTypesList[i] = WebEncryptedMediaInitDataType::Webm;
        } else {
            initDataTypesList[i] = WebEncryptedMediaInitDataType::Unknown;
        }
    }
    return initDataTypesList;
}

void WebMediaKeySystemConfiguration::setInitDataTypes(const WebVector<WebEncryptedMediaInitDataType>& newInitDataTypes)
{
    WebVector<WebString> initDataTypesList(newInitDataTypes.size());
    for (size_t i = 0; i < newInitDataTypes.size(); ++i) {
        switch (newInitDataTypes[i]) {
        case WebEncryptedMediaInitDataType::Cenc:
            initDataTypesList[i] = "cenc";
            break;
        case WebEncryptedMediaInitDataType::Keyids:
            initDataTypesList[i] = "keyids";
            break;
        case WebEncryptedMediaInitDataType::Webm:
            initDataTypesList[i] = "webm";
            break;
        case WebEncryptedMediaInitDataType::Unknown:
            initDataTypesList[i] = "unknown";
            break;
        }
    }
    initDataTypes.swap(initDataTypesList);
}

WebVector<WebEncryptedMediaSessionType> WebMediaKeySystemConfiguration::getSessionTypes() const
{
    WebVector<WebEncryptedMediaSessionType> sessionTypesList(sessionTypes.size());
    for (size_t i = 0; i < sessionTypes.size(); ++i) {
        if (sessionTypes[i] == "temporary") {
            sessionTypesList[i] = WebEncryptedMediaSessionType::Temporary;
        } else if (sessionTypes[i] == "persistent-license") {
            sessionTypesList[i] = WebEncryptedMediaSessionType::PersistentLicense;
        } else if (sessionTypes[i] == "persistent-release-message") {
            sessionTypesList[i] = WebEncryptedMediaSessionType::PersistentReleaseMessage;
        } else {
            sessionTypesList[i] = WebEncryptedMediaSessionType::Unknown;
        }
    }
    return sessionTypesList;
}

void WebMediaKeySystemConfiguration::setSessionTypes(const WebVector<WebEncryptedMediaSessionType>& newSessionTypes)
{
    WebVector<WebString> sessionTypesList(newSessionTypes.size());
    for (size_t i = 0; i < newSessionTypes.size(); ++i) {
        switch (newSessionTypes[i]) {
        case WebEncryptedMediaSessionType::Temporary:
            sessionTypesList[i] = "temporary";
            break;
        case WebEncryptedMediaSessionType::PersistentLicense:
            sessionTypesList[i] = "persistent-license";
            break;
        case WebEncryptedMediaSessionType::PersistentReleaseMessage:
            sessionTypesList[i] = "persistent-release-message";
            break;
        case WebEncryptedMediaSessionType::Unknown:
            sessionTypesList[i] = "unknown";
            break;
        }
    }
    sessionTypes.swap(sessionTypesList);
}

} // namespace blink
