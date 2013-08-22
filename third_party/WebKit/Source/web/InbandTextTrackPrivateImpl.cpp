/*
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

#include "InbandTextTrackPrivateImpl.h"
#include "WebInbandTextTrack.h"
#include "core/platform/graphics/InbandTextTrackPrivateClient.h"
#include "public/platform/WebString.h"

namespace WebKit {

InbandTextTrackPrivateImpl::InbandTextTrackPrivateImpl(WebInbandTextTrack* track)
    : m_track(track)
{
    ASSERT(track);
    track->setClient(this);
}

InbandTextTrackPrivateImpl::~InbandTextTrackPrivateImpl()
{
    m_track->setClient(0);
}

void InbandTextTrackPrivateImpl::setMode(Mode mode)
{
    m_track->setMode(static_cast<WebInbandTextTrack::Mode>(mode));
}

WebCore::InbandTextTrackPrivate::Mode InbandTextTrackPrivateImpl::mode() const
{
    return static_cast<WebCore::InbandTextTrackPrivate::Mode>(m_track->mode());
}

WebCore::InbandTextTrackPrivate::Kind InbandTextTrackPrivateImpl::kind() const
{
    return static_cast<WebCore::InbandTextTrackPrivate::Kind>(m_track->kind());
}

bool InbandTextTrackPrivateImpl::isClosedCaptions() const
{
    return m_track->isClosedCaptions();
}

AtomicString InbandTextTrackPrivateImpl::label() const
{
    return m_track->label();
}

AtomicString InbandTextTrackPrivateImpl::language() const
{
    return m_track->language();
}

bool InbandTextTrackPrivateImpl::isDefault() const
{
    return m_track->isDefault();
}

int InbandTextTrackPrivateImpl::textTrackIndex() const
{
    return m_track->textTrackIndex();
}

void InbandTextTrackPrivateImpl::addWebVTTCue(
    double start,
    double end,
    const WebString& id,
    const WebString& content,
    const WebString& settings)
{
    client()->addWebVTTCue(this, start, end, id, content, settings);
}

} // namespace WebKit
