// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_WEBGPU_IMPLEMENTATION_H_
#define GPU_COMMAND_BUFFER_CLIENT_WEBGPU_IMPLEMENTATION_H_

#include <dawn/webgpu.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/client/implementation_base.h"
#include "gpu/command_buffer/client/logging.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/client/webgpu_cmd_helper.h"
#include "gpu/command_buffer/client/webgpu_export.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "ui/gl/buildflags.h"

namespace gpu {
namespace webgpu {

class DawnWireServices;

class WEBGPU_EXPORT WebGPUImplementation final : public WebGPUInterface,
                                                 public ImplementationBase {
 public:
  explicit WebGPUImplementation(WebGPUCmdHelper* helper,
                                TransferBufferInterface* transfer_buffer,
                                GpuControl* gpu_control);

  WebGPUImplementation(const WebGPUImplementation&) = delete;
  WebGPUImplementation& operator=(const WebGPUImplementation&) = delete;

  ~WebGPUImplementation() override;

  gpu::ContextResult Initialize(const SharedMemoryLimits& limits);

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_implementation_autogen.h"

  // ContextSupport implementation.
  void SetAggressivelyFreeResources(bool aggressively_free_resources) override;
  uint64_t ShareGroupTracingGUID() const override;
  void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char*, int32_t)> callback) override;
  bool ThreadSafeShallowLockDiscardableTexture(uint32_t texture_id) override;
  void CompleteLockDiscardableTexureOnContextThread(
      uint32_t texture_id) override;
  bool ThreadsafeDiscardableTextureIsDeletedForTracing(
      uint32_t texture_id) override;
  void* MapTransferCacheEntry(uint32_t serialized_size) override;
  void UnmapAndCreateTransferCacheEntry(uint32_t type, uint32_t id) override;
  bool ThreadsafeLockTransferCacheEntry(uint32_t type, uint32_t id) override;
  void UnlockTransferCacheEntries(
      const std::vector<std::pair<uint32_t, uint32_t>>& entries) override;
  void DeleteTransferCacheEntry(uint32_t type, uint32_t id) override;
  unsigned int GetTransferBufferFreeSize() const override;
  bool IsJpegDecodeAccelerationSupported() const override;
  bool IsWebPDecodeAccelerationSupported() const override;
  bool CanDecodeWithHardwareAcceleration(
      const cc::ImageHeaderMetadata* image_metadata) const override;

  // InterfaceBase implementation.
  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void VerifySyncTokensCHROMIUM(GLbyte** sync_tokens, GLsizei count) override;
  void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) override;
  void ShallowFlushCHROMIUM() override;
  bool HasGrContextSupport() const override;

  // ImplementationBase implementation.
  void IssueShallowFlush() override;
  void SetGLError(GLenum error,
                  const char* function_name,
                  const char* msg) override;

  // GpuControlClient implementation.
  void OnGpuControlLostContext() final;
  void OnGpuControlLostContextMaybeReentrant() final;
  void OnGpuControlErrorMessage(const char* message, int32_t id) final;
  void OnGpuControlReturnData(base::span<const uint8_t> data) final;

  // WebGPUInterface implementation
  void FlushCommands() override;
  bool EnsureAwaitingFlush() override;
  void FlushAwaitingCommands() override;
  scoped_refptr<APIChannel> GetAPIChannel() const override;
  ReservedTexture ReserveTexture(WGPUDevice device) override;
  WGPUDevice DeprecatedEnsureDefaultDeviceSync() override;

 private:
  const char* GetLogPrefix() const { return "webgpu"; }
  void CheckGLError() {}
  void LoseContext();

  raw_ptr<WebGPUCmdHelper> helper_;
#if BUILDFLAG(USE_DAWN)
  scoped_refptr<DawnWireServices> dawn_wire_;
#endif

  LogSettings log_settings_;

  std::atomic_bool lost_{false};
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_IMPLEMENTATION_H_
