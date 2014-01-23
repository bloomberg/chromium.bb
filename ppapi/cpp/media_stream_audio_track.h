// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_MEDIA_STREAM_AUDIO_TRACK_H_
#define PPAPI_CPP_MEDIA_STREAM_AUDIO_TRACK_H_

#include <string>

#include "ppapi/c/ppb_media_stream_audio_track.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

/// @file
/// This file defines the <code>MediaStreamAudioTrack</code> interface for an
/// audio source resource, which receives audio frames from a MediaStream audio
/// track in the browser.

namespace pp {

class AudioFrame;
class CompletionCallback;
template <typename T> class CompletionCallbackWithOutput;

/// The <code>MediaStreamAudioTrack</code> class contains methods for
/// receiving audio frames from a MediaStream audio track in the browser.
class MediaStreamAudioTrack : public Resource {
 public:
  /// Default constructor for creating an is_null()
  /// <code>MediaStreamAudioTrack</code> object.
  MediaStreamAudioTrack();

  /// The copy constructor for <code>MediaStreamAudioTrack</code>.
  ///
  /// @param[in] other A reference to a <code>MediaStreamAudioTrack</code>.
  MediaStreamAudioTrack(const MediaStreamAudioTrack& other);

  /// Constructs a <code>MediaStreamAudioTrack</code> from
  /// a <code>Resource</code>.
  ///
  /// @param[in] resource A <code>PPB_MediaStreamAudioTrack</code> resource.
  explicit MediaStreamAudioTrack(const Resource& resource);

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has had 1 ref added for you.
  ///
  /// @param[in] resource A <code>PPB_MediaStreamAudioTrack</code> resource.
  MediaStreamAudioTrack(PassRef, PP_Resource resource);

  ~MediaStreamAudioTrack();

  /// Configures underlying frame buffers for incoming frames.
  /// If the application doesn't want to drop frames, then the
  /// <code>PP_MEDIASTREAMAUDIOTRACK_ATTRIB_BUFFERED_FRAMES</code> should be
  /// chosen such that inter-frame processing time variability won't overrun the
  /// input buffer. If the buffer is overfilled, then frames will be dropped.
  /// The application can detect this by examining the timestamp on returned
  /// frames. If <code>Configure()</code> is not called, default settings will
  /// be used.
  /// Example usage from plugin code:
  /// @code
  /// int32_t attribs[] = {
  ///     PP_MEDIASTREAMAUDIOTRACK_ATTRIB_BUFFERED_FRAMES, 4,
  ///     PP_MEDIASTREAMAUDIOTRACK_ATTRIB_DURATION, 10,
  ///     PP_MEDIASTREAMAUDIOTRACK_ATTRIB_NONE};
  /// track.Configure(attribs, callback);
  /// @endcode
  ///
  /// @param[in] attrib_list A list of attribute name-value pairs in which each
  /// attribute is immediately followed by the corresponding desired value.
  /// The list is terminated by
  /// <code>PP_MEDIASTREAMAUDIOTRACK_AUDIO_NONE</code>.
  /// @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
  /// completion of <code>Configure()</code>.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  int32_t Configure(const int32_t attributes[],
                    const CompletionCallback& callback);

  /// Gets attribute value for a given attribute name.
  ///
  /// @param[in] attrib A <code>PP_MediaStreamAudioTrack_Attrib</code> for
  /// querying.
  /// @param[out] value A int32_t for storing the attribute value.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  int32_t GetAttrib(PP_MediaStreamAudioTrack_Attrib attrib,
                    int32_t* value);

  /// Returns the track ID of the underlying MediaStream audio track.
  std::string GetId() const;

  /// Checks whether the underlying MediaStream track has ended.
  /// Calls to GetFrame while the track has ended are safe to make and will
  /// complete, but will fail.
  bool HasEnded() const;

  /// Gets the next audio frame from the MediaStream track.
  /// If internal processing is slower than the incoming frame rate, new frames
  /// will be dropped from the incoming stream. Once the input buffer is full,
  /// frames will be dropped until <code>RecycleFrame()</code> is called to free
  /// a spot for another frame to be buffered.
  /// If there are no frames in the input buffer,
  /// <code>PP_OK_COMPLETIONPENDING</code> will be returned immediately and the
  /// <code>callback</code> will be called when a new frame is received or some
  /// error happens.
  ///
  /// @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
  /// completion of <code>GetFrame()</code>. If success, an AudioFrame will be
  /// passed into the completion callback function.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  /// Returns PP_ERROR_NOMEMORY if <code>max_buffered_frames</code> frames
  /// buffer was not allocated successfully.
  int32_t GetFrame(
      const CompletionCallbackWithOutput<AudioFrame>& callback);

  /// Recycles a frame returned by <code>GetFrame()</code>, so the track can
  /// reuse the underlying buffer of this frame. And the frame will become
  /// invalid. The caller should release all references it holds to
  /// <code>frame</code> and not use it anymore.
  ///
  /// @param[in] frame A AudioFrame returned by <code>GetFrame()</code>.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  int32_t RecycleFrame(const AudioFrame& frame);

  /// Closes the MediaStream audio track, and disconnects it from the audio
  /// source.
  /// After calling <code>Close()</code>, no new frames will be received.
  void Close();

  /// Checks whether a <code>Resource</code> is a MediaStream audio track,
  /// to test whether it is appropriate for use with the
  /// <code>MediaStreamAudioTrack</code> constructor.
  ///
  /// @param[in] resource A <code>Resource</code> to test.
  ///
  /// @return True if <code>resource</code> is a MediaStream audio track.
  static bool IsMediaStreamAudioTrack(const Resource& resource);
};

}  // namespace pp

#endif  // PPAPI_CPP_MEDIA_STREAM_AUDIO_TRACK_H_
