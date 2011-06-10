// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_VIDEO_DECODER_DEV_H_
#define PPAPI_CPP_DEV_VIDEO_DECODER_DEV_H_

#include <vector>

#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Instance;

// C++ wrapper for the Pepper Video Decoder interface. For more detailed
// documentation refer to PPB_VideoDecoder_Dev and PPP_VideoDecoder_Dev
// interfaces.
//
// C++ version of the PPB_VideoDecoder_Dev interface.
class VideoDecoder : public Resource {
 public:
  // C++ version of PPP_VideoDecoder_Dev interface.
  class Client {
   public:
    virtual ~Client();

    // Callback to provide buffers for the decoded output pictures.
    virtual void ProvidePictureBuffers(
        uint32_t req_num_of_bufs,
        struct PP_Size dimensions,
        enum PP_PictureBufferType_Dev type);

    // Callback for decoder to delivered unneeded picture buffers back to the
    // plugin.
    virtual void DismissPictureBuffer(int32_t picture_buffer_id) = 0;

    // Callback to deliver decoded pictures ready to be displayed.
    virtual void PictureReady(const PP_Picture_Dev& picture) = 0;

    // Callback to notify that decoder has decoded end of stream marker and has
    // outputted all displayable pictures.
    virtual void EndOfStream() = 0;

    // Callback to notify about decoding errors.
    virtual void NotifyError(PP_VideoDecodeError_Dev error) = 0;
  };

  // Constructor for the video decoder. Calls the Create on the
  // PPB_VideoDecoder_Dev interface.
  //
  // Parameters:
  //  |instance| is the pointer to the plug-in instance.
  //  |callback| will be called when decoder is initialized.
  //  |client| is the pointer to the client object. Ownership of the object is
  //  not transferred and it must outlive the lifetime of this class.
  VideoDecoder(const Instance* instance, Client* client);
  ~VideoDecoder();

  // Initializates the video decoder with a requested configuration.
  // Calls Init() on PPB_VideoDecoder_Dev interface.
  //
  // Parameters:
  //  |config| is the configuration on which the decoder should be initialized.
  //  |callback| will be called when decoder is initialized.
  int32_t Initialize(const PP_VideoConfigElement* config,
                     CompletionCallback callback);

  // GetConfigs returns supported configurations that are subsets of given
  // |prototype_config|.
  bool GetConfigs(Instance* instance,
                  const PP_VideoConfigElement* prototype_config,
                  PP_VideoConfigElement* matching_configs,
                  uint32_t matching_configs_size,
                  uint32_t* num_of_matching_configs);

  // Provides the decoder with picture buffers for video decoding.
  // AssignGLESBuffers provides texture-backed buffers, whereas
  // AssignSysmemBuffers provides system memory-backed buffers.
  void AssignGLESBuffers(const std::vector<PP_GLESBuffer_Dev>& buffers);
  void AssignSysmemBuffers(const std::vector<PP_SysmemBuffer_Dev>& buffers);

  // Decodes given bitstream buffer. Once decoder is done with processing
  // |bitstream_buffer| is will call |callback| with provided user data.
  int32_t Decode(const PP_VideoBitstreamBuffer_Dev& bitstream_buffer,
                 CompletionCallback callback);

  // Tells the decoder to reuse given picture buffer.
  void ReusePictureBuffer(int32_t picture_buffer_id);

  // Flushes the decoder. |callback| will be called as soon as Flush has been
  // finished.
  int32_t Flush(CompletionCallback callback);

  // Dispatches abortion request to the decoder to abort decoding as soon as
  // possible. |callback| will be called as soon as abortion has been finished.
  int32_t Abort(CompletionCallback callback);

 private:
  // Pointer to the plugin's video decoder support interface for providing the
  // buffers for video decoding.
  Client* client_;

  // Suppress compiler-generated copy constructors.
  VideoDecoder(const VideoDecoder&);
  void operator=(const VideoDecoder&);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_VIDEO_DECODER_DEV_H_
