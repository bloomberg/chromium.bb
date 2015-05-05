// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClientHintsPreferences_h
#define ClientHintsPreferences_h

#include "core/CoreExport.h"
#include "wtf/text/WTFString.h"

namespace blink {

class DocumentLoader;

class ClientHintsPreferences {
public:
    ClientHintsPreferences()
        : m_shouldSendDPR(false)
        , m_shouldSendRW(false)
    {
    }

    void set(const ClientHintsPreferences& other)
    {
        m_shouldSendDPR = other.m_shouldSendDPR;
        m_shouldSendRW = other.m_shouldSendRW;
    }

    void setShouldSendDPR(bool should) { m_shouldSendDPR = should; }
    void setShouldSendRW(bool should) { m_shouldSendRW = should; }
    bool shouldSendDPR() const { return m_shouldSendDPR; }
    bool shouldSendRW() const { return m_shouldSendRW; }
private:
    bool m_shouldSendDPR;
    bool m_shouldSendRW;
};

CORE_EXPORT void handleAcceptClientHintsHeader(const String& headerValue, ClientHintsPreferences&);
} // namespace blink
#endif

