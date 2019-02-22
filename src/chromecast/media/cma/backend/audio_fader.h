// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_FADER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_FADER_H_

#include <memory>

#include "base/macros.h"

namespace media {
class AudioBus;
}  // namespace media

namespace chromecast {
namespace media {

// AudioFader handles smoothly fading audio in/out when a stream underruns
// (ie, when the data source does not have any data to provide when the output
// requests it). This prevents pops and clicks. Internally, it buffers enough
// data to ensure that a full fade can always take place if necessary; note that
// this increases output latency by |fade_frames| samples. All methods except
// constructor/destructor must be called on the same thread.
class AudioFader {
 public:
  // The source of real audio data for the fader.
  class Source {
   public:
    // Fills at most |num_frames| frames of audio into |buffer|, starting at
    // |frame_offset|. Returns the actual number of frames of audio that were
    // filled (may be less than |num_frames| if the source does not have
    // enough data). This method is only called synchronously from within
    // a call to FillFrames().
    virtual int FillFaderFrames(::media::AudioBus* buffer,
                                int frame_offset,
                                int num_frames) = 0;

   protected:
    virtual ~Source() = default;
  };

  // |fade_frames| is the number of frames over which a complete fade in/out
  // will take place.
  AudioFader(Source* source, int num_channels, int fade_frames);
  ~AudioFader();

  int buffered_frames() const { return buffered_frames_; }

  // Fills |buffer| with up to |num_frames| frames of data, starting at
  // |write_offset| within |buffer|, and fading as appropriate to avoid
  // pops/clicks. This will call through to the source to get more data. Returns
  // the number of frames filled.
  int FillFrames(int num_frames, ::media::AudioBus* buffer, int write_offset);

  // Returns the total number of frames that will be requested from the source
  // (potentially over multiple calls to source_->FillFaderFrames()) if
  // FillFrames() is called to fill |num_fill_frames| frames.
  int FramesNeededFromSource(int num_fill_frames) const;

  // Helper methods to fade in/out an AudioBus. |buffer| contains the data to
  // fade; |filled_frames| is the amount of data actually in |buffer| (if the
  // buffer was partially filled, this will not be equal to buffer->frames()).
  // |write_offset| is the offset within |buffer| to starting writing frames
  // to. |fade_frames| is the number of frames over which a complete fade should
  // happen (ie, how many frames it takes to go from a 1.0 to 0.0 multiplier).
  // |fade_frames_remaining| is the number of frames left in the current fade
  // (which will be less than |fade_frames| if part of the fade has already
  // been completed on a previous buffer).
  static void FadeInHelper(::media::AudioBus* buffer,
                           int filled_frames,
                           int write_offset,
                           int fade_frames,
                           int fade_frames_remaining);
  static void FadeOutHelper(::media::AudioBus* buffer,
                            int filled_frames,
                            int write_offset,
                            int fade_frames,
                            int fade_frames_remaining);

 private:
  enum class State {
    kSilent,
    kFadingIn,
    kPlaying,
    kFadingOut,
  };

  void CompleteFill(::media::AudioBus* buffer,
                    int filled_frames,
                    int write_offset);
  void IncompleteFill(::media::AudioBus* buffer,
                      int filled_frames,
                      int write_offset);
  void FadeIn(::media::AudioBus* buffer, int filled_frames, int write_offset);
  void FadeOut(::media::AudioBus* buffer, int filled_frames, int write_offset);

  Source* const source_;
  const int fade_frames_;

  State state_;
  std::unique_ptr<::media::AudioBus> fade_buffer_;
  int buffered_frames_;
  int fade_frames_remaining_;

  DISALLOW_COPY_AND_ASSIGN(AudioFader);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_FADER_H_
