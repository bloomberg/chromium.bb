/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "core/platform/chromium/support/WebMediaStreamPrivate.h"

#include "core/platform/UUID.h"
#include "core/platform/mediastream/MediaStreamComponent.h"
#include "core/platform/mediastream/MediaStreamSource.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebKit {

PassRefPtr<WebMediaStreamPrivate> WebMediaStreamPrivate::create(const WebCore::MediaStreamSourceVector& audioSources, const WebCore::MediaStreamSourceVector& videoSources)
{
    RefPtr<WebMediaStreamPrivate> privateStream = adoptRef(new WebMediaStreamPrivate(WebCore::createCanonicalUUIDString()));
    privateStream->initialize(audioSources, videoSources);
    return privateStream;
}

PassRefPtr<WebMediaStreamPrivate> WebMediaStreamPrivate::create(const String& id, const WebCore::MediaStreamComponentVector& audioComponents, const WebCore::MediaStreamComponentVector& videoComponents)
{
    RefPtr<WebMediaStreamPrivate> privateStream = adoptRef(new WebMediaStreamPrivate(id));
    privateStream->initialize(audioComponents, videoComponents);
    return privateStream;
}

void WebMediaStreamPrivate::addComponent(WebCore::MediaStreamComponent* component)
{
    switch (component->source()->type()) {
    case WebCore::MediaStreamSource::TypeAudio:
        if (m_audioComponents.find(component) == notFound)
            m_audioComponents.append(component);
        break;
    case WebCore::MediaStreamSource::TypeVideo:
        if (m_videoComponents.find(component) == notFound)
            m_videoComponents.append(component);
        break;
    }
}

void WebMediaStreamPrivate::removeComponent(WebCore::MediaStreamComponent* component)
{
    size_t pos = notFound;
    switch (component->source()->type()) {
    case WebCore::MediaStreamSource::TypeAudio:
        pos = m_audioComponents.find(component);
        if (pos != notFound)
            m_audioComponents.remove(pos);
        break;
    case WebCore::MediaStreamSource::TypeVideo:
        pos = m_videoComponents.find(component);
        if (pos != notFound)
            m_videoComponents.remove(pos);
        break;
    }
}

void WebMediaStreamPrivate::addRemoteTrack(WebCore::MediaStreamComponent* component)
{
    if (m_client)
        m_client->addRemoteTrack(component);
    else
        addComponent(component);
}

void WebMediaStreamPrivate::removeRemoteTrack(WebCore::MediaStreamComponent* component)
{
    if (m_client)
        m_client->removeRemoteTrack(component);
    else
        removeComponent(component);
}

WebMediaStreamPrivate::WebMediaStreamPrivate(const String& id)
    : m_client(0)
    , m_id(id)
    , m_ended(false)
{
    ASSERT(m_id.length());
}

void WebMediaStreamPrivate::initialize(const WebCore::MediaStreamSourceVector& audioSources, const WebCore::MediaStreamSourceVector& videoSources)
{
    for (size_t i = 0; i < audioSources.size(); i++)
        m_audioComponents.append(WebCore::MediaStreamComponent::create(WebMediaStream(this), audioSources[i]));

    for (size_t i = 0; i < videoSources.size(); i++)
        m_videoComponents.append(WebCore::MediaStreamComponent::create(WebMediaStream(this), videoSources[i]));
}

void WebMediaStreamPrivate::initialize(const WebCore::MediaStreamComponentVector& audioComponents, const WebCore::MediaStreamComponentVector& videoComponents)
{
    for (WebCore::MediaStreamComponentVector::const_iterator iter = audioComponents.begin(); iter != audioComponents.end(); ++iter) {
        (*iter)->setStream(this);
        m_audioComponents.append((*iter));
    }
    for (WebCore::MediaStreamComponentVector::const_iterator iter = videoComponents.begin(); iter != videoComponents.end(); ++iter) {
        (*iter)->setStream(this);
        m_videoComponents.append((*iter));
    }
}

} // namespace WebKit
