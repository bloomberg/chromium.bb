// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "WebMediaPlayerClientImpl.h"

#include "InbandTextTrackPrivateImpl.h"
#include "WebAudioSourceProvider.h"
#include "WebDocument.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebHelperPluginImpl.h"
#include "WebInbandTextTrack.h"
#include "WebMediaPlayer.h"
#include "WebMediaSourceImpl.h"
#include "WebViewImpl.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/TimeRanges.h"
#include "core/page/Frame.h"
#include "core/platform/KURL.h"
#include "core/platform/NotImplemented.h"
#include "core/platform/audio/AudioBus.h"
#include "core/platform/audio/AudioSourceProvider.h"
#include "core/platform/audio/AudioSourceProviderClient.h"
#include "core/platform/chromium/support/GraphicsContext3DPrivate.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/IntSize.h"
#include "core/platform/graphics/MediaPlayer.h"
#include "core/platform/graphics/chromium/GraphicsLayerChromium.h"
#include "core/platform/graphics/skia/PlatformContextSkia.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderView.h"
#include <public/Platform.h>
#include <public/WebCanvas.h>
#include <public/WebCompositorSupport.h>
#include <public/WebCString.h>
#include <public/WebMimeRegistry.h>
#include <public/WebRect.h>
#include <public/WebSize.h>
#include <public/WebString.h>
#include <public/WebURL.h>

#if defined(OS_ANDROID)
#include "GrContext.h"
#include "GrTypes.h"
#include "SkCanvas.h"
#include "SkGrPixelRef.h"
#include "core/platform/graphics/gpu/SharedGraphicsContext3D.h"
#endif


#include <wtf/Assertions.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebKit {

static PassOwnPtr<WebMediaPlayer> createWebMediaPlayer(WebMediaPlayerClient* client, const WebURL& url, Frame* frame)
{
    WebFrameImpl* webFrame = WebFrameImpl::fromFrame(frame);

    if (!webFrame->client())
        return nullptr;
    return adoptPtr(webFrame->client()->createMediaPlayer(webFrame, url, client));
}

bool WebMediaPlayerClientImpl::m_isEnabled = false;

bool WebMediaPlayerClientImpl::isEnabled()
{
    return m_isEnabled;
}

void WebMediaPlayerClientImpl::setIsEnabled(bool isEnabled)
{
    m_isEnabled = isEnabled;
}

void WebMediaPlayerClientImpl::registerSelf(MediaEngineRegistrar registrar)
{
    if (m_isEnabled) {
        registrar(WebMediaPlayerClientImpl::create, WebMediaPlayerClientImpl::supportsType);
    }
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

    // FIXME(ddorwin): Uncomment the ASSERT and remove the closeHelperPlugin()
    // call after fixing http://crbug.com/173755.
    // Ensure the m_webMediaPlayer destroyed any WebHelperPlugin used.
    // ASSERT(!m_helperPlugin);
    if (m_helperPlugin)
        closeHelperPlugin();
}

void WebMediaPlayerClientImpl::networkStateChanged()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->networkStateChanged();
}

void WebMediaPlayerClientImpl::readyStateChanged()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->readyStateChanged();
}

void WebMediaPlayerClientImpl::volumeChanged(double newVolume)
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->volumeChanged(newVolume);
}

void WebMediaPlayerClientImpl::muteChanged(bool newMute)
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->muteChanged(newMute);
}

void WebMediaPlayerClientImpl::timeChanged()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->timeChanged();
}

void WebMediaPlayerClientImpl::repaint()
{
    ASSERT(m_mediaPlayer);
    if (m_videoLayer)
        m_videoLayer->invalidate();
    m_mediaPlayer->repaint();
}

void WebMediaPlayerClientImpl::durationChanged()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->durationChanged();
}

void WebMediaPlayerClientImpl::rateChanged()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->rateChanged();
}

void WebMediaPlayerClientImpl::sizeChanged()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->sizeChanged();
}

void WebMediaPlayerClientImpl::setOpaque(bool opaque)
{
    m_opaque = opaque;
    if (m_videoLayer)
        m_videoLayer->setOpaque(m_opaque);
}

void WebMediaPlayerClientImpl::sawUnsupportedTracks()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->mediaPlayerClient()->mediaPlayerSawUnsupportedTracks(m_mediaPlayer);
}

double WebMediaPlayerClientImpl::volume() const
{
    if (m_mediaPlayer)
        return m_mediaPlayer->volume();
    return 0.0;
}

void WebMediaPlayerClientImpl::playbackStateChanged()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->playbackStateChanged();
}

WebMediaPlayer::Preload WebMediaPlayerClientImpl::preload() const
{
    if (m_mediaPlayer)
        return static_cast<WebMediaPlayer::Preload>(m_mediaPlayer->preload());
    return static_cast<WebMediaPlayer::Preload>(m_preload);
}

void WebMediaPlayerClientImpl::keyAdded(const WebString& keySystem, const WebString& sessionId)
{
#if ENABLE(ENCRYPTED_MEDIA)
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->keyAdded(keySystem, sessionId);
#else
    UNUSED_PARAM(keySystem);
    UNUSED_PARAM(sessionId);
#endif
}

void WebMediaPlayerClientImpl::keyError(const WebString& keySystem, const WebString& sessionId, MediaKeyErrorCode errorCode, unsigned short systemCode)
{
#if ENABLE(ENCRYPTED_MEDIA)
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->keyError(keySystem, sessionId, static_cast<MediaPlayerClient::MediaKeyErrorCode>(errorCode), systemCode);
#else
    UNUSED_PARAM(keySystem);
    UNUSED_PARAM(sessionId);
    UNUSED_PARAM(errorCode);
    UNUSED_PARAM(systemCode);
#endif
}

void WebMediaPlayerClientImpl::keyMessage(const WebString& keySystem, const WebString& sessionId, const unsigned char* message, unsigned messageLength, const WebURL& defaultURL)
{
#if ENABLE(ENCRYPTED_MEDIA)
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->keyMessage(keySystem, sessionId, message, messageLength, defaultURL);
#else
    UNUSED_PARAM(keySystem);
    UNUSED_PARAM(sessionId);
    UNUSED_PARAM(message);
    UNUSED_PARAM(messageLength);
    UNUSED_PARAM(defaultURL);
#endif
}

void WebMediaPlayerClientImpl::keyNeeded(const WebString& keySystem, const WebString& sessionId, const unsigned char* initData, unsigned initDataLength)
{
#if ENABLE(ENCRYPTED_MEDIA)
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->keyNeeded(keySystem, sessionId, initData, initDataLength);
#else
    UNUSED_PARAM(keySystem);
    UNUSED_PARAM(sessionId);
    UNUSED_PARAM(initData);
    UNUSED_PARAM(initDataLength);
#endif
}

WebPlugin* WebMediaPlayerClientImpl::createHelperPlugin(const WebString& pluginType, WebFrame* frame)
{
    ASSERT(!m_helperPlugin);

    WebViewImpl* webView = static_cast<WebViewImpl*>(frame->view());
    m_helperPlugin = webView->createHelperPlugin(pluginType, frame->document());
    if (!m_helperPlugin)
        return 0;

    WebPlugin* plugin = m_helperPlugin->getPlugin();
    if (!plugin) {
        // There is no need to keep the helper plugin around and the caller
        // should not be expected to call close after a failure (null pointer).
        closeHelperPlugin();
        return 0;
    }

    return plugin;
}

void WebMediaPlayerClientImpl::closeHelperPlugin()
{
    ASSERT(m_helperPlugin);
    m_helperPlugin->closeHelperPlugin();
    m_helperPlugin = 0;
}

void WebMediaPlayerClientImpl::setWebLayer(WebLayer* layer)
{
    if (layer == m_videoLayer)
        return;

    // If either of the layers is null we need to enable or disable compositing. This is done by triggering a style recalc.
    if (!m_videoLayer || !layer) {
        HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_mediaPlayer->mediaPlayerClient());
        if (element)
            element->setNeedsStyleRecalc(WebCore::SyntheticStyleChange);
    }

    if (m_videoLayer)
        GraphicsLayerChromium::unregisterContentsLayer(m_videoLayer);
    m_videoLayer = layer;
    if (m_videoLayer) {
        m_videoLayer->setOpaque(m_opaque);
        GraphicsLayerChromium::registerContentsLayer(m_videoLayer);
    }
}

void WebMediaPlayerClientImpl::addTextTrack(WebInbandTextTrack* textTrack)
{
    m_mediaPlayer->addTextTrack(adoptRef(new InbandTextTrackPrivateImpl(textTrack)));
}

void WebMediaPlayerClientImpl::removeTextTrack(WebInbandTextTrack* textTrack)
{
    // The following static_cast is safe, because we created the object with the textTrack
    // that was passed to addTextTrack.  (The object from which we are downcasting includes
    // WebInbandTextTrack as one of the intefaces from which inherits.)
    m_mediaPlayer->removeTextTrack(static_cast<InbandTextTrackPrivateImpl*>(textTrack->client()));
}

// MediaPlayerPrivateInterface -------------------------------------------------

void WebMediaPlayerClientImpl::load(const String& url)
{
    m_url = KURL(ParsedURLString, url);
    m_mediaSource = 0;
    loadRequested();
}

void WebMediaPlayerClientImpl::load(const String& url, PassRefPtr<WebCore::MediaSource> mediaSource)
{
    m_url = KURL(ParsedURLString, url);
    m_mediaSource = mediaSource;
    loadRequested();
}

void WebMediaPlayerClientImpl::loadRequested()
{
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

    Frame* frame = static_cast<HTMLMediaElement*>(m_mediaPlayer->mediaPlayerClient())->document()->frame();

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

        WebMediaPlayer::CORSMode corsMode = static_cast<WebMediaPlayer::CORSMode>(m_mediaPlayer->mediaPlayerClient()->mediaPlayerCORSMode());
        if (m_mediaSource) {
            m_webMediaPlayer->load(m_url, new WebMediaSourceImpl(m_mediaSource), corsMode);
            return;
        }
        m_webMediaPlayer->load(m_url, corsMode);
    }
}

void WebMediaPlayerClientImpl::cancelLoad()
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->cancelLoad();
}

WebLayer* WebMediaPlayerClientImpl::platformLayer() const
{
    return m_videoLayer;
}

PlatformMedia WebMediaPlayerClientImpl::platformMedia() const
{
    PlatformMedia pm;
    pm.type = PlatformMedia::ChromiumMediaPlayerType;
    pm.media.chromiumMediaPlayer = const_cast<WebMediaPlayerClientImpl*>(this);
    return pm;
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

#if USE(NATIVE_FULLSCREEN_VIDEO)
void WebMediaPlayerClientImpl::enterFullscreen()
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->enterFullscreen();
}

void WebMediaPlayerClientImpl::exitFullscreen()
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->exitFullscreen();
}

bool WebMediaPlayerClientImpl::canEnterFullscreen() const
{
    return m_webMediaPlayer && m_webMediaPlayer->canEnterFullscreen();
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
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
#endif

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

void WebMediaPlayerClientImpl::setVisible(bool visible)
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->setVisible(visible);
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

void WebMediaPlayerClientImpl::setRate(double rate)
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->setRate(rate);
}

bool WebMediaPlayerClientImpl::paused() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->paused();
    return false;
}

bool WebMediaPlayerClientImpl::supportsFullscreen() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->supportsFullscreen();
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
    if (m_webMediaPlayer)
        m_webMediaPlayer->setVolume(volume);
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
    if (m_webMediaPlayer) {
        const WebTimeRanges& webRanges = m_webMediaPlayer->buffered();

        // FIXME: Save the time ranges in a member variable and update it when needed.
        RefPtr<TimeRanges> ranges = TimeRanges::create();
        for (size_t i = 0; i < webRanges.size(); ++i)
            ranges->add(webRanges[i].start, webRanges[i].end);
        return ranges.release();
    }
    return TimeRanges::create();
}

int WebMediaPlayerClientImpl::dataRate() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->dataRate();
    return 0;
}

bool WebMediaPlayerClientImpl::totalBytesKnown() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->totalBytesKnown();
    return false;
}

unsigned WebMediaPlayerClientImpl::totalBytes() const
{
    if (m_webMediaPlayer)
        return static_cast<unsigned>(m_webMediaPlayer->totalBytes());
    return 0;
}

bool WebMediaPlayerClientImpl::didLoadingProgress() const
{
    return m_webMediaPlayer && m_webMediaPlayer->didLoadingProgress();
}

void WebMediaPlayerClientImpl::setSize(const IntSize& size)
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->setSize(WebSize(size.width(), size.height()));
}

void WebMediaPlayerClientImpl::paint(GraphicsContext* context, const IntRect& rect)
{
    // If we are using GPU to render video, ignore requests to paint frames into
    // canvas because it will be taken care of by the VideoLayer.
    if (acceleratedRenderingInUse())
        return;
    paintCurrentFrameInContext(context, rect);
}

void WebMediaPlayerClientImpl::paintCurrentFrameInContext(GraphicsContext* context, const IntRect& rect)
{
    // Normally GraphicsContext operations do nothing when painting is disabled.
    // Since we're accessing platformContext() directly we have to manually
    // check.
    if (m_webMediaPlayer && !context->paintingDisabled()) {
        PlatformGraphicsContext* platformContext = context->platformContext();

        // On Android, video frame is emitted as GL_TEXTURE_EXTERNAL_OES texture. We use a different path to
        // paint the video frame into the context.
#if defined(OS_ANDROID)
        RefPtr<GraphicsContext3D> context3D = SharedGraphicsContext3D::get();
        paintOnAndroid(context, context3D.get(), rect, platformContext->getNormalizedAlpha());
#else
        WebCanvas* canvas = platformContext->canvas();
        m_webMediaPlayer->paint(canvas, rect, platformContext->getNormalizedAlpha());
#endif
    }
}

bool WebMediaPlayerClientImpl::copyVideoTextureToPlatformTexture(WebCore::GraphicsContext3D* context, Platform3DObject texture, GC3Dint level, GC3Denum type, GC3Denum internalFormat, bool premultiplyAlpha, bool flipY)
{
    if (!context || !m_webMediaPlayer)
        return false;
    Extensions3D* extensions = context->getExtensions();
    if (!extensions || !extensions->supports("GL_CHROMIUM_copy_texture") || !extensions->supports("GL_CHROMIUM_flipy") || !context->makeContextCurrent())
        return false;
    WebGraphicsContext3D* webGraphicsContext3D = GraphicsContext3DPrivate::extractWebGraphicsContext3D(context);
    return m_webMediaPlayer->copyVideoTextureToPlatformTexture(webGraphicsContext3D, texture, level, internalFormat, premultiplyAlpha, flipY);
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

MediaPlayer::MovieLoadType WebMediaPlayerClientImpl::movieLoadType() const
{
    if (m_webMediaPlayer)
        return static_cast<MediaPlayer::MovieLoadType>(
            m_webMediaPlayer->movieLoadType());
    return MediaPlayer::Unknown;
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

bool WebMediaPlayerClientImpl::supportsAcceleratedRendering() const
{
    return !!m_videoLayer;
}

bool WebMediaPlayerClientImpl::acceleratedRenderingInUse()
{
    return m_videoLayer && !m_videoLayer->isOrphan();
}

PassOwnPtr<MediaPlayerPrivateInterface> WebMediaPlayerClientImpl::create(MediaPlayer* player)
{
    OwnPtr<WebMediaPlayerClientImpl> client = adoptPtr(new WebMediaPlayerClientImpl());
    client->m_mediaPlayer = player;
    return client.release();
}

#if ENABLE(ENCRYPTED_MEDIA)
MediaPlayer::SupportsType WebMediaPlayerClientImpl::supportsType(const String& type,
                                                                 const String& codecs,
                                                                 const String& keySystem,
                                                                 const KURL&)
{
#else
MediaPlayer::SupportsType WebMediaPlayerClientImpl::supportsType(const String& type,
                                                                 const String& codecs,
                                                                 const KURL&)
{
    String keySystem;
#endif
    WebMimeRegistry::SupportsType supportsType = WebKit::Platform::current()->mimeRegistry()->supportsMediaMIMEType(type, codecs, keySystem);

    switch (supportsType) {
    default:
        ASSERT_NOT_REACHED();
    case WebMimeRegistry::IsNotSupported:
        return MediaPlayer::IsNotSupported;
    case WebMimeRegistry::IsSupported:
        return MediaPlayer::IsSupported;
    case WebMimeRegistry::MayBeSupported:
        return MediaPlayer::MayBeSupported;
    }
    return MediaPlayer::IsNotSupported;
}

#if defined(OS_ANDROID)
void WebMediaPlayerClientImpl::paintOnAndroid(WebCore::GraphicsContext* context, WebCore::GraphicsContext3D* context3D, const IntRect& rect, uint8_t alpha)
{
    if (!context || !context3D || !m_webMediaPlayer || context->paintingDisabled())
        return;

    Extensions3D* extensions = context3D->getExtensions();
    if (!extensions || !extensions->supports("GL_CHROMIUM_copy_texture") || !extensions->supports("GL_CHROMIUM_flipy")
        || !context3D->makeContextCurrent())
        return;

    // Copy video texture into a RGBA texture based bitmap first as video texture on Android is GL_TEXTURE_EXTERNAL_OES
    // which is not supported by Skia yet. The bitmap's size needs to be the same as the video.
    int videoWidth = naturalSize().width();
    int videoHeight = naturalSize().height();

    // Check if we could reuse existing texture based bitmap.
    // Otherwise, release existing texture based bitmap and allocate a new one based on video size.
    if (videoWidth != m_bitmap.width() || videoHeight != m_bitmap.height() || !m_texture.get()) {
        GrTextureDesc desc;
        desc.fConfig = kSkia8888_GrPixelConfig;
        desc.fWidth = videoWidth;
        desc.fHeight = videoHeight;
        desc.fOrigin = kTopLeft_GrSurfaceOrigin;
        desc.fFlags = (kRenderTarget_GrTextureFlagBit | kNoStencil_GrTextureFlagBit);
        GrContext* ct = context3D->grContext();
        if (!ct)
            return;
        m_texture.reset(ct->createUncachedTexture(desc, NULL, 0));
        if (!m_texture.get())
            return;
        m_bitmap.setConfig(SkBitmap::kARGB_8888_Config, videoWidth, videoHeight);
        m_bitmap.setPixelRef(new SkGrPixelRef(m_texture))->unref();
    }

    // Copy video texture to bitmap texture.
    WebGraphicsContext3D* webGraphicsContext3D = GraphicsContext3DPrivate::extractWebGraphicsContext3D(context3D);
    PlatformGraphicsContext* platformContext = context->platformContext();
    WebCanvas* canvas = platformContext->canvas();
    unsigned int textureId = static_cast<unsigned int>(m_texture->getTextureHandle());
    if (!m_webMediaPlayer->copyVideoTextureToPlatformTexture(webGraphicsContext3D, textureId, 0, GraphicsContext3D::RGBA, true, false)) { return; }

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

WebMediaPlayerClientImpl::WebMediaPlayerClientImpl()
    : m_mediaPlayer(0)
    , m_delayingLoad(false)
    , m_preload(MediaPlayer::MetaData)
    , m_videoLayer(0)
    , m_opaque(false)
    , m_needsWebLayerForVideo(false)
{
}

#if ENABLE(WEB_AUDIO)
void WebMediaPlayerClientImpl::AudioSourceProviderImpl::wrap(WebAudioSourceProvider* provider)
{
    m_webAudioSourceProvider = provider;
    if (m_webAudioSourceProvider)
        m_webAudioSourceProvider->setClient(m_client.get());
}

void WebMediaPlayerClientImpl::AudioSourceProviderImpl::setClient(AudioSourceProviderClient* client)
{
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

    if (!m_webAudioSourceProvider) {
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

} // namespace WebKit
