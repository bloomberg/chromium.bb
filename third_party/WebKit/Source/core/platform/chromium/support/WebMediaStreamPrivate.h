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

#ifndef WebMediaStreamPrivate_h
#define WebMediaStreamPrivate_h

#include "core/platform/chromium/support/WebMediaStreamClient.h"
#include "core/platform/mediastream/MediaStreamComponent.h"
#include "core/platform/mediastream/MediaStreamSource.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebKit {

class WebMediaStreamPrivate : public RefCounted<WebMediaStreamPrivate> {
public:
    class ExtraData : public RefCounted<ExtraData> {
    public:
        virtual ~ExtraData() { }
    };

    static PassRefPtr<WebMediaStreamPrivate> create(const WebCore::MediaStreamSourceVector& audioSources, const WebCore::MediaStreamSourceVector& videoSources);

    static PassRefPtr<WebMediaStreamPrivate> create(const String& id, const WebCore::MediaStreamComponentVector& audioComponents, const WebCore::MediaStreamComponentVector& videoComponents);

    WebKit::WebMediaStreamClient* client() const { return m_client; }
    void setClient(WebKit::WebMediaStreamClient* client) { m_client = client; }

    String id() const { return m_id; }

    unsigned numberOfAudioComponents() const { return m_audioComponents.size(); }
    WebCore::MediaStreamComponent* audioComponent(unsigned index) const { return m_audioComponents[index].get(); }

    unsigned numberOfVideoComponents() const { return m_videoComponents.size(); }
    WebCore::MediaStreamComponent* videoComponent(unsigned index) const { return m_videoComponents[index].get(); }

    void addComponent(WebCore::MediaStreamComponent*);
    void removeComponent(WebCore::MediaStreamComponent*);

    void addRemoteTrack(WebCore::MediaStreamComponent*);
    void removeRemoteTrack(WebCore::MediaStreamComponent*);

    bool ended() const { return m_ended; }
    void setEnded() { m_ended = true; }

    PassRefPtr<ExtraData> extraData() const { return m_extraData; }
    void setExtraData(PassRefPtr<ExtraData> extraData) { m_extraData = extraData; }

private:
    WebMediaStreamPrivate(const String& id);
    void initialize(const WebCore::MediaStreamSourceVector& audioSources, const WebCore::MediaStreamSourceVector& videoSources);
    void initialize(const WebCore::MediaStreamComponentVector& audioComponents, const WebCore::MediaStreamComponentVector& videoComponents);

    WebMediaStreamClient* m_client;
    String m_id;
    Vector<RefPtr<WebCore::MediaStreamComponent> > m_audioComponents;
    Vector<RefPtr<WebCore::MediaStreamComponent> > m_videoComponents;
    bool m_ended;

    RefPtr<ExtraData> m_extraData;
};

typedef Vector<RefPtr<WebMediaStreamPrivate> > WebMediaStreamPrivateVector;

} // namespace WebKit

#endif // WebMediaStreamPrivate_h
