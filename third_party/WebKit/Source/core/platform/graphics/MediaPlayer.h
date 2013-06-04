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

#include "core/platform/KURL.h"
#include "core/platform/graphics/GraphicsTypes3D.h"
#include "core/platform/graphics/InbandTextTrackPrivate.h"
#include "core/platform/graphics/IntRect.h"
#include "core/platform/graphics/LayoutRect.h"
#include "core/platform/graphics/PlatformLayer.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class AudioSourceProvider;
class ContentType;
class Document;
class GraphicsContext;
class GraphicsContext3D;
class HostWindow;
class IntRect;
class IntSize;
class MediaPlayer;
class MediaPlayerPrivateInterface;
class TimeRanges;
class WebKitMediaSource;

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

    virtual void mediaPlayerNeedsStyleRecalc() = 0;

    virtual void mediaPlayerDidAddTrack(PassRefPtr<InbandTextTrackPrivate>) = 0;
    virtual void mediaPlayerDidRemoveTrack(PassRefPtr<InbandTextTrackPrivate>) = 0;
};

typedef PassOwnPtr<MediaPlayerPrivateInterface> (*CreateMediaEnginePlayer)(MediaPlayer*);

class MediaPlayer {
    WTF_MAKE_NONCOPYABLE(MediaPlayer); WTF_MAKE_FAST_ALLOCATED;
public:
    static void setMediaEngineCreateFunction(CreateMediaEnginePlayer);

    static PassOwnPtr<MediaPlayer> create(MediaPlayerClient* client)
    {
        return adoptPtr(new MediaPlayer(client));
    }
    virtual ~MediaPlayer();

    bool supportsFullscreen() const;
    bool supportsSave() const;
    PlatformLayer* platformLayer() const;

    IntSize naturalSize();
    bool hasVideo() const;
    bool hasAudio() const;

    void setSize(const IntSize& size);

    void load(const KURL&);
    void load(const KURL&, PassRefPtr<WebKitMediaSource>);

    void setVisible(bool);

    void prepareToPlay();
    void play();
    void pause();

    // Represents synchronous exceptions that can be thrown from the Encrypted Media methods.
    // This is different from the asynchronous MediaKeyError.
    enum MediaKeyException { NoError, InvalidPlayerState, KeySystemNotSupported };

    MediaKeyException generateKeyRequest(const String& keySystem, const unsigned char* initData, unsigned initDataLength);
    MediaKeyException addKey(const String& keySystem, const unsigned char* key, unsigned keyLength, const unsigned char* initData, unsigned initDataLength, const String& sessionId);
    MediaKeyException cancelKeyRequest(const String& keySystem, const String& sessionId);

    bool paused() const;
    bool seeking() const;

    static double invalidTime() { return -1.0;}
    double duration() const;
    double currentTime() const;
    void seek(double time);

    double rate() const;
    void setRate(double);

    PassRefPtr<TimeRanges> buffered();
    double maxTimeSeekable();

    bool didLoadingProgress();

    void setVolume(double);
    void setMuted(bool);

    void paint(GraphicsContext*, const IntRect&);
    void paintCurrentFrameInContext(GraphicsContext*, const IntRect&);

    // copyVideoTextureToPlatformTexture() is used to do the GPU-GPU textures copy without a readback to system memory.
    // The first five parameters denote the corresponding GraphicsContext, destination texture, requested level, requested type and the required internalFormat for destination texture.
    // The last two parameters premultiplyAlpha and flipY denote whether addtional premultiplyAlpha and flip operation are required during the copy.
    // It returns true on success and false on failure.

    // In the GPU-GPU textures copy, the source texture(Video texture) should have valid target, internalFormat and size, etc.
    // The destination texture may need to be resized to to the dimensions of the source texture or re-defined to the required internalFormat.
    // The current restrictions require that format shoud be RGB or RGBA, type should be UNSIGNED_BYTE and level should be 0. It may be lifted in the future.

    // Each platform port can have its own implementation on this function. The default implementation for it is a single "return false" in MediaPlayerPrivate.h.
    // In chromium, the implementation is based on GL_CHROMIUM_copy_texture extension which is documented at
    // http://src.chromium.org/viewvc/chrome/trunk/src/gpu/GLES2/extensions/CHROMIUM/CHROMIUM_copy_texture.txt and implemented at
    // http://src.chromium.org/viewvc/chrome/trunk/src/gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.cc via shaders.
    bool copyVideoTextureToPlatformTexture(GraphicsContext3D*, Platform3DObject texture, GC3Dint level, GC3Denum type, GC3Denum internalFormat, bool premultiplyAlpha, bool flipY);

    enum NetworkState { Empty, Idle, Loading, Loaded, FormatError, NetworkError, DecodeError };
    NetworkState networkState();

    enum ReadyState  { HaveNothing, HaveMetadata, HaveCurrentData, HaveFutureData, HaveEnoughData };
    ReadyState readyState() const;

    enum MovieLoadType { Unknown, Download, StoredStream, LiveStream };
    MovieLoadType movieLoadType() const;

    enum Preload { None, MetaData, Auto };
    Preload preload() const;
    void setPreload(Preload);

    MediaPlayerClient* mediaPlayerClient() const { return m_mediaPlayerClient; }

#if USE(NATIVE_FULLSCREEN_VIDEO)
    void enterFullscreen();
    void exitFullscreen();
#endif

#if USE(NATIVE_FULLSCREEN_VIDEO)
    bool canEnterFullscreen() const;
#endif

    // whether accelerated rendering is supported by the media engine for the current media.
    bool supportsAcceleratedRendering() const;

    bool hasSingleSecurityOrigin() const;

    bool didPassCORSAccessCheck() const;

    double mediaTimeForTimeValue(double) const;

    unsigned decodedFrameCount() const;
    unsigned droppedFrameCount() const;
    unsigned audioDecodedByteCount() const;
    unsigned videoDecodedByteCount() const;

    void setNeedsStyleRecalc();

#if ENABLE(WEB_AUDIO)
    AudioSourceProvider* audioSourceProvider();
#endif

#if ENABLE(ENCRYPTED_MEDIA_V2)
    bool keyNeeded(Uint8Array* initData);
#endif

private:
    explicit MediaPlayer(MediaPlayerClient*);
    void initializeMediaPlayerPrivate();

    static CreateMediaEnginePlayer s_createMediaEngineFunction;

    MediaPlayerClient* m_mediaPlayerClient;
    OwnPtr<MediaPlayerPrivateInterface> m_private;
    Preload m_preload;
    bool m_inDestructor;
};

}

#endif
