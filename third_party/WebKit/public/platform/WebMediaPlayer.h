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

#ifndef WebMediaPlayer_h
#define WebMediaPlayer_h

#include "WebCallbacks.h"
#include "WebCanvas.h"
#include "WebContentDecryptionModule.h"
#include "WebMediaSource.h"
#include "WebSetSinkIdCallbacks.h"
#include "WebString.h"

#include "cc/paint/paint_flags.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class WebAudioSourceProvider;
class WebContentDecryptionModule;
class WebMediaPlayerSource;
class WebSecurityOrigin;
class WebString;
class WebURL;
struct WebRect;
struct WebSize;

class WebMediaPlayer {
 public:
  enum NetworkState {
    kNetworkStateEmpty,
    kNetworkStateIdle,
    kNetworkStateLoading,
    kNetworkStateLoaded,
    kNetworkStateFormatError,
    kNetworkStateNetworkError,
    kNetworkStateDecodeError,
  };

  enum ReadyState {
    kReadyStateHaveNothing,
    kReadyStateHaveMetadata,
    kReadyStateHaveCurrentData,
    kReadyStateHaveFutureData,
    kReadyStateHaveEnoughData,
  };

  enum Preload {
    kPreloadNone,
    kPreloadMetaData,
    kPreloadAuto,
  };

  enum CORSMode {
    kCORSModeUnspecified,
    kCORSModeAnonymous,
    kCORSModeUseCredentials,
  };

  // Reported to UMA. Do not change existing values.
  enum LoadType {
    kLoadTypeURL = 0,
    kLoadTypeMediaSource = 1,
    kLoadTypeMediaStream = 2,
    kLoadTypeMax = kLoadTypeMediaStream,
  };

  typedef WebString TrackId;
  enum TrackType { kTextTrack, kAudioTrack, kVideoTrack };

  // This must stay in sync with WebGLRenderingContextBase::TexImageFunctionID.
  enum TexImageFunctionID {
    kTexImage2D,
    kTexSubImage2D,
    kTexImage3D,
    kTexSubImage3D
  };

  virtual ~WebMediaPlayer() {}

  virtual void Load(LoadType, const WebMediaPlayerSource&, CORSMode) = 0;

  // Playback controls.
  virtual void Play() = 0;
  virtual void Pause() = 0;
  virtual bool SupportsSave() const = 0;
  virtual void Seek(double seconds) = 0;
  virtual void SetRate(double) = 0;
  virtual void SetVolume(double) = 0;

  virtual void RequestRemotePlayback() {}
  virtual void RequestRemotePlaybackControl() {}
  virtual void RequestRemotePlaybackStop() {}
  virtual void RequestRemotePlaybackDisabled(bool disabled) {}
  virtual void SetPreload(Preload) {}
  virtual WebTimeRanges Buffered() const = 0;
  virtual WebTimeRanges Seekable() const = 0;

  // Attempts to switch the audio output device.
  // Implementations of setSinkId take ownership of the WebSetSinkCallbacks
  // object.
  // Note also that setSinkId implementations must make sure that all
  // methods of the WebSetSinkCallbacks object, including constructors and
  // destructors, run in the same thread where the object is created
  // (i.e., the blink thread).
  virtual void SetSinkId(const WebString& sink_id,
                         const WebSecurityOrigin&,
                         WebSetSinkIdCallbacks*) = 0;

  // True if the loaded media has a playable video/audio track.
  virtual bool HasVideo() const = 0;
  virtual bool HasAudio() const = 0;

  // True if the media is being played on a remote device.
  virtual bool IsRemote() const { return false; }

  // Dimension of the video.
  virtual WebSize NaturalSize() const = 0;

  // Getters of playback state.
  virtual bool Paused() const = 0;
  virtual bool Seeking() const = 0;
  virtual double Duration() const = 0;
  virtual double CurrentTime() const = 0;

  // Internal states of loading and network.
  virtual NetworkState GetNetworkState() const = 0;
  virtual ReadyState GetReadyState() const = 0;

  // Returns an implementation-specific human readable error message, or an
  // empty string if no message is available. The message should begin with a
  // UA-specific-error-code (without any ':'), optionally followed by ': ' and
  // further description of the error.
  virtual WebString GetErrorMessage() const = 0;

  virtual bool DidLoadingProgress() = 0;

  virtual bool HasSingleSecurityOrigin() const = 0;
  virtual bool DidPassCORSAccessCheck() const = 0;

  virtual double MediaTimeForTimeValue(double time_value) const = 0;

  virtual unsigned DecodedFrameCount() const = 0;
  virtual unsigned DroppedFrameCount() const = 0;
  virtual unsigned CorruptedFrameCount() const { return 0; }
  virtual size_t AudioDecodedByteCount() const = 0;
  virtual size_t VideoDecodedByteCount() const = 0;

  virtual void Paint(WebCanvas*, const WebRect&, cc::PaintFlags&) = 0;

  // Do a GPU-GPU texture copy of the current video frame to |texture|,
  // reallocating |texture| at the appropriate size with given internal
  // format, format, and type if necessary. If the copy is impossible
  // or fails, it returns false.
  virtual bool CopyVideoTextureToPlatformTexture(gpu::gles2::GLES2Interface*,
                                                 unsigned target,
                                                 unsigned texture,
                                                 unsigned internal_format,
                                                 unsigned format,
                                                 unsigned type,
                                                 int level,
                                                 bool premultiply_alpha,
                                                 bool flip_y) {
    return false;
  }

  // Copy sub video frame texture to |texture|. If the copy is impossible or
  // fails, it returns false.
  virtual bool CopyVideoSubTextureToPlatformTexture(gpu::gles2::GLES2Interface*,
                                                    unsigned target,
                                                    unsigned texture,
                                                    int level,
                                                    int xoffset,
                                                    int yoffset,
                                                    bool premultiply_alpha,
                                                    bool flip_y) {
    return false;
  }

  // Do tex(Sub)Image2D/3D for current frame. If it is not implemented for given
  // parameters or fails, it returns false.
  // The method is wrapping calls to glTexImage2D, glTexSubImage2D,
  // glTexImage3D and glTexSubImage3D and parameters have the same name and
  // meaning.
  // Texture |texture| needs to be created and bound to active texture unit
  // before this call. In addition, TexSubImage2D and TexSubImage3D require that
  // previous TexImage2D and TexSubImage3D calls, respectively, defined the
  // texture content.
  virtual bool TexImageImpl(TexImageFunctionID function_id,
                            unsigned target,
                            gpu::gles2::GLES2Interface* gl,
                            unsigned texture,
                            int level,
                            int internalformat,
                            unsigned format,
                            unsigned type,
                            int xoffset,
                            int yoffset,
                            int zoffset,
                            bool flip_y,
                            bool premultiply_alpha) {
    return false;
  }

  virtual WebAudioSourceProvider* GetAudioSourceProvider() { return nullptr; }

  virtual void SetContentDecryptionModule(
      WebContentDecryptionModule* cdm,
      WebContentDecryptionModuleResult result) {
    result.CompleteWithError(
        kWebContentDecryptionModuleExceptionNotSupportedError, 0, "ERROR");
  }

  // Sets the poster image URL.
  virtual void SetPoster(const WebURL& poster) {}

  // Whether the WebMediaPlayer supports overlay fullscreen video mode. When
  // this is true, the video layer will be removed from the layer tree when
  // entering fullscreen, and the WebMediaPlayer is responsible for displaying
  // the video in enteredFullscreen().
  virtual bool SupportsOverlayFullscreenVideo() { return false; }
  // Inform WebMediaPlayer when the element has entered/exited fullscreen.
  virtual void EnteredFullscreen() {}
  virtual void ExitedFullscreen() {}

  // Inform WebMediaPlayer when the element starts/stops being the dominant
  // visible content. This will only be called after the monitoring of the
  // intersection with viewport is activated by calling
  // WebMediaPlayerClient::activateViewportIntersectionMonitoring().
  virtual void BecameDominantVisibleContent(bool is_dominant) {}

  // Inform WebMediaPlayer when the element starts/stops being the effectively
  // fullscreen video, i.e. being the fullscreen element or child of the
  // fullscreen element, and being dominant in the viewport.
  //
  // TODO(zqzhang): merge with becameDominantVisibleContent(). See
  // https://crbug.com/696211
  virtual void SetIsEffectivelyFullscreen(bool) {}

  virtual void EnabledAudioTracksChanged(
      const WebVector<TrackId>& enabled_track_ids) {}
  // |selectedTrackId| is null if no track is selected.
  virtual void SelectedVideoTrackChanged(TrackId* selected_track_id) {}

  // TODO(kainino): This is for a prototype implementation for getting the
  // width, height, and timestamp of the last frame uploaded to a WebGL
  // texture. https://crbug.com/639174
  virtual bool GetLastUploadedFrameInfo(unsigned* width,
                                        unsigned* height,
                                        double* timestamp) {
    return false;
  }

  // Callback called whenever the media element may have received or last native
  // controls. It might be called twice with the same value: the caller has to
  // check if the value have changed if it only wants to handle this case.
  // This method is not used to say express if the native controls are visible
  // but if the element is using them.
  virtual void OnHasNativeControlsChanged(bool) {}

  enum class DisplayType {
    kInline,
    kFullscreen,
    kPictureInPicture,
  };

  // Callback called whenever the media element display type changes. By
  // default, the display type is `kInline`.
  virtual void OnDisplayTypeChanged(DisplayType) {}
};

}  // namespace blink

#endif
