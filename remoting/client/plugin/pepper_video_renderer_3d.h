// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_VIDEO_RENDERER_3D_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_VIDEO_RENDERER_3D_H_

#include <deque>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/video_decoder.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "remoting/client/chromoting_stats.h"
#include "remoting/client/plugin/pepper_video_renderer.h"
#include "remoting/protocol/video_stub.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

struct PPB_OpenGLES2;

namespace remoting {

// PepperVideoRenderer that uses the PPB_VideoDecoder interface for video
// decoding and Graphics3D for rendering.
class PepperVideoRenderer3D : public PepperVideoRenderer,
                              public protocol::VideoStub {
 public:
  PepperVideoRenderer3D();
  ~PepperVideoRenderer3D() override;

  // PepperVideoRenderer interface.
  bool Initialize(pp::Instance* instance,
                  const ClientContext& context,
                  EventHandler* event_handler) override;
  void OnViewChanged(const pp::View& view) override;
  void EnableDebugDirtyRegion(bool enable) override;

  // VideoRenderer interface.
  void OnSessionConfig(const protocol::SessionConfig& config) override;
  ChromotingStats* GetStats() override;
  protocol::VideoStub* GetVideoStub() override;

  // protocol::VideoStub interface.
  void ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                          const base::Closure& done) override;

 private:
  class PendingPacket;
  class Picture;

  struct FrameDecodeTimestamp {
    FrameDecodeTimestamp(uint32_t frame_id,
                         base::TimeTicks decode_started_time);
    uint32_t frame_id;
    base::TimeTicks decode_started_time;
  };

  // Callback for pp::VideoDecoder::Initialize().
  void OnInitialized(int32_t result);

  // Passes one picture from |pending_packets_| to the |video_decoder_|.
  void DecodeNextPacket();

  // Callback for pp::VideoDecoder::Decode().
  void OnDecodeDone(int32_t result);

  // Fetches next picture from the |video_decoder_|.
  void GetNextPicture();

  // Callback for pp::VideoDecoder::GetPicture().
  void OnPictureReady(int32_t result, PP_VideoPicture picture);

  // Copies |next_picture_| to |current_picture_| if |next_picture_| is set and
  // then renders |current_picture_|. Doesn't do anything if |need_repaint_| is
  // false.
  void PaintIfNeeded();

  // Callback for pp::Graphics3D::SwapBuffers().
  void OnPaintDone(int32_t result);

  // Initializes |shader_program_| for |texture_target|.
  void EnsureProgramForTexture(uint32_t texture_target);

  // Initializes |shader_program_| with the given shaders.
  void CreateProgram(const char* vertex_shader, const char* fragment_shader);

  // Creates a new shader and compiles |source| for it.
  void CreateShaderProgram(int type, const char* source);

  // CHECKs that the last OpenGL call has completed successfully.
  void CheckGLError();

  EventHandler* event_handler_;

  pp::Graphics3D graphics_;
  const PPB_OpenGLES2* gles2_if_;
  pp::VideoDecoder video_decoder_;

  webrtc::DesktopSize frame_size_;
  webrtc::DesktopVector frame_dpi_;
  scoped_ptr<webrtc::DesktopRegion> frame_shape_;

  webrtc::DesktopSize view_size_;

  ChromotingStats stats_;
  int64 latest_input_event_timestamp_ ;

  bool initialization_finished_;
  bool decode_pending_;
  bool get_picture_pending_;
  bool paint_pending_;

  uint32_t latest_frame_id_;

  // Queue of packets that that have been received, but haven't been passed to
  // the decoder yet.
  std::deque<PendingPacket*> pending_packets_;

  // Timestamps for all frames currently being processed by the decoder.
  std::deque<FrameDecodeTimestamp> frame_decode_timestamps_;

  // The current picture shown on the screen or being rendered. Must be deleted
  // before |video_decoder_|.
  scoped_ptr<Picture> current_picture_;

  // The next picture to be rendered. PaintIfNeeded() will copy it to
  // |current_picture_| and render it after that. Must be deleted
  // before |video_decoder_|.
  scoped_ptr<Picture> next_picture_;

  // Set to true if the screen has been resized and needs to be repainted.
  bool force_repaint_;

  // Time the last paint operation was started.
  base::TimeTicks latest_paint_started_time_;

  // The texture type for which |shader_program| was initialized. Can be either
  // 0, GL_TEXTURE_2D, GL_TEXTURE_RECTANGLE_ARB or GL_TEXTURE_EXTERNAL_OES. 0
  // indicates that |shader_program_| hasn't been intialized.
  uint32_t current_shader_program_texture_target_;

  // Shader program ID.
  unsigned int shader_program_;

  // Location of the scale value to be passed to the |shader_program_|.
  int shader_texcoord_scale_location_;

  // True if dirty regions are to be sent to |event_handler_| for debugging.
  bool debug_dirty_region_;

  pp::CompletionCallbackFactory<PepperVideoRenderer3D> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperVideoRenderer3D);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_VIDEO_RENDERER_3D_H_
