// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "WebMediaPlayerClientImpl.h"

#if ENABLE(VIDEO)

#include "TemporaryGlue.h"
#include "WebCanvas.h"
#include "WebCString.h"
#include "WebKit.h"
#include "WebKitClient.h"
#include "WebMediaPlayer.h"
#include "WebMimeRegistry.h"
#include "WebRect.h"
#include "WebSize.h"
#include "WebString.h"
#include "WebURL.h"

#include "CString.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLMediaElement.h"
#include "IntSize.h"
#include "KURL.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include <wtf/Assertions.h>

#if WEBKIT_USING_SKIA
#include "PlatformContextSkia.h"
#endif

using namespace WebCore;

namespace WebKit {

static bool s_isEnabled = false;

void WebMediaPlayerClientImpl::setIsEnabled(bool isEnabled)
{
    s_isEnabled = isEnabled;
}

void WebMediaPlayerClientImpl::registerSelf(MediaEngineRegistrar registrar)
{
    if (s_isEnabled) {
        registrar(WebMediaPlayerClientImpl::create,
                  WebMediaPlayerClientImpl::getSupportedTypes,
                  WebMediaPlayerClientImpl::supportsType);
    }
}


// WebMediaPlayerClient --------------------------------------------------------

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

void WebMediaPlayerClientImpl::volumeChanged()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->volumeChanged();
}

void WebMediaPlayerClientImpl::timeChanged()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->timeChanged();
}

void WebMediaPlayerClientImpl::repaint()
{
    ASSERT(m_mediaPlayer);
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

void WebMediaPlayerClientImpl::sawUnsupportedTracks()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->mediaPlayerClient()->mediaPlayerSawUnsupportedTracks(m_mediaPlayer);
}

// MediaPlayerPrivateInterface -------------------------------------------------

void WebMediaPlayerClientImpl::load(const String& url)
{
    Frame* frame = static_cast<HTMLMediaElement*>(
        m_mediaPlayer->mediaPlayerClient())->document()->frame();
    m_webMediaPlayer.set(TemporaryGlue::createWebMediaPlayer(this, frame));
    if (m_webMediaPlayer.get())
        m_webMediaPlayer->load(KURL(ParsedURLString, url));
}

void WebMediaPlayerClientImpl::cancelLoad()
{
    if (m_webMediaPlayer.get())
        m_webMediaPlayer->cancelLoad();
}

void WebMediaPlayerClientImpl::play()
{
    if (m_webMediaPlayer.get())
        m_webMediaPlayer->play();
}

void WebMediaPlayerClientImpl::pause()
{
    if (m_webMediaPlayer.get())
        m_webMediaPlayer->pause();
}

IntSize WebMediaPlayerClientImpl::naturalSize() const
{
    if (m_webMediaPlayer.get())
        return m_webMediaPlayer->naturalSize();
    return IntSize();
}

bool WebMediaPlayerClientImpl::hasVideo() const
{
    if (m_webMediaPlayer.get())
        return m_webMediaPlayer->hasVideo();
    return false;
}

bool WebMediaPlayerClientImpl::hasAudio() const
{
    if (m_webMediaPlayer.get())
        return m_webMediaPlayer->hasAudio();
    return false;
}

void WebMediaPlayerClientImpl::setVisible(bool visible)
{
    if (m_webMediaPlayer.get())
        m_webMediaPlayer->setVisible(visible);
}

float WebMediaPlayerClientImpl::duration() const
{
    if (m_webMediaPlayer.get())
        return m_webMediaPlayer->duration();
    return 0.0f;
}

float WebMediaPlayerClientImpl::currentTime() const
{
    if (m_webMediaPlayer.get())
        return m_webMediaPlayer->currentTime();
    return 0.0f;
}

void WebMediaPlayerClientImpl::seek(float time)
{
    if (m_webMediaPlayer.get())
        m_webMediaPlayer->seek(time);
}

bool WebMediaPlayerClientImpl::seeking() const
{
    if (m_webMediaPlayer.get())
        return m_webMediaPlayer->seeking();
    return false;
}

void WebMediaPlayerClientImpl::setEndTime(float time)
{
    if (m_webMediaPlayer.get())
        m_webMediaPlayer->setEndTime(time);
}

void WebMediaPlayerClientImpl::setRate(float rate)
{
    if (m_webMediaPlayer.get())
        m_webMediaPlayer->setRate(rate);
}

bool WebMediaPlayerClientImpl::paused() const
{
    if (m_webMediaPlayer.get())
        return m_webMediaPlayer->paused();
    return false;
}

bool WebMediaPlayerClientImpl::supportsFullscreen() const
{
    if (m_webMediaPlayer.get())
        return m_webMediaPlayer->supportsFullscreen();
    return false;
}

bool WebMediaPlayerClientImpl::supportsSave() const
{
    if (m_webMediaPlayer.get())
        return m_webMediaPlayer->supportsSave();
    return false;
}

void WebMediaPlayerClientImpl::setVolume(float volume)
{
    if (m_webMediaPlayer.get())
        m_webMediaPlayer->setVolume(volume);
}

MediaPlayer::NetworkState WebMediaPlayerClientImpl::networkState() const
{
    COMPILE_ASSERT(
        int(WebMediaPlayer::Empty) == int(MediaPlayer::Empty), Empty);
    COMPILE_ASSERT(
        int(WebMediaPlayer::Idle) == int(MediaPlayer::Idle), Idle);
    COMPILE_ASSERT(
        int(WebMediaPlayer::Loading) == int(MediaPlayer::Loading), Loading);
    COMPILE_ASSERT(
        int(WebMediaPlayer::Loaded) == int(MediaPlayer::Loaded), Loaded);
    COMPILE_ASSERT(
        int(WebMediaPlayer::FormatError) == int(MediaPlayer::FormatError),
        FormatError);
    COMPILE_ASSERT(
        int(WebMediaPlayer::NetworkError) == int(MediaPlayer::NetworkError),
        NetworkError);
    COMPILE_ASSERT(
        int(WebMediaPlayer::DecodeError) == int(MediaPlayer::DecodeError),
        DecodeError);

    if (m_webMediaPlayer.get())
        return static_cast<MediaPlayer::NetworkState>(m_webMediaPlayer->networkState());
    return MediaPlayer::Empty;
}

MediaPlayer::ReadyState WebMediaPlayerClientImpl::readyState() const
{
    COMPILE_ASSERT(
        int(WebMediaPlayer::HaveNothing) == int(MediaPlayer::HaveNothing),
        HaveNothing);
    COMPILE_ASSERT(
        int(WebMediaPlayer::HaveMetadata) == int(MediaPlayer::HaveMetadata),
        HaveMetadata);
    COMPILE_ASSERT(
        int(WebMediaPlayer::HaveCurrentData) == int(MediaPlayer::HaveCurrentData),
        HaveCurrentData);
    COMPILE_ASSERT(
        int(WebMediaPlayer::HaveFutureData) == int(MediaPlayer::HaveFutureData),
        HaveFutureData);
    COMPILE_ASSERT(
        int(WebMediaPlayer::HaveEnoughData) == int(MediaPlayer::HaveEnoughData),
        HaveEnoughData);

    if (m_webMediaPlayer.get())
        return static_cast<MediaPlayer::ReadyState>(m_webMediaPlayer->readyState());
    return MediaPlayer::HaveNothing;
}

float WebMediaPlayerClientImpl::maxTimeSeekable() const
{
    if (m_webMediaPlayer.get())
        return m_webMediaPlayer->maxTimeSeekable();
    return 0.0f;
}

float WebMediaPlayerClientImpl::maxTimeBuffered() const
{
    if (m_webMediaPlayer.get())
        return m_webMediaPlayer->maxTimeBuffered();
    return 0.0f;
}

int WebMediaPlayerClientImpl::dataRate() const
{
    if (m_webMediaPlayer.get())
        return m_webMediaPlayer->dataRate();
    return 0;
}

bool WebMediaPlayerClientImpl::totalBytesKnown() const
{
    if (m_webMediaPlayer.get())
        return m_webMediaPlayer->totalBytesKnown();
    return false;
}

unsigned WebMediaPlayerClientImpl::totalBytes() const
{
    if (m_webMediaPlayer.get())
        return static_cast<unsigned>(m_webMediaPlayer->totalBytes());
    return 0;
}

unsigned WebMediaPlayerClientImpl::bytesLoaded() const
{
    if (m_webMediaPlayer.get())
        return static_cast<unsigned>(m_webMediaPlayer->bytesLoaded());
    return 0;
}

void WebMediaPlayerClientImpl::setSize(const IntSize& size)
{
    if (m_webMediaPlayer.get())
        m_webMediaPlayer->setSize(WebSize(size.width(), size.height()));
}

void WebMediaPlayerClientImpl::paint(GraphicsContext* context, const IntRect& rect)
{
    // Normally GraphicsContext operations do nothing when painting is disabled.
    // Since we're accessing platformContext() directly we have to manually
    // check.
    if (m_webMediaPlayer.get() && !context->paintingDisabled()) {
#if WEBKIT_USING_SKIA
        m_webMediaPlayer->paint(context->platformContext()->canvas(), rect);
#elif WEBKIT_USING_CG
        m_webMediaPlayer->paint(context->platformContext(), rect);
#else
        notImplemented();
#endif
    }
}

void WebMediaPlayerClientImpl::setAutobuffer(bool autoBuffer)
{
    if (m_webMediaPlayer.get())
        m_webMediaPlayer->setAutoBuffer(autoBuffer);
}

bool WebMediaPlayerClientImpl::hasSingleSecurityOrigin() const
{
    if (m_webMediaPlayer.get())
        return m_webMediaPlayer->hasSingleSecurityOrigin();
    return false;
}

MediaPlayer::MovieLoadType WebMediaPlayerClientImpl::movieLoadType() const
{
    COMPILE_ASSERT(
        int(WebMediaPlayer::Unknown) == int(MediaPlayer::Unknown),
        Unknown);
    COMPILE_ASSERT(
        int(WebMediaPlayer::Download) == int(MediaPlayer::Download),
        Download);
    COMPILE_ASSERT(
        int(WebMediaPlayer::StoredStream) == int(MediaPlayer::StoredStream),
        StoredStream);
    COMPILE_ASSERT(
        int(WebMediaPlayer::LiveStream) == int(MediaPlayer::LiveStream),
        LiveStream);

    if (m_webMediaPlayer.get())
        return static_cast<MediaPlayer::MovieLoadType>(
            m_webMediaPlayer->movieLoadType());
    return MediaPlayer::Unknown;
}

MediaPlayerPrivateInterface* WebMediaPlayerClientImpl::create(MediaPlayer* player)
{
    WebMediaPlayerClientImpl* client = new WebMediaPlayerClientImpl();
    client->m_mediaPlayer = player;
    return client;
}

void WebMediaPlayerClientImpl::getSupportedTypes(HashSet<String>& supportedTypes)
{
    // FIXME: integrate this list with WebMediaPlayerClientImpl::supportsType.
    notImplemented();
}

MediaPlayer::SupportsType WebMediaPlayerClientImpl::supportsType(const String& type,
                                                                 const String& codecs)
{
    WebMimeRegistry::SupportsType supportsType =
        webKitClient()->mimeRegistry()->supportsMediaMIMEType(type, codecs);

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
}

WebMediaPlayerClientImpl::WebMediaPlayerClientImpl()
    : m_mediaPlayer(0)
{
}

} // namespace WebKit

#endif  // ENABLE(VIDEO)
