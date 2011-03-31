// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_VIDEO_DECODER_DEV_H_
#define PPAPI_CPP_DEV_VIDEO_DECODER_DEV_H_

#include <map>
#include <set>
#include <vector>

#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Instance;

// Convenience C++ wrapper around video decoder output picture that allows easy
// manipulation of the object properties.
class VideoDecoderPicture {
 public:
  explicit VideoDecoderPicture(struct PP_Picture_Dev* picture);
  ~VideoDecoderPicture();

  // Picture size related functions.
  // There are three types of logical picture sizes that applications using
  // video decoder should be aware of:
  //   - Visible picture size,
  //   - Decoded picture size, and
  //   - Picture buffer size.
  //
  // Visible picture size means the actual picture size that is intended to be
  // displayed from the decoded output.
  //
  // Decoded picture size might vary from the visible size of the picture,
  // because of the underlying properties of the codec. Vast majority of modern
  // video compression algorithms are based on (macro)block-based transforms and
  // therefore process the picture in small windows (usually of size 16x16
  // pixels) one by one. However, if the native picture size does not happen to
  // match the block-size of the algorithm, there may be redundant data left on
  // the sides of the output picture, which are not intended for display. For
  // example, this happens to video of size 854x480 and H.264 codec. Since the
  // width (854 pixels) is not multiple of the block size of the coding format
  // (16 pixels), pixel columns 854-863 contain garbage data which is not
  // intended for display.
  //
  // Plugin is providing the buffers for output decoding and it should know the
  // picture buffer size it has provided to the decoder. Thus, there is no
  // function to query the buffer size from this class.
  //
  // GetDecodedPictureSize returns the decoded size of the decoded picture.
  // Returns PP_Size telling the decoded size of the picture in pixels.
  PP_Size GetDecodedPictureSize() const;

  // GetVisiblePictureSize returns the visible size of the decoded picture.
  // Returns PP_Size telling the visible size of the picture in pixels.
  PP_Size GetVisiblePictureSize() const;

  // GetPictureFlags returns flags associated with the picture.
  // Returns bitmask containing values from PP_PictureInfoFlag_Dev enumeration.
  uint32_t GetPictureFlags() const;

  // GetMetadata returns metadata associated with the picture.
  // Returns Buffer_Dev object representing the buffer with the metadata.
  Buffer_Dev GetMetadata() const;

 private:
  PP_Picture_Dev picture;
};

// Interface for collaborating with picture interface to provide memory for
// output picture and blitting them.
class VideoDecoderClient {
 public:
  virtual ~VideoDecoderClient();

  // Callback to provide buffers for the decoded output pictures.
  virtual std::vector<Resource> ProvidePictureBuffers(
      uint32_t requested_num_of_buffers,
      const std::vector<uint32_t>& buffer_properties) = 0;

  // Callback to deliver decoded pictures ready to be displayed.
  virtual void PictureReady(struct PP_Picture_Dev* picture) = 0;

  // Callback to notify that decoder has decoded end of stream marker and has
  // outputted all displayable pictures.
  virtual void NotifyEndOfStream() = 0;

  // Callback to notify about decoding errors.
  virtual void NotifyError(PP_VideoDecodeError_Dev error) = 0;
};

// C++ wrapper for the Pepper Video Decoder interface.
class VideoDecoder : public Resource {
 public:
  VideoDecoder(const Instance* instance,
               VideoDecoderClient* picture_interface);
  ~VideoDecoder();

  // GetConfig returns supported configurations that are subsets of given
  // |prototype_config|.
  // Parameters:
  //  |instance| is the pointer to the plug-in instance.
  //  |prototype_config| is the prototypical configuration.
  //
  // Returns std::vector containing all the configurations that match the
  // prototypical configuration.
  std::vector<uint32_t> GetConfig(
      Instance* instance,
      const std::vector<uint32_t>& prototype_config);

  // Initializes the video decoder with specific configuration.
  // Parameters:
  //  |config| is the configuration on which the decoder should be initialized.
  //
  // Returns true when command successfully accepted. Otherwise false.
  bool Initialize(const std::vector<uint32_t>& config);

  // Decodes given bitstream buffer. Once decoder is done with processing
  // |bitstream_buffer| is will call |callback| with provided user data.
  // Parameters:
  //  |bitstream_buffer| is the input bitstream that is sent for decoding.
  //  |callback| contains the callback function pointer with the custom userdata
  //  plugin wants to associate with the callback.
  //
  // Returns true when command successfully accepted. Otherwise false.
  bool Decode(const PP_VideoBitstreamBuffer_Dev& bitstream_buffer,
              PP_CompletionCallback callback);

  // Flushes the decoder. Once decoder has released pending bitstream buffers
  // and pictures and reset internal picture state it will call |callback| with
  // the user provided user data.
  // Parameters:
  //  |callback| contains the callback function pointer with the custom userdata
  //  plugin wants to associate with the callback.
  //
  // Returns true when command successfully accepted. Otherwise false.
  int32_t Flush(PP_CompletionCallback callback);

  // Dispatches abortion request to the decoder to abort decoding as soon as
  // possible and will not output anything or generate new callbacks. |callback|
  // will be called as soon as abortion has been finished. After abortion all
  // buffers can be considered dismissed even when there has not been callbacks
  // to dismiss them.
  //
  // Parameters:
  //   |callback| is one-time callback that will be called once the abortion
  //   request has been completed.
  //
  // Returns true when command successfully accepted. Otherwise false.
  int32_t Abort(PP_CompletionCallback callback);

 private:
  // Functions that handle event notification to all the relevant interfaces.
  // Function to handle event when a picture is ready.
  virtual void EventPicture(struct PP_Picture_Dev* picture);
  // Function to handle event when end of stream occurs.
  virtual void EventEndOfStream();
  // Function to handle event when error occurs.
  virtual void EventError(PP_VideoDecodeError_Dev error);

  // Pointer to the plugin's video decoder support interface for providing the
  // buffers for video decoding.
  VideoDecoderClient* picture_if_;

  VideoDecoder(const VideoDecoder&);
  void operator=(const VideoDecoder&);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_VIDEO_DECODER_DEV_H_
