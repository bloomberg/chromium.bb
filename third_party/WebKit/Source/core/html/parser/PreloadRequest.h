// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PreloadRequest_h
#define PreloadRequest_h

#include "core/fetch/ClientHintsPreferences.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/Resource.h"
#include "wtf/text/TextPosition.h"

namespace blink {

class Document;

class PreloadRequest {
public:
    static PassOwnPtr<PreloadRequest> create(const String& initiatorName, const TextPosition& initiatorPosition, const String& resourceURL, const KURL& baseURL, Resource::Type resourceType, const FetchRequest::ResourceWidth& resourceWidth = FetchRequest::ResourceWidth(), const ClientHintsPreferences& clientHintsPreferences = ClientHintsPreferences())
    {
        return adoptPtr(new PreloadRequest(initiatorName, initiatorPosition, resourceURL, baseURL, resourceType, resourceWidth, clientHintsPreferences));
    }

    bool isSafeToSendToAnotherThread() const;

    FetchRequest resourceRequest(Document*);

    const String& charset() const { return m_charset; }
    double discoveryTime() const { return m_discoveryTime; }
    void setDefer(FetchRequest::DeferOption defer) { m_defer = defer; }
    void setCharset(const String& charset) { m_charset = charset.isolatedCopy(); }
    void setCrossOriginEnabled(StoredCredentials allowCredentials)
    {
        m_isCORSEnabled = true;
        m_allowCredentials = allowCredentials;
    }

    Resource::Type resourceType() const { return m_resourceType; }

    const String& resourceURL() const { return m_resourceURL; }
    float resourceWidth() const { return m_resourceWidth.isSet ? m_resourceWidth.width : 0; }
    const String& baseURL() const { return m_baseURL.string(); }

private:
    PreloadRequest(const String& initiatorName,
        const TextPosition& initiatorPosition,
        const String& resourceURL,
        const KURL& baseURL,
        Resource::Type resourceType,
        const FetchRequest::ResourceWidth& resourceWidth,
        const ClientHintsPreferences& clientHintsPreferences)
        : m_initiatorName(initiatorName)
        , m_initiatorPosition(initiatorPosition)
        , m_resourceURL(resourceURL.isolatedCopy())
        , m_baseURL(baseURL.copy())
        , m_resourceType(resourceType)
        , m_isCORSEnabled(false)
        , m_allowCredentials(DoNotAllowStoredCredentials)
        , m_discoveryTime(monotonicallyIncreasingTime())
        , m_defer(FetchRequest::NoDefer)
        , m_resourceWidth(resourceWidth)
        , m_clientHintsPreferences(clientHintsPreferences)
    {
    }

    KURL completeURL(Document*);

    String m_initiatorName;
    TextPosition m_initiatorPosition;
    String m_resourceURL;
    KURL m_baseURL;
    String m_charset;
    Resource::Type m_resourceType;
    bool m_isCORSEnabled;
    StoredCredentials m_allowCredentials;
    double m_discoveryTime;
    FetchRequest::DeferOption m_defer;
    FetchRequest::ResourceWidth m_resourceWidth;
    ClientHintsPreferences m_clientHintsPreferences;
};

typedef Vector<OwnPtr<PreloadRequest>> PreloadRequestStream;

}

#endif
