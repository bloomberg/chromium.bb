// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/tests/webgpu_test.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

namespace gpu {
namespace {

class MockBufferMapReadCallback {
 public:
  MOCK_METHOD4(Call,
               void(WGPUBufferMapAsyncStatus status,
                    const uint32_t* ptr,
                    uint64_t data_length,
                    void* userdata));
};

std::unique_ptr<testing::StrictMock<MockBufferMapReadCallback>>
    mock_buffer_map_read_callback;
void ToMockBufferMapReadCallback(WGPUBufferMapAsyncStatus status,
                                 const void* ptr,
                                 uint64_t data_length,
                                 void* userdata) {
  // Assume the data is uint32_t
  mock_buffer_map_read_callback->Call(status, static_cast<const uint32_t*>(ptr),
                                      data_length, userdata);
}

}  // namespace

class SharedImageGLBackingProduceDawnTest : public WebGPUTest {
 protected:
  void SetUp() override {
    WebGPUTest::SetUp();
    WebGPUTest::Options option;
    Initialize(option);

    gpu::ContextCreationAttribs attributes;
    attributes.alpha_size = 8;
    attributes.depth_size = 24;
    attributes.red_size = 8;
    attributes.green_size = 8;
    attributes.blue_size = 8;
    attributes.stencil_size = 8;
    attributes.samples = 4;
    attributes.sample_buffers = 1;
    attributes.bind_generates_resource = false;

    gl_context_ = std::make_unique<GLInProcessContext>();
    ContextResult result = gl_context_->Initialize(
        GetGpuServiceHolder()->task_executor(), nullptr, true,
        gpu::kNullSurfaceHandle, attributes, option.shared_memory_limits,
        nullptr, nullptr, base::ThreadTaskRunnerHandle::Get());
    ASSERT_EQ(result, ContextResult::kSuccess);
    mock_buffer_map_read_callback =
        std::make_unique<testing::StrictMock<MockBufferMapReadCallback>>();
  }

  void TearDown() override {
    WebGPUTest::TearDown();
    gl_context_.reset();
    mock_buffer_map_read_callback = nullptr;
  }

  bool ShouldSkipTest() {
// Windows is the only platform enabled passthrough in this test.
#if defined(OS_WIN)
    return false;
#else
    return true;
#endif  // defined(OS_WIN)
  }

  gles2::GLES2Implementation* gl() { return gl_context_->GetImplementation(); }

  std::unique_ptr<GLInProcessContext> gl_context_;
};

// Tests using Associate/DissociateMailbox to share an image with Dawn.
// For simplicity of the test the image is shared between a Dawn device and
// itself: we render to it using the Dawn device, then re-associate it to a
// Dawn texture and read back the values that were written.
TEST_F(SharedImageGLBackingProduceDawnTest, Basic) {
  if (ShouldSkipTest())
    return;
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }
  if (!WebGPUSharedImageSupported()) {
    LOG(ERROR) << "Test skipped because WebGPUSharedImage isn't supported";
    return;
  }

  // Create the shared image
  SharedImageInterface* sii = gl_context_->GetSharedImageInterface();
  Mailbox gl_mailbox = sii->CreateSharedImage(
      viz::ResourceFormat::RGBA_8888, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      SHARED_IMAGE_USAGE_GLES2);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  gl()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());
  GLuint texture =
      gl()->CreateAndTexStorage2DSharedImageCHROMIUM(gl_mailbox.name);

  gl()->BeginSharedImageAccessDirectCHROMIUM(
      texture, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  GLuint fbo = 0;
  gl()->GenFramebuffers(1, &fbo);
  gl()->BindFramebuffer(GL_FRAMEBUFFER, fbo);

  // Attach the texture to FBO.
  gl()->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, /* Hard code */
                             texture, 0);

  // Set the clear color to green.
  gl()->ClearColor(0.0f, 1.0f, 0.0f, 1.0f);
  gl()->Clear(GL_COLOR_BUFFER_BIT);
  gl()->EndSharedImageAccessDirectCHROMIUM(texture);

  SyncToken gl_op_token;
  gl()->GenUnverifiedSyncTokenCHROMIUM(gl_op_token.GetData());
  webgpu()->WaitSyncTokenCHROMIUM(gl_op_token.GetConstData());

  DeviceAndClientID device_and_id = GetNewDeviceAndClientID();
  wgpu::Device device = device_and_id.device;
  webgpu::DawnDeviceClientID device_client_id = device_and_id.client_id;

  {
    // Register the shared image as a Dawn texture in the wire.
    gpu::webgpu::ReservedTexture reservation =
        webgpu()->ReserveTexture(device_client_id);

    webgpu()->AssociateMailbox(device_client_id, 0, reservation.id,
                               reservation.generation, WGPUTextureUsage_CopySrc,
                               reinterpret_cast<GLbyte*>(&gl_mailbox));
    wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

    // Copy the texture in a mappable buffer.
    wgpu::BufferDescriptor buffer_desc;
    buffer_desc.size = 4;
    buffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer readback_buffer = device.CreateBuffer(&buffer_desc);

    wgpu::TextureCopyView copy_src;
    copy_src.texture = texture;
    copy_src.mipLevel = 0;
    copy_src.arrayLayer = 0;
    copy_src.origin = {0, 0, 0};

    wgpu::BufferCopyView copy_dst;
    copy_dst.buffer = readback_buffer;
    copy_dst.offset = 0;
    copy_dst.bytesPerRow = 256;
    copy_dst.rowsPerImage = 0;

    wgpu::Extent3D copy_size = {1, 1, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToBuffer(&copy_src, &copy_dst, &copy_size);
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetDefaultQueue();
    queue.Submit(1, &commands);

    webgpu()->DissociateMailbox(device_client_id, reservation.id,
                                reservation.generation);

    // Map the buffer and assert the pixel is the correct value.
    readback_buffer.MapReadAsync(ToMockBufferMapReadCallback, this);
    uint32_t buffer_contents = 0xFF00FF00;
    EXPECT_CALL(*mock_buffer_map_read_callback,
                Call(WGPUBufferMapAsyncStatus_Success,
                     testing::Pointee(testing::Eq(buffer_contents)),
                     sizeof(uint32_t), this))
        .Times(1);

    WaitForCompletion(device);
  }
}

}  // namespace gpu
