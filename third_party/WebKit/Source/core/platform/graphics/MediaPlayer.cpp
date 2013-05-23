/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include "core/platform/graphics/MediaPlayer.h"

#include "core/dom/Document.h"
#include "core/html/TimeRanges.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Settings.h"
#include "core/platform/ContentType.h"
#include "core/platform/Logging.h"
#include "core/platform/MIMETypeFromURL.h"
#include "core/platform/MIMETypeRegistry.h"
#include "core/platform/graphics/IntRect.h"
#include "core/platform/graphics/MediaPlayerPrivate.h"
#include "modules/mediasource/WebKitMediaSource.h"
#include <wtf/text/CString.h>

#include "core/platform/graphics/InbandTextTrackPrivate.h"

#include "core/platform/graphics/chromium/MediaPlayerPrivateChromium.h"

namespace WebCore {

// engine support

struct MediaPlayerFactory {
    WTF_MAKE_NONCOPYABLE(MediaPlayerFactory); WTF_MAKE_FAST_ALLOCATED;
public:
    MediaPlayerFactory(CreateMediaEnginePlayer constructor, MediaEngineSupportsType supportsTypeAndCodecs)
        : constructor(constructor)
        , supportsTypeAndCodecs(supportsTypeAndCodecs)
    {
    }

    CreateMediaEnginePlayer constructor;
    MediaEngineSupportsType supportsTypeAndCodecs;
};

static void addMediaEngine(CreateMediaEnginePlayer, MediaEngineSupportsType);

// gInstalledEngine should not be accessed directly; call installedMediaEngine() instead.
static MediaPlayerFactory* gInstalledEngine = 0;

static MediaPlayerFactory* installedMediaEngine()
{
    static bool enginesQueried = false;
    if (!enginesQueried) {
        enginesQueried = true;
        MediaPlayerPrivate::registerMediaEngine(addMediaEngine);
    }

    return gInstalledEngine;
}

static void addMediaEngine(CreateMediaEnginePlayer constructor, MediaEngineSupportsType supportsType)
{
    ASSERT(constructor);
    ASSERT(supportsType);

    gInstalledEngine = new MediaPlayerFactory(constructor, supportsType);
}

static const AtomicString& applicationOctetStream()
{
    DEFINE_STATIC_LOCAL(const AtomicString, applicationOctetStream, ("application/octet-stream", AtomicString::ConstructFromLiteral));
    return applicationOctetStream;
}

static const AtomicString& textPlain()
{
    DEFINE_STATIC_LOCAL(const AtomicString, textPlain, ("text/plain", AtomicString::ConstructFromLiteral));
    return textPlain;
}

static const AtomicString& codecs()
{
    DEFINE_STATIC_LOCAL(const AtomicString, codecs, ("codecs", AtomicString::ConstructFromLiteral));
    return codecs;
}

static MediaPlayerFactory* bestMediaEngineForTypeAndCodecs(const String& type, const String& codecs, const String& keySystem, const KURL& url)
{
    if (type.isEmpty())
        return 0;

    MediaPlayerFactory* engine = installedMediaEngine();
    if (!engine)
        return 0;

    // 4.8.10.3 MIME types - In the absence of a specification to the contrary, the MIME type "application/octet-stream" 
    // when used with parameters, e.g. "application/octet-stream;codecs=theora", is a type that the user agent knows 
    // it cannot render.
    if (type == applicationOctetStream()) {
        if (!codecs.isEmpty())
            return 0;
    }

    MediaPlayer::SupportsType engineSupport = engine->supportsTypeAndCodecs(type, codecs, keySystem, url);
    if (engineSupport > MediaPlayer::IsNotSupported)
        return engine;

    return 0;
}

// media player

MediaPlayer::MediaPlayer(MediaPlayerClient* client)
    : m_mediaPlayerClient(client)
    , m_currentMediaEngine(0)
    , m_frameView(0)
    , m_preload(Auto)
    , m_visible(false)
    , m_rate(1.0f)
    , m_volume(1.0f)
    , m_muted(false)
    , m_contentMIMETypeWasInferredFromExtension(false)
{
}

MediaPlayer::~MediaPlayer()
{
    m_mediaPlayerClient = 0;
}

bool MediaPlayer::load(const KURL& url, const ContentType& contentType, const String& keySystem)
{
    m_contentMIMEType = contentType.type().lower();
    m_contentTypeCodecs = contentType.parameter(codecs());
    m_url = url;
    m_keySystem = keySystem.lower();
    m_contentMIMETypeWasInferredFromExtension = false;
    m_mediaSource = 0;

    // If the MIME type is missing or is not meaningful, try to figure it out from the URL.
    if (m_contentMIMEType.isEmpty() || m_contentMIMEType == applicationOctetStream() || m_contentMIMEType == textPlain()) {
        if (m_url.protocolIsData())
            m_contentMIMEType = mimeTypeFromDataURL(m_url.string());
        else {
            String lastPathComponent = url.lastPathComponent();
            size_t pos = lastPathComponent.reverseFind('.');
            if (pos != notFound) {
                String extension = lastPathComponent.substring(pos + 1);
                String mediaType = MIMETypeRegistry::getMediaMIMETypeForExtension(extension);
                if (!mediaType.isEmpty()) {
                    m_contentMIMEType = mediaType;
                    m_contentMIMETypeWasInferredFromExtension = true;
                }
            }
        }
    }

    loadWithMediaEngine();
    return m_currentMediaEngine;
}

bool MediaPlayer::load(const KURL& url, PassRefPtr<WebKitMediaSource> mediaSource)
{
    m_mediaSource = mediaSource;
    m_contentMIMEType = "";
    m_contentTypeCodecs = "";
    m_url = url;
    m_keySystem = "";
    m_contentMIMETypeWasInferredFromExtension = false;
    loadWithMediaEngine();
    return m_currentMediaEngine;
}

void MediaPlayer::loadWithMediaEngine()
{
    MediaPlayerFactory* engine = 0;

    if (!m_contentMIMEType.isEmpty())
        engine = bestMediaEngineForTypeAndCodecs(m_contentMIMEType, m_contentTypeCodecs, m_keySystem, m_url);

    // If no MIME type is specified or the type was inferred from the file extension, just use the next engine.
    if (!engine && (m_contentMIMEType.isEmpty() || m_contentMIMETypeWasInferredFromExtension))
        engine = installedMediaEngine();

    // Don't delete and recreate the player unless it comes from a different engine.
    if (!engine) {
        LOG(Media, "MediaPlayer::loadWithMediaEngine - no media engine found for type \"%s\"", m_contentMIMEType.utf8().data());
        m_currentMediaEngine = engine;
        m_private = nullptr;
    } else if (m_currentMediaEngine != engine) {
        m_currentMediaEngine = engine;
        m_private = engine->constructor(this);
        ASSERT(m_private);
        if (m_mediaPlayerClient)
            m_mediaPlayerClient->mediaPlayerEngineUpdated(this);
        m_private->setPreload(m_preload);
    }

    if (m_private) {
        if (m_mediaSource)
            m_private->load(m_url.string(), m_mediaSource);
        else
            m_private->load(m_url.string());
    } else {
        if (m_mediaPlayerClient) {
            m_mediaPlayerClient->mediaPlayerEngineUpdated(this);
            m_mediaPlayerClient->mediaPlayerResourceNotSupported(this);
        }
    }
}

void MediaPlayer::cancelLoad()
{
    if (m_private)
        m_private->cancelLoad();
}    

void MediaPlayer::prepareToPlay()
{
    if (m_private)
        m_private->prepareToPlay();
}

void MediaPlayer::play()
{
    if (m_private)
        m_private->play();
}

void MediaPlayer::pause()
{
    if (m_private)
        m_private->pause();
}

MediaPlayer::MediaKeyException MediaPlayer::generateKeyRequest(const String& keySystem, const unsigned char* initData, unsigned initDataLength)
{
    return m_private ? m_private->generateKeyRequest(keySystem.lower(), initData, initDataLength) : InvalidPlayerState;
}

MediaPlayer::MediaKeyException MediaPlayer::addKey(const String& keySystem, const unsigned char* key, unsigned keyLength, const unsigned char* initData, unsigned initDataLength, const String& sessionId)
{
    return m_private ? m_private->addKey(keySystem.lower(), key, keyLength, initData, initDataLength, sessionId) : InvalidPlayerState;
}

MediaPlayer::MediaKeyException MediaPlayer::cancelKeyRequest(const String& keySystem, const String& sessionId)
{
    return m_private ? m_private->cancelKeyRequest(keySystem.lower(), sessionId) : InvalidPlayerState;
}

double MediaPlayer::duration() const
{
    return m_private ? m_private->duration() : 0;
}

double MediaPlayer::currentTime() const
{
    return m_private ? m_private->currentTime() : 0;
}

void MediaPlayer::seek(double time)
{
    if (m_private)
        m_private->seek(time);
}

bool MediaPlayer::paused() const
{
    return m_private && m_private->paused();
}

bool MediaPlayer::seeking() const
{
    return m_private && m_private->seeking();
}

bool MediaPlayer::supportsFullscreen() const
{
    return m_private && m_private->supportsFullscreen();
}

bool MediaPlayer::supportsSave() const
{
    return m_private && m_private->supportsSave();
}

IntSize MediaPlayer::naturalSize()
{
    return m_private ? m_private->naturalSize() : IntSize(0, 0);
}

bool MediaPlayer::hasVideo() const
{
    return m_private && m_private->hasVideo();
}

bool MediaPlayer::hasAudio() const
{
    return m_private && m_private->hasAudio();
}

bool MediaPlayer::inMediaDocument()
{
    Frame* frame = m_frameView ? m_frameView->frame() : 0;
    Document* document = frame ? frame->document() : 0;

    return document && document->isMediaDocument();
}

PlatformLayer* MediaPlayer::platformLayer() const
{
    return m_private ? m_private->platformLayer() : 0;
}

MediaPlayer::NetworkState MediaPlayer::networkState()
{
    return m_private ? m_private->networkState() : Empty;
}

MediaPlayer::ReadyState MediaPlayer::readyState() const
{
    return m_private ? m_private->readyState() : HaveNothing;
}

double MediaPlayer::volume() const
{
    return m_volume;
}

void MediaPlayer::setVolume(double volume)
{
    m_volume = volume;

    if (m_private && !m_muted)
        m_private->setVolume(volume);
}

bool MediaPlayer::muted() const
{
    return m_muted;
}

void MediaPlayer::setMuted(bool muted)
{
    m_muted = muted;

    if (m_private)
        m_private->setVolume(muted ? 0 : m_volume);
}

double MediaPlayer::rate() const
{
    return m_rate;
}

void MediaPlayer::setRate(double rate)
{
    m_rate = rate;
    if (m_private)
        m_private->setRate(rate);
}

PassRefPtr<TimeRanges> MediaPlayer::buffered()
{
    return m_private ? m_private->buffered() : TimeRanges::create();
}

PassRefPtr<TimeRanges> MediaPlayer::seekable()
{
    double maxSeekable = maxTimeSeekable();
    return maxSeekable ? TimeRanges::create(0, maxSeekable) : TimeRanges::create();
}

double MediaPlayer::maxTimeSeekable()
{
    return m_private ? m_private->maxTimeSeekable() : 0;
}

bool MediaPlayer::didLoadingProgress()
{
    return m_private && m_private->didLoadingProgress();
}

void MediaPlayer::setSize(const IntSize& size)
{
    m_size = size;
    if (m_private)
        m_private->setSize(size);
}

bool MediaPlayer::visible() const
{
    return m_visible;
}

void MediaPlayer::setVisible(bool b)
{
    m_visible = b;
    if (m_private)
        m_private->setVisible(b);
}

MediaPlayer::Preload MediaPlayer::preload() const
{
    return m_preload;
}

void MediaPlayer::setPreload(MediaPlayer::Preload preload)
{
    m_preload = preload;
    if (m_private)
        m_private->setPreload(preload);
}

void MediaPlayer::paint(GraphicsContext* p, const IntRect& r)
{
    if (m_private)
        m_private->paint(p, r);
}

void MediaPlayer::paintCurrentFrameInContext(GraphicsContext* p, const IntRect& r)
{
    if (m_private)
        m_private->paintCurrentFrameInContext(p, r);
}

bool MediaPlayer::copyVideoTextureToPlatformTexture(GraphicsContext3D* context, Platform3DObject texture, GC3Dint level, GC3Denum type, GC3Denum internalFormat, bool premultiplyAlpha, bool flipY)
{
    return m_private && m_private->copyVideoTextureToPlatformTexture(context, texture, level, type, internalFormat, premultiplyAlpha, flipY);
}

MediaPlayer::SupportsType MediaPlayer::supportsType(const ContentType& contentType, const String& keySystem, const KURL& url)
{
    String type = contentType.type().lower();
    // The codecs string is not lower-cased because MP4 values are case sensitive
    // per http://tools.ietf.org/html/rfc4281#page-7.
    String typeCodecs = contentType.parameter(codecs());
    String system = keySystem.lower();

    // 4.8.10.3 MIME types - The canPlayType(type) method must return the empty string if type is a type that the 
    // user agent knows it cannot render or is the type "application/octet-stream"
    if (type == applicationOctetStream())
        return IsNotSupported;

    MediaPlayerFactory* engine = bestMediaEngineForTypeAndCodecs(type, typeCodecs, system, url);
    if (!engine)
        return IsNotSupported;

    return engine->supportsTypeAndCodecs(type, typeCodecs, system, url);
}

bool MediaPlayer::isAvailable()
{
    return !!installedMediaEngine();
} 

#if USE(NATIVE_FULLSCREEN_VIDEO)
void MediaPlayer::enterFullscreen()
{
    if (m_private)
        m_private->enterFullscreen();
}

void MediaPlayer::exitFullscreen()
{
    if (m_private)
        m_private->exitFullscreen();
}
#endif

#if USE(NATIVE_FULLSCREEN_VIDEO)
bool MediaPlayer::canEnterFullscreen() const
{
    return m_private && m_private->canEnterFullscreen();
}
#endif

bool MediaPlayer::supportsAcceleratedRendering() const
{
    return m_private && m_private->supportsAcceleratedRendering();
}

bool MediaPlayer::hasSingleSecurityOrigin() const
{
    return !m_private || m_private->hasSingleSecurityOrigin();
}

bool MediaPlayer::didPassCORSAccessCheck() const
{
    return m_private && m_private->didPassCORSAccessCheck();
}

MediaPlayer::MovieLoadType MediaPlayer::movieLoadType() const
{
    return m_private ? m_private->movieLoadType() : Unknown;
}

double MediaPlayer::mediaTimeForTimeValue(double timeValue) const
{
    return m_private ? m_private->mediaTimeForTimeValue(timeValue) : timeValue;
}

unsigned MediaPlayer::decodedFrameCount() const
{
    return m_private ? m_private->decodedFrameCount() : 0;
}

unsigned MediaPlayer::droppedFrameCount() const
{
    return m_private ? m_private->droppedFrameCount() : 0;
}

unsigned MediaPlayer::audioDecodedByteCount() const
{
    return m_private ? m_private->audioDecodedByteCount() : 0;
}

unsigned MediaPlayer::videoDecodedByteCount() const
{
    return m_private ? m_private->videoDecodedByteCount() : 0;
}

// Client callbacks.
void MediaPlayer::networkStateChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerNetworkStateChanged(this);
}

void MediaPlayer::readyStateChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerReadyStateChanged(this);
}

void MediaPlayer::volumeChanged(double newVolume)
{
    m_volume = newVolume;
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerVolumeChanged(this);
}

void MediaPlayer::muteChanged(bool newMuted)
{
    m_muted = newMuted;
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerMuteChanged(this);
}

void MediaPlayer::timeChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerTimeChanged(this);
}

void MediaPlayer::sizeChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerSizeChanged(this);
}

void MediaPlayer::repaint()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerRepaint(this);
}

void MediaPlayer::durationChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerDurationChanged(this);
}

void MediaPlayer::rateChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerRateChanged(this);
}

void MediaPlayer::playbackStateChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerPlaybackStateChanged(this);
}

#if ENABLE(WEB_AUDIO)
AudioSourceProvider* MediaPlayer::audioSourceProvider()
{
    return m_private ? m_private->audioSourceProvider() : 0;
}
#endif // WEB_AUDIO

void MediaPlayer::keyAdded(const String& keySystem, const String& sessionId)
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerKeyAdded(this, keySystem, sessionId);
}

void MediaPlayer::keyError(const String& keySystem, const String& sessionId, MediaPlayerClient::MediaKeyErrorCode errorCode, unsigned short systemCode)
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerKeyError(this, keySystem, sessionId, errorCode, systemCode);
}

void MediaPlayer::keyMessage(const String& keySystem, const String& sessionId, const unsigned char* message, unsigned messageLength, const KURL& defaultURL)
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerKeyMessage(this, keySystem, sessionId, message, messageLength, defaultURL);
}

bool MediaPlayer::keyNeeded(const String& keySystem, const String& sessionId, const unsigned char* initData, unsigned initDataLength)
{
    if (m_mediaPlayerClient)
        return m_mediaPlayerClient->mediaPlayerKeyNeeded(this, keySystem, sessionId, initData, initDataLength);
    return false;
}

#if ENABLE(ENCRYPTED_MEDIA_V2)
bool MediaPlayer::keyNeeded(Uint8Array* initData)
{
    if (m_mediaPlayerClient)
        return m_mediaPlayerClient->mediaPlayerKeyNeeded(this, initData);
    return false;
}
#endif

void MediaPlayer::addTextTrack(PassRefPtr<InbandTextTrackPrivate> track)
{
    if (!m_mediaPlayerClient)
        return;

    m_mediaPlayerClient->mediaPlayerDidAddTrack(track);
}

void MediaPlayer::removeTextTrack(PassRefPtr<InbandTextTrackPrivate> track)
{
    if (!m_mediaPlayerClient)
        return;

    m_mediaPlayerClient->mediaPlayerDidRemoveTrack(track);
}
}
