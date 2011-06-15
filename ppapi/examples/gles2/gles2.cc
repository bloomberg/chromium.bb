// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "ppapi/c/dev/ppb_opengles_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/dev/context_3d_dev.h"
#include "ppapi/cpp/dev/graphics_3d_client_dev.h"
#include "ppapi/cpp/dev/graphics_3d_dev.h"
#include "ppapi/cpp/dev/surface_3d_dev.h"
#include "ppapi/cpp/dev/video_decoder_client_dev.h"
#include "ppapi/cpp/dev/video_decoder_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/examples/gles2/testdata.h"
#include "ppapi/lib/gl/include/GLES2/gl2.h"

namespace {

class GLES2DemoInstance : public pp::Instance, public pp::Graphics3DClient_Dev,
                          public pp::VideoDecoderClient_Dev {
 public:
  GLES2DemoInstance(PP_Instance instance, pp::Module* module);
  virtual ~GLES2DemoInstance();

  // pp::Instance implementation (see PPP_Instance).
  virtual void DidChangeView(const pp::Rect& position_ignored,
                             const pp::Rect& clip);

  // pp::Graphics3DClient_Dev implementation.
  virtual void Graphics3DContextLost() {
    // TODO(vrk/fischman): Properly reset after a lost graphics context.
    assert(!"Unexpectedly lost graphics context");
  }

  // pp::VideoDecoderClient_Dev implementation.
  virtual void ProvidePictureBuffers(
      pp::VideoDecoder_Dev decoder, uint32_t req_num_of_bufs,
      PP_Size dimensions, PP_PictureBufferType_Dev type);
  virtual void DismissPictureBuffer(
      pp::VideoDecoder_Dev decoder, int32_t picture_buffer_id);
  virtual void PictureReady(
      pp::VideoDecoder_Dev decoder, const PP_Picture_Dev& picture);
  virtual void EndOfStream(pp::VideoDecoder_Dev decoder);
  virtual void NotifyError(
      pp::VideoDecoder_Dev decoder, PP_VideoDecodeError_Dev error);

 private:
  // Helper struct that stores data used by the shader program.
  struct ShaderInfo {
    GLint pos_location;
    GLint tc_location;
    GLint tex_location;
    GLuint vertex_buffers[2];
  };

  // Initialize Video Decoder.
  void InitializeDecoder();

  // Callbacks passed into pp:VideoDecoder_Dev functions.
  void DecoderInitDone(int32_t result);
  void DecoderBitstreamDone(int32_t result, int bitstream_buffer_id);
  void DecoderFlushDone(int32_t result);
  void DecoderAbortDone(int32_t result);

  // Decode helpers.
  void DecodeNextNALU();
  void GetNextNALUBoundary(size_t start_pos, size_t* end_pos);
  void Render(const PP_GLESBuffer_Dev& buffer);

  // GL-related functions.
  void InitGL(int width, int height);
  GLuint CreateTexture(int32_t width, int32_t height);
  void CreateGLObjects();
  void CreateShader(GLuint program, GLenum type, const char* source, int size);
  void DeleteTexture(GLuint id);
  void PaintFinished(int32_t result, int picture_buffer_id);

  // Assert |context_| isn't holding any GL Errors.
  void assertNoGLError() {
    assert(!gles2_if_->GetError(context_->pp_resource()));
  }

  ShaderInfo program_data_;
  int next_picture_buffer_id_;
  int next_bitstream_buffer_id_;
  bool is_painting_;
  pp::CompletionCallbackFactory<GLES2DemoInstance> callback_factory_;
  size_t encoded_data_next_pos_to_decode_;

  // Map of texture buffers indexed by buffer id.
  typedef std::map<int, PP_GLESBuffer_Dev> PictureBufferMap;
  PictureBufferMap buffers_by_id_;
  // Map of bitstream buffers indexed by id.
  typedef std::map<int, pp::Buffer_Dev*> BitstreamBufferMap;
  BitstreamBufferMap bitstream_buffers_by_id_;

  // Unowned pointers.
  const struct PPB_OpenGLES2_Dev* gles2_if_;

  // Owned data.
  pp::Context3D_Dev* context_;
  pp::Surface3D_Dev* surface_;
  pp::VideoDecoder_Dev* video_decoder_;
};

GLES2DemoInstance::GLES2DemoInstance(PP_Instance instance, pp::Module* module)
    : pp::Instance(instance), pp::Graphics3DClient_Dev(this),
      pp::VideoDecoderClient_Dev(this),
      next_picture_buffer_id_(0),
      next_bitstream_buffer_id_(0),
      callback_factory_(this),
      encoded_data_next_pos_to_decode_(0),
      context_(NULL),
      surface_(NULL),
      video_decoder_(NULL) {
  gles2_if_ = static_cast<const struct PPB_OpenGLES2_Dev*>(
      module->GetBrowserInterface(PPB_OPENGLES2_DEV_INTERFACE));
  assert(gles2_if_);
}

GLES2DemoInstance::~GLES2DemoInstance() {
  delete video_decoder_;
  delete surface_;
  delete context_;
}

void GLES2DemoInstance::DidChangeView(
    const pp::Rect& position_ignored, const pp::Rect& clip) {
  if (clip.width() == 0 || clip.height() == 0)
    return;

  // Initialize graphics.
  InitGL(clip.width(), clip.height());
  InitializeDecoder();
}

void GLES2DemoInstance::InitializeDecoder() {
  if (video_decoder_)
    return;
  video_decoder_ = new pp::VideoDecoder_Dev(*this);

  PP_VideoConfigElement configs = PP_VIDEOATTR_DICTIONARY_TERMINATOR;
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&GLES2DemoInstance::DecoderInitDone);
  video_decoder_->Initialize(&configs, *context_, cb);
}

void GLES2DemoInstance::DecoderInitDone(int32_t result) {
  DecodeNextNALU();
}

void GLES2DemoInstance::DecoderBitstreamDone(
    int32_t result, int bitstream_buffer_id) {
  BitstreamBufferMap::iterator it =
      bitstream_buffers_by_id_.find(bitstream_buffer_id);
  assert(it != bitstream_buffers_by_id_.end());
  delete it->second;
  DecodeNextNALU();
}

void GLES2DemoInstance::DecoderFlushDone(int32_t result) {
}

void GLES2DemoInstance::DecoderAbortDone(int32_t result) {
}

static bool LookingAtNAL(const unsigned char* encoded, size_t pos) {
  return pos + 3 < kDataLen &&
      encoded[pos] == 0 && encoded[pos + 1] == 0 &&
      encoded[pos + 2] == 0 && encoded[pos + 3] == 1;
}

void GLES2DemoInstance::GetNextNALUBoundary(
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

void GLES2DemoInstance::DecodeNextNALU() {
  if (encoded_data_next_pos_to_decode_ == kDataLen) {
    pp::CompletionCallback cb =
        callback_factory_.NewCallback(&GLES2DemoInstance::DecoderFlushDone);
    video_decoder_->Flush(cb);
    return;
  }
  size_t start_pos = encoded_data_next_pos_to_decode_;
  size_t end_pos;
  GetNextNALUBoundary(start_pos, &end_pos);
  pp::Buffer_Dev* buffer = new pp::Buffer_Dev (this, end_pos - start_pos);
  PP_VideoBitstreamBuffer_Dev bitstream_buffer;
  int id = ++next_bitstream_buffer_id_;
  bitstream_buffer.id = id;
  bitstream_buffer.size = end_pos - start_pos;
  bitstream_buffer.data = buffer->pp_resource();
  memcpy(buffer->data(), kData + start_pos, end_pos - start_pos);
  bool result =
      bitstream_buffers_by_id_.insert(std::make_pair(id, buffer)).second;
  assert(result);

  pp::CompletionCallback cb =
      callback_factory_.NewCallback(
          &GLES2DemoInstance::DecoderBitstreamDone, id);
  video_decoder_->Decode(bitstream_buffer, cb);
  encoded_data_next_pos_to_decode_ = end_pos;
}

void GLES2DemoInstance::ProvidePictureBuffers(
    pp::VideoDecoder_Dev decoder, uint32_t req_num_of_bufs, PP_Size dimensions,
    PP_PictureBufferType_Dev type) {
  std::vector<PP_GLESBuffer_Dev> buffers;
  for (uint32_t i = 0; i < req_num_of_bufs; i++) {
    PP_GLESBuffer_Dev buffer;
    buffer.texture_id = CreateTexture(dimensions.width, dimensions.height);
    int id = ++next_picture_buffer_id_;
    buffer.info.id= id;
    buffers.push_back(buffer);
    bool result = buffers_by_id_.insert(std::make_pair(id, buffer)).second;
    assert(result);
  }
  video_decoder_->AssignGLESBuffers(buffers);
}

void GLES2DemoInstance::DismissPictureBuffer(
    pp::VideoDecoder_Dev decoder, int32_t picture_buffer_id) {
  PictureBufferMap::iterator it = buffers_by_id_.find(picture_buffer_id);
  assert(it != buffers_by_id_.end());
  DeleteTexture(it->second.texture_id);
  buffers_by_id_.erase(it);
}

void GLES2DemoInstance::PictureReady(
    pp::VideoDecoder_Dev decoder, const PP_Picture_Dev& picture) {
  PictureBufferMap::iterator it =
      buffers_by_id_.find(picture.picture_buffer_id);
  assert(it != buffers_by_id_.end());
  Render(it->second);
}

void GLES2DemoInstance::EndOfStream(pp::VideoDecoder_Dev decoder) {
}

void GLES2DemoInstance::NotifyError(
    pp::VideoDecoder_Dev decoder, PP_VideoDecodeError_Dev error) {
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

void GLES2DemoInstance::InitGL(int width, int height) {
  assert(width && height);
  is_painting_ = false;

  if (context_)
    delete(context_);
  context_ = new pp::Context3D_Dev(*this, 0, pp::Context3D_Dev(), NULL);
  assert(!context_->is_null());

  int32_t surface_attributes[] = {
    PP_GRAPHICS3DATTRIB_WIDTH, width,
    PP_GRAPHICS3DATTRIB_HEIGHT, height,
    PP_GRAPHICS3DATTRIB_NONE
  };
  if (surface_)
    delete(surface_);
  surface_ = new pp::Surface3D_Dev(*this, 0, surface_attributes);
  assert(!surface_->is_null());

  int32_t bind_error = context_->BindSurfaces(*surface_, *surface_);
  assert(!bind_error);

  // Set viewport window size and clear color bit.
  gles2_if_->Clear(context_->pp_resource(), GL_COLOR_BUFFER_BIT);
  gles2_if_->Viewport(context_->pp_resource(), 0, 0, width, height);

  bool success = BindGraphics(*surface_);
  assert(success);
  assertNoGLError();

  CreateGLObjects();
}

void GLES2DemoInstance::Render(const PP_GLESBuffer_Dev& buffer) {
  if (is_painting_) {
    // We are dropping frames if we don't render fast enough -
    // that is why sometimes the last frame rendered is < 249.
    if (video_decoder_)
      video_decoder_->ReusePictureBuffer(buffer.info.id);
    return;
  }
  is_painting_ = true;
  gles2_if_->ActiveTexture(context_->pp_resource(), GL_TEXTURE0);
  gles2_if_->BindTexture(
      context_->pp_resource(), GL_TEXTURE_2D, buffer.texture_id);
  gles2_if_->Uniform1i(context_->pp_resource(), program_data_.tex_location, 0);
  gles2_if_->DrawArrays(context_->pp_resource(), GL_TRIANGLE_STRIP, 0, 4);
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(
          &GLES2DemoInstance::PaintFinished, buffer.info.id);
  int32_t error = surface_->SwapBuffers(cb);
  assert(error == PP_ERROR_WOULDBLOCK);
  assertNoGLError();
}

void GLES2DemoInstance::PaintFinished(int32_t result, int picture_buffer_id) {
  is_painting_ = false;
  if (video_decoder_)
    video_decoder_->ReusePictureBuffer(picture_buffer_id);
}

GLuint GLES2DemoInstance::CreateTexture(int32_t width, int32_t height) {
  GLuint texture_id;
  gles2_if_->GenTextures(context_->pp_resource(), 1, &texture_id);
  assertNoGLError();
  // Assign parameters.
  gles2_if_->ActiveTexture(context_->pp_resource(), GL_TEXTURE0);
  gles2_if_->BindTexture(
      context_->pp_resource(), GL_TEXTURE_2D, texture_id);
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

  // Allocate texture.
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
      "precision mediump int;              \n"
      "varying vec2 v_texCoord;            \n"
      "uniform sampler2D s_texture;        \n"
      "void main()                         \n"
      "{"
      "    vec4 value = texture2D(s_texture, vec2(v_texCoord.x, 1.0 - v_texCoord.y)); \n"
      "    gl_FragColor = vec4(value.rgb, 1); \n"
      "}";

  static const GLfloat kVertices[] = {
      -1.f,  1.f,   // Position 0
      0.0f,  0.0f,  // TexCoord 0
      -1.f, -1.f,   // Position 1
      0.0f,  1.0f,  // TexCoord 1
      1.f, 1.f,     // Position 2
      1.0f,  0.0f,  // TexCoord 2
      1.0f,  -1.f,  // Position 3
      1.0f,  1.0f   // TexCoord 3
  };

  static const GLushort kIndices[] = { 0, 1, 2, 3 };

  // Create vertex position and texture coordinate buffers.
  gles2_if_->GenBuffers(
      context_->pp_resource(), 2, program_data_.vertex_buffers);

  // Creates and binds vertex/texture data to buffers.
  gles2_if_->BindBuffer(
      context_->pp_resource(), GL_ARRAY_BUFFER,
      program_data_.vertex_buffers[0]);
  gles2_if_->BufferData(
      context_->pp_resource(), GL_ARRAY_BUFFER, sizeof(kVertices),
      kVertices, GL_STATIC_DRAW);
  gles2_if_->BindBuffer(
      context_->pp_resource(),GL_ELEMENT_ARRAY_BUFFER,
      program_data_.vertex_buffers[1]);
  gles2_if_->BufferData(
      context_->pp_resource(),GL_ELEMENT_ARRAY_BUFFER,
      sizeof(kIndices), kIndices, GL_STATIC_DRAW);
  assertNoGLError();

  // Create shader program.
  GLuint program = gles2_if_->CreateProgram(context_->pp_resource());
  CreateShader(program, GL_VERTEX_SHADER, kVertexShader, sizeof(kVertexShader));
  CreateShader(
      program, GL_FRAGMENT_SHADER, kFragmentShader, sizeof(kFragmentShader));
  gles2_if_->LinkProgram(context_->pp_resource(), program);
  gles2_if_->UseProgram(context_->pp_resource(), program);
  gles2_if_->DeleteProgram(context_->pp_resource(), program);
  assertNoGLError();

  // Remember locations for shader variables.
  program_data_.pos_location = gles2_if_->GetAttribLocation(
      context_->pp_resource(), program, "a_position");
  program_data_.tc_location = gles2_if_->GetAttribLocation(
      context_->pp_resource(), program, "a_texCoord");
  program_data_.tex_location = gles2_if_->GetAttribLocation(
      context_->pp_resource(), program, "s_texture");
  assertNoGLError();

  // Assign vertex positions and texture coordinates to buffers for use in
  // shader program.
  GLfloat* ptr = (GLfloat*) 0;
  gles2_if_->VertexAttribPointer(
      context_->pp_resource(), program_data_.pos_location, 2, GL_FLOAT,
      GL_FALSE, 4 * sizeof(GLfloat), ptr);
  gles2_if_->EnableVertexAttribArray(
      context_->pp_resource(), program_data_.pos_location);
  ptr += 2;
  gles2_if_->VertexAttribPointer(
      context_->pp_resource(), program_data_.tc_location, 2, GL_FLOAT,
      GL_FALSE, 4 * sizeof(GLfloat), ptr);
  gles2_if_->EnableVertexAttribArray(
      context_->pp_resource(), program_data_.tc_location);
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
