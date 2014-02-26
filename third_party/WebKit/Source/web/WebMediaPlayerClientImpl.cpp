// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "WebMediaPlayerClientImpl.h"

#include "WebDocument.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebHelperPlugin.h"
#include "WebViewImpl.h"
#include "core/frame/Frame.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/TimeRanges.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "modules/mediastream/MediaStreamRegistry.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioSourceProviderClient.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/gpu/Extensions3DUtil.h"
#include "platform/graphics/skia/GaneshUtils.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAudioSourceProvider.h"
#include "public/platform/WebCString.h"
#include "public/platform/WebCanvas.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebContentDecryptionModule.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "public/platform/WebInbandTextTrack.h"
#include "public/platform/WebMediaPlayer.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

#if OS(ANDROID)
#include "GrContext.h"
#include "GrTypes.h"
#include "SkCanvas.h"
#include "SkGrPixelRef.h"
#endif


#include "wtf/Assertions.h"
#include "wtf/text/CString.h"

using namespace WebCore;

namespace blink {

static PassOwnPtr<WebMediaPlayer> createWebMediaPlayer(WebMediaPlayerClient* client, const WebURL& url, Frame* frame)
{
    WebFrameImpl* webFrame = WebFrameImpl::fromFrame(frame);

    if (!webFrame->client())
        return nullptr;
    return adoptPtr(webFrame->client()->createMediaPlayer(webFrame, url, client));
}

WebMediaPlayer* WebMediaPlayerClientImpl::mediaPlayer() const
{
    return m_webMediaPlayer.get();
}

// WebMediaPlayerClient --------------------------------------------------------

WebMediaPlayerClientImpl::~WebMediaPlayerClientImpl()
{
    // Explicitly destroy the WebMediaPlayer to allow verification of tear down.
    m_webMediaPlayer.clear();

    // Ensure the m_webMediaPlayer destroyed any WebHelperPlugin used.
    ASSERT(!m_helperPlugin);
}

void WebMediaPlayerClientImpl::networkStateChanged()
{
    m_client->mediaPlayerNetworkStateChanged();
}

void WebMediaPlayerClientImpl::readyStateChanged()
{
    m_client->mediaPlayerReadyStateChanged();
}

void WebMediaPlayerClientImpl::timeChanged()
{
    m_client->mediaPlayerTimeChanged();
}

void WebMediaPlayerClientImpl::repaint()
{
    m_client->mediaPlayerRepaint();
}

void WebMediaPlayerClientImpl::durationChanged()
{
    m_client->mediaPlayerDurationChanged();
}

void WebMediaPlayerClientImpl::sizeChanged()
{
    m_client->mediaPlayerSizeChanged();
}

void WebMediaPlayerClientImpl::setOpaque(bool opaque)
{
    m_client->mediaPlayerSetOpaque(opaque);
}

double WebMediaPlayerClientImpl::volume() const
{
    return m_volume;
}

void WebMediaPlayerClientImpl::playbackStateChanged()
{
    m_client->mediaPlayerPlaybackStateChanged();
}

WebMediaPlayer::Preload WebMediaPlayerClientImpl::preload() const
{
    return static_cast<WebMediaPlayer::Preload>(m_preload);
}

void WebMediaPlayerClientImpl::keyAdded(const WebString& keySystem, const WebString& sessionId)
{
    m_client->mediaPlayerKeyAdded(keySystem, sessionId);
}

void WebMediaPlayerClientImpl::keyError(const WebString& keySystem, const WebString& sessionId, MediaKeyErrorCode errorCode, unsigned short systemCode)
{
    m_client->mediaPlayerKeyError(keySystem, sessionId, static_cast<MediaPlayerClient::MediaKeyErrorCode>(errorCode), systemCode);
}

void WebMediaPlayerClientImpl::keyMessage(const WebString& keySystem, const WebString& sessionId, const unsigned char* message, unsigned messageLength, const WebURL& defaultURL)
{
    m_client->mediaPlayerKeyMessage(keySystem, sessionId, message, messageLength, defaultURL);
}

void WebMediaPlayerClientImpl::keyNeeded(const WebString& contentType, const unsigned char* initData, unsigned initDataLength)
{
    m_client->mediaPlayerKeyNeeded(contentType, initData, initDataLength);
}

WebPlugin* WebMediaPlayerClientImpl::createHelperPlugin(const WebString& pluginType, WebFrame* frame)
{
    ASSERT(!m_helperPlugin);

    m_helperPlugin = adoptPtr(frame->view()->createHelperPlugin(pluginType, frame->document()));
    if (!m_helperPlugin)
        return 0;

    WebPlugin* plugin = m_helperPlugin->getPlugin();
    if (!plugin) {
        // There is no need to keep the helper plugin around and the caller
        // should not be expected to call close after a failure (null pointer).
        closeHelperPluginSoon(frame);
        return 0;
    }

    return plugin;
}

// FIXME: |frame| no longer needed.
void WebMediaPlayerClientImpl::closeHelperPluginSoon(WebFrame* frame)
{
    ASSERT(m_helperPlugin);
    m_helperPlugin.clear();
}

void WebMediaPlayerClientImpl::setWebLayer(blink::WebLayer* layer)
{
    m_client->mediaPlayerSetWebLayer(layer);
}

void WebMediaPlayerClientImpl::addTextTrack(WebInbandTextTrack* textTrack)
{
    m_client->mediaPlayerDidAddTrack(textTrack);
}

void WebMediaPlayerClientImpl::removeTextTrack(WebInbandTextTrack* textTrack)
{
    m_client->mediaPlayerDidRemoveTrack(textTrack);
}

void WebMediaPlayerClientImpl::mediaSourceOpened(WebMediaSource* webMediaSource)
{
    ASSERT(webMediaSource);
    m_client->mediaPlayerMediaSourceOpened(webMediaSource);
}

void WebMediaPlayerClientImpl::requestFullscreen()
{
    m_client->mediaPlayerRequestFullscreen();
}

void WebMediaPlayerClientImpl::requestSeek(double time)
{
    m_client->mediaPlayerRequestSeek(time);
}

// MediaPlayer -------------------------------------------------

void WebMediaPlayerClientImpl::load(WebMediaPlayer::LoadType loadType, const WTF::String& url)
{
    m_url = KURL(ParsedURLString, url);
    m_loadType = loadType;

    if (m_preload == MediaPlayer::None) {
#if ENABLE(WEB_AUDIO)
        m_audioSourceProvider.wrap(0); // Clear weak reference to m_webMediaPlayer's WebAudioSourceProvider.
#endif
        m_webMediaPlayer.clear();
        m_delayingLoad = true;
    } else
        loadInternal();
}

void WebMediaPlayerClientImpl::loadInternal()
{
#if ENABLE(WEB_AUDIO)
    m_audioSourceProvider.wrap(0); // Clear weak reference to m_webMediaPlayer's WebAudioSourceProvider.
#endif

    // FIXME: Remove this cast
    Frame* frame = static_cast<HTMLMediaElement*>(m_client)->document().frame();

    WebURL poster = m_client->mediaPlayerPosterURL();

    // This does not actually check whether the hardware can support accelerated
    // compositing, but only if the flag is set. However, this is checked lazily
    // in WebViewImpl::setIsAcceleratedCompositingActive() and will fail there
    // if necessary.
    m_needsWebLayerForVideo = frame->contentRenderer()->compositor()->hasAcceleratedCompositing();

    m_webMediaPlayer = createWebMediaPlayer(this, m_url, frame);
    if (m_webMediaPlayer) {
#if ENABLE(WEB_AUDIO)
        // Make sure if we create/re-create the WebMediaPlayer that we update our wrapper.
        m_audioSourceProvider.wrap(m_webMediaPlayer->audioSourceProvider());
#endif

        // Tell WebMediaPlayer about the poster image URL.
        m_webMediaPlayer->setPoster(poster);

        // Tell WebMediaPlayer about any connected CDM (may be null).
        m_webMediaPlayer->setContentDecryptionModule(m_cdm);

        WebMediaPlayer::CORSMode corsMode = static_cast<WebMediaPlayer::CORSMode>(m_client->mediaPlayerCORSMode());
        m_webMediaPlayer->load(m_loadType, m_url, corsMode);
    }
}

void WebMediaPlayerClientImpl::play()
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->play();
}

void WebMediaPlayerClientImpl::pause()
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->pause();
}

void WebMediaPlayerClientImpl::showFullscreenOverlay()
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->enterFullscreen();
}

void WebMediaPlayerClientImpl::hideFullscreenOverlay()
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->exitFullscreen();
}

bool WebMediaPlayerClientImpl::canShowFullscreenOverlay() const
{
    return m_webMediaPlayer && m_webMediaPlayer->canEnterFullscreen();
}

MediaPlayer::MediaKeyException WebMediaPlayerClientImpl::generateKeyRequest(const String& keySystem, const unsigned char* initData, unsigned initDataLength)
{
    if (!m_webMediaPlayer)
        return MediaPlayer::InvalidPlayerState;

    WebMediaPlayer::MediaKeyException result = m_webMediaPlayer->generateKeyRequest(keySystem, initData, initDataLength);
    return static_cast<MediaPlayer::MediaKeyException>(result);
}

MediaPlayer::MediaKeyException WebMediaPlayerClientImpl::addKey(const String& keySystem, const unsigned char* key, unsigned keyLength, const unsigned char* initData, unsigned initDataLength, const String& sessionId)
{
    if (!m_webMediaPlayer)
        return MediaPlayer::InvalidPlayerState;

    WebMediaPlayer::MediaKeyException result = m_webMediaPlayer->addKey(keySystem, key, keyLength, initData, initDataLength, sessionId);
    return static_cast<MediaPlayer::MediaKeyException>(result);
}

MediaPlayer::MediaKeyException WebMediaPlayerClientImpl::cancelKeyRequest(const String& keySystem, const String& sessionId)
{
    if (!m_webMediaPlayer)
        return MediaPlayer::InvalidPlayerState;

    WebMediaPlayer::MediaKeyException result = m_webMediaPlayer->cancelKeyRequest(keySystem, sessionId);
    return static_cast<MediaPlayer::MediaKeyException>(result);
}

void WebMediaPlayerClientImpl::setContentDecryptionModule(WebContentDecryptionModule* cdm)
{
    m_cdm = cdm;
    if (m_webMediaPlayer)
        m_webMediaPlayer->setContentDecryptionModule(cdm);
}

void WebMediaPlayerClientImpl::prepareToPlay()
{
    if (m_delayingLoad)
        startDelayedLoad();
}

IntSize WebMediaPlayerClientImpl::naturalSize() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->naturalSize();
    return IntSize();
}

bool WebMediaPlayerClientImpl::hasVideo() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->hasVideo();
    return false;
}

bool WebMediaPlayerClientImpl::hasAudio() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->hasAudio();
    return false;
}

double WebMediaPlayerClientImpl::duration() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->duration();
    return 0.0;
}

double WebMediaPlayerClientImpl::currentTime() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->currentTime();
    return 0.0;
}

void WebMediaPlayerClientImpl::seek(double time)
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->seek(time);
}

bool WebMediaPlayerClientImpl::seeking() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->seeking();
    return false;
}

double WebMediaPlayerClientImpl::rate() const
{
    return m_rate;
}

void WebMediaPlayerClientImpl::setRate(double rate)
{
    m_rate = rate;
    if (m_webMediaPlayer)
        m_webMediaPlayer->setRate(rate);
}

bool WebMediaPlayerClientImpl::paused() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->paused();
    return false;
}

bool WebMediaPlayerClientImpl::supportsSave() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->supportsSave();
    return false;
}

void WebMediaPlayerClientImpl::setVolume(double volume)
{
    m_volume = volume;
    if (m_webMediaPlayer && !m_muted)
        m_webMediaPlayer->setVolume(volume);
}

void WebMediaPlayerClientImpl::setMuted(bool muted)
{
    m_muted = muted;
    if (m_webMediaPlayer)
        m_webMediaPlayer->setVolume(muted ? 0 : m_volume);
}

void WebMediaPlayerClientImpl::setPoster(const KURL& poster)
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->setPoster(WebURL(poster));
}

MediaPlayer::NetworkState WebMediaPlayerClientImpl::networkState() const
{
    if (m_webMediaPlayer)
        return static_cast<MediaPlayer::NetworkState>(m_webMediaPlayer->networkState());
    return MediaPlayer::Empty;
}

MediaPlayer::ReadyState WebMediaPlayerClientImpl::readyState() const
{
    if (m_webMediaPlayer)
        return static_cast<MediaPlayer::ReadyState>(m_webMediaPlayer->readyState());
    return MediaPlayer::HaveNothing;
}

double WebMediaPlayerClientImpl::maxTimeSeekable() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->maxTimeSeekable();
    return 0.0;
}

PassRefPtr<TimeRanges> WebMediaPlayerClientImpl::buffered() const
{
    if (m_webMediaPlayer)
        return TimeRanges::create(m_webMediaPlayer->buffered());
    return TimeRanges::create();
}

bool WebMediaPlayerClientImpl::didLoadingProgress() const
{
    return m_webMediaPlayer && m_webMediaPlayer->didLoadingProgress();
}

void WebMediaPlayerClientImpl::paint(GraphicsContext* context, const IntRect& rect)
{
    // Normally GraphicsContext operations do nothing when painting is disabled.
    // Since we're accessing platformContext() directly we have to manually
    // check.
    if (m_webMediaPlayer && !context->paintingDisabled()) {
        // On Android, video frame is emitted as GL_TEXTURE_EXTERNAL_OES texture. We use a different path to
        // paint the video frame into the context.
#if OS(ANDROID)
        if (m_loadType != WebMediaPlayer::LoadTypeMediaStream) {
            OwnPtr<blink::WebGraphicsContext3DProvider> provider = adoptPtr(blink::Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
            if (!provider)
                return;
            paintOnAndroid(context, provider->context3d(), rect, context->getNormalizedAlpha());
            return;
        }
#endif
        WebCanvas* canvas = context->canvas();
        m_webMediaPlayer->paint(canvas, rect, context->getNormalizedAlpha());
    }
}

bool WebMediaPlayerClientImpl::copyVideoTextureToPlatformTexture(WebGraphicsContext3D* context, Platform3DObject texture, GLint level, GLenum type, GLenum internalFormat, bool premultiplyAlpha, bool flipY)
{
    if (!context || !m_webMediaPlayer)
        return false;
    if (!Extensions3DUtil::canUseCopyTextureCHROMIUM(internalFormat, type, level) || !context->makeContextCurrent())
        return false;

    return m_webMediaPlayer->copyVideoTextureToPlatformTexture(context, texture, level, internalFormat, type, premultiplyAlpha, flipY);
}

void WebMediaPlayerClientImpl::setPreload(MediaPlayer::Preload preload)
{
    m_preload = preload;

    if (m_webMediaPlayer)
        m_webMediaPlayer->setPreload(static_cast<WebMediaPlayer::Preload>(preload));

    if (m_delayingLoad && m_preload != MediaPlayer::None)
        startDelayedLoad();
}

bool WebMediaPlayerClientImpl::hasSingleSecurityOrigin() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->hasSingleSecurityOrigin();
    return false;
}

bool WebMediaPlayerClientImpl::didPassCORSAccessCheck() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->didPassCORSAccessCheck();
    return false;
}

double WebMediaPlayerClientImpl::mediaTimeForTimeValue(double timeValue) const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->mediaTimeForTimeValue(timeValue);
    return timeValue;
}

unsigned WebMediaPlayerClientImpl::decodedFrameCount() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->decodedFrameCount();
    return 0;
}

unsigned WebMediaPlayerClientImpl::droppedFrameCount() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->droppedFrameCount();
    return 0;
}

unsigned WebMediaPlayerClientImpl::corruptedFrameCount() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->corruptedFrameCount();
    return 0;
}

unsigned WebMediaPlayerClientImpl::audioDecodedByteCount() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->audioDecodedByteCount();
    return 0;
}

unsigned WebMediaPlayerClientImpl::videoDecodedByteCount() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->videoDecodedByteCount();
    return 0;
}

#if ENABLE(WEB_AUDIO)
AudioSourceProvider* WebMediaPlayerClientImpl::audioSourceProvider()
{
    return &m_audioSourceProvider;
}
#endif

bool WebMediaPlayerClientImpl::needsWebLayerForVideo() const
{
    return m_needsWebLayerForVideo;
}

PassOwnPtr<MediaPlayer> WebMediaPlayerClientImpl::create(MediaPlayerClient* client)
{
    return adoptPtr(new WebMediaPlayerClientImpl(client));
}

#if OS(ANDROID)
void WebMediaPlayerClientImpl::paintOnAndroid(WebCore::GraphicsContext* context, WebGraphicsContext3D* context3D, const IntRect& rect, uint8_t alpha)
{
    if (!context || !context3D || !m_webMediaPlayer || context->paintingDisabled())
        return;

    if (!context3D->makeContextCurrent())
        return;

    // Copy video texture into a RGBA texture based bitmap first as video texture on Android is GL_TEXTURE_EXTERNAL_OES
    // which is not supported by Skia yet. The bitmap's size needs to be the same as the video and use naturalSize() here.
    // Check if we could reuse existing texture based bitmap.
    // Otherwise, release existing texture based bitmap and allocate a new one based on video size.
    OwnPtr<blink::WebGraphicsContext3DProvider> provider = adoptPtr(blink::Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    if (!provider)
        return;
    if (!ensureTextureBackedSkBitmap(provider->grContext(), m_bitmap, naturalSize(), kTopLeft_GrSurfaceOrigin, kSkia8888_GrPixelConfig))
        return;

    // Copy video texture to bitmap texture.
    WebCanvas* canvas = context->canvas();
    unsigned textureId = static_cast<unsigned>((m_bitmap.getTexture())->getTextureHandle());
    if (!m_webMediaPlayer->copyVideoTextureToPlatformTexture(context3D, textureId, 0, GL_RGBA, GL_UNSIGNED_BYTE, true, false))
        return;

    // Draw the texture based bitmap onto the Canvas. If the canvas is hardware based, this will do a GPU-GPU texture copy. If the canvas is software based,
    // the texture based bitmap will be readbacked to system memory then draw onto the canvas.
    SkRect dest;
    dest.set(rect.x(), rect.y(), rect.x() + rect.width(), rect.y() + rect.height());
    SkPaint paint;
    paint.setAlpha(alpha);
    // It is not necessary to pass the dest into the drawBitmap call since all the context have been set up before calling paintCurrentFrameInContext.
    canvas->drawBitmapRect(m_bitmap, NULL, dest, &paint);
}
#endif

void WebMediaPlayerClientImpl::startDelayedLoad()
{
    ASSERT(m_delayingLoad);
    ASSERT(!m_webMediaPlayer);

    m_delayingLoad = false;

    loadInternal();
}

WebMediaPlayerClientImpl::WebMediaPlayerClientImpl(MediaPlayerClient* client)
    : m_client(client)
    , m_delayingLoad(false)
    , m_preload(MediaPlayer::Auto)
    , m_needsWebLayerForVideo(false)
    , m_volume(1.0)
    , m_muted(false)
    , m_rate(1.0)
    , m_cdm(0)
    , m_loadType(WebMediaPlayer::LoadTypeURL)
{
    ASSERT(m_client);
}

#if ENABLE(WEB_AUDIO)
void WebMediaPlayerClientImpl::AudioSourceProviderImpl::wrap(WebAudioSourceProvider* provider)
{
    MutexLocker locker(provideInputLock);

    if (m_webAudioSourceProvider && provider != m_webAudioSourceProvider)
        m_webAudioSourceProvider->setClient(0);

    m_webAudioSourceProvider = provider;
    if (m_webAudioSourceProvider)
        m_webAudioSourceProvider->setClient(m_client.get());
}

void WebMediaPlayerClientImpl::AudioSourceProviderImpl::setClient(AudioSourceProviderClient* client)
{
    MutexLocker locker(provideInputLock);

    if (client)
        m_client = adoptPtr(new WebMediaPlayerClientImpl::AudioClientImpl(client));
    else
        m_client.clear();

    if (m_webAudioSourceProvider)
        m_webAudioSourceProvider->setClient(m_client.get());
}

void WebMediaPlayerClientImpl::AudioSourceProviderImpl::provideInput(AudioBus* bus, size_t framesToProcess)
{
    ASSERT(bus);
    if (!bus)
        return;

    MutexTryLocker tryLocker(provideInputLock);
    if (!tryLocker.locked() || !m_webAudioSourceProvider || !m_client.get()) {
        bus->zero();
        return;
    }

    // Wrap the AudioBus channel data using WebVector.
    size_t n = bus->numberOfChannels();
    WebVector<float*> webAudioData(n);
    for (size_t i = 0; i < n; ++i)
        webAudioData[i] = bus->channel(i)->mutableData();

    m_webAudioSourceProvider->provideInput(webAudioData, framesToProcess);
}

void WebMediaPlayerClientImpl::AudioClientImpl::setFormat(size_t numberOfChannels, float sampleRate)
{
    if (m_client)
        m_client->setFormat(numberOfChannels, sampleRate);
}

#endif

} // namespace blink
