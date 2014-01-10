// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_MEDIA_STREAM_VIDEO_TRACK_H_
#define PPAPI_CPP_MEDIA_STREAM_VIDEO_TRACK_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/resource.h"

/// @file
/// This file defines the <code>MediaStreamVideoTrack</code> interface for a
/// video source resource, which receives video frames from a MediaStream video
/// track in the browser.

namespace pp {

class VideoFrame;
template <typename T> class CompletionCallbackWithOutput;

/// The <code>MediaStreamVideoTrack</code> class contains methods for
/// receiving video frames from a MediaStream video track in the browser.
class MediaStreamVideoTrack : public Resource {
 public:
  /// Default constructor for creating an is_null()
  /// <code>MediaStreamVideoTrack</code> object.
  MediaStreamVideoTrack();

  /// The copy constructor for <code>MediaStreamVideoTrack</code>.
  ///
  /// @param[in] other A reference to a <code>MediaStreamVideoTrack</code>.
  MediaStreamVideoTrack(const MediaStreamVideoTrack& other);

  /// Constructs a <code>MediaStreamVideoTrack</code> from
  /// a <code>Resource</code>.
  ///
  /// @param[in] resource A <code>Resource</code> containing a file system.
  explicit MediaStreamVideoTrack(const Resource& resource);

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has had 1 ref added for you.
  ///
  /// @param[in] resource A <code>PPB_MediaStreamVideoTrack</code> resource.
  MediaStreamVideoTrack(PassRef, PP_Resource resource);

  ~MediaStreamVideoTrack();

  /// Configures underlying frame buffers for incoming frames.
  /// If the application doesn't want to drop frames, then the
  /// <code>max_buffered_frames</code> should be chosen such that inter-frame
  /// processing time variability won't overrun the input buffer. If the buffer
  /// is overfilled, then frames will be dropped. The application can detect
  /// this by examining the timestamp on returned frames.
  /// If <code>Configure()</code> is not used, default settings will be used.
  ///
  /// @param[in] max_buffered_frames The maximum number of video frames to
  /// hold in the input buffer.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  int32_t Configure(uint32_t max_buffered_frames);

  /// Returns the track ID of the underlying MediaStream video track.
  std::string GetId() const;

  /// Checks whether the underlying MediaStream track has ended.
  /// Calls to GetFrame while the track has ended are safe to make and will
  /// complete, but will fail.
  bool HasEnded() const;

  /// Gets the next video frame from the MediaStream track.
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
  /// completion of <code>GetFrame()</code>. If success, a VideoFrame will be
  /// passed into the completion callback function.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  /// Returns PP_ERROR_NOMEMORY if <code>max_buffered_frames</code> frames
  /// buffer was not allocated successfully.
  int32_t GetFrame(
      const CompletionCallbackWithOutput<VideoFrame>& callback);

  /// Recycles a frame returned by <code>GetFrame()</code>, so the track can
  /// reuse the underlying buffer of this frame. And the frame will become
  /// invalid. The caller should release all references it holds to
  /// <code>frame</code> and not use it anymore.
  ///
  /// @param[in] frame A VideoFrame returned by <code>GetFrame()</code>.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  int32_t RecycleFrame(const VideoFrame& frame);

  /// Closes the MediaStream video track, and disconnects it from video source.
  /// After calling <code>Close()</code>, no new frames will be received.
  void Close();

  /// Checks whether a <code>Resource</code> is a MediaStream video track,
  /// to test whether it is appropriate for use with the
  /// <code>MediaStreamVideoTrack</code> constructor.
  ///
  /// @param[in] resource A <code>Resource</code> to test.
  ///
  /// @return True if <code>resource</code> is a MediaStream video track.
  static bool IsMediaStreamVideoTrack(const Resource& resource);
};

}  // namespace pp

#endif  // PPAPI_CPP_MEDIA_STREAM_VIDEO_TRACK_H_
