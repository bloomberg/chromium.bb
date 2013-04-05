// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_VIDEO_FRAME_H_
#define PPAPI_CPP_VIDEO_FRAME_H_

#include "ppapi/c/pp_time.h"
#include "ppapi/c/pp_video_frame.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/pass_ref.h"

/// @file
/// This file defines the video frame struct used by video readers and writers.

namespace pp {

// VideoFrame ------------------------------------------------------------------

/// The <code>VideoFrame</code> class represents a frame of video in a stream.
class VideoFrame {
 public:
  /// Default constructor for creating a <code>VideoFrame</code> object.
  VideoFrame();

  /// Constructor that takes an existing <code>PP_VideoFrame</code> structure.
  /// The 'image_data' PP_Resource field in the structure will be managed by
  /// this instance.
  VideoFrame(PassRef, const PP_VideoFrame& pp_video_frame);

  /// Constructor that takes an existing <code>ImageData</code> instance and
  /// a timestamp.
  VideoFrame(const ImageData& image_data, PP_TimeTicks timestamp);

  /// The copy constructor for <code>VideoFrame</code>.
  ///
  /// @param[in] other A reference to a <code>VideoFrame</code>.
  VideoFrame(const VideoFrame& other);

  ~VideoFrame();

  VideoFrame& operator=(const VideoFrame& other);

  const PP_VideoFrame& pp_video_frame() const {
    return video_frame_;
  }

  ImageData image_data() const {
    return image_data_;
  }
  void set_image_data(const ImageData& image_data) {
    image_data_ = image_data;
    // The assignment above manages the underlying PP_Resources. Copy the new
    // one into our internal video frame struct.
    video_frame_.image_data = image_data_.pp_resource();
  }

  PP_TimeTicks timestamp() const { return video_frame_.timestamp; }
  void set_timestamp(PP_TimeTicks timestamp) {
    video_frame_.timestamp = timestamp;
  }

 private:
  ImageData image_data_;  // This manages the PP_Resource in video_frame_.
  PP_VideoFrame video_frame_;
};

namespace internal {

// A specialization of CallbackOutputTraits to provide the callback system the
// information on how to handle pp::VideoFrame. This converts PP_VideoFrame to
// pp::VideoFrame when passing to the plugin, and specifically manages the
// PP_Resource embedded in the video_frame_ field.
template<>
struct CallbackOutputTraits<pp::VideoFrame> {
  typedef PP_VideoFrame* APIArgType;
  typedef PP_VideoFrame StorageType;

  static inline APIArgType StorageToAPIArg(StorageType& t) {
    return &t;
  }

  static inline pp::VideoFrame StorageToPluginArg(StorageType& t) {
    return pp::VideoFrame(PASS_REF, t);
  }
};

}  // namespace internal

}  // namespace pp

#endif  // PPAPI_CPP_VIDEO_FRAME_H_
