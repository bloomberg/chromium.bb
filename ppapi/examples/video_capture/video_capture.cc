// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <string.h>

#include <map>
#include <vector>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_opengles.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/dev/video_capture_dev.h"
#include "ppapi/cpp/dev/video_capture_client_dev.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/graphics_3d_client.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/lib/gl/include/GLES2/gl2.h"

// Assert |context_| isn't holding any GL Errors.  Done as a macro instead of a
// function to preserve line number information in the failure message.
#define assertNoGLError() \
  assert(!gles2_if_->GetError(context_->pp_resource()));

namespace {

// This object is the global object representing this plugin library as long
// as it is loaded.
class VCDemoModule : public pp::Module {
 public:
  VCDemoModule() : pp::Module() {}
  virtual ~VCDemoModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance);
};

class VCDemoInstance : public pp::Instance,
                       public pp::Graphics3DClient,
                       public pp::VideoCaptureClient_Dev {
 public:
  VCDemoInstance(PP_Instance instance, pp::Module* module);
  virtual ~VCDemoInstance();

  // pp::Instance implementation (see PPP_Instance).
  virtual void DidChangeView(const pp::Rect& position,
                             const pp::Rect& clip_ignored);

  // pp::Graphics3DClient implementation.
  virtual void Graphics3DContextLost() {
    InitGL();
    CreateYUVTextures();
    Render();
  }

  virtual void OnDeviceInfo(PP_Resource resource,
                            const PP_VideoCaptureDeviceInfo_Dev& info,
                            const std::vector<pp::Buffer_Dev>& buffers) {
    capture_info_ = info;
    buffers_ = buffers;
    CreateYUVTextures();
  }

  virtual void OnStatus(PP_Resource resource, uint32_t status) {
  }

  virtual void OnError(PP_Resource resource, uint32_t error) {
  }

  virtual void OnBufferReady(PP_Resource resource, uint32_t buffer) {
    const char* data = static_cast<const char*>(buffers_[buffer].data());
    int32_t width = capture_info_.width;
    int32_t height = capture_info_.height;
    gles2_if_->ActiveTexture(context_->pp_resource(), GL_TEXTURE0);
    gles2_if_->TexSubImage2D(
        context_->pp_resource(), GL_TEXTURE_2D, 0, 0, 0, width, height,
        GL_LUMINANCE, GL_UNSIGNED_BYTE, data);

    data += width * height;
    width /= 2;
    height /= 2;

    gles2_if_->ActiveTexture(context_->pp_resource(), GL_TEXTURE1);
    gles2_if_->TexSubImage2D(
        context_->pp_resource(), GL_TEXTURE_2D, 0, 0, 0, width, height,
        GL_LUMINANCE, GL_UNSIGNED_BYTE, data);

    data += width * height;
    gles2_if_->ActiveTexture(context_->pp_resource(), GL_TEXTURE2);
    gles2_if_->TexSubImage2D(
        context_->pp_resource(), GL_TEXTURE_2D, 0, 0, 0, width, height,
        GL_LUMINANCE, GL_UNSIGNED_BYTE, data);

    video_capture_.ReuseBuffer(buffer);
    if (is_painting_)
      needs_paint_ = true;
    else
      Render();
  }

 private:
  void Render();

  // GL-related functions.
  void InitGL();
  void InitializeCapture();
  GLuint CreateTexture(int32_t width, int32_t height, int unit);
  void CreateGLObjects();
  void CreateShader(GLuint program, GLenum type, const char* source, int size);
  void PaintFinished(int32_t result);
  void CreateYUVTextures();

  pp::Size position_size_;
  bool is_painting_;
  bool needs_paint_;
  GLuint texture_y_;
  GLuint texture_u_;
  GLuint texture_v_;
  pp::VideoCapture_Dev video_capture_;
  PP_VideoCaptureDeviceInfo_Dev capture_info_;
  std::vector<pp::Buffer_Dev> buffers_;
  pp::CompletionCallbackFactory<VCDemoInstance> callback_factory_;

  // Unowned pointers.
  const struct PPB_OpenGLES2* gles2_if_;

  // Owned data.
  pp::Graphics3D* context_;
};

VCDemoInstance::VCDemoInstance(PP_Instance instance, pp::Module* module)
    : pp::Instance(instance),
      pp::Graphics3DClient(this),
      pp::VideoCaptureClient_Dev(this),
      is_painting_(false),
      needs_paint_(false),
      texture_y_(0),
      texture_u_(0),
      texture_v_(0),
      video_capture_(*this),
      callback_factory_(this),
      context_(NULL) {
  gles2_if_ = static_cast<const struct PPB_OpenGLES2*>(
      module->GetBrowserInterface(PPB_OPENGLES2_INTERFACE));
  assert(gles2_if_);
  InitializeCapture();
}

VCDemoInstance::~VCDemoInstance() {
  delete context_;
}

void VCDemoInstance::DidChangeView(
    const pp::Rect& position, const pp::Rect& clip_ignored) {
  if (position.width() == 0 || position.height() == 0)
    return;
  if (position.size() == position_size_)
    return;

  position_size_ = position.size();

  // Initialize graphics.
  InitGL();

  Render();
}

void VCDemoInstance::InitializeCapture() {
  capture_info_.width = 320;
  capture_info_.height = 240;
  capture_info_.frames_per_second = 30;

  video_capture_.StartCapture(capture_info_, 4);
}

void VCDemoInstance::InitGL() {
  assert(position_size_.width() && position_size_.height());
  is_painting_ = false;

  delete context_;
  int32_t attributes[] = {
    PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 0,
    PP_GRAPHICS3DATTRIB_BLUE_SIZE, 8,
    PP_GRAPHICS3DATTRIB_GREEN_SIZE, 8,
    PP_GRAPHICS3DATTRIB_RED_SIZE, 8,
    PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 0,
    PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 0,
    PP_GRAPHICS3DATTRIB_SAMPLES, 0,
    PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
    PP_GRAPHICS3DATTRIB_WIDTH, position_size_.width(),
    PP_GRAPHICS3DATTRIB_HEIGHT, position_size_.height(),
    PP_GRAPHICS3DATTRIB_NONE,
  };
  context_ = new pp::Graphics3D(this, attributes);
  assert(!context_->is_null());

  // Set viewport window size and clear color bit.
  gles2_if_->ClearColor(context_->pp_resource(), 1, 0, 0, 1);
  gles2_if_->Clear(context_->pp_resource(), GL_COLOR_BUFFER_BIT);
  gles2_if_->Viewport(context_->pp_resource(), 0, 0,
                      position_size_.width(), position_size_.height());

  BindGraphics(*context_);
  assertNoGLError();

  CreateGLObjects();
}

void VCDemoInstance::Render() {
  assert(!is_painting_);
  is_painting_ = true;
  needs_paint_ = false;
  if (texture_y_) {
    gles2_if_->DrawArrays(context_->pp_resource(), GL_TRIANGLE_STRIP, 0, 4);
  } else {
    gles2_if_->Clear(context_->pp_resource(), GL_COLOR_BUFFER_BIT);
  }
  pp::CompletionCallback cb = callback_factory_.NewCallback(
      &VCDemoInstance::PaintFinished);
  context_->SwapBuffers(cb);
}

void VCDemoInstance::PaintFinished(int32_t result) {
  is_painting_ = false;
  if (needs_paint_)
    Render();
}

GLuint VCDemoInstance::CreateTexture(int32_t width, int32_t height, int unit) {
  GLuint texture_id;
  gles2_if_->GenTextures(context_->pp_resource(), 1, &texture_id);
  assertNoGLError();
  // Assign parameters.
  gles2_if_->ActiveTexture(context_->pp_resource(), GL_TEXTURE0 + unit);
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

  // Allocate texture.
  gles2_if_->TexImage2D(
      context_->pp_resource(), GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0,
      GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
  assertNoGLError();
  return texture_id;
}

void VCDemoInstance::CreateGLObjects() {
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
      "precision mediump float;                                   \n"
      "varying vec2 v_texCoord;                                   \n"
      "uniform sampler2D y_texture;                               \n"
      "uniform sampler2D u_texture;                               \n"
      "uniform sampler2D v_texture;                               \n"
      "uniform mat3 color_matrix;                                 \n"
      "void main()                                                \n"
      "{                                                          \n"
      "  vec3 yuv;                                                \n"
      "  yuv.x = texture2D(y_texture, v_texCoord).r;              \n"
      "  yuv.y = texture2D(u_texture, v_texCoord).r;              \n"
      "  yuv.z = texture2D(v_texture, v_texCoord).r;              \n"
      "  vec3 rgb = color_matrix * (yuv - vec3(0.0625, 0.5, 0.5));\n"
      "  gl_FragColor = vec4(rgb, 1.0);                           \n"
      "}";

  static const float kColorMatrix[9] = {
    1.1643828125f, 1.1643828125f, 1.1643828125f,
    0.0f, -0.39176171875f, 2.017234375f,
    1.59602734375f, -0.81296875f, 0.0f
  };

  PP_Resource context = context_->pp_resource();

  // Create shader program.
  GLuint program = gles2_if_->CreateProgram(context);
  CreateShader(program, GL_VERTEX_SHADER, kVertexShader, sizeof(kVertexShader));
  CreateShader(
      program, GL_FRAGMENT_SHADER, kFragmentShader, sizeof(kFragmentShader));
  gles2_if_->LinkProgram(context, program);
  gles2_if_->UseProgram(context, program);
  gles2_if_->DeleteProgram(context, program);
  gles2_if_->Uniform1i(
      context, gles2_if_->GetUniformLocation(context, program, "y_texture"), 0);
  gles2_if_->Uniform1i(
      context, gles2_if_->GetUniformLocation(context, program, "u_texture"), 1);
  gles2_if_->Uniform1i(
      context, gles2_if_->GetUniformLocation(context, program, "v_texture"), 2);
  gles2_if_->UniformMatrix3fv(
      context,
      gles2_if_->GetUniformLocation(context, program, "color_matrix"),
      1, GL_FALSE, kColorMatrix);
  assertNoGLError();

  // Assign vertex positions and texture coordinates to buffers for use in
  // shader program.
  static const float kVertices[] = {
    -1, 1, -1, -1, 1, 1, 1, -1,  // Position coordinates.
    0, 0, 0, 1, 1, 0, 1, 1,  // Texture coordinates.
  };

  GLuint buffer;
  gles2_if_->GenBuffers(context, 1, &buffer);
  gles2_if_->BindBuffer(context, GL_ARRAY_BUFFER, buffer);
  gles2_if_->BufferData(context, GL_ARRAY_BUFFER,
                        sizeof(kVertices), kVertices, GL_STATIC_DRAW);
  assertNoGLError();
  GLint pos_location = gles2_if_->GetAttribLocation(
      context, program, "a_position");
  GLint tc_location = gles2_if_->GetAttribLocation(
      context, program, "a_texCoord");
  assertNoGLError();
  gles2_if_->EnableVertexAttribArray(context, pos_location);
  gles2_if_->VertexAttribPointer(context, pos_location, 2,
                                 GL_FLOAT, GL_FALSE, 0, 0);
  gles2_if_->EnableVertexAttribArray(context, tc_location);
  gles2_if_->VertexAttribPointer(
      context, tc_location, 2, GL_FLOAT, GL_FALSE, 0,
      static_cast<float*>(0) + 8);  // Skip position coordinates.
  assertNoGLError();
}

void VCDemoInstance::CreateShader(
    GLuint program, GLenum type, const char* source, int size) {
  PP_Resource context = context_->pp_resource();
  GLuint shader = gles2_if_->CreateShader(context, type);
  gles2_if_->ShaderSource(context, shader, 1, &source, &size);
  gles2_if_->CompileShader(context, shader);
  gles2_if_->AttachShader(context, program, shader);
  gles2_if_->DeleteShader(context, shader);
}

void VCDemoInstance::CreateYUVTextures() {
  int32_t width = capture_info_.width;
  int32_t height = capture_info_.height;
  texture_y_ = CreateTexture(width, height, 0);

  width /= 2;
  height /= 2;
  texture_u_ = CreateTexture(width, height, 1);
  texture_v_ = CreateTexture(width, height, 2);
}

pp::Instance* VCDemoModule::CreateInstance(PP_Instance instance) {
  return new VCDemoInstance(instance, this);
}

}  // anonymous namespace

namespace pp {
// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new VCDemoModule();
}
}  // namespace pp
