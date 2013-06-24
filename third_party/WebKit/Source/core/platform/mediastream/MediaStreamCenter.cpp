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

#include "core/platform/mediastream/MediaStreamDescriptor.h"
#include "modules/mediastream/MediaStreamTrackSourcesRequest.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMediaStream.h"
#include "public/platform/WebMediaStreamCenter.h"
#include "public/platform/WebMediaStreamTrack.h"
#include "public/platform/WebMediaStreamTrackSourcesRequest.h"
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

bool MediaStreamCenter::getMediaStreamTrackSources(PassRefPtr<MediaStreamTrackSourcesRequest> request)
{
    return m_private && m_private->getMediaStreamTrackSources(request);
}

void MediaStreamCenter::didSetMediaStreamTrackEnabled(MediaStreamDescriptor* stream,  MediaStreamComponent* component)
{
    if (m_private) {
        if (component->enabled())
            m_private->didEnableMediaStreamTrack(stream, component);
        else
            m_private->didDisableMediaStreamTrack(stream, component);
    }
}

bool MediaStreamCenter::didAddMediaStreamTrack(MediaStreamDescriptor* stream, MediaStreamComponent* component)
{
    return m_private && m_private->didAddMediaStreamTrack(stream, component);
}

bool MediaStreamCenter::didRemoveMediaStreamTrack(MediaStreamDescriptor* stream, MediaStreamComponent* component)
{
    return m_private && m_private->didRemoveMediaStreamTrack(stream, component);
}

void MediaStreamCenter::didStopLocalMediaStream(MediaStreamDescriptor* stream)
{
    if (m_private) {
        m_private->didStopLocalMediaStream(stream);
        for (unsigned i = 0; i < stream->numberOfAudioComponents(); i++)
            stream->audioComponent(i)->source()->setReadyState(MediaStreamSource::ReadyStateEnded);
        for (unsigned i = 0; i < stream->numberOfVideoComponents(); i++)
            stream->videoComponent(i)->source()->setReadyState(MediaStreamSource::ReadyStateEnded);
    }
}

void MediaStreamCenter::didCreateMediaStream(MediaStreamDescriptor* stream)
{
    if (m_private) {
        WebKit::WebMediaStream webStream(stream);
        m_private->didCreateMediaStream(webStream);
    }
}

void MediaStreamCenter::stopLocalMediaStream(const WebKit::WebMediaStream& webStream)
{
    MediaStreamDescriptor* stream = webStream;
    MediaStreamDescriptorClient* client = stream->client();
    if (client)
        client->streamEnded();
    else
        stream->setEnded();
}

} // namespace WebCore
