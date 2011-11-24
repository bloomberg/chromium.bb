// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <iostream>
#include <sstream>
#include <list>
#include <map>
#include <set>
#include <vector>

#include "ppapi/c/dev/ppb_console_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/cpp/dev/video_decoder_client_dev.h"
#include "ppapi/cpp/dev/video_decoder_dev.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/graphics_3d_client.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"
#include "ppapi/examples/gles2/testdata.h"
#include "ppapi/lib/gl/include/GLES2/gl2.h"

// Use assert as a poor-man's CHECK, even in non-debug mode.
// Since <assert.h> redefines assert on every inclusion (it doesn't use
// include-guards), make sure this is the last file #include'd in this file.
#undef NDEBUG
#include <assert.h>

// Assert |context_| isn't holding any GL Errors.  Done as a macro instead of a
// function to preserve line number information in the failure message.
#define assertNoGLError() \
  assert(!gles2_if_->GetError(context_->pp_resource()));

namespace {

class GLES2DemoInstance : public pp::Instance,
                          public pp::Graphics3DClient,
                          public pp::VideoDecoderClient_Dev {
 public:
  GLES2DemoInstance(PP_Instance instance, pp::Module* module);
  virtual ~GLES2DemoInstance();

  // pp::Instance implementation (see PPP_Instance).
  virtual void DidChangeView(const pp::Rect& position,
                             const pp::Rect& clip_ignored);

  // pp::Graphics3DClient implementation.
  virtual void Graphics3DContextLost() {
    // TODO(vrk/fischman): Properly reset after a lost graphics context.  In
    // particular need to delete context_ and re-create textures.
    // Probably have to recreate the decoder from scratch, because old textures
    // can still be outstanding in the decoder!
    assert(!"Unexpectedly lost graphics context");
  }

  // pp::VideoDecoderClient_Dev implementation.
  virtual void ProvidePictureBuffers(PP_Resource decoder,
                                     uint32_t req_num_of_bufs,
                                     const PP_Size& dimensions);
  virtual void DismissPictureBuffer(PP_Resource decoder,
                                    int32_t picture_buffer_id);
  virtual void PictureReady(PP_Resource decoder, const PP_Picture_Dev& picture);
  virtual void EndOfStream(PP_Resource decoder);
  virtual void NotifyError(PP_Resource decoder, PP_VideoDecodeError_Dev error);

 private:
  enum { kNumConcurrentDecodes = 7,
         kNumDecoders = 2 };  // Baked into viewport rendering.

  // A single decoder's client interface.
  class DecoderClient {
   public:
    DecoderClient(GLES2DemoInstance* gles2, pp::VideoDecoder_Dev* decoder);
    ~DecoderClient();

    void DecodeNextNALUs();

    // Per-decoder implementation of part of pp::VideoDecoderClient_Dev.
    void ProvidePictureBuffers(uint32_t req_num_of_bufs,
                               PP_Size dimensions);
    void DismissPictureBuffer(int32_t picture_buffer_id);

    const PP_PictureBuffer_Dev& GetPictureBufferById(int id);
    pp::VideoDecoder_Dev* decoder() { return decoder_; }

   private:
    void DecodeNextNALU();
    static void GetNextNALUBoundary(size_t start_pos, size_t* end_pos);
    void DecoderBitstreamDone(int32_t result, int bitstream_buffer_id);
    void DecoderFlushDone(int32_t result);

    GLES2DemoInstance* gles2_;
    pp::VideoDecoder_Dev* decoder_;
    pp::CompletionCallbackFactory<DecoderClient> callback_factory_;
    int next_picture_buffer_id_;
    int next_bitstream_buffer_id_;
    size_t encoded_data_next_pos_to_decode_;
    std::set<int> bitstream_ids_at_decoder_;
    // Map of texture buffers indexed by buffer id.
    typedef std::map<int, PP_PictureBuffer_Dev> PictureBufferMap;
    PictureBufferMap picture_buffers_by_id_;
    // Map of bitstream buffers indexed by id.
    typedef std::map<int, pp::Buffer_Dev*> BitstreamBufferMap;
    BitstreamBufferMap bitstream_buffers_by_id_;
  };

  // Initialize Video Decoders.
  void InitializeDecoders();

  // GL-related functions.
  void InitGL();
  GLuint CreateTexture(int32_t width, int32_t height);
  void CreateGLObjects();
  void CreateShader(GLuint program, GLenum type, const char* source, int size);
  void DeleteTexture(GLuint id);
  void PaintFinished(int32_t result, PP_Resource decoder,
                     int picture_buffer_id);

  // Log an error to the developer console and stderr (though the latter may be
  // closed due to sandboxing or blackholed for other reasons) by creating a
  // temporary of this type and streaming to it.  Example usage:
  // LogError(this).s() << "Hello world: " << 42;
  class LogError {
   public:
    LogError(GLES2DemoInstance* demo) : demo_(demo) {}
    ~LogError() {
      const std::string& msg = stream_.str();
      demo_->console_if_->Log(demo_->pp_instance(), PP_LOGLEVEL_ERROR,
                              pp::Var(msg).pp_var());
      std::cerr << msg << std::endl;
    }
    // Impl note: it would have been nicer to have LogError derive from
    // std::ostringstream so that it can be streamed to directly, but lookup
    // rules turn streamed string literals to hex pointers on output.
    std::ostringstream& s() { return stream_; }
   private:
    GLES2DemoInstance* demo_;  // Unowned.
    std::ostringstream stream_;
  };

  pp::Size plugin_size_;
  bool is_painting_;
  // When decode outpaces render, we queue up decoded pictures for later
  // painting.  Elements are <decoder,picture>.
  std::list<std::pair<PP_Resource, PP_Picture_Dev> > pictures_pending_paint_;
  int num_frames_rendered_;
  PP_TimeTicks first_frame_delivered_ticks_;
  PP_TimeTicks last_swap_request_ticks_;
  PP_TimeTicks swap_ticks_;
  pp::CompletionCallbackFactory<GLES2DemoInstance> callback_factory_;

  // Unowned pointers.
  const struct PPB_Console_Dev* console_if_;
  const struct PPB_Core* core_if_;
  const struct PPB_OpenGLES2* gles2_if_;

  // Owned data.
  pp::Graphics3D* context_;
  typedef std::map<int, DecoderClient*> Decoders;
  Decoders video_decoders_;
};

GLES2DemoInstance::DecoderClient::DecoderClient(GLES2DemoInstance* gles2,
                                              pp::VideoDecoder_Dev* decoder)
    : gles2_(gles2), decoder_(decoder), callback_factory_(this),
      next_picture_buffer_id_(0),
      next_bitstream_buffer_id_(0), encoded_data_next_pos_to_decode_(0) {
};

GLES2DemoInstance::DecoderClient::~DecoderClient() {
  delete decoder_;
  decoder_ = NULL;

  for (BitstreamBufferMap::iterator it = bitstream_buffers_by_id_.begin();
       it != bitstream_buffers_by_id_.end(); ++it) {
    delete it->second;
  }
  bitstream_buffers_by_id_.clear();

  for (PictureBufferMap::iterator it = picture_buffers_by_id_.begin();
       it != picture_buffers_by_id_.end(); ++it) {
    gles2_->DeleteTexture(it->second.texture_id);
  }
  picture_buffers_by_id_.clear();
}

GLES2DemoInstance::GLES2DemoInstance(PP_Instance instance, pp::Module* module)
    : pp::Instance(instance), pp::Graphics3DClient(this),
      pp::VideoDecoderClient_Dev(this),
      num_frames_rendered_(0),
      first_frame_delivered_ticks_(-1),
      swap_ticks_(0),
      callback_factory_(this),
      context_(NULL) {
  assert((console_if_ = static_cast<const struct PPB_Console_Dev*>(
      module->GetBrowserInterface(PPB_CONSOLE_DEV_INTERFACE))));
  assert((core_if_ = static_cast<const struct PPB_Core*>(
      module->GetBrowserInterface(PPB_CORE_INTERFACE))));
  assert((gles2_if_ = static_cast<const struct PPB_OpenGLES2*>(
      module->GetBrowserInterface(PPB_OPENGLES2_INTERFACE))));
}

GLES2DemoInstance::~GLES2DemoInstance() {
  for (Decoders::iterator it = video_decoders_.begin();
       it != video_decoders_.end(); ++it) {
    delete it->second;
  }
  video_decoders_.clear();
  delete context_;
}

void GLES2DemoInstance::DidChangeView(
    const pp::Rect& position, const pp::Rect& clip_ignored) {
  if (position.width() == 0 || position.height() == 0)
    return;
  if (plugin_size_.width()) {
    assert(position.size() == plugin_size_);
    return;
  }
  plugin_size_ = position.size();

  // Initialize graphics.
  InitGL();
  InitializeDecoders();
}

void GLES2DemoInstance::InitializeDecoders() {
  assert(video_decoders_.empty());
  for (int i = 0; i < kNumDecoders; ++i) {
    DecoderClient* client = new DecoderClient(
        this, new pp::VideoDecoder_Dev(
            this, *context_, PP_VIDEODECODER_H264PROFILE_BASELINE));
    assert(!client->decoder()->is_null());
    assert(video_decoders_.insert(std::make_pair(
        client->decoder()->pp_resource(), client)).second);
    client->DecodeNextNALUs();
  }
}

void GLES2DemoInstance::DecoderClient::DecoderBitstreamDone(
    int32_t result, int bitstream_buffer_id) {
  assert(bitstream_ids_at_decoder_.erase(bitstream_buffer_id) == 1);
  BitstreamBufferMap::iterator it =
      bitstream_buffers_by_id_.find(bitstream_buffer_id);
  assert(it != bitstream_buffers_by_id_.end());
  delete it->second;
  bitstream_buffers_by_id_.erase(it);
  DecodeNextNALUs();
}

void GLES2DemoInstance::DecoderClient::DecoderFlushDone(int32_t result) {
  assert(result == PP_OK);
  // Check that each bitstream buffer ID we handed to the decoder got handed
  // back to us.
  assert(bitstream_ids_at_decoder_.empty());
  delete decoder_;
  decoder_ = NULL;
}

static bool LookingAtNAL(const unsigned char* encoded, size_t pos) {
  return pos + 3 < kDataLen &&
      encoded[pos] == 0 && encoded[pos + 1] == 0 &&
      encoded[pos + 2] == 0 && encoded[pos + 3] == 1;
}

void GLES2DemoInstance::DecoderClient::GetNextNALUBoundary(
    size_t start_pos, size_t* end_pos) {
  assert(LookingAtNAL(kData, start_pos));
  *end_pos = start_pos;
  *end_pos += 4;
  while (*end_pos + 3 < kDataLen &&
         !LookingAtNAL(kData, *end_pos)) {
    ++*end_pos;
  }
  if (*end_pos + 3 >= kDataLen) {
    *end_pos = kDataLen;
    return;
  }
}

void GLES2DemoInstance::DecoderClient::DecodeNextNALUs() {
  while (encoded_data_next_pos_to_decode_ <= kDataLen &&
         bitstream_ids_at_decoder_.size() < kNumConcurrentDecodes) {
    DecodeNextNALU();
  }
}

void GLES2DemoInstance::DecoderClient::DecodeNextNALU() {
  if (encoded_data_next_pos_to_decode_ == kDataLen) {
    ++encoded_data_next_pos_to_decode_;
    pp::CompletionCallback cb = callback_factory_.NewCallback(
        &GLES2DemoInstance::DecoderClient::DecoderFlushDone);
    decoder_->Flush(cb);
    return;
  }
  size_t start_pos = encoded_data_next_pos_to_decode_;
  size_t end_pos;
  GetNextNALUBoundary(start_pos, &end_pos);
  pp::Buffer_Dev* buffer = new pp::Buffer_Dev(gles2_, end_pos - start_pos);
  PP_VideoBitstreamBuffer_Dev bitstream_buffer;
  int id = ++next_bitstream_buffer_id_;
  bitstream_buffer.id = id;
  bitstream_buffer.size = end_pos - start_pos;
  bitstream_buffer.data = buffer->pp_resource();
  memcpy(buffer->data(), kData + start_pos, end_pos - start_pos);
  assert(bitstream_buffers_by_id_.insert(std::make_pair(id, buffer)).second);

  pp::CompletionCallback cb =
      callback_factory_.NewCallback(
          &GLES2DemoInstance::DecoderClient::DecoderBitstreamDone, id);
  assert(bitstream_ids_at_decoder_.insert(id).second);
  encoded_data_next_pos_to_decode_ = end_pos;
  decoder_->Decode(bitstream_buffer, cb);
}

void GLES2DemoInstance::ProvidePictureBuffers(
    PP_Resource decoder, uint32_t req_num_of_bufs, const PP_Size& dimensions) {
  DecoderClient* client = video_decoders_[decoder];
  assert(client);
  client->ProvidePictureBuffers(req_num_of_bufs, dimensions);
}

void GLES2DemoInstance::DecoderClient::ProvidePictureBuffers(
    uint32_t req_num_of_bufs, PP_Size dimensions) {
  std::vector<PP_PictureBuffer_Dev> buffers;
  for (uint32_t i = 0; i < req_num_of_bufs; ++i) {
    PP_PictureBuffer_Dev buffer;
    buffer.size = dimensions;
    buffer.texture_id =
        gles2_->CreateTexture(dimensions.width, dimensions.height);
    int id = ++next_picture_buffer_id_;
    buffer.id = id;
    buffers.push_back(buffer);
    assert(picture_buffers_by_id_.insert(std::make_pair(id, buffer)).second);
  }
  decoder_->AssignPictureBuffers(buffers);
}

const PP_PictureBuffer_Dev&
GLES2DemoInstance::DecoderClient::GetPictureBufferById(
    int id) {
  PictureBufferMap::iterator it = picture_buffers_by_id_.find(id);
  assert(it != picture_buffers_by_id_.end());
  return it->second;
}

void GLES2DemoInstance::DismissPictureBuffer(PP_Resource decoder,
                                             int32_t picture_buffer_id) {
  DecoderClient* client = video_decoders_[decoder];
  assert(client);
  client->DismissPictureBuffer(picture_buffer_id);
}

void GLES2DemoInstance::DecoderClient::DismissPictureBuffer(
    int32_t picture_buffer_id) {
  gles2_->DeleteTexture(GetPictureBufferById(picture_buffer_id).texture_id);
  picture_buffers_by_id_.erase(picture_buffer_id);
}

void GLES2DemoInstance::PictureReady(PP_Resource decoder,
                                     const PP_Picture_Dev& picture) {
  if (first_frame_delivered_ticks_ == -1)
    assert((first_frame_delivered_ticks_ = core_if_->GetTimeTicks()) != -1);
  if (is_painting_) {
    pictures_pending_paint_.push_back(std::make_pair(decoder, picture));
    return;
  }
  DecoderClient* client = video_decoders_[decoder];
  assert(client);
  const PP_PictureBuffer_Dev& buffer =
      client->GetPictureBufferById(picture.picture_buffer_id);
  assert(!is_painting_);
  is_painting_ = true;
  int x = 0;
  int y = 0;
  if (client != video_decoders_.begin()->second) {
    x = plugin_size_.width() / kNumDecoders;
    y = plugin_size_.height() / kNumDecoders;
  }

  gles2_if_->Viewport(context_->pp_resource(), x, y,
                      plugin_size_.width() / kNumDecoders,
                      plugin_size_.height() / kNumDecoders);
  gles2_if_->ActiveTexture(context_->pp_resource(), GL_TEXTURE0);
  gles2_if_->BindTexture(
      context_->pp_resource(), GL_TEXTURE_2D, buffer.texture_id);
  gles2_if_->DrawArrays(context_->pp_resource(), GL_TRIANGLE_STRIP, 0, 4);
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(
          &GLES2DemoInstance::PaintFinished, decoder, buffer.id);
  last_swap_request_ticks_ = core_if_->GetTimeTicks();
  assert(context_->SwapBuffers(cb) == PP_OK_COMPLETIONPENDING);
}

void GLES2DemoInstance::EndOfStream(PP_Resource decoder) {
}

void GLES2DemoInstance::NotifyError(PP_Resource decoder,
                                    PP_VideoDecodeError_Dev error) {
  LogError(this).s() << "Received error: " << error;
  assert(!"Unexpected error; see stderr for details");
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class GLES2DemoModule : public pp::Module {
 public:
  GLES2DemoModule() : pp::Module() {}
  virtual ~GLES2DemoModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new GLES2DemoInstance(instance, this);
  }
};

void GLES2DemoInstance::InitGL() {
  assert(plugin_size_.width() && plugin_size_.height());
  is_painting_ = false;

  assert(!context_);
  int32_t context_attributes[] = {
    PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 8,
    PP_GRAPHICS3DATTRIB_BLUE_SIZE, 8,
    PP_GRAPHICS3DATTRIB_GREEN_SIZE, 8,
    PP_GRAPHICS3DATTRIB_RED_SIZE, 8,
    PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 0,
    PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 0,
    PP_GRAPHICS3DATTRIB_SAMPLES, 0,
    PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
    PP_GRAPHICS3DATTRIB_WIDTH, plugin_size_.width(),
    PP_GRAPHICS3DATTRIB_HEIGHT, plugin_size_.height(),
    PP_GRAPHICS3DATTRIB_NONE,
  };
  context_ = new pp::Graphics3D(this, context_attributes);
  assert(!context_->is_null());
  assert(BindGraphics(*context_));

  // Clear color bit.
  gles2_if_->ClearColor(context_->pp_resource(), 1, 0, 0, 1);
  gles2_if_->Clear(context_->pp_resource(), GL_COLOR_BUFFER_BIT);

  assertNoGLError();

  CreateGLObjects();
}

void GLES2DemoInstance::PaintFinished(int32_t result, PP_Resource decoder,
                                      int picture_buffer_id) {
  assert(result == PP_OK);
  swap_ticks_ += core_if_->GetTimeTicks() - last_swap_request_ticks_;
  is_painting_ = false;
  ++num_frames_rendered_;
  if (num_frames_rendered_ % 50 == 0) {
    double elapsed = core_if_->GetTimeTicks() - first_frame_delivered_ticks_;
    double fps = (elapsed > 0) ? num_frames_rendered_ / elapsed : 1000;
    double ms_per_swap = (swap_ticks_ * 1e3) / num_frames_rendered_;
    LogError(this).s() << "Rendered frames: " << num_frames_rendered_
                       << ", fps: " << fps << ", with average ms/swap of: "
                       << ms_per_swap;
  }
  DecoderClient* client = video_decoders_[decoder];
  if (client && client->decoder())
    client->decoder()->ReusePictureBuffer(picture_buffer_id);
  if (!pictures_pending_paint_.empty()) {
    std::pair<PP_Resource, PP_Picture_Dev> decoder_picture =
        pictures_pending_paint_.front();
    pictures_pending_paint_.pop_front();
    PictureReady(decoder_picture.first, decoder_picture.second);
  }
}

GLuint GLES2DemoInstance::CreateTexture(int32_t width, int32_t height) {
  GLuint texture_id;
  gles2_if_->GenTextures(context_->pp_resource(), 1, &texture_id);
  assertNoGLError();
  // Assign parameters.
  gles2_if_->ActiveTexture(context_->pp_resource(), GL_TEXTURE0);
  gles2_if_->BindTexture(context_->pp_resource(), GL_TEXTURE_2D, texture_id);
  gles2_if_->TexParameteri(
      context_->pp_resource(), GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
      GL_NEAREST);
  gles2_if_->TexParameteri(
      context_->pp_resource(), GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
      GL_NEAREST);
  gles2_if_->TexParameterf(
      context_->pp_resource(), GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
      GL_CLAMP_TO_EDGE);
  gles2_if_->TexParameterf(
      context_->pp_resource(), GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
      GL_CLAMP_TO_EDGE);

  gles2_if_->TexImage2D(
      context_->pp_resource(), GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
      GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  assertNoGLError();
  return texture_id;
}

void GLES2DemoInstance::DeleteTexture(GLuint id) {
  gles2_if_->DeleteTextures(context_->pp_resource(), 1, &id);
}

void GLES2DemoInstance::CreateGLObjects() {
  // Code and constants for shader.
  static const char kVertexShader[] =
      "varying vec2 v_texCoord;            \n"
      "attribute vec4 a_position;          \n"
      "attribute vec2 a_texCoord;          \n"
      "void main()                         \n"
      "{                                   \n"
      "    v_texCoord = a_texCoord;        \n"
      "    gl_Position = a_position;       \n"
      "}";

  static const char kFragmentShader[] =
      "precision mediump float;            \n"
      "varying vec2 v_texCoord;            \n"
      "uniform sampler2D s_texture;        \n"
      "void main()                         \n"
      "{"
      "    gl_FragColor = texture2D(s_texture, v_texCoord); \n"
      "}";

  // Create shader program.
  GLuint program = gles2_if_->CreateProgram(context_->pp_resource());
  CreateShader(program, GL_VERTEX_SHADER, kVertexShader, sizeof(kVertexShader));
  CreateShader(
      program, GL_FRAGMENT_SHADER, kFragmentShader, sizeof(kFragmentShader));
  gles2_if_->LinkProgram(context_->pp_resource(), program);
  gles2_if_->UseProgram(context_->pp_resource(), program);
  gles2_if_->DeleteProgram(context_->pp_resource(), program);
  gles2_if_->Uniform1i(
      context_->pp_resource(),
      gles2_if_->GetUniformLocation(
          context_->pp_resource(), program, "s_texture"), 0);
  assertNoGLError();

  // Assign vertex positions and texture coordinates to buffers for use in
  // shader program.
  static const float kVertices[] = {
    -1, 1, -1, -1, 1, 1, 1, -1,  // Position coordinates.
    0, 1, 0, 0, 1, 1, 1, 0,  // Texture coordinates.
  };

  GLuint buffer;
  gles2_if_->GenBuffers(context_->pp_resource(), 1, &buffer);
  gles2_if_->BindBuffer(context_->pp_resource(), GL_ARRAY_BUFFER, buffer);
  gles2_if_->BufferData(context_->pp_resource(), GL_ARRAY_BUFFER,
                        sizeof(kVertices), kVertices, GL_STATIC_DRAW);
  assertNoGLError();
  GLint pos_location = gles2_if_->GetAttribLocation(
      context_->pp_resource(), program, "a_position");
  GLint tc_location = gles2_if_->GetAttribLocation(
      context_->pp_resource(), program, "a_texCoord");
  assertNoGLError();
  gles2_if_->EnableVertexAttribArray(context_->pp_resource(), pos_location);
  gles2_if_->VertexAttribPointer(context_->pp_resource(), pos_location, 2,
                                 GL_FLOAT, GL_FALSE, 0, 0);
  gles2_if_->EnableVertexAttribArray(context_->pp_resource(), tc_location);
  gles2_if_->VertexAttribPointer(
      context_->pp_resource(), tc_location, 2, GL_FLOAT, GL_FALSE, 0,
      static_cast<float*>(0) + 8);  // Skip position coordinates.
  assertNoGLError();
}

void GLES2DemoInstance::CreateShader(
    GLuint program, GLenum type, const char* source, int size) {
  GLuint shader = gles2_if_->CreateShader(context_->pp_resource(), type);
  gles2_if_->ShaderSource(context_->pp_resource(), shader, 1, &source, &size);
  gles2_if_->CompileShader(context_->pp_resource(), shader);
  gles2_if_->AttachShader(context_->pp_resource(), program, shader);
  gles2_if_->DeleteShader(context_->pp_resource(), shader);
}
}  // anonymous namespace

namespace pp {
// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new GLES2DemoModule();
}
}  // namespace pp
