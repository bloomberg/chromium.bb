// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_AUDIO_FRAME_H_
#define PPAPI_CPP_AUDIO_FRAME_H_

#include "ppapi/c/ppb_audio_frame.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class AudioFrame : public Resource {
 public:
  /// Default constructor for creating an is_null()
  /// <code>AudioFrame</code> object.
  AudioFrame();

  /// The copy constructor for <code>AudioFrame</code>.
  ///
  /// @param[in] other A reference to an <code>AudioFrame</code>.
  AudioFrame(const AudioFrame& other);

  /// Constructs an <code>AudioFrame</code> from a <code>Resource</code>.
  ///
  /// @param[in] resource A <code>PPB_AudioFrame</code> resource.
  explicit AudioFrame(const Resource& resource);

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has had 1 ref added for you.
  ///
  /// @param[in] resource A <code>PPB_AudioFrame</code> resource.
  AudioFrame(PassRef, PP_Resource resource);

  virtual ~AudioFrame();

  /// Gets the timestamp of the audio frame.
  ///
  /// @return A <code>PP_TimeDelta</code> containing the timestamp of the audio
  /// frame. Given in seconds since the start of the containing audio stream.
  PP_TimeDelta GetTimestamp() const;

  /// Sets the timestamp of the audio frame.
  ///
  /// @param[in] timestamp A <code>PP_TimeDelta</code> containing the timestamp
  /// of the audio frame. Given in seconds since the start of the containing
  /// audio stream.
  void SetTimestamp(PP_TimeDelta timestamp);

  /// Gets the sample size of the audio frame in bytes.
  ///
  /// @return The sample size of the audio frame in bytes.
  uint32_t GetSampleSize() const;

  /// Gets the number of channels in the audio frame.
  ///
  /// @return The number of channels in the audio frame.
  uint32_t GetNumberOfChannels() const;

  /// Gets the number of samples in the audio frame.
  ///
  /// @return The number of samples in the audio frame.
  /// For example, at a sampling rate of 44,100 Hz in stereo audio, a frame
  /// containing 4,410 * 2 samples would have a duration of 100 milliseconds.
  uint32_t GetNumberOfSamples() const;

  /// Gets the data buffer containing the audio frame samples.
  ///
  /// @return A pointer to the beginning of the data buffer.
  void* GetDataBuffer();

  /// Gets the size of data buffer in bytes.
  ///
  /// @return The size of the data buffer in bytes.
  uint32_t GetDataBufferSize() const;
};

}  // namespace pp

#endif  // PPAPI_CPP_AUDIO_FRAME_H_
