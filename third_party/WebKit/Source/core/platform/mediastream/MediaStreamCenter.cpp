/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "core/platform/mediastream/MediaStreamCenter.h"

#include "core/platform/chromium/support/WebMediaStreamClient.h"
#include "core/platform/mediastream/MediaStreamSourcesQueryClient.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMediaStream.h"
#include "public/platform/WebMediaStreamCenter.h"
#include "public/platform/WebMediaStreamSourcesRequest.h"
#include "public/platform/WebMediaStreamTrack.h"
#include "wtf/MainThread.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

MediaStreamCenter& MediaStreamCenter::instance()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(MediaStreamCenter, center, ());
    return center;
}

MediaStreamCenter::MediaStreamCenter()
    : m_private(adoptPtr(WebKit::Platform::current()->createMediaStreamCenter(this)))
{
}

MediaStreamCenter::~MediaStreamCenter()
{
}

void MediaStreamCenter::queryMediaStreamSources(PassRefPtr<MediaStreamSourcesQueryClient> client)
{
    if (m_private) {
        m_private->queryMediaStreamSources(client);
    } else {
        MediaStreamSourceVector audioSources, videoSources;
        client->didCompleteQuery(audioSources, videoSources);
    }
}

bool MediaStreamCenter::getSourceInfos(const String& url, WebKit::WebVector<WebKit::WebSourceInfo>& sourceInfos)
{
    return m_private && m_private->getSourceInfos(url, sourceInfos);
}

void MediaStreamCenter::didSetMediaStreamTrackEnabled(WebKit::WebMediaStream webStream,  MediaStreamComponent* component)
{
    if (m_private) {
        if (component->enabled())
            m_private->didEnableMediaStreamTrack(webStream, component);
        else
            m_private->didDisableMediaStreamTrack(webStream, component);
    }
}

bool MediaStreamCenter::didAddMediaStreamTrack(WebKit::WebMediaStream webStream, MediaStreamComponent* component)
{
    return m_private && m_private->didAddMediaStreamTrack(webStream, component);
}

bool MediaStreamCenter::didRemoveMediaStreamTrack(WebKit::WebMediaStream webStream, MediaStreamComponent* component)
{
    return m_private && m_private->didRemoveMediaStreamTrack(webStream, component);
}

void MediaStreamCenter::didStopLocalMediaStream(WebKit::WebMediaStream webStream)
{
    if (m_private) {
        m_private->didStopLocalMediaStream(webStream);
        for (unsigned i = 0; i < webStream.numberOfAudioComponents(); i++)
            webStream.audioComponent(i)->source()->setReadyState(MediaStreamSource::ReadyStateEnded);
        for (unsigned i = 0; i < webStream.numberOfVideoComponents(); i++)
            webStream.videoComponent(i)->source()->setReadyState(MediaStreamSource::ReadyStateEnded);
    }
}

void MediaStreamCenter::didCreateMediaStream(WebKit::WebMediaStream webStream)
{
    if (m_private) {
        m_private->didCreateMediaStream(webStream);
    }
}

void MediaStreamCenter::stopLocalMediaStream(WebKit::WebMediaStream webStream)
{

    WebKit::WebMediaStreamClient* client = webStream.client();
    if (client)
        client->streamEnded();
    else
        webStream.setEnded();
}

} // namespace WebCore
