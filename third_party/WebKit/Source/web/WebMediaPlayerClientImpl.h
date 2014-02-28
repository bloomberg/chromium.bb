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
#include "third_party/khronos/GLES2/gl2.h"
#if OS(ANDROID)
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrTexture.h"
#endif
#include "platform/weborigin/KURL.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/ThreadingPrimitives.h"

namespace WebCore {
class AudioSourceProviderClient;
}

namespace blink {

class WebAudioSourceProvider;
class WebContentDecryptionModule;
class WebMediaPlayer;
class WebGraphicsContext3D;

// This class serves as a bridge between WebCore::MediaPlayer and
// blink::WebMediaPlayer.
class WebMediaPlayerClientImpl FINAL : public WebCore::MediaPlayer, public WebMediaPlayerClient {

public:
    static PassOwnPtr<WebCore::MediaPlayer> create(WebCore::MediaPlayerClient*);

    virtual ~WebMediaPlayerClientImpl();

    // Returns the encapsulated blink::WebMediaPlayer.
    WebMediaPlayer* mediaPlayer() const;

    // WebMediaPlayerClient methods:
    virtual void networkStateChanged() OVERRIDE;
    virtual void readyStateChanged() OVERRIDE;
    virtual void timeChanged() OVERRIDE;
    virtual void repaint() OVERRIDE;
    virtual void durationChanged() OVERRIDE;
    virtual void sizeChanged() OVERRIDE;
    virtual void setOpaque(bool) OVERRIDE;
    virtual double volume() const OVERRIDE;
    virtual void playbackStateChanged() OVERRIDE;
    virtual WebMediaPlayer::Preload preload() const OVERRIDE;
    virtual void keyAdded(const WebString& keySystem, const WebString& sessionId) OVERRIDE;
    virtual void keyError(const WebString& keySystem, const WebString& sessionId, MediaKeyErrorCode, unsigned short systemCode) OVERRIDE;
    virtual void keyMessage(const WebString& keySystem, const WebString& sessionId, const unsigned char* message, unsigned messageLength, const WebURL& defaultURL) OVERRIDE;
    virtual void keyNeeded(const WebString& contentType, const unsigned char* initData, unsigned initDataLength) OVERRIDE;
    virtual bool needsWebLayerForVideo() const OVERRIDE;
    virtual void setWebLayer(WebLayer*) OVERRIDE;
    virtual void addTextTrack(WebInbandTextTrack*) OVERRIDE;
    virtual void removeTextTrack(WebInbandTextTrack*) OVERRIDE;
    virtual void mediaSourceOpened(WebMediaSource*) OVERRIDE;
    virtual void requestFullscreen() OVERRIDE;
    virtual void requestSeek(double) OVERRIDE;

    // MediaPlayer methods:
    virtual void load(WebMediaPlayer::LoadType, const WTF::String& url) OVERRIDE;

    virtual void play() OVERRIDE;
    virtual void pause() OVERRIDE;
    virtual void prepareToPlay() OVERRIDE;
    virtual bool supportsSave() const OVERRIDE;
    virtual WebCore::IntSize naturalSize() const OVERRIDE;
    virtual bool hasVideo() const OVERRIDE;
    virtual bool hasAudio() const OVERRIDE;
    virtual double duration() const OVERRIDE;
    virtual double currentTime() const OVERRIDE;
    virtual void seek(double time) OVERRIDE;
    virtual bool seeking() const OVERRIDE;
    virtual double rate() const OVERRIDE;
    virtual void setRate(double) OVERRIDE;
    virtual bool paused() const OVERRIDE;
    virtual void setVolume(double) OVERRIDE;
    virtual void setMuted(bool) OVERRIDE;
    virtual void setPoster(const WebCore::KURL&) OVERRIDE;
    virtual WebCore::MediaPlayer::NetworkState networkState() const OVERRIDE;
    virtual WebCore::MediaPlayer::ReadyState readyState() const OVERRIDE;
    virtual double maxTimeSeekable() const OVERRIDE;
    virtual WTF::PassRefPtr<WebCore::TimeRanges> buffered() const OVERRIDE;
    virtual bool didLoadingProgress() const OVERRIDE;
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect&) OVERRIDE;
    virtual bool copyVideoTextureToPlatformTexture(WebGraphicsContext3D*, Platform3DObject texture, GLint level, GLenum type, GLenum internalFormat, bool premultiplyAlpha, bool flipY) OVERRIDE;
    virtual void setPreload(WebCore::MediaPlayer::Preload) OVERRIDE;
    virtual bool hasSingleSecurityOrigin() const OVERRIDE;
    virtual bool didPassCORSAccessCheck() const OVERRIDE;
    virtual double mediaTimeForTimeValue(double timeValue) const OVERRIDE;
    virtual unsigned decodedFrameCount() const OVERRIDE;
    virtual unsigned droppedFrameCount() const OVERRIDE;
    virtual unsigned corruptedFrameCount() const OVERRIDE;
    virtual unsigned audioDecodedByteCount() const OVERRIDE;
    virtual unsigned videoDecodedByteCount() const OVERRIDE;
    virtual void showFullscreenOverlay() OVERRIDE;
    virtual void hideFullscreenOverlay() OVERRIDE;
    virtual bool canShowFullscreenOverlay() const OVERRIDE;

#if ENABLE(WEB_AUDIO)
    virtual WebCore::AudioSourceProvider* audioSourceProvider() OVERRIDE;
#endif

    virtual WebCore::MediaPlayer::MediaKeyException generateKeyRequest(const String& keySystem, const unsigned char* initData, unsigned initDataLength) OVERRIDE;
    virtual WebCore::MediaPlayer::MediaKeyException addKey(const String& keySystem, const unsigned char* key, unsigned keyLength, const unsigned char* initData, unsigned initDataLength, const String& sessionId) OVERRIDE;
    virtual WebCore::MediaPlayer::MediaKeyException cancelKeyRequest(const String& keySystem, const String& sessionId) OVERRIDE;
    virtual void setContentDecryptionModule(WebContentDecryptionModule*) OVERRIDE;

private:
    explicit WebMediaPlayerClientImpl(WebCore::MediaPlayerClient*);

    void startDelayedLoad();
    void loadInternal();

#if OS(ANDROID)
    // FIXME: This path "only works" on Android. It is a workaround for the problem that Skia could not handle Android's GL_TEXTURE_EXTERNAL_OES
    // texture internally. It should be removed and replaced by the normal paint path.
    // https://code.google.com/p/skia/issues/detail?id=1189
    void paintOnAndroid(WebCore::GraphicsContext*, WebGraphicsContext3D*, const WebCore::IntRect&, uint8_t alpha);
    SkBitmap m_bitmap;
#endif

    WebCore::MediaPlayerClient* m_client;
    OwnPtr<WebMediaPlayer> m_webMediaPlayer;
    WebCore::KURL m_url;
    bool m_delayingLoad;
    WebCore::MediaPlayer::Preload m_preload;
    bool m_needsWebLayerForVideo;
    double m_volume;
    bool m_muted;
    double m_rate;
    WebContentDecryptionModule* m_cdm;

#if ENABLE(WEB_AUDIO)
    // AudioClientImpl wraps an AudioSourceProviderClient.
    // When the audio format is known, Chromium calls setFormat() which then dispatches into WebCore.

    class AudioClientImpl FINAL : public blink::WebAudioSourceProviderClient {
    public:
        AudioClientImpl(WebCore::AudioSourceProviderClient* client)
            : m_client(client)
        {
        }

        virtual ~AudioClientImpl() { }

        // WebAudioSourceProviderClient
        virtual void setFormat(size_t numberOfChannels, float sampleRate) OVERRIDE;

    private:
        WebCore::AudioSourceProviderClient* m_client;
    };

    // AudioSourceProviderImpl wraps a WebAudioSourceProvider.
    // provideInput() calls into Chromium to get a rendered audio stream.

    class AudioSourceProviderImpl FINAL : public WebCore::AudioSourceProvider {
    public:
        AudioSourceProviderImpl()
            : m_webAudioSourceProvider(0)
        {
        }

        virtual ~AudioSourceProviderImpl() { }

        // Wraps the given WebAudioSourceProvider.
        void wrap(WebAudioSourceProvider*);

        // WebCore::AudioSourceProvider
        virtual void setClient(WebCore::AudioSourceProviderClient*) OVERRIDE;
        virtual void provideInput(WebCore::AudioBus*, size_t framesToProcess) OVERRIDE;

    private:
        WebAudioSourceProvider* m_webAudioSourceProvider;
        OwnPtr<AudioClientImpl> m_client;
        Mutex provideInputLock;
    };

    AudioSourceProviderImpl m_audioSourceProvider;
#endif

    WebMediaPlayer::LoadType m_loadType;
};

} // namespace blink

#endif
