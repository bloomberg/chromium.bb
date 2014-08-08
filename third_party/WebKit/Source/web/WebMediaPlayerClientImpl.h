/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebMediaPlayerClientImpl_h
#define WebMediaPlayerClientImpl_h

#include "platform/audio/AudioSourceProvider.h"
#include "platform/graphics/media/MediaPlayer.h"
#include "public/platform/WebAudioSourceProviderClient.h"
#include "public/platform/WebMediaPlayerClient.h"
#include "platform/weborigin/KURL.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/ThreadingPrimitives.h"

namespace blink {
class AudioSourceProviderClient;
class HTMLMediaElement;
}

namespace blink {

class WebAudioSourceProvider;
class WebContentDecryptionModule;
class WebMediaPlayer;

// This class serves as a bridge between blink::MediaPlayer and
// blink::WebMediaPlayer.
class WebMediaPlayerClientImpl FINAL : public blink::MediaPlayer, public WebMediaPlayerClient {

public:
    static PassOwnPtr<blink::MediaPlayer> create(blink::MediaPlayerClient*);

    virtual ~WebMediaPlayerClientImpl();

    // WebMediaPlayerClient methods:
    virtual void networkStateChanged() OVERRIDE;
    virtual void readyStateChanged() OVERRIDE;
    virtual void timeChanged() OVERRIDE;
    virtual void repaint() OVERRIDE;
    virtual void durationChanged() OVERRIDE;
    virtual void sizeChanged() OVERRIDE;
    virtual void playbackStateChanged() OVERRIDE;

    // WebEncryptedMediaPlayerClient methods:
    virtual void keyAdded(const WebString& keySystem, const WebString& sessionId) OVERRIDE;
    virtual void keyError(const WebString& keySystem, const WebString& sessionId, MediaKeyErrorCode, unsigned short systemCode) OVERRIDE;
    virtual void keyMessage(const WebString& keySystem, const WebString& sessionId, const unsigned char* message, unsigned messageLength, const WebURL& defaultURL) OVERRIDE;
    virtual void keyNeeded(const WebString& contentType, const unsigned char* initData, unsigned initDataLength) OVERRIDE;

    virtual void setWebLayer(WebLayer*) OVERRIDE;
    virtual WebMediaPlayer::TrackId addAudioTrack(const WebString& id, AudioTrackKind, const WebString& label, const WebString& language, bool enabled) OVERRIDE;
    virtual void removeAudioTrack(WebMediaPlayer::TrackId) OVERRIDE;
    virtual WebMediaPlayer::TrackId addVideoTrack(const WebString& id, VideoTrackKind, const WebString& label, const WebString& language, bool selected) OVERRIDE;
    virtual void removeVideoTrack(WebMediaPlayer::TrackId) OVERRIDE;
    virtual void addTextTrack(WebInbandTextTrack*) OVERRIDE;
    virtual void removeTextTrack(WebInbandTextTrack*) OVERRIDE;
    virtual void mediaSourceOpened(WebMediaSource*) OVERRIDE;
    virtual void requestFullscreen() OVERRIDE;
    virtual void requestSeek(double) OVERRIDE;

    // MediaPlayer methods:
    virtual WebMediaPlayer* webMediaPlayer() const OVERRIDE;
    virtual void load(WebMediaPlayer::LoadType, const WTF::String& url, WebMediaPlayer::CORSMode) OVERRIDE;
    virtual void setPreload(blink::MediaPlayer::Preload) OVERRIDE;

#if ENABLE(WEB_AUDIO)
    virtual blink::AudioSourceProvider* audioSourceProvider() OVERRIDE;
#endif

private:
    explicit WebMediaPlayerClientImpl(blink::MediaPlayerClient*);

    blink::HTMLMediaElement& mediaElement() const;

    blink::MediaPlayerClient* m_client;
    OwnPtr<WebMediaPlayer> m_webMediaPlayer;

#if ENABLE(WEB_AUDIO)
    // AudioClientImpl wraps an AudioSourceProviderClient.
    // When the audio format is known, Chromium calls setFormat() which then dispatches into WebCore.

    class AudioClientImpl FINAL : public NoBaseWillBeGarbageCollectedFinalized<AudioClientImpl>, public blink::WebAudioSourceProviderClient {
    public:
        explicit AudioClientImpl(blink::AudioSourceProviderClient* client)
            : m_client(client)
        {
        }

        virtual ~AudioClientImpl() { }

        // WebAudioSourceProviderClient
        virtual void setFormat(size_t numberOfChannels, float sampleRate) OVERRIDE;

        void trace(Visitor*);

    private:
        RawPtrWillBeMember<blink::AudioSourceProviderClient> m_client;
    };

    // AudioSourceProviderImpl wraps a WebAudioSourceProvider.
    // provideInput() calls into Chromium to get a rendered audio stream.

    class AudioSourceProviderImpl FINAL : public blink::AudioSourceProvider {
    public:
        AudioSourceProviderImpl()
            : m_webAudioSourceProvider(0)
        {
        }

        virtual ~AudioSourceProviderImpl() { }

        // Wraps the given WebAudioSourceProvider.
        void wrap(WebAudioSourceProvider*);

        // blink::AudioSourceProvider
        virtual void setClient(blink::AudioSourceProviderClient*) OVERRIDE;
        virtual void provideInput(blink::AudioBus*, size_t framesToProcess) OVERRIDE;

    private:
        WebAudioSourceProvider* m_webAudioSourceProvider;
        OwnPtrWillBePersistent<AudioClientImpl> m_client;
        Mutex provideInputLock;
    };

    AudioSourceProviderImpl m_audioSourceProvider;
#endif
};

} // namespace blink

#endif
