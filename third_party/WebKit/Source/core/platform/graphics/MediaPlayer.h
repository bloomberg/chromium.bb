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

#ifndef MediaPlayer_h
#define MediaPlayer_h

#include "core/platform/graphics/GraphicsTypes3D.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace WebKit { class WebLayer; }

namespace WebCore {

class AudioSourceProvider;
class GraphicsContext;
class GraphicsContext3D;
class InbandTextTrackPrivate;
class IntRect;
class IntSize;
class KURL;
class MediaPlayer;
class HTMLMediaSource;
class TimeRanges;

class MediaPlayerClient {
public:
    enum CORSMode { Unspecified, Anonymous, UseCredentials };

    virtual ~MediaPlayerClient() { }

    // the network state has changed
    virtual void mediaPlayerNetworkStateChanged() = 0;

    // the ready state has changed
    virtual void mediaPlayerReadyStateChanged() = 0;

    // time has jumped, eg. not as a result of normal playback
    virtual void mediaPlayerTimeChanged() = 0;

    // the media file duration has changed, or is now known
    virtual void mediaPlayerDurationChanged() = 0;

    // the play/pause status changed
    virtual void mediaPlayerPlaybackStateChanged() = 0;

    virtual void mediaPlayerRequestSeek(double) = 0;

// Presentation-related methods
    // a new frame of video is available
    virtual void mediaPlayerRepaint() = 0;

    // the movie size has changed
    virtual void mediaPlayerSizeChanged() = 0;

    virtual void mediaPlayerEngineUpdated() = 0;

    enum MediaKeyErrorCode { UnknownError = 1, ClientError, ServiceError, OutputError, HardwareChangeError, DomainError };
    virtual void mediaPlayerKeyAdded(const String& /* keySystem */, const String& /* sessionId */) = 0;
    virtual void mediaPlayerKeyError(const String& /* keySystem */, const String& /* sessionId */, MediaKeyErrorCode, unsigned short /* systemCode */) = 0;
    virtual void mediaPlayerKeyMessage(const String& /* keySystem */, const String& /* sessionId */, const unsigned char* /* message */, unsigned /* messageLength */, const KURL& /* defaultURL */) = 0;
    virtual bool mediaPlayerKeyNeeded(const String& /* keySystem */, const String& /* sessionId */, const unsigned char* /* initData */, unsigned /* initDataLength */) = 0;

#if ENABLE(ENCRYPTED_MEDIA_V2)
    virtual bool mediaPlayerKeyNeeded(Uint8Array*) = 0;
#endif

    virtual CORSMode mediaPlayerCORSMode() const = 0;

    virtual void mediaPlayerScheduleLayerUpdate() = 0;

    virtual void mediaPlayerDidAddTrack(PassRefPtr<InbandTextTrackPrivate>) = 0;
    virtual void mediaPlayerDidRemoveTrack(PassRefPtr<InbandTextTrackPrivate>) = 0;
};

typedef PassOwnPtr<MediaPlayer> (*CreateMediaEnginePlayer)(MediaPlayerClient*);

class MediaPlayer {
    WTF_MAKE_NONCOPYABLE(MediaPlayer);
public:
    static PassOwnPtr<MediaPlayer> create(MediaPlayerClient*);
    static void setMediaEngineCreateFunction(CreateMediaEnginePlayer);

    static double invalidTime() { return -1.0; }

    MediaPlayer() { }
    virtual ~MediaPlayer() { }

    virtual void load(const String& url) = 0;
    virtual void load(const String& url, PassRefPtr<HTMLMediaSource>) = 0;

    virtual void prepareToPlay() = 0;
    virtual WebKit::WebLayer* platformLayer() const = 0;

    virtual void play() = 0;
    virtual void pause() = 0;

    virtual bool supportsFullscreen() const = 0;
    virtual bool supportsSave() const = 0;
    virtual IntSize naturalSize() const = 0;

    virtual bool hasVideo() const = 0;
    virtual bool hasAudio() const = 0;

    virtual double duration() const = 0;

    virtual double currentTime() const = 0;

    virtual void seek(double) = 0;

    virtual bool seeking() const = 0;

    virtual double rate() const = 0;
    virtual void setRate(double) = 0;

    virtual bool paused() const = 0;

    virtual void setVolume(double) = 0;
    virtual void setMuted(bool) = 0;

    enum NetworkState { Empty, Idle, Loading, Loaded, FormatError, NetworkError, DecodeError };
    virtual NetworkState networkState() const = 0;

    enum ReadyState  { HaveNothing, HaveMetadata, HaveCurrentData, HaveFutureData, HaveEnoughData };
    virtual ReadyState readyState() const = 0;

    virtual double maxTimeSeekable() const = 0;
    virtual PassRefPtr<TimeRanges> buffered() const = 0;

    virtual bool didLoadingProgress() const = 0;

    virtual void paint(GraphicsContext*, const IntRect&) = 0;

    virtual void paintCurrentFrameInContext(GraphicsContext*, const IntRect&) = 0;
    virtual bool copyVideoTextureToPlatformTexture(GraphicsContext3D*, Platform3DObject, GC3Dint, GC3Denum, GC3Denum, bool, bool) = 0;

    enum Preload { None, MetaData, Auto };
    virtual void setPreload(Preload) = 0;

    virtual void showFullscreenOverlay() = 0;
    virtual void hideFullscreenOverlay() = 0;
    virtual bool canShowFullscreenOverlay() const = 0;

    // whether accelerated rendering is supported by the media engine for the current media.
    virtual bool supportsAcceleratedRendering() const = 0;

    virtual bool hasSingleSecurityOrigin() const = 0;

    virtual bool didPassCORSAccessCheck() const = 0;

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

    enum MediaKeyException { NoError, InvalidPlayerState, KeySystemNotSupported };
    virtual MediaKeyException addKey(const String&, const unsigned char*, unsigned, const unsigned char*, unsigned, const String&) = 0;
    virtual MediaKeyException generateKeyRequest(const String&, const unsigned char*, unsigned) = 0;
    virtual MediaKeyException cancelKeyRequest(const String&, const String&) = 0;
};

}

#endif // MediaPlayer_h
