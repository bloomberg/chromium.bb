/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/loader/TextTrackLoader.h"

#include "FetchInitiatorTypeNames.h"
#include "core/dom/Document.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/html/track/WebVTTParser.h"
#include "platform/Logging.h"
#include "platform/SharedBuffer.h"
#include "weborigin/SecurityOrigin.h"

namespace WebCore {

TextTrackLoader::TextTrackLoader(TextTrackLoaderClient& client, Document& document)
    : m_client(client)
    , m_document(document)
    , m_cueLoadTimer(this, &TextTrackLoader::cueLoadTimerFired)
    , m_state(Idle)
    , m_newCuesAvailable(false)
{
}

TextTrackLoader::~TextTrackLoader()
{
    if (m_resource)
        m_resource->removeClient(this);
}

void TextTrackLoader::cueLoadTimerFired(Timer<TextTrackLoader>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_cueLoadTimer);

    if (m_newCuesAvailable) {
        m_newCuesAvailable = false;
        m_client.newCuesAvailable(this);
    }

    if (m_state >= Finished)
        m_client.cueLoadingCompleted(this, m_state == Failed);
}

void TextTrackLoader::cancelLoad()
{
    if (m_resource) {
        m_resource->removeClient(this);
        m_resource = 0;
    }
}

void TextTrackLoader::dataReceived(Resource* resource, const char* data, int length)
{
    ASSERT(m_resource == resource);

    if (m_state == Failed)
        return;

    if (!m_cueParser)
        m_cueParser = WebVTTParser::create(this, m_document);

    m_cueParser->parseBytes(data, length);
}

void TextTrackLoader::corsPolicyPreventedLoad()
{
    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Cross-origin text track load denied by Cross-Origin Resource Sharing policy."));
    m_document.addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, consoleMessage);
    m_state = Failed;
}

void TextTrackLoader::notifyFinished(Resource* resource)
{
    ASSERT(m_resource == resource);

    if (!m_crossOriginMode.isNull()
        && !m_document.securityOrigin()->canRequest(resource->response().url())
        && !resource->passesAccessControlCheck(m_document.securityOrigin())) {

        corsPolicyPreventedLoad();
    }

    if (m_state != Failed)
        m_state = resource->errorOccurred() ? Failed : Finished;

    if (!m_cueLoadTimer.isActive())
        m_cueLoadTimer.startOneShot(0);

    cancelLoad();
}

bool TextTrackLoader::load(const KURL& url, const String& crossOriginMode)
{
    cancelLoad();

    FetchRequest cueRequest(ResourceRequest(m_document.completeURL(url)), FetchInitiatorTypeNames::texttrack);

    if (!crossOriginMode.isNull()) {
        m_crossOriginMode = crossOriginMode;
        StoredCredentials allowCredentials = equalIgnoringCase(crossOriginMode, "use-credentials") ? AllowStoredCredentials : DoNotAllowStoredCredentials;
        updateRequestForAccessControl(cueRequest.mutableResourceRequest(), m_document.securityOrigin(), allowCredentials);
    } else {
        // Cross-origin resources that are not suitably CORS-enabled may not load.
        if (!m_document.securityOrigin()->canRequest(url)) {
            corsPolicyPreventedLoad();
            return false;
        }
    }

    ResourceFetcher* fetcher = m_document.fetcher();
    m_resource = fetcher->fetchRawResource(cueRequest);
    if (!m_resource)
        return false;
    m_resource->addClient(this);
    return true;
}

void TextTrackLoader::newCuesParsed()
{
    if (m_cueLoadTimer.isActive())
        return;

    m_newCuesAvailable = true;
    m_cueLoadTimer.startOneShot(0);
}

void TextTrackLoader::newRegionsParsed()
{
    m_client.newRegionsAvailable(this);
}

void TextTrackLoader::fileFailedToParse()
{
    LOG(Media, "TextTrackLoader::fileFailedToParse");

    m_state = Failed;

    if (!m_cueLoadTimer.isActive())
        m_cueLoadTimer.startOneShot(0);

    cancelLoad();
}

void TextTrackLoader::getNewCues(Vector<RefPtr<TextTrackCue> >& outputCues)
{
    ASSERT(m_cueParser);
    if (m_cueParser)
        m_cueParser->getNewCues(outputCues);
}

void TextTrackLoader::getNewRegions(Vector<RefPtr<VTTRegion> >& outputRegions)
{
    ASSERT(m_cueParser);
    if (m_cueParser)
        m_cueParser->getNewRegions(outputRegions);
}

}
