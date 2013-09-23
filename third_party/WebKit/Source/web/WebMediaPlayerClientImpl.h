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

#include "WebAudioSourceProviderClient.h"
#include "WebMediaPlayerClient.h"
#include "core/platform/audio/AudioSourceProvider.h"
#include "core/platform/graphics/InbandTextTrackPrivate.h"
#include "core/platform/graphics/MediaPlayer.h"
#if OS(ANDROID)
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrTexture.h"
#endif
#include "weborigin/KURL.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/ThreadingPrimitives.h"

namespace WebCore {
class AudioSourceProviderClient;
class HTMLMediaSource;
}

namespace WebKit {

class WebHelperPluginImpl;
class WebAudioSourceProvider;
class WebMediaPlayer;

// This class serves as a bridge between WebCore::MediaPlayer and
// WebKit::WebMediaPlayer.
class WebMediaPlayerClientImpl : public WebCore::MediaPlayer, public WebMediaPlayerClient {

public:
    static PassOwnPtr<WebCore::MediaPlayer> create(WebCore::MediaPlayerClient*);

    // Returns the encapsulated WebKit::WebMediaPlayer.
    WebMediaPlayer* mediaPlayer() const;

    // WebMediaPlayerClient methods:
    virtual ~WebMediaPlayerClientImpl();
    virtual void networkStateChanged();
    virtual void readyStateChanged();
    virtual void timeChanged();
    virtual void repaint();
    virtual void durationChanged();
    virtual void sizeChanged();
    virtual void setOpaque(bool);
    virtual double volume() const;
    virtual void playbackStateChanged();
    virtual WebMediaPlayer::Preload preload() const;
    virtual void keyAdded(const WebString& keySystem, const WebString& sessionId);
    virtual void keyError(const WebString& keySystem, const WebString& sessionId, MediaKeyErrorCode, unsigned short systemCode);
    virtual void keyMessage(const WebString& keySystem, const WebString& sessionId, const unsigned char* message, unsigned messageLength, const WebURL& defaultURL);
    virtual void keyNeeded(const WebString& keySystem, const WebString& sessionId, const unsigned char* initData, unsigned initDataLength);
    virtual WebPlugin* createHelperPlugin(const WebString& pluginType, WebFrame*);
    virtual void closeHelperPlugin();
    virtual void closeHelperPluginSoon(WebFrame*);
    virtual bool needsWebLayerForVideo() const;
    virtual void setWebLayer(WebLayer*);
    virtual void addTextTrack(WebInbandTextTrack*);
    virtual void removeTextTrack(WebInbandTextTrack*);
    virtual void mediaSourceOpened(WebMediaSource*);
    virtual void requestSeek(double);

    // MediaPlayer methods:
    virtual void load(const WTF::String& url) OVERRIDE;
    virtual void load(const WTF::String& url, PassRefPtr<WebCore::HTMLMediaSource>) OVERRIDE;

    virtual WebKit::WebLayer* platformLayer() const OVERRIDE;
    virtual void play() OVERRIDE;
    virtual void pause() OVERRIDE;
    virtual void prepareToPlay() OVERRIDE;
    virtual bool supportsFullscreen() const OVERRIDE;
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
    virtual WebCore::MediaPlayer::NetworkState networkState() const OVERRIDE;
    virtual WebCore::MediaPlayer::ReadyState readyState() const OVERRIDE;
    virtual double maxTimeSeekable() const OVERRIDE;
    virtual WTF::PassRefPtr<WebCore::TimeRanges> buffered() const OVERRIDE;
    virtual bool didLoadingProgress() const OVERRIDE;
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect&) OVERRIDE;
    virtual void paintCurrentFrameInContext(WebCore::GraphicsContext*, const WebCore::IntRect&) OVERRIDE;
    virtual bool copyVideoTextureToPlatformTexture(WebCore::GraphicsContext3D*, Platform3DObject texture, GC3Dint level, GC3Denum type, GC3Denum internalFormat, bool premultiplyAlpha, bool flipY) OVERRIDE;
    virtual void setPreload(WebCore::MediaPlayer::Preload) OVERRIDE;
    virtual bool hasSingleSecurityOrigin() const OVERRIDE;
    virtual bool didPassCORSAccessCheck() const OVERRIDE;
    virtual double mediaTimeForTimeValue(double timeValue) const OVERRIDE;
    virtual unsigned decodedFrameCount() const OVERRIDE;
    virtual unsigned droppedFrameCount() const OVERRIDE;
    virtual unsigned audioDecodedByteCount() const OVERRIDE;
    virtual unsigned videoDecodedByteCount() const OVERRIDE;
    virtual void showFullscreenOverlay() OVERRIDE;
    virtual void hideFullscreenOverlay() OVERRIDE;
    virtual bool canShowFullscreenOverlay() const OVERRIDE;

#if ENABLE(WEB_AUDIO)
    virtual WebCore::AudioSourceProvider* audioSourceProvider() OVERRIDE;
#endif

    virtual bool supportsAcceleratedRendering() const OVERRIDE;

    virtual WebCore::MediaPlayer::MediaKeyException generateKeyRequest(const String& keySystem, const unsigned char* initData, unsigned initDataLength) OVERRIDE;
    virtual WebCore::MediaPlayer::MediaKeyException addKey(const String& keySystem, const unsigned char* key, unsigned keyLength, const unsigned char* initData, unsigned initDataLength, const String& sessionId) OVERRIDE;
    virtual WebCore::MediaPlayer::MediaKeyException cancelKeyRequest(const String& keySystem, const String& sessionId) OVERRIDE;

private:
    explicit WebMediaPlayerClientImpl(WebCore::MediaPlayerClient*);

    void startDelayedLoad();
    void loadRequested();
    void loadInternal();

    bool acceleratedRenderingInUse();

#if OS(ANDROID)
    // FIXME: This path "only works" on Android. It is a workaround for the problem that Skia could not handle Android's GL_TEXTURE_EXTERNAL_OES
    // texture internally. It should be removed and replaced by the normal paint path.
    // https://code.google.com/p/skia/issues/detail?id=1189
    void paintOnAndroid(WebCore::GraphicsContext* context, WebCore::GraphicsContext3D* context3D, const WebCore::IntRect& rect, uint8_t alpha);
    SkAutoTUnref<GrTexture> m_texture;
    SkBitmap m_bitmap;
#endif

    WebCore::MediaPlayerClient* m_client;
    OwnPtr<WebMediaPlayer> m_webMediaPlayer;
    WebCore::KURL m_url;
    bool m_isMediaStream;
    bool m_delayingLoad;
    WebCore::MediaPlayer::Preload m_preload;
    RefPtr<WebHelperPluginImpl> m_helperPlugin;
    WebLayer* m_videoLayer;
    bool m_opaque;
    bool m_needsWebLayerForVideo;
    double m_volume;
    bool m_muted;
    double m_rate;

#if ENABLE(WEB_AUDIO)
    // AudioClientImpl wraps an AudioSourceProviderClient.
    // When the audio format is known, Chromium calls setFormat() which then dispatches into WebCore.

    class AudioClientImpl : public WebKit::WebAudioSourceProviderClient {
    public:
        AudioClientImpl(WebCore::AudioSourceProviderClient* client)
            : m_client(client)
        {
        }

        virtual ~AudioClientImpl() { }

        // WebAudioSourceProviderClient
        virtual void setFormat(size_t numberOfChannels, float sampleRate);

    private:
        WebCore::AudioSourceProviderClient* m_client;
    };

    // AudioSourceProviderImpl wraps a WebAudioSourceProvider.
    // provideInput() calls into Chromium to get a rendered audio stream.

    class AudioSourceProviderImpl : public WebCore::AudioSourceProvider {
    public:
        AudioSourceProviderImpl()
            : m_webAudioSourceProvider(0)
        {
        }

        virtual ~AudioSourceProviderImpl() { }

        // Wraps the given WebAudioSourceProvider.
        void wrap(WebAudioSourceProvider*);

        // WebCore::AudioSourceProvider
        virtual void setClient(WebCore::AudioSourceProviderClient*);
        virtual void provideInput(WebCore::AudioBus*, size_t framesToProcess);

    private:
        WebAudioSourceProvider* m_webAudioSourceProvider;
        OwnPtr<AudioClientImpl> m_client;
        Mutex provideInputLock;
    };

    AudioSourceProviderImpl m_audioSourceProvider;
#endif

    RefPtr<WebCore::HTMLMediaSource> m_mediaSource;
};

} // namespace WebKit

#endif
