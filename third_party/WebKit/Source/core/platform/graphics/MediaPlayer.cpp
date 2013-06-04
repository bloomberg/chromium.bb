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
#include "core/platform/graphics/InbandTextTrackPrivate.h"
#include "core/platform/graphics/IntRect.h"
#include "core/platform/graphics/MediaPlayerPrivate.h"
#include "modules/mediasource/WebKitMediaSource.h"
#include "wtf/text/WTFString.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMimeRegistry.h"

using WebKit::WebMimeRegistry;

namespace WebCore {

CreateMediaEnginePlayer MediaPlayer::s_createMediaEngineFunction = 0;

void MediaPlayer::setMediaEngineCreateFunction(CreateMediaEnginePlayer createFunction)
{
    ASSERT(createFunction);
    ASSERT(!s_createMediaEngineFunction);
    s_createMediaEngineFunction = createFunction;
}

MediaPlayer::MediaPlayer(MediaPlayerClient* client)
    : m_mediaPlayerClient(client)
    , m_preload(Auto)
    , m_inDestructor(false)
{
    ASSERT(s_createMediaEngineFunction);
    ASSERT(m_mediaPlayerClient);
}

MediaPlayer::~MediaPlayer()
{
    m_inDestructor = true;

    // Explicitly destroyed because its destructor may call back into this.
    m_private.clear();
}

bool MediaPlayer::load(const KURL& url, const ContentType& contentType, const String& keySystem)
{
    DEFINE_STATIC_LOCAL(const String, codecs, (ASCIILiteral("codecs")));

    String contentMIMEType = contentType.type().lower();
    String contentTypeCodecs = contentType.parameter(codecs);
    bool contentMIMETypeWasInferredFromExtension = false;

    // If the MIME type is missing or is not meaningful, try to figure it out from the URL.
    if (contentMIMEType.isEmpty() || contentMIMEType == "application/octet-stream" || contentMIMEType == "text/plain") {
        if (url.protocolIsData())
            contentMIMEType = mimeTypeFromDataURL(url.string());
        else {
            String lastPathComponent = url.lastPathComponent();
            size_t pos = lastPathComponent.reverseFind('.');
            if (pos != notFound) {
                String extension = lastPathComponent.substring(pos + 1);
                String mediaType = MIMETypeRegistry::getMediaMIMETypeForExtension(extension);
                if (!mediaType.isEmpty()) {
                    contentMIMEType = mediaType;
                    contentMIMETypeWasInferredFromExtension = true;
                }
            }
        }
    }

    bool canCreateMediaEngine = false;

    // 4.8.10.3 MIME types - In the absence of a specification to the contrary, the MIME type "application/octet-stream"
    // when used with parameters, e.g. "application/octet-stream;codecs=theora", is a type that the user agent knows
    // it cannot render.
    if (!contentMIMEType.isEmpty() && (contentMIMEType != "application/octet-stream" || contentTypeCodecs.isEmpty())) {
        WebMimeRegistry::SupportsType supported = WebKit::Platform::current()->mimeRegistry()->supportsMediaMIMEType(contentMIMEType, contentTypeCodecs, keySystem.lower());
        canCreateMediaEngine = (supported > WebMimeRegistry::IsNotSupported);
    }

    // If no MIME type is specified or the type was inferred from the file extension, always create the engine.
    if (contentMIMEType.isEmpty() || contentMIMETypeWasInferredFromExtension)
        canCreateMediaEngine = true;

    // Don't delete and recreate the player if it's already present.
    if (!canCreateMediaEngine) {
        LOG(Media, "MediaPlayer::load - no media engine found for type \"%s\"", contentMIMEType.utf8().data());
        m_private = nullptr;
        m_mediaPlayerClient->mediaPlayerEngineUpdated();
        m_mediaPlayerClient->mediaPlayerResourceNotSupported();
    } else if (!m_private)
        initializeMediaPlayerPrivate();

    if (m_private)
        m_private->load(url.string());

    return m_private;
}

bool MediaPlayer::load(const KURL& url, PassRefPtr<WebKitMediaSource> mediaSource)
{
    if (!m_private)
        initializeMediaPlayerPrivate();
    m_private->load(url.string(), mediaSource);
    return true;
}

void MediaPlayer::initializeMediaPlayerPrivate()
{
    m_private = s_createMediaEngineFunction(this);
    ASSERT(m_private);
    m_mediaPlayerClient->mediaPlayerEngineUpdated();
    m_private->setPreload(m_preload);
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

void MediaPlayer::setVolume(double volume)
{
    if (m_private)
        m_private->setVolume(volume);
}

void MediaPlayer::setMuted(bool muted)
{
    if (m_private)
        m_private->setMuted(muted);
}

double MediaPlayer::rate() const
{
    return m_private ? m_private->rate() : 1.0;
}

void MediaPlayer::setRate(double rate)
{
    if (m_private)
        m_private->setRate(rate);
}

PassRefPtr<TimeRanges> MediaPlayer::buffered()
{
    return m_private ? m_private->buffered() : TimeRanges::create();
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
    if (m_private)
        m_private->setSize(size);
}

void MediaPlayer::setVisible(bool b)
{
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

bool MediaPlayer::canEnterFullscreen() const
{
    return m_private && m_private->canEnterFullscreen();
}
#endif // USE(NATIVE_FULLSCREEN_VIDEO)

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
void MediaPlayer::setNeedsStyleRecalc()
{
    // FIXME: This m_inDestructor check retains legacy behavior, but it's probably unnecessary.
    if (m_inDestructor)
        return;

    m_mediaPlayerClient->mediaPlayerNeedsStyleRecalc();
}

#if ENABLE(WEB_AUDIO)
AudioSourceProvider* MediaPlayer::audioSourceProvider()
{
    return m_private ? m_private->audioSourceProvider() : 0;
}
#endif // WEB_AUDIO

#if ENABLE(ENCRYPTED_MEDIA_V2)
bool MediaPlayer::keyNeeded(Uint8Array* initData)
{
    if (m_mediaPlayerClient)
        return m_mediaPlayerClient->mediaPlayerKeyNeeded(initData);
    return false;
}
#endif

} // namespace WebCore
