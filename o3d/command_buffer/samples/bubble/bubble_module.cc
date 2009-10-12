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


// This is the Soap Bubble sample / demo. It runs as a native client NPAPI
// plug-in.

#include <math.h>
#ifdef __native_client__
#include <sys/nacl_syscalls.h>
#else
#include "third_party/native_client/googleclient/native_client/src/trusted/desc/nrd_all_modules.h"
#endif
#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/common/cross/rpc_imc.h"
#include "command_buffer/client/cross/fenced_allocator.h"
#include "command_buffer/client/cross/cmd_buffer_helper.h"
#include "command_buffer/client/cross/buffer_sync_proxy.h"
#include "command_buffer/samples/bubble/utils.h"
#include "command_buffer/samples/bubble/iridescence_texture.h"
#include "command_buffer/samples/bubble/perlin_noise.h"
#include "third_party/vectormath/files/vectormathlibrary/include/vectormath/scalar/cpp/vectormath_aos.h"  // NOLINT

#if OS_WIN
  typedef unsigned __int64   uint64_t;
#endif

// Cube map data is hard-coded in cubemap.cc as a byte array.
// Format is 64x64xBRGA, D3D face ordering (+X, -X, +Y, -Y, +Z, -Z).
extern unsigned char g_cubemap_data[];
const unsigned int kCubeMapWidth = 64;
const unsigned int kCubeMapHeight = 64;
const unsigned int kCubeMapFaceSize = kCubeMapWidth * kCubeMapHeight * 4;

// Define this to debug command buffers: it will call Finish() and check for a
// parse error. If there was one, it will warn about it in the console window.
// #define DEBUG_CMD
#ifdef DEBUG_CMD
#define CHECK_ERROR(HELPER) do {                                        \
    HELPER->Finish();                                                   \
    BufferSyncInterface::ParseError error =                             \
        HELPER->interface()->GetParseError();                           \
    if (error != BufferSyncInterface::kParseNoError) {                  \
      printf("CMD error %d at %s:%d\n", error, __FILE__, __LINE__);     \
    }                                                                   \
  } while (false)
#else
#define CHECK_ERROR(HELPER) do { } while (false)
#endif

namespace Vectormath {
namespace Aos {

// Creates a perspective projection matrix.
Matrix4 CreatePerspectiveMatrix(float vertical_field_of_view_radians,
                                float aspect,
                                float z_near,
                                float z_far) {
  float dz = z_near - z_far;
  float vertical_scale = 1.0f / tanf(vertical_field_of_view_radians / 2.0f);
  float horizontal_scale = vertical_scale / aspect;
  return Matrix4(Vector4(horizontal_scale, 0.0f, 0.0f, 0.0f),
                 Vector4(0.0f, vertical_scale, 0.0f, 0.0f),
                 Vector4(0.0f, 0.0f, z_far / dz, -1.0f),
                 Vector4(0.0f, 0.0f, z_near * z_far / dz, 0.0f));
}

}  // namespace Aos
}  // namespace Vectormath

namespace o3d {
namespace command_buffer {

namespace math = Vectormath::Aos;

// Copy a data buffer to args, for IMMEDIATE commands. Returns the number of
// args used.
unsigned int CopyToArgs(CommandBufferEntry *args,
                        const void *data,
                        size_t size) {
  memcpy(args, data, size);
  const unsigned int arg_size = sizeof(args[0]);
  return static_cast<unsigned int>((size + arg_size - 1) / arg_size);
}

// Our effect: this computes the reflection color using the iridescence texture,
// and modulates it with the cubemap lookup on the reflected ray, passing the
// transmission factor in the alpha (this should be used with One/SourceAlpha
// blending mode).
// We also attenuate the back face color because the incident light has already
// been through the front face.
// The width is modulated by an exponential factor of the Y component, to
// simulate gravity, and by a noise texture.
const char effect_data[] =
    "vs\0"  // Vertex program entry point
    "ps\0"  // Fragment program entry point
    "struct a2v {\n"
    "  float3 position: POSITION;\n"  // Object-space position
    "  float3 normal: NORMAL;\n"      // Object-space normal
    "  float2 uv: TEXCOORD0;\n"       // Noise texture UVs
    "};\n"
    "struct v2f {\n"
    "  float4 position: POSITION;\n"    // Homogeneous clip space position
    "  float2 uv: TEXCOORD0;\n"         // Noise texture UVs
    "  float2 params: TEXCOORD1;\n"     // (cos i, thickness), where i is the
                                        // angle of the incident ray
    "  float3 reflected: TEXCOORD4;\n"  // Reflected ray direction
    "};\n"
    "\n"
    "float4x4 worldViewProj : WorldViewProjection;\n"  // MVP matrix
    "float4x4 world : World;\n"  // Object-to-world matrix
    "float4x4 worldIT : WorldInverseTranspose;\n"  // Object-to-world matrix
    "float3 eye;\n"  // World-space eye position
    "float4 thickness_params;\n"  // Thickness modulation parameters (xyz),
                                  // Back face attenuation (w)
    "sampler noise_sampler;\n"  // Noise sampler
    "sampler iridescence_sampler;\n"  // Iridescence sampler
    "sampler env_sampler;\n"  // Environment map sampler
    "\n"
    "v2f vs(a2v i) {\n"
    "  v2f o;\n"
    // Object-space position and normal
    "  float4 object_position =\n"
    "      float4(i.position.x, i.position.y, i.position.z, 1);\n"
    "  float4 object_normal = float4(i.normal.x, i.normal.y, i.normal.z, 0);\n"
    // World-space position, normal, object center, and eye vector (incident
    // ray)
    "  float3 normal = normalize(mul(object_normal, worldIT).xyz);\n"
    "  float4 position = mul(object_position, world);\n"
    "  float4 center = mul(float4(0, 0, 0, 1), world);\n"
    "  float3 eye_vector = normalize(position.xyz - eye);\n"
    // cos i (absolute value for back faces)
    "  float cos_i = abs(dot(normal, eye_vector));\n"
    "  float thickness =\n"
    "      exp(-(position.y-center.y)*thickness_params.x)*thickness_params.y;\n"
    // Output parameters
    "  o.position = mul(object_position, worldViewProj);\n"
    "  o.params = float2(cos_i, thickness);\n"
    "  o.reflected = reflect(eye_vector, normal);\n"
    "  o.uv = i.uv;\n"
    "  return o;\n"
    "}\n"
    "float4 ps(v2f i) : COLOR {\n"
    // noise: remap [0 .. 1] to [-0.5 .. 0.5]
    "  float noise = tex2D(noise_sampler, i.uv).x - .5;\n"
    "  float thickness = i.params.y - noise * thickness_params.z;\n"
    "  float cos_i = i.params.x;\n"
    // Modulate iridescence color by the environment looked up along the
    // reflected ray.
    "  float4 color = tex2D(iridescence_sampler, float2(cos_i, thickness));\n"
    "  color *= texCUBE(env_sampler, i.reflected);\n"
    // Modulate by per-face attenuation
    "  color.rgb *= thickness_params.w;\n"
    "  return color;\n"
    "};\n"
    "\n";

// Custom vertex, with position, normal and UVs.
struct CustomVertex {
  float x, y, z;
  float nx, ny, nz;
  float u, v;
};

// Creates a sphere shape, filling vertices and indices.
// There must be space for (rows+1) * (cols+1) vertices, and 6 * rows * cols
// indices.
void MakeSphere(unsigned int rows,
                unsigned int cols,
                CustomVertex *vertices,
                unsigned int *indices) {
  for (unsigned int y = 0; y < rows + 1; ++y) {
    float phi = y * kPi / rows;
    float y1 = cosf(phi);
    float r = sinf(phi);
    for (unsigned int x = 0; x < cols + 1; ++x) {
      float theta = x * 2.f * kPi / cols;
      float x1 = cosf(theta) * r;
      float z1 = sinf(theta) * r;
      unsigned int index = x + y * (cols + 1);
      CustomVertex * vertex = vertices + index;
      vertex->x = x1;
      vertex->y = y1;
      vertex->z = z1;
      vertex->nx = x1;
      vertex->ny = y1;
      vertex->nz = z1;
      vertex->u = x * 1.f / cols;
      vertex->v = y * 1.f / rows;
      if (x != cols && y != rows) {
        // For each vertex, we add indices for 2 triangles representing the
        // quad using this vertex as the upper left corner:
        // [(x, y), (x+1, y), (x+1, y+1)] and
        // [(x, y), (x+1, y+1), (x, y+1)].
        // Note that we don't create triangles for the last row and the last
        // column of vertices.
        *indices++ = index;
        *indices++ = index + 1;
        *indices++ = index + cols + 2;
        *indices++ = index;
        *indices++ = index + cols + 2;
        *indices++ = index + cols + 1;
      }
    }
  }
}

// Makes a BGRA noise texture.
void MakeNoiseTexture(unsigned int width,
                      unsigned int height,
                      unsigned int frequency,
                      unsigned int *seed,
                      unsigned char *texture) {
  PerlinNoise2D perlin(frequency);
  perlin.Initialize(seed);
  scoped_array<float> values(new float[width * height]);
  perlin.Generate(width, height, values.get());
  for (unsigned int y = 0; y < height; ++y) {
    // Attenuate the noise value with a function that goes to 0 on the poles to
    // avoid discontinuities.
    // Note: it'd be better to have a 3D noise texture, but they are way
    // too expensive.
    float attenuation = sinf(y * kPi / height);
    for (unsigned int x = 0; x < width; ++x) {
      float attenuated_value = values[y * width + x] * attenuation;
      // remap [-1..1] to [0..1] and convert to color values.
      unsigned char value = ToChar(attenuated_value * .5f + .5f);
      *texture++ = value;
      *texture++ = value;
      *texture++ = value;
      *texture++ = value;
    }
  }
}

class BubbleDemo {
 public:
  BubbleDemo();
  ~BubbleDemo() {}

  // Creates the socket pair for the connection.
  nacl::HtpHandle CreateSockets();

  // Sets a handle.
  void SetHandle(int index, nacl::HtpHandle handle);
  // Initializes the demo once connected.
  void Initialize();
  // Finalizes the demo.
  void Finalize();
  // Renders a frame.
  void Render();
 private:
  // A bubble instance.
  struct Bubble {
    math::Point3 position;
    math::Vector3 rotation_speed;
    float scale;
    float base_thickness;
    float thickness_falloff;
    float noise_ratio;
  };

  // Depth sort functor for bubbles.
  class BubbleSorter {
   public:
    explicit BubbleSorter(const math::Matrix4 &view) : view_(view) {}
    bool operator()(const Bubble &l, const Bubble &r) {
      return (view_ * l.position)[2] < (view_ * r.position)[2];
    }
   private:
    const math::Matrix4 &view_;
  };

  // Draws one bubble.
  void DrawBubble(const math::Matrix4& view,
                  const math::Matrix4& proj,
                  const BubbleDemo::Bubble &bubble,
                  const math::Vector3& rotation);

  nacl::HtpHandle handle_pair_[2];
  ResourceId vertex_buffer_id_;
  ResourceId index_buffer_id_;
  ResourceId vertex_struct_id_;
  ResourceId effect_id_;
  ResourceId noise_texture_id_;
  ResourceId iridescence_texture_id_;
  ResourceId cubemap_id_;
  ResourceId noise_sampler_id_;
  ResourceId iridescence_sampler_id_;
  ResourceId cubemap_sampler_id_;
  ResourceId noise_sampler_param_id_;
  ResourceId iridescence_sampler_param_id_;
  ResourceId cubemap_sampler_param_id_;
  ResourceId mvp_param_id_;
  ResourceId world_param_id_;
  ResourceId worldIT_param_id_;
  ResourceId eye_param_id_;
  ResourceId thickness_param_id_;
  scoped_ptr<IMCSender> sender_;
  scoped_ptr<BufferSyncProxy> proxy_;
  scoped_ptr<CommandBufferHelper> helper_;
  scoped_ptr<FencedAllocatorWrapper> allocator_;
  RPCShmHandle shm_;
  unsigned int shm_id_;
  void *shm_address_;
  CustomVertex *vertices_;
  unsigned int *indices_;
  unsigned char *noise_texture_;
  unsigned char *iridescence_texture_;
  unsigned int seed_;
  float time_;
  std::vector<Bubble> bubbles_;
};

BubbleDemo::BubbleDemo()
    : vertex_buffer_id_(1),
      index_buffer_id_(1),
      vertex_struct_id_(1),
      effect_id_(1),
      noise_texture_id_(1),
      iridescence_texture_id_(2),
      cubemap_id_(3),
      noise_sampler_id_(1),
      iridescence_sampler_id_(2),
      cubemap_sampler_id_(3),
      noise_sampler_param_id_(1),
      iridescence_sampler_param_id_(2),
      cubemap_sampler_param_id_(3),
      mvp_param_id_(4),
      world_param_id_(5),
      worldIT_param_id_(6),
      eye_param_id_(7),
      thickness_param_id_(8),
      shm_(kRPCInvalidHandle),
      shm_id_(UINT_MAX),
      shm_address_(NULL),
      vertices_(NULL),
      indices_(NULL),
      noise_texture_(NULL),
      iridescence_texture_(NULL),
      seed_(0x12345678u),  // pick some non-zero seed.
      time_(0.f) {
  handle_pair_[0] = nacl::kInvalidHtpHandle;
  handle_pair_[1] = nacl::kInvalidHtpHandle;
}

void BubbleDemo::SetHandle(int index, nacl::HtpHandle handle) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, arraysize(handle_pair_));
  handle_pair_[index] = handle;
}

nacl::HtpHandle BubbleDemo::CreateSockets() {
  nacl::Handle socket_pair[2];
  if (nacl::SocketPair(socket_pair) < 0) {
    return nacl::kInvalidHtpHandle;
  }
  handle_pair_[0] = nacl::CreateImcDesc(socket_pair[0]);
  handle_pair_[1] = nacl::CreateImcDesc(socket_pair[1]);
  return handle_pair_[1];
}

const unsigned int kRows = 50;
const unsigned int kCols = 100;
const unsigned int kVertexCount = (kRows + 1) * (kCols + 1);
const unsigned int kIndexCount = 6 * kRows * kCols;
const unsigned int kVertexBufferSize = kVertexCount * sizeof(CustomVertex);
const unsigned int kIndexBufferSize = kIndexCount * sizeof(unsigned int);

const unsigned int kTexWidth = 512;
const unsigned int kTexHeight = 512;
const unsigned int kTexSize = kTexWidth * kTexHeight * 4;

const unsigned int kShmSize = 3 << 20;
const unsigned int kCommandBufferEntries = 1 << 16;

const float kRefractionIndex = 1.33;
const unsigned int kBubbleCount = 10;

void BubbleDemo::Initialize() {
  sender_.reset(new IMCSender(handle_pair_[0]));
  proxy_.reset(new BufferSyncProxy(sender_.get()));
  proxy_->InitConnection();

  DCHECK_LE(kVertexBufferSize + kIndexBufferSize + 2 * kTexSize, kShmSize);
  shm_ = CreateShm(kShmSize);
  shm_address_ = MapShm(shm_, kShmSize);
  shm_id_ = proxy_->RegisterSharedMemory(shm_, kShmSize);
  helper_.reset(new CommandBufferHelper(proxy_.get()));
  helper_->Init(kCommandBufferEntries);

  allocator_.reset(new FencedAllocatorWrapper(kShmSize,
                                              helper_.get(),
                                              shm_address_));
  vertices_ = allocator_->AllocTyped<CustomVertex>(kVertexCount);
  indices_ = allocator_->AllocTyped<unsigned int>(kIndexCount);
  MakeSphere(kRows, kCols, vertices_, indices_);
  noise_texture_ = allocator_->AllocTyped<unsigned char>(kTexSize);
  MakeNoiseTexture(kTexWidth, kTexHeight, 8, &seed_, noise_texture_);
  iridescence_texture_ = allocator_->AllocTyped<unsigned char>(kTexSize);
  MakeIridescenceTexture(kTexWidth, kTexHeight, kRefractionIndex,
                         kRedWavelength, iridescence_texture_);

  // Clear the buffers.
  RGBA color = {0.f, 0.f, 0.f, 1.f};
  helper_->Clear(command_buffer::kColor | command_buffer::kDepth,
                 color.red, color.green, color.blue, color.alpha,
                 1.f, 0);

  // Create geometry arrays and structures
  helper_->CreateVertexBuffer(vertex_buffer_id_, kVertexBufferSize,
                              vertex_buffer::kNone);
  CHECK_ERROR(helper_);

  helper_->SetVertexBufferData(vertex_buffer_id_, 0, kVertexBufferSize,
                               shm_id_, allocator_->GetOffset(vertices_));
  CHECK_ERROR(helper_);

  helper_->CreateIndexBuffer(index_buffer_id_, kIndexBufferSize,
                             index_buffer::kIndex32Bit);
  CHECK_ERROR(helper_);

  helper_->SetIndexBufferData(index_buffer_id_, 0, kIndexBufferSize,
                              shm_id_, allocator_->GetOffset(indices_));
  CHECK_ERROR(helper_);

  helper_->CreateVertexStruct(vertex_struct_id_, 3);
  CHECK_ERROR(helper_);

  // Set POSITION input stream
  helper_->SetVertexInput(vertex_struct_id_, 0,
                          vertex_buffer_id_, offsetof(CustomVertex, x),
                          vertex_struct::kPosition, 0,
                          vertex_struct::kFloat3, sizeof(CustomVertex));
  CHECK_ERROR(helper_);

  // Set NORMAL input stream
  helper_->SetVertexInput(vertex_struct_id_, 1,
                          vertex_buffer_id_, offsetof(CustomVertex, nx),
                          vertex_struct::kNormal, 0,
                          vertex_struct::kFloat3, sizeof(CustomVertex));
  CHECK_ERROR(helper_);

  // Set TEXCOORD0 input stream
  helper_->SetVertexInput(vertex_struct_id_, 2,
                          vertex_buffer_id_, offsetof(CustomVertex, u),
                          vertex_struct::kTexCoord, 0,
                          vertex_struct::kFloat3, sizeof(CustomVertex));
  CHECK_ERROR(helper_);

  // Create a 2D texture.
  helper_->CreateTexture2d(noise_texture_id_,
                           kTexWidth, kTexHeight,
                           1, texture::kARGB8, false);
  CHECK_ERROR(helper_);

  helper_->SetTextureData(noise_texture_id_,
                          0, 0, 0,
                          kTexWidth, kTexHeight, 1,
                          0, texture::kFaceNone,
                          kTexWidth * 4,  // row_pitch
                          0,  // slice_pitch
                          kTexSize,  // size
                          shm_id_,
                          allocator_->GetOffset(noise_texture_));
  CHECK_ERROR(helper_);

  helper_->CreateSampler(noise_sampler_id_);
  CHECK_ERROR(helper_);

  helper_->SetSamplerTexture(noise_sampler_id_, noise_texture_id_);
  CHECK_ERROR(helper_);

  helper_->SetSamplerStates(noise_sampler_id_,
                            sampler::kWrap,
                            sampler::kWrap,
                            sampler::kWrap,
                            sampler::kLinear,
                            sampler::kLinear,
                            sampler::kNone,
                            1);
  CHECK_ERROR(helper_);

  // Create a 2D texture.
  helper_->CreateTexture2d(iridescence_texture_id_,
                           kTexWidth, kTexHeight,
                           1, texture::kARGB8, false);
  CHECK_ERROR(helper_);

  helper_->SetTextureData(iridescence_texture_id_,
                          0, 0, 0,
                          kTexWidth, kTexHeight, 1,
                          0, texture::kFaceNone,
                          kTexWidth * 4,  // row_pitch
                          0,  // slice_pitch
                          kTexSize,  // size
                          shm_id_,
                          allocator_->GetOffset(iridescence_texture_));
  CHECK_ERROR(helper_);

  helper_->CreateSampler(iridescence_sampler_id_);
  CHECK_ERROR(helper_);

  helper_->SetSamplerTexture(iridescence_sampler_id_, iridescence_texture_id_);
  CHECK_ERROR(helper_);

  helper_->SetSamplerStates(iridescence_sampler_id_,
                            sampler::kClampToEdge,
                            sampler::kClampToEdge,
                            sampler::kClampToEdge,
                            sampler::kLinear,
                            sampler::kLinear,
                            sampler::kNone,
                            1);
  CHECK_ERROR(helper_);

  // Create a Cubemap texture.
  helper_->CreateTextureCube(cubemap_id_,
                             kCubeMapWidth,
                             1, texture::kARGB8, false);
  CHECK_ERROR(helper_);

  for (unsigned int face = 0; face < 6; ++face) {
    void *data = allocator_->Alloc(kCubeMapFaceSize);
    memcpy(data, g_cubemap_data + face*kCubeMapFaceSize, kCubeMapFaceSize);
    helper_->SetTextureData(cubemap_id_,
                            0, 0, 0,
                            kCubeMapWidth, kCubeMapHeight, 1,
                            0, static_cast<texture::Face>(face),
                            kCubeMapWidth * 4,  // row_pitch
                            0,  // slice_pitch
                            kCubeMapFaceSize,  // size
                            shm_id_,
                            allocator_->GetOffset(data));
    CHECK_ERROR(helper_);
    allocator_->FreePendingToken(data, helper_->InsertToken());
  }

  helper_->CreateSampler(cubemap_sampler_id_);
  CHECK_ERROR(helper_);

  helper_->SetSamplerTexture(cubemap_sampler_id_, cubemap_id_);
  CHECK_ERROR(helper_);

  helper_->SetSamplerStates(cubemap_sampler_id_,
                            sampler::kClampToEdge,
                            sampler::kClampToEdge,
                            sampler::kClampToEdge,
                            sampler::kLinear,
                            sampler::kLinear,
                            sampler::kNone,
                            1);
  CHECK_ERROR(helper_);

  // Create the effect, and parameters.

  // TODO(gman): expand the size param of the command buffer because it would
  // be nice if we could just do this
  //
  // helper_->CreateEffectImmediate(effect_id_,
  //                                sizeof(effect_data), effect_data);
  //
  // instead of all this alloc, memcpy, free stuff. Currently we can't because a
  // command's size is only 8 bits and so the most we can pass in in an
  // immediate command is 1024 bytes.

  void *data = allocator_->Alloc(sizeof(effect_data));
  memcpy(data, effect_data, sizeof(effect_data));
  helper_->CreateEffect(effect_id_, sizeof(effect_data),
                        shm_id_, allocator_->GetOffset(data));
  CHECK_ERROR(helper_);
  allocator_->FreePendingToken(data, helper_->InsertToken());

  {
    const char param_name[] = "noise_sampler";
    helper_->CreateParamByNameImmediate(noise_sampler_param_id_,
                                        effect_id_,
                                        sizeof(param_name),
                                        param_name);
    CHECK_ERROR(helper_);
  }

  {
    const char param_name[] = "iridescence_sampler";
    helper_->CreateParamByNameImmediate(iridescence_sampler_param_id_,
                                        effect_id_,
                                        sizeof(param_name),
                                        param_name);
    CHECK_ERROR(helper_);
  }

  {
    const char param_name[] = "env_sampler";
    helper_->CreateParamByNameImmediate(cubemap_sampler_param_id_,
                                        effect_id_,
                                        sizeof(param_name),
                                        param_name);
    CHECK_ERROR(helper_);
  }

  {
    const char param_name[] = "worldViewProj";
    helper_->CreateParamByNameImmediate(mvp_param_id_,
                                        effect_id_,
                                        sizeof(param_name),
                                        param_name);
    CHECK_ERROR(helper_);
  }

  {
    const char param_name[] = "world";
    helper_->CreateParamByNameImmediate(world_param_id_,
                                        effect_id_,
                                        sizeof(param_name),
                                        param_name);
    CHECK_ERROR(helper_);
  }

  {
    const char param_name[] = "worldIT";
    helper_->CreateParamByNameImmediate(worldIT_param_id_,
                                        effect_id_,
                                        sizeof(param_name),
                                        param_name);
    CHECK_ERROR(helper_);
  }

  {
    const char param_name[] = "eye";
    helper_->CreateParamByNameImmediate(eye_param_id_,
                                        effect_id_,
                                        sizeof(param_name),
                                        param_name);
    CHECK_ERROR(helper_);
  }

  {
    const char param_name[] = "thickness_params";
    helper_->CreateParamByNameImmediate(thickness_param_id_,
                                        effect_id_,
                                        sizeof(param_name),
                                        param_name);
    CHECK_ERROR(helper_);
  }

  // Bind textures to the parameters
  helper_->SetParamDataImmediate(noise_sampler_param_id_,
                                 sizeof(noise_sampler_id_),
                                 &noise_sampler_id_);
  CHECK_ERROR(helper_);

  helper_->SetParamDataImmediate(iridescence_sampler_param_id_,
                                 sizeof(iridescence_sampler_id_),
                                 &iridescence_sampler_id_);
  CHECK_ERROR(helper_);

  helper_->SetParamDataImmediate(cubemap_sampler_param_id_,
                                 sizeof(cubemap_sampler_id_),
                                 &cubemap_sampler_id_);
  CHECK_ERROR(helper_);

  // Create our random bubbles.
  for (unsigned int i = 0; i < kBubbleCount; ++i) {
    Bubble bubble;
    bubble.position = math::Point3(Randf(-6.f, 6.f, &seed_),
                                   Randf(-6.f, 6.f, &seed_),
                                   Randf(-6.f, 6.f, &seed_));
    bubble.rotation_speed = math::Vector3(Randf(-.1f, .1f, &seed_),
                                          Randf(-.1f, .1f, &seed_),
                                          Randf(-.1f, .1f, &seed_));
    bubble.scale = Randf(.5, 2.f, &seed_);
    float max_thickness = Randf(.3f, .5f, &seed_);
    float min_thickness = Randf(.3f, max_thickness, &seed_);
    // thickness = base * e^(-y * falloff) for y in [-scale..scale].
    bubble.thickness_falloff =
        logf(max_thickness / min_thickness) / (2 * bubble.scale);
    bubble.base_thickness =
        max_thickness * expf(-bubble.scale * bubble.thickness_falloff);
    bubble.noise_ratio = Randf(.2f, .5f, &seed_);
    bubbles_.push_back(bubble);
  }
}

void BubbleDemo::Finalize() {
  helper_->Finish();

  allocator_->Free(iridescence_texture_);
  allocator_->Free(noise_texture_);
  allocator_->Free(indices_);
  allocator_->Free(vertices_);
  allocator_.reset(NULL);

  helper_.reset(NULL);

  proxy_->CloseConnection();
  proxy_->UnregisterSharedMemory(shm_id_);
  proxy_.reset(NULL);
  DestroyShm(shm_);

  sender_->SendCall(POISONED_MESSAGE_ID, NULL, 0, NULL, 0);
  sender_.reset(NULL);
  nacl::Close(handle_pair_[0]);
  handle_pair_[0] = nacl::kInvalidHtpHandle;
  nacl::Close(handle_pair_[1]);
  handle_pair_[1] = nacl::kInvalidHtpHandle;
}

void BubbleDemo::DrawBubble(const math::Matrix4& view,
                            const math::Matrix4& proj,
                            const BubbleDemo::Bubble &bubble,
                            const math::Vector3& rotation) {
  math::Matrix4 view_inv = math::inverse(view);
  math::Point3 eye(view_inv.getTranslation());
  math::Matrix4 model =
      math::Matrix4::translation(math::Vector3(bubble.position)) *
      math::Matrix4::scale(math::Vector3(bubble.scale, bubble.scale,
                                         bubble.scale)) *
      math::Matrix4::rotationZYX(rotation);
  math::Matrix4 modelIT = math::inverse(math::transpose(model));
  math::Matrix4 mvp = proj * view * model;

  helper_->SetVertexStruct(vertex_struct_id_);
  CHECK_ERROR(helper_);

  helper_->SetParamDataImmediate(mvp_param_id_, sizeof(mvp), &mvp);
  CHECK_ERROR(helper_);

  helper_->SetParamDataImmediate(world_param_id_, sizeof(model), &model);
  CHECK_ERROR(helper_);

  helper_->SetParamDataImmediate(worldIT_param_id_, sizeof(modelIT), &modelIT);
  CHECK_ERROR(helper_);

  helper_->SetParamDataImmediate(eye_param_id_, sizeof(eye), &eye);
  CHECK_ERROR(helper_);

  helper_->SetBlending(command_buffer::kBlendFuncOne,
                       command_buffer::kBlendFuncSrcAlpha,
                       command_buffer::kBlendEqAdd,
                       command_buffer::kBlendFuncOne,
                       command_buffer::kBlendFuncOne,
                       command_buffer::kBlendEqAdd,
                       false,
                       true);
  CHECK_ERROR(helper_);

  helper_->SetEffect(effect_id_);
  CHECK_ERROR(helper_);

  // Draw back faces first.
  helper_->SetPolygonRaster(command_buffer::kPolygonModeFill,
                            command_buffer::kCullCCW);
  CHECK_ERROR(helper_);

  float temp[4] = {
    bubble.thickness_falloff,
    bubble.base_thickness,
    bubble.noise_ratio,
    .5f,  // back face attenuation
  };
  helper_->SetParamDataImmediate(thickness_param_id_, sizeof(temp), temp);
  CHECK_ERROR(helper_);

  helper_->DrawIndexed(command_buffer::kTriangles,
                       index_buffer_id_,
                       0,  // first
                       kIndexCount / 3,  // primitive count
                       0,  // min index
                       kVertexCount - 1);  // max index
  CHECK_ERROR(helper_);

  // Then front faces.
  helper_->SetPolygonRaster(command_buffer::kPolygonModeFill,
                            command_buffer::kCullCW);
  CHECK_ERROR(helper_);

  temp[3] = 1.f;  // back face attenuation
  helper_->SetParamDataImmediate(thickness_param_id_, sizeof(temp), temp);
  CHECK_ERROR(helper_);

  helper_->DrawIndexed(command_buffer::kTriangles,
                       index_buffer_id_,
                       0,  // first
                       kIndexCount / 3,  // primitive count
                       0,  // min index
                       kVertexCount - 1);  // max index
  CHECK_ERROR(helper_);
}

void BubbleDemo::Render() {
  time_ = time_ + .01f;

  // Camera path
  float r = 20.f;
  float theta = time_ / 4.f;
  float phi = 2 * theta;
  math::Point3 eye(r * cosf(theta), r / 3.f * sinf(phi), r * sinf(theta));
  math::Point3 target(0.f, 0.f, 0.f);
  math::Vector3 up(0.f, 1.f, 0.f);
  math::Matrix4 proj =
      math::CreatePerspectiveMatrix(kPi / 4.f, 1.f, .1f, 10000.f);
  math::Matrix4 view = math::Matrix4::lookAt(eye, target, up);

  helper_->BeginFrame();
  CHECK_ERROR(helper_);
  RGBA color = {0.2f, 0.2f, 0.2f, 1.f};
  helper_->Clear(command_buffer::kColor | command_buffer::kDepth,
                 color.red, color.green, color.blue, color.alpha,
                 1.f, 0);

  // Sort bubbles back-to-front.
  std::sort(bubbles_.begin(), bubbles_.end(), BubbleSorter(view));
  // Then render them all.
  for (unsigned int i = 0; i < bubbles_.size(); ++i) {
    const Bubble &bubble = bubbles_[i];
    DrawBubble(view, proj, bubble, time_ * 2.f * kPi * bubble.rotation_speed);
  }

  helper_->EndFrame();
  CHECK_ERROR(helper_);
  helper_->Flush();
  helper_->Finish();
}

}  // namespace command_buffer
}  // namespace o3d

#if 0
// Scriptable object for the plug-in, provides the glue with the browser.
// Creates a BubbleDemo object and delegates calls to it.
class Plugin : public NPObject {
 public:
  NPError SetWindow(NPWindow *window) { return NPERR_NO_ERROR;}

  static NPClass *GetNPClass() {
    return &class_;
  }

 private:
  explicit Plugin(NPP npp);
  ~Plugin();

  nacl::HtpHandle Create();
  void Initialize();
  void Destroy();
  void Render();

  static NPObject *Allocate(NPP npp, NPClass *npclass);
  static void Deallocate(NPObject *object);
  static bool HasMethod(NPObject *header, NPIdentifier name);
  static bool Invoke(NPObject *header, NPIdentifier name,
                     const NPVariant *args, uint32_t argCount,
                     NPVariant *result);
  static bool InvokeDefault(NPObject *header, const NPVariant *args,
                            uint32_t argCount, NPVariant *result);
  static bool HasProperty(NPObject *header, NPIdentifier name);
  static bool GetProperty(NPObject *header, NPIdentifier name,
                          NPVariant *variant);
  static bool SetProperty(NPObject *header, NPIdentifier name,
                          const NPVariant *variant);
  static bool Enumerate(NPObject *header, NPIdentifier **value,
                        uint32_t *count);

  NPP npp_;
  static NPClass class_;
  NPIdentifier create_id_;
  NPIdentifier initialize_id_;
  NPIdentifier destroy_id_;
  NPIdentifier render_id_;

  scoped_ptr<o3d::command_buffer::BubbleDemo> demo_;
};

Plugin::Plugin(NPP npp)
    : npp_(npp) {
  const char *names[4] = {"create", "initialize", "destroy", "render"};
  NPIdentifier ids[4];
  NPN_GetStringIdentifiers(names, 4, ids);
  create_id_ = ids[0];
  initialize_id_ = ids[1];
  destroy_id_ = ids[2];
  render_id_ = ids[3];
}

Plugin::~Plugin() {
  if (demo_.get()) demo_->Finalize();
}

nacl::HtpHandle Plugin::Create() {
  demo_.reset(new o3d::command_buffer::BubbleDemo());
  return demo_->CreateSockets();
}

void Plugin::Initialize() {
  if (demo_.get()) demo_->Initialize();
}

void Plugin::Destroy() {
  if (demo_.get()) {
    demo_->Finalize();
    demo_.reset(NULL);
  }
}

void Plugin::Render() {
  if (demo_.get()) demo_->Render();
}

NPClass Plugin::class_ = {
  NP_CLASS_STRUCT_VERSION,
  Plugin::Allocate,
  Plugin::Deallocate,
  0,
  Plugin::HasMethod,
  Plugin::Invoke,
  0,
  Plugin::HasProperty,
  Plugin::GetProperty,
  Plugin::SetProperty,
  0,
  Plugin::Enumerate,
};

NPObject *Plugin::Allocate(NPP npp, NPClass *npclass) {
  return new Plugin(npp);
}

void Plugin::Deallocate(NPObject *object) {
  delete static_cast<Plugin *>(object);
}

bool Plugin::HasMethod(NPObject *header, NPIdentifier name) {
  Plugin *plugin = static_cast<Plugin *>(header);
  return (name == plugin->create_id_ ||
          name == plugin->initialize_id_ ||
          name == plugin->destroy_id_ ||
          name == plugin->render_id_);
}

bool Plugin::Invoke(NPObject *header, NPIdentifier name,
                    const NPVariant *args, uint32_t argCount,
                    NPVariant *result) {
  Plugin *plugin = static_cast<Plugin *>(header);
  VOID_TO_NPVARIANT(*result);
  if (name == plugin->create_id_ && argCount == 0) {
    nacl::HtpHandle handle = plugin->Create();
    if (handle == nacl::kInvalidHtpHandle) {
      return false;
    }
    HANDLE_TO_NPVARIANT(handle, *result);
    return true;
  } else if (name == plugin->initialize_id_ && argCount == 0) {
    plugin->Initialize();
    return true;
  } else if (name == plugin->destroy_id_ && argCount == 0) {
    plugin->Destroy();
    return true;
  } else if (name == plugin->render_id_ && argCount == 0) {
    plugin->Render();
    return true;
  } else {
    return false;
  }
}

bool Plugin::InvokeDefault(NPObject *header, const NPVariant *args,
                           uint32_t argCount, NPVariant *result) {
  return false;
}

bool Plugin::HasProperty(NPObject *header, NPIdentifier name) {
  return false;
}

bool Plugin::GetProperty(NPObject *header, NPIdentifier name,
                         NPVariant *variant) {
  return false;
}

bool Plugin::SetProperty(NPObject *header, NPIdentifier name,
                         const NPVariant *variant) {
  return false;
}

bool Plugin::Enumerate(NPObject *header, NPIdentifier **value,
                       uint32_t *count) {
  Plugin *plugin = static_cast<Plugin *>(header);
  *count = 4;
  NPIdentifier *ids = static_cast<NPIdentifier *>(
      NPN_MemAlloc(*count * sizeof(NPIdentifier)));
  ids[0] = plugin->create_id_;
  ids[1] = plugin->initialize_id_;
  ids[2] = plugin->destroy_id_;
  ids[3] = plugin->render_id_;
  *value = ids;
  return true;
}

NPError NPP_New(NPMIMEType mime_type, NPP instance, uint16_t mode,
                int16_t argc, char* argn[], char* argv[],
                NPSavedData* saved) {
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  NPObject *object = NPN_CreateObject(instance, Plugin::GetNPClass());
  if (object == NULL) {
    return NPERR_OUT_OF_MEMORY_ERROR;
  }

  instance->pdata = object;
  return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  Plugin* plugin = static_cast<Plugin*>(instance->pdata);
  if (plugin != NULL) {
    NPN_ReleaseObject(plugin);
  }
  return NPERR_NO_ERROR;
}

NPObject* NPP_GetScriptableInstance(NPP instance) {
  if (instance == NULL) {
    return NULL;
  }

  NPObject* object = static_cast<NPObject*>(instance->pdata);
  if (object) {
    NPN_RetainObject(object);
  }
  return object;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  if (window == NULL) {
    return NPERR_GENERIC_ERROR;
  }
  Plugin* plugin = static_cast<Plugin*>(instance->pdata);
  if (plugin != NULL) {
    return plugin->SetWindow(window);
  }
  return NPERR_GENERIC_ERROR;
}

int main(int argc, char **argv) {
  printf("Bubble demo\n");
  NaClNP_Init(&argc, argv);
  NaClNP_MainLoop(0);
  return 0;
}
#endif

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

  o3d::command_buffer::BubbleDemo demo;
  demo.SetHandle(0, htp_handle);
  demo.Initialize();
  for (;;) {
    // TODO(gman): Add a way out of this loop.
    demo.Render();
  }
  demo.Finalize();
  CloseConnection(htp_handle);
  return 0;
}


