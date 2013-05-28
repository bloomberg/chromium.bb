/*
 * Copyright (C) 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef MediaPlayerPrivate_h
#define MediaPlayerPrivate_h

#include "core/html/TimeRanges.h"
#include "core/platform/graphics/MediaPlayer.h"
#include <wtf/Forward.h>

namespace WebCore {

class IntRect;
class IntSize;

class MediaPlayerPrivateInterface {
    WTF_MAKE_NONCOPYABLE(MediaPlayerPrivateInterface); WTF_MAKE_FAST_ALLOCATED;
public:
    MediaPlayerPrivateInterface() { }
    virtual ~MediaPlayerPrivateInterface() { }

    virtual void load(const String& url) = 0;
    virtual void load(const String& url, PassRefPtr<WebKitMediaSource>) = 0;

    virtual void prepareToPlay() = 0;
    virtual PlatformLayer* platformLayer() const = 0;

    virtual void play() = 0;
    virtual void pause() = 0;

    virtual bool supportsFullscreen() const = 0;
    virtual bool supportsSave() const = 0;
    virtual IntSize naturalSize() const = 0;

    virtual bool hasVideo() const = 0;
    virtual bool hasAudio() const = 0;

    virtual void setVisible(bool) = 0;

    virtual double duration() const = 0;

    virtual double currentTime() const = 0;

    virtual void seek(double) = 0;

    virtual bool seeking() const = 0;

    virtual void setRate(double) = 0;

    virtual bool paused() const = 0;

    virtual void setVolume(double) = 0;

    virtual MediaPlayer::NetworkState networkState() const = 0;
    virtual MediaPlayer::ReadyState readyState() const = 0;

    virtual double maxTimeSeekable() const = 0;
    virtual PassRefPtr<TimeRanges> buffered() const = 0;

    virtual bool didLoadingProgress() const = 0;

    virtual void setSize(const IntSize&) = 0;

    virtual void paint(GraphicsContext*, const IntRect&) = 0;

    virtual void paintCurrentFrameInContext(GraphicsContext*, const IntRect&) = 0;
    virtual bool copyVideoTextureToPlatformTexture(GraphicsContext3D*, Platform3DObject, GC3Dint, GC3Denum, GC3Denum, bool, bool) = 0;

    virtual void setPreload(MediaPlayer::Preload) = 0;

#if USE(NATIVE_FULLSCREEN_VIDEO)
    virtual void enterFullscreen() = 0;
    virtual void exitFullscreen() = 0;
    virtual bool canEnterFullscreen() const = 0;
#endif

    // whether accelerated rendering is supported by the media engine for the current media.
    virtual bool supportsAcceleratedRendering() const = 0;

    virtual bool hasSingleSecurityOrigin() const = 0;

    virtual bool didPassCORSAccessCheck() const = 0;

    virtual MediaPlayer::MovieLoadType movieLoadType() const = 0;

    // Time value in the movie's time scale. It is only necessary to override this if the media
    // engine uses rational numbers to represent media time.
    virtual double mediaTimeForTimeValue(double timeValue) const = 0;

    virtual unsigned decodedFrameCount() const = 0;
    virtual unsigned droppedFrameCount() const = 0;
    virtual unsigned audioDecodedByteCount() const = 0;
    virtual unsigned videoDecodedByteCount() const = 0;

#if ENABLE(WEB_AUDIO)
    virtual AudioSourceProvider* audioSourceProvider() = 0;
#endif

    virtual MediaPlayer::MediaKeyException addKey(const String&, const unsigned char*, unsigned, const unsigned char*, unsigned, const String&) = 0;
    virtual MediaPlayer::MediaKeyException generateKeyRequest(const String&, const unsigned char*, unsigned) = 0;
    virtual MediaPlayer::MediaKeyException cancelKeyRequest(const String&, const String&) = 0;
};

}

#endif
