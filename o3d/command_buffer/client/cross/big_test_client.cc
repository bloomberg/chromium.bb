/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <math.h>
#ifdef __native_client__
#include <sys/nacl_syscalls.h>
#else
#include "third_party/native_client/googleclient/native_client/src/trusted/desc/nrd_all_modules.h"
#endif
#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/common/cross/rpc_imc.h"
#include "command_buffer/client/cross/cmd_buffer_helper.h"
#include "command_buffer/client/cross/buffer_sync_proxy.h"
#include "third_party/vectormath/files/vectormathlibrary/include/vectormath/scalar/cpp/vectormath_aos.h"  // NOLINT

namespace o3d {
namespace command_buffer {

namespace math = Vectormath::Aos;

// Adds a SetViewport command into the buffer.
// Parameters:
//   cmd_buffer: the command buffer helper.
//   x, y, width, height: the dimensions of the Viewport.
//   z_near, z_far: the near and far clip plane distances.
void SetViewportCmd(CommandBufferHelper *cmd_buffer,
                    unsigned int x,
                    unsigned int y,
                    unsigned int width,
                    unsigned int height,
                    float z_near,
                    float z_far) {
  cmd_buffer->SetViewport(x, y, width, height, z_near, z_far);
}

// Copy a data buffer to args, for IMMEDIATE commands. Returns the number of
// args used.
unsigned int CopyToArgs(CommandBufferEntry *args,
                        const void *data,
                        size_t size) {
  memcpy(args, data, size);
  const unsigned int arg_size = sizeof(args[0]);
  return static_cast<unsigned int>((size + arg_size - 1) / arg_size);
}

// Our effect: pass through position and UV, look up texture.
// This follows the command buffer effect format :
// vertex_program_entry \0 fragment_program_entry \0 effect_code.
const char effect_data[] =
    "vs\0"  // Vertex program entry point
    "ps\0"  // Fragment program entry point
    "struct a2v {float4 pos: POSITION; float2 uv: TEXCOORD0;};\n"
    "struct v2f {float4 pos: POSITION; float2 uv: TEXCOORD0;};\n"
    "float4x4 worldViewProj : WorldViewProjection;\n"
    "v2f vs(a2v i) {\n"
    "  v2f o;\n"
    "  o.pos = mul(i.pos, worldViewProj);\n"
    "  o.uv = i.uv;\n"
    "  return o;\n"
    "}\n"
    "sampler s0;\n"
    "float4 ps(v2f i) : COLOR { return tex2D(s0, i.uv); }\n";

// Custom vertex, with position and color.
struct CustomVertex {
  float x, y, z, w;
  float u, v;
};

void BigTestClient(nacl::HtpHandle handle) {
  IMCSender sender(handle);
  BufferSyncProxy proxy(&sender);

  proxy.InitConnection();
  const int kShmSize = 2048;
  RPCShmHandle shm = CreateShm(kShmSize);
  void *shm_address = MapShm(shm, kShmSize);
  unsigned int shm_id = proxy.RegisterSharedMemory(shm, kShmSize);

  {
    CommandBufferHelper cmd_buffer(&proxy);
    cmd_buffer.Init(500);

    // Clear the buffers.
    RGBA color = {0.2f, 0.2f, 0.2f, 1.f};
    cmd_buffer.Clear(command_buffer::kColor | command_buffer::kDepth,
                     color.red, color.green, color.blue, color.alpha,
                     1.f, 0);

    const ResourceId vertex_buffer_id = 1;
    const ResourceId vertex_struct_id = 1;

    static const CustomVertex vertices[4] = {
      {-.5f, -.5f, 0.f, 1.f,  0, 0},
      {.5f,  -.5f, 0.f, 1.f,  1, 0},
      {-.5f,  .5f, 0.f, 1.f,  0, 1},
      {.5f,   .5f, 0.f, 1.f,  1, 1},
    };
    cmd_buffer.CreateVertexBuffer(vertex_buffer_id, sizeof(vertices),
                                  vertex_buffer::kNone);

    memcpy(shm_address, vertices, sizeof(vertices));
    cmd_buffer.SetVertexBufferData(
        vertex_buffer_id, 0, sizeof(vertices), shm_id, 0);
    unsigned int token = cmd_buffer.InsertToken();

    cmd_buffer.CreateVertexStruct(vertex_struct_id, 2);

    // Set POSITION input stream
    cmd_buffer.SetVertexInput(vertex_struct_id, 0, vertex_buffer_id, 0,
                              vertex_struct::kPosition, 0,
                              vertex_struct::kFloat4, sizeof(CustomVertex));

    // Set TEXCOORD0 input stream
    cmd_buffer.SetVertexInput(vertex_struct_id, 1, vertex_buffer_id, 16,
                              vertex_struct::kTexCoord, 0,
                              vertex_struct::kFloat2, sizeof(CustomVertex));

    // wait for previous transfer to be executed, so that we can re-use the
    // transfer shared memory buffer.
    cmd_buffer.WaitForToken(token);
    memcpy(shm_address, effect_data, sizeof(effect_data));
    const ResourceId effect_id = 1;
    cmd_buffer.CreateEffect(effect_id, sizeof(effect_data), shm_id, 0);
    token = cmd_buffer.InsertToken();

    // Create a 4x4 2D texture.
    const ResourceId texture_id = 1;
    cmd_buffer.CreateTexture2d(texture_id, 4, 4, 1, texture::kARGB8, 0);

    static const unsigned int texels[4] = {
      0xff0000ff,
      0xffff00ff,
      0xff00ffff,
      0xffffffff,
    };
    // wait for previous transfer to be executed, so that we can re-use the
    // transfer shared memory buffer.
    cmd_buffer.WaitForToken(token);
    memcpy(shm_address, texels, sizeof(texels));
    // Creates a 4x4 texture by uploading 2x2 data in each quadrant.
    for (unsigned int x = 0; x < 2; ++x)
      for (unsigned int y = 0; y < 2; ++y) {
        cmd_buffer.SetTextureData(texture_id, x * 2, y * 2, 0, 2, 2, 1, 0,
                                  texture::kFaceNone,
                                  sizeof(texels[0]) * 2,  // row_pitch
                                  0,  // slice_pitch
                                  sizeof(texels),  // size
                                  shm_id,
                                  0);
      }
    token = cmd_buffer.InsertToken();

    const ResourceId sampler_id = 1;
    cmd_buffer.CreateSampler(sampler_id);
    cmd_buffer.SetSamplerTexture(sampler_id, texture_id);
    cmd_buffer.SetSamplerStates(sampler_id,
                                sampler::kClampToEdge,
                                sampler::kClampToEdge,
                                sampler::kClampToEdge,
                                sampler::kPoint,
                                sampler::kPoint,
                                sampler::kNone,
                                1);

    // Create a parameter for the sampler.
    const ResourceId sampler_param_id = 1;
    {
      static const char param_name[] = "s0";
      cmd_buffer.CreateParamByNameImmediate(sampler_param_id, effect_id,
                                            sizeof(param_name), param_name);
    }

    const ResourceId matrix_param_id = 2;
    {
      static const char param_name[] = "worldViewProj";
      cmd_buffer.CreateParamByNameImmediate(matrix_param_id, effect_id,
                                            sizeof(param_name), param_name);
    }

    float t = 0.f;
    while (true) {
      t = fmodf(t + .01f, 1.f);
      math::Matrix4 m =
          math::Matrix4::translation(math::Vector3(0.f, 0.f, .5f));
      m *= math::Matrix4::rotationY(t * 2 * 3.1415926f);
      cmd_buffer.BeginFrame();
      // Clear the background with an animated color (black to red).
      cmd_buffer.Clear(command_buffer::kColor | command_buffer::kDepth,
                       color.red, color.green, color.blue, color.alpha,
                       1.f, 0);

      cmd_buffer.SetVertexStruct(vertex_struct_id);
      cmd_buffer.SetEffect(effect_id);
      cmd_buffer.SetParamDataImmediate(
          sampler_param_id, sizeof(uint32), &sampler_id);  // NOLINT
      cmd_buffer.SetParamDataImmediate(
          matrix_param_id, sizeof(m), &m);
      cmd_buffer.Draw(command_buffer::kTriangleStrips, 0, 2);

      cmd_buffer.EndFrame();
      cmd_buffer.Flush();
    }

    cmd_buffer.Finish();
  }

  proxy.CloseConnection();
  proxy.UnregisterSharedMemory(shm_id);
  DestroyShm(shm);

  sender.SendCall(POISONED_MESSAGE_ID, NULL, 0, NULL, 0);
}

}  // namespace command_buffer
}  // namespace o3d

nacl::HtpHandle InitConnection(int argc, char **argv) {
  nacl::Handle handle = nacl::kInvalidHandle;
#ifndef __native_client__
  NaClNrdAllModulesInit();

  static nacl::SocketAddress g_address = { "command-buffer" };
  static nacl::SocketAddress g_local_address = { "cb-client" };

  nacl::Handle sockets[2];
  nacl::SocketPair(sockets);

  nacl::MessageHeader msg;
  msg.iov = NULL;
  msg.iov_length = 0;
  msg.handles = &sockets[1];
  msg.handle_count = 1;
  nacl::Handle local_socket = nacl::BoundSocket(&g_local_address);
  nacl::SendDatagramTo(local_socket, &msg, 0, &g_address);
  nacl::Close(local_socket);
  nacl::Close(sockets[1]);
  handle = sockets[0];
#else
  if (argc < 3 || strcmp(argv[1], "-fd") != 0) {
    fprintf(stderr, "Usage: %s -fd file_descriptor\n", argv[0]);
    return nacl::kInvalidHtpHandle;
  }
  int fd = atoi(argv[2]);
  handle = imc_connect(fd);
  if (handle < 0) {
    fprintf(stderr, "Could not connect to file descriptor %d.\n"
            "Did you use the -a and -X options to sel_ldr ?\n", fd);
    return nacl::kInvalidHtpHandle;
  }
#endif
  return nacl::CreateImcDesc(handle);
}

void CloseConnection(nacl::HtpHandle handle) {
  nacl::Close(handle);
#ifndef __native_client__
  NaClNrdAllModulesFini();
#endif
}

int main(int argc, char **argv) {
  nacl::HtpHandle htp_handle = InitConnection(argc, argv);
  if (htp_handle == nacl::kInvalidHtpHandle) {
    return 1;
  }

  o3d::command_buffer::BigTestClient(htp_handle);
  CloseConnection(htp_handle);
  return 0;
}
