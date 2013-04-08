// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_VIDEO_READER_H_
#define PPAPI_CPP_VIDEO_READER_H_

#include <string>

#include "ppapi/c/pp_time.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/resource.h"

/// @file
/// This file defines the API to create and use video stream readers.

namespace pp {

class InstanceHandle;
class VideoFrame;

// VideoReader -----------------------------------------------------------------

/// The <code>VideoReader</code> class represents a video reader resource.
class VideoReader : public Resource {
 public:
  /// Default constructor for creating a <code>VideoReader</code> object.
  VideoReader();

  /// Constructor for creating a <code>VideoReader</code> for an instance.
  explicit VideoReader(const InstanceHandle& instance);

  /// The copy constructor for <code>VideoReader</code>.
  ///
  /// @param[in] other A reference to a <code>VideoReader</code>.
  VideoReader(const VideoReader& other);

  /// A constructor used when you have received a PP_Resource as a return
  /// value that has had its reference count incremented for you.
  ///
  /// @param[in] resource A PP_Resource corresponding to a video reader.
  VideoReader(PassRef, PP_Resource resource);

  /// Opens a stream for reading video and associates it with the given id.
  ///
  /// @param[in] stream_id A <code>Var</code> uniquely identifying the stream
  /// to read from.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion of Open.
  ///
  /// @return A return code from <code>pp_errors.h</code>.
  int32_t Open(const Var& stream_id,
               const CompletionCallback& cc);

  /// Gets the next frame of video from the reader's stream.
  ///
  /// @param[in] callback A <code>CompletionCallbackWithOutput</code> to be
  /// called upon completion of GetNextFrame.
  ///
  /// @return A return code from <code>pp_errors.h</code>.
  int32_t GetFrame(const CompletionCallbackWithOutput<VideoFrame>& cc);

  /// Closes the reader's current stream.
  void Close();
};

}  // namespace pp

#endif  // PPAPI_CPP_VIDEO_READER_H_
