// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/fetch/ClientHintsPreferences.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/network/HTTPParsers.h"

namespace blink {

void handleAcceptClientHintsHeader(const String& headerValue, ClientHintsPreferences& preferences)
{
    if (!RuntimeEnabledFeatures::clientHintsEnabled() || headerValue.isEmpty())
        return;
    CommaDelimitedHeaderSet acceptCH;
    parseCommaDelimitedHeader(headerValue, acceptCH);
    if (acceptCH.contains("dpr"))
        preferences.setShouldSendDPR(true);

    if (acceptCH.contains("width"))
        preferences.setShouldSendResourceWidth(true);

    if (acceptCH.contains("viewport-width"))
        preferences.setShouldSendViewportWidth(true);
}

}
