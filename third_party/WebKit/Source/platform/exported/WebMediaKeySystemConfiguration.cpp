// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebMediaKeySystemConfiguration.h"

namespace blink {

WebVector<WebEncryptedMediaInitDataType> WebMediaKeySystemConfiguration::getInitDataTypes() const
{
    return initDataTypes;
}

void WebMediaKeySystemConfiguration::setInitDataTypes(const WebVector<WebEncryptedMediaInitDataType>& newInitDataTypes)
{
    initDataTypes = newInitDataTypes;
}

void WebMediaKeySystemConfiguration::setSessionTypes(const WebVector<WebEncryptedMediaSessionType>& newSessionTypes)
{
    sessionTypes = newSessionTypes;
}

} // namespace blink
