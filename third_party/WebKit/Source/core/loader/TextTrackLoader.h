/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TextTrackLoader_h
#define TextTrackLoader_h

#include "core/fetch/RawResource.h"
#include "core/fetch/ResourcePtr.h"
#include "core/html/track/WebVTTParser.h"
#include "platform/Timer.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class Document;
class TextTrackLoader;

class TextTrackLoaderClient {
public:
    virtual ~TextTrackLoaderClient() { }

    virtual void newCuesAvailable(TextTrackLoader*) = 0;
    virtual void cueLoadingCompleted(TextTrackLoader*, bool loadingFailed) = 0;
    virtual void newRegionsAvailable(TextTrackLoader*) = 0;
};

class TextTrackLoader : public RawResourceClient, private WebVTTParserClient {
    WTF_MAKE_NONCOPYABLE(TextTrackLoader);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<TextTrackLoader> create(TextTrackLoaderClient& client, Document& document)
    {
        return adoptPtr(new TextTrackLoader(client, document));
    }
    virtual ~TextTrackLoader();

    bool load(const KURL&, const String& crossOriginMode);
    void cancelLoad();

    enum State { Idle, Loading, Finished, Failed };
    State loadState() { return m_state; }

    void getNewCues(Vector<RefPtr<TextTrackCue> >& outputCues);
    void getNewRegions(Vector<RefPtr<VTTRegion> >& outputRegions);
private:

    // RawResourceClient
    virtual void dataReceived(Resource*, const char* data, int length) OVERRIDE;
    virtual void notifyFinished(Resource*) OVERRIDE;

    // WebVTTParserClient
    virtual void newCuesParsed() OVERRIDE;
    virtual void newRegionsParsed() OVERRIDE;
    virtual void fileFailedToParse() OVERRIDE;

    TextTrackLoader(TextTrackLoaderClient&, Document&);

    void cueLoadTimerFired(Timer<TextTrackLoader>*);
    void corsPolicyPreventedLoad();

    TextTrackLoaderClient& m_client;
    OwnPtr<WebVTTParser> m_cueParser;
    ResourcePtr<RawResource> m_resource;
    // FIXME: Remove this pointer and get the Document from m_client.
    Document& m_document;
    Timer<TextTrackLoader> m_cueLoadTimer;
    String m_crossOriginMode;
    State m_state;
    bool m_newCuesAvailable;
};

} // namespace WebCore

#endif
