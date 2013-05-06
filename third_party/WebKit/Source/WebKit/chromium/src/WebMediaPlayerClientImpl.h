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

#include "core/platform/audio/AudioSourceProvider.h"
#include "core/platform/graphics/MediaPlayerPrivate.h"
#if defined(OS_ANDROID)
#include "GrTexture.h"
#include "SkBitmap.h"
#include "SkRefCnt.h"
#endif
#include "WebAudioSourceProviderClient.h"
#include "WebMediaPlayerClient.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore { class AudioSourceProviderClient; }

namespace WebKit {

class WebHelperPluginImpl;
class WebAudioSourceProvider;
class WebMediaPlayer;

// This class serves as a bridge between WebCore::MediaPlayer and
// WebKit::WebMediaPlayer.
class WebMediaPlayerClientImpl : public WebCore::MediaPlayerPrivateInterface, public WebMediaPlayerClient {

public:
    static bool isEnabled();
    static void setIsEnabled(bool);
    static void registerSelf(WebCore::MediaEngineRegistrar);

    // Returns the encapsulated WebKit::WebMediaPlayer.
    WebMediaPlayer* mediaPlayer() const;

    // WebMediaPlayerClient methods:
    virtual ~WebMediaPlayerClientImpl();
    virtual void networkStateChanged();
    virtual void readyStateChanged();
    virtual void volumeChanged(double);
    virtual void muteChanged(bool);
    virtual void timeChanged();
    virtual void repaint();
    virtual void durationChanged();
    virtual void rateChanged();
    virtual void sizeChanged();
    virtual void setOpaque(bool);
    virtual void sawUnsupportedTracks();
    virtual double volume() const;
    virtual void playbackStateChanged();
    virtual WebMediaPlayer::Preload preload() const;
    virtual void keyAdded(const WebString& keySystem, const WebString& sessionId);
    virtual void keyError(const WebString& keySystem, const WebString& sessionId, MediaKeyErrorCode, unsigned short systemCode);
    virtual void keyMessage(const WebString& keySystem, const WebString& sessionId, const unsigned char* message, unsigned messageLength, const WebURL& defaultURL);
    virtual void keyNeeded(const WebString& keySystem, const WebString& sessionId, const unsigned char* initData, unsigned initDataLength);
    virtual WebPlugin* createHelperPlugin(const WebString& pluginType, WebFrame*);
    virtual void closeHelperPlugin();
    virtual bool needsWebLayerForVideo() const;
    virtual void setWebLayer(WebLayer*);
    virtual void addTextTrack(WebInbandTextTrack*);
    virtual void removeTextTrack(WebInbandTextTrack*);

    // MediaPlayerPrivateInterface methods:
    virtual void load(const WTF::String& url);
    virtual void load(const WTF::String& url, PassRefPtr<WebCore::MediaSource>);

    virtual void cancelLoad();
    virtual WebKit::WebLayer* platformLayer() const;
    virtual WebCore::PlatformMedia platformMedia() const;
    virtual void play();
    virtual void pause();
    virtual void prepareToPlay();
    virtual bool supportsFullscreen() const;
    virtual bool supportsSave() const;
    virtual WebCore::IntSize naturalSize() const;
    virtual bool hasVideo() const;
    virtual bool hasAudio() const;
    virtual void setVisible(bool);
    virtual double duration() const;
    virtual double currentTime() const;
    virtual void seek(double time);
    virtual bool seeking() const;
    virtual void setRate(double);
    virtual bool paused() const;
    virtual void setVolume(double);
    virtual WebCore::MediaPlayer::NetworkState networkState() const;
    virtual WebCore::MediaPlayer::ReadyState readyState() const;
    virtual double maxTimeSeekable() const;
    virtual WTF::PassRefPtr<WebCore::TimeRanges> buffered() const;
    virtual int dataRate() const;
    virtual bool totalBytesKnown() const;
    virtual unsigned totalBytes() const;
    virtual bool didLoadingProgress() const;
    virtual void setSize(const WebCore::IntSize&);
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect&);
    virtual void paintCurrentFrameInContext(WebCore::GraphicsContext*, const WebCore::IntRect&);
    virtual bool copyVideoTextureToPlatformTexture(WebCore::GraphicsContext3D*, Platform3DObject texture, GC3Dint level, GC3Denum type, GC3Denum internalFormat, bool premultiplyAlpha, bool flipY);
    virtual void setPreload(WebCore::MediaPlayer::Preload);
    virtual bool hasSingleSecurityOrigin() const;
    virtual bool didPassCORSAccessCheck() const;
    virtual WebCore::MediaPlayer::MovieLoadType movieLoadType() const;
    virtual double mediaTimeForTimeValue(double timeValue) const;
    virtual unsigned decodedFrameCount() const;
    virtual unsigned droppedFrameCount() const;
    virtual unsigned audioDecodedByteCount() const;
    virtual unsigned videoDecodedByteCount() const;
#if USE(NATIVE_FULLSCREEN_VIDEO)
    virtual void enterFullscreen();
    virtual void exitFullscreen();
    virtual bool canEnterFullscreen() const;
#endif

#if ENABLE(WEB_AUDIO)
    virtual WebCore::AudioSourceProvider* audioSourceProvider();
#endif

    virtual bool supportsAcceleratedRendering() const;

#if ENABLE(ENCRYPTED_MEDIA)
    virtual WebCore::MediaPlayer::MediaKeyException generateKeyRequest(const String& keySystem, const unsigned char* initData, unsigned initDataLength) OVERRIDE;
    virtual WebCore::MediaPlayer::MediaKeyException addKey(const String& keySystem, const unsigned char* key, unsigned keyLength, const unsigned char* initData, unsigned initDataLength, const String& sessionId) OVERRIDE;
    virtual WebCore::MediaPlayer::MediaKeyException cancelKeyRequest(const String& keySystem, const String& sessionId) OVERRIDE;
#endif

protected:
    WebMediaPlayerClientImpl();
private:
    void startDelayedLoad();
    void loadRequested();
    void loadInternal();

    static PassOwnPtr<WebCore::MediaPlayerPrivateInterface> create(WebCore::MediaPlayer*);
#if ENABLE(ENCRYPTED_MEDIA)
    static WebCore::MediaPlayer::SupportsType supportsType(
        const WTF::String& type, const WTF::String& codecs, const String& keySystem, const WebCore::KURL&);
#else
    static WebCore::MediaPlayer::SupportsType supportsType(
        const WTF::String& type, const WTF::String& codecs, const WebCore::KURL&);
#endif
    bool acceleratedRenderingInUse();

#if defined(OS_ANDROID)
    // FIXME: This path "only works" on Android. It is a workaround for the problem that Skia could not handle Android's GL_TEXTURE_EXTERNAL_OES
    // texture internally. It should be removed and replaced by the normal paint path.
    // https://code.google.com/p/skia/issues/detail?id=1189
    void paintOnAndroid(WebCore::GraphicsContext* context, WebCore::GraphicsContext3D* context3D, const WebCore::IntRect& rect, uint8_t alpha);
    SkAutoTUnref<GrTexture> m_texture;
    SkBitmap m_bitmap;
#endif

    WebCore::MediaPlayer* m_mediaPlayer;
    OwnPtr<WebMediaPlayer> m_webMediaPlayer;
    WebCore::KURL m_url;
    bool m_delayingLoad;
    WebCore::MediaPlayer::Preload m_preload;
    RefPtr<WebHelperPluginImpl> m_helperPlugin;
    WebLayer* m_videoLayer;
    bool m_opaque;
    bool m_needsWebLayerForVideo;
    static bool m_isEnabled;

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
    };

    AudioSourceProviderImpl m_audioSourceProvider;
#endif

    RefPtr<WebCore::MediaSource> m_mediaSource;
};

} // namespace WebKit

#endif
