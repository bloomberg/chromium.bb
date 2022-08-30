// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/webgpu_decoder_impl.h"

#include <dawn/native/DawnNative.h>
#include <dawn/native/OpenGLBackend.h>
#include <dawn_platform/DawnPlatform.h>
#include <dawn_wire/WireServer.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/bits.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/webgpu_cmd_format.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/dawn_instance.h"
#include "gpu/command_buffer/service/dawn_platform.h"
#include "gpu/command_buffer/service/dawn_service_memory_transfer_service.h"
#include "gpu/command_buffer/service/dawn_service_serializer.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/webgpu/callback.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_surface_egl.h"

#if BUILDFLAG(IS_WIN)
#include <dawn/native/D3D12Backend.h>
#include <dawn/native/VulkanBackend.h>
#include "ui/gl/gl_angle_util_win.h"
#endif

namespace gpu {
namespace webgpu {

namespace {

static constexpr uint32_t kAllowedWritableMailboxTextureUsages =
    static_cast<uint32_t>(WGPUTextureUsage_CopyDst |
                          WGPUTextureUsage_RenderAttachment |
                          WGPUTextureUsage_StorageBinding);

static constexpr uint32_t kAllowedReadableMailboxTextureUsages =
    static_cast<uint32_t>(WGPUTextureUsage_CopySrc |
                          WGPUTextureUsage_TextureBinding);

static constexpr uint32_t kAllowedMailboxTextureUsages =
    kAllowedWritableMailboxTextureUsages | kAllowedReadableMailboxTextureUsages;

WGPUAdapterType PowerPreferenceToDawnAdapterType(
    WGPUPowerPreference power_preference) {
  switch (power_preference) {
    case WGPUPowerPreference_LowPower:
      return WGPUAdapterType_IntegratedGPU;
    case WGPUPowerPreference_HighPerformance:
    // Currently for simplicity we always choose discrete GPU as the device
    // related to default power preference.
    case WGPUPowerPreference_Undefined:
      return WGPUAdapterType_DiscreteGPU;
    default:
      NOTREACHED();
      return WGPUAdapterType_CPU;
  }
}

}  // namespace

class WebGPUDecoderImpl final : public WebGPUDecoder {
 public:
  WebGPUDecoderImpl(DecoderClient* client,
                    CommandBufferServiceBase* command_buffer_service,
                    SharedImageManager* shared_image_manager,
                    MemoryTracker* memory_tracker,
                    gles2::Outputter* outputter,
                    const GpuPreferences& gpu_preferences,
                    scoped_refptr<SharedContextState> shared_context_state);

  WebGPUDecoderImpl(const WebGPUDecoderImpl&) = delete;
  WebGPUDecoderImpl& operator=(const WebGPUDecoderImpl&) = delete;

  ~WebGPUDecoderImpl() override;

  // WebGPUDecoder implementation
  ContextResult Initialize(const GpuFeatureInfo& gpu_feature_info) override;

  // DecoderContext implementation.
  base::WeakPtr<DecoderContext> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }
  const gles2::ContextState* GetContextState() override {
    NOTREACHED();
    return nullptr;
  }
  void Destroy(bool have_context) override;
  bool MakeCurrent() override {
    if (gl_context_.get()) {
      gl_context_->MakeCurrent(gl_surface_.get());
    }
    return true;
  }
  gl::GLContext* GetGLContext() override { return nullptr; }
  gl::GLSurface* GetGLSurface() override {
    NOTREACHED();
    return nullptr;
  }
  const gles2::FeatureInfo* GetFeatureInfo() const override {
    NOTREACHED();
    return nullptr;
  }
  Capabilities GetCapabilities() override { return {}; }
  void RestoreGlobalState() const override { NOTREACHED(); }
  void ClearAllAttributes() const override { NOTREACHED(); }
  void RestoreAllAttributes() const override { NOTREACHED(); }
  void RestoreState(const gles2::ContextState* prev_state) override {
    NOTREACHED();
  }
  void RestoreActiveTexture() const override { NOTREACHED(); }
  void RestoreAllTextureUnitAndSamplerBindings(
      const gles2::ContextState* prev_state) const override {
    NOTREACHED();
  }
  void RestoreActiveTextureUnitBinding(unsigned int target) const override {
    NOTREACHED();
  }
  void RestoreBufferBinding(unsigned int target) override { NOTREACHED(); }
  void RestoreBufferBindings() const override { NOTREACHED(); }
  void RestoreFramebufferBindings() const override { NOTREACHED(); }
  void RestoreRenderbufferBindings() override { NOTREACHED(); }
  void RestoreProgramBindings() const override { NOTREACHED(); }
  void RestoreTextureState(unsigned service_id) override { NOTREACHED(); }
  void RestoreTextureUnitBindings(unsigned unit) const override {
    NOTREACHED();
  }
  void RestoreVertexAttribArray(unsigned index) override { NOTREACHED(); }
  void RestoreAllExternalTextureBindingsIfNeeded() override { NOTREACHED(); }
  QueryManager* GetQueryManager() override {
    NOTREACHED();
    return nullptr;
  }
  void SetQueryCallback(unsigned int query_client_id,
                        base::OnceClosure callback) override {
    NOTREACHED();
  }
  gles2::GpuFenceManager* GetGpuFenceManager() override {
    NOTREACHED();
    return nullptr;
  }
  bool HasPendingQueries() const override { return false; }
  void ProcessPendingQueries(bool did_finish) override {}
  bool HasMoreIdleWork() const override { return false; }
  void PerformIdleWork() override {}

  bool HasPollingWork() const override {
    return has_polling_work_ || wire_serializer_->NeedsFlush();
  }

  void PerformPollingWork() override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                 "WebGPUDecoderImpl::PerformPollingWork");
    has_polling_work_ = false;
    if (known_device_metadata_.empty()) {
      wire_serializer_->Flush();
      return;
    }

    // Call DeviceTick() on all known devices, and prune any that are no longer
    // on the wire and do not need a tick.
    for (auto it = known_device_metadata_.begin();
         it != known_device_metadata_.end();) {
      WGPUDevice device = it->first;
      const bool known = wire_server_->IsDeviceKnown(device);
      const bool needs_tick = dawn::native::DeviceTick(device);
      if (needs_tick) {
        has_polling_work_ = true;
      }
      if (!known) {
        // The client has dropped all references and the device has been
        // removed from the wire.
        // Release the device and erase it from the map.
        dawn::native::GetProcs().deviceRelease(device);
        it = known_device_metadata_.erase(it);
      } else {
        ++it;
      }
    }
    wire_serializer_->Flush();
  }

  TextureBase* GetTextureBase(uint32_t client_id) override {
    NOTREACHED();
    return nullptr;
  }
  void SetLevelInfo(uint32_t client_id,
                    int level,
                    unsigned internal_format,
                    unsigned width,
                    unsigned height,
                    unsigned depth,
                    unsigned format,
                    unsigned type,
                    const gfx::Rect& cleared_rect) override {
    NOTREACHED();
  }
  bool WasContextLost() const override {
    NOTIMPLEMENTED();
    return false;
  }
  bool WasContextLostByRobustnessExtension() const override { return false; }
  void MarkContextLost(error::ContextLostReason reason) override {
    NOTIMPLEMENTED();
  }
  bool CheckResetStatus() override {
    NOTREACHED();
    return false;
  }
  void BeginDecoding() override {}
  void EndDecoding() override {}
  const char* GetCommandName(unsigned int command_id) const;
  error::Error DoCommands(unsigned int num_commands,
                          const volatile void* buffer,
                          int num_entries,
                          int* entries_processed) override;
  base::StringPiece GetLogPrefix() override { return "WebGPUDecoderImpl"; }
  void BindImage(uint32_t client_texture_id,
                 uint32_t texture_target,
                 gl::GLImage* image,
                 bool can_bind_to_sampler) override {
    NOTREACHED();
  }
  gles2::ContextGroup* GetContextGroup() override { return nullptr; }
  gles2::ErrorState* GetErrorState() override {
    NOTREACHED();
    return nullptr;
  }
  std::unique_ptr<gles2::AbstractTexture> CreateAbstractTexture(
      GLenum target,
      GLenum internal_format,
      GLsizei width,
      GLsizei height,
      GLsizei depth,
      GLint border,
      GLenum format,
      GLenum type) override {
    NOTREACHED();
    return nullptr;
  }
  bool IsCompressedTextureFormat(unsigned format) override {
    NOTREACHED();
    return false;
  }
  bool ClearLevel(gles2::Texture* texture,
                  unsigned target,
                  int level,
                  unsigned format,
                  unsigned type,
                  int xoffset,
                  int yoffset,
                  int width,
                  int height) override {
    NOTREACHED();
    return false;
  }
  bool ClearCompressedTextureLevel(gles2::Texture* texture,
                                   unsigned target,
                                   int level,
                                   unsigned format,
                                   int width,
                                   int height) override {
    NOTREACHED();
    return false;
  }
  bool ClearCompressedTextureLevel3D(gles2::Texture* texture,
                                     unsigned target,
                                     int level,
                                     unsigned format,
                                     int width,
                                     int height,
                                     int depth) override {
    NOTREACHED();
    return false;
  }
  bool ClearLevel3D(gles2::Texture* texture,
                    unsigned target,
                    int level,
                    unsigned format,
                    unsigned type,
                    int width,
                    int height,
                    int depth) override {
    NOTREACHED();
    return false;
  }
  bool initialized() const override { return true; }
  void SetLogCommands(bool log_commands) override { NOTIMPLEMENTED(); }
  gles2::Outputter* outputter() const override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  int GetRasterDecoderId() const override {
    NOTREACHED();
    return -1;
  }

 private:
  typedef error::Error (WebGPUDecoderImpl::*CmdHandler)(
      uint32_t immediate_data_size,
      const volatile void* data);

  // A struct to hold info about each command.
  struct CommandInfo {
    CmdHandler cmd_handler;
    uint8_t arg_flags;   // How to handle the arguments for this command
    uint8_t cmd_flags;   // How to handle this command
    uint16_t arg_count;  // How many arguments are expected for this command.
  };

  // A table of CommandInfo for all the commands.
  static const CommandInfo command_info[kNumCommands - kFirstWebGPUCommand];

// Generate a member function prototype for each command in an automated and
// typesafe way.
#define WEBGPU_CMD_OP(name) \
  Error Handle##name(uint32_t immediate_data_size, const volatile void* data);
  WEBGPU_COMMAND_LIST(WEBGPU_CMD_OP)
#undef WEBGPU_CMD_OP

  // The current decoder error communicates the decoder error through command
  // processing functions that do not return the error value. Should be set
  // only if not returning an error.
  error::Error current_decoder_error_ = error::kNoError;

  void DiscoverAdapters();

  int32_t GetPreferredAdapterIndex(WGPUPowerPreference power_preference,
                                   bool force_fallback) const;

  bool IsFeatureExposed(WGPUFeatureName feature) const;

  // Dawn wire uses procs which forward their calls to these methods.
  void RequestAdapterImpl(WGPUInstance instance,
                          const WGPURequestAdapterOptions* options,
                          WGPURequestAdapterCallback callback,
                          void* userdata);
  bool AdapterHasFeatureImpl(WGPUAdapter adapter, WGPUFeatureName feature);
  size_t AdapterEnumerateFeaturesImpl(WGPUAdapter adapter,
                                      WGPUFeatureName* features);
  void RequestDeviceImpl(WGPUAdapter adapter,
                         const WGPUDeviceDescriptor* descriptor,
                         WGPURequestDeviceCallback callback,
                         void* userdata);

  class SharedImageRepresentationAndAccess;

  std::unique_ptr<SharedImageRepresentationAndAccess> AssociateMailboxDawn(
      const Mailbox& mailbox,
      MailboxFlags flags,
      WGPUDevice device,
      WGPUBackendType backendType,
      WGPUTextureUsage usage);

  std::unique_ptr<SharedImageRepresentationAndAccess>
  AssociateMailboxUsingSkiaFallback(const Mailbox& mailbox,
                                    MailboxFlags flags,
                                    WGPUDevice device,
                                    WGPUTextureUsage usage);

  scoped_refptr<SharedContextState> shared_context_state_;
  const GrContextType gr_context_type_;

  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;

  std::unique_ptr<dawn::platform::Platform> dawn_platform_;
  std::unique_ptr<DawnInstance> dawn_instance_;
  std::unique_ptr<DawnServiceMemoryTransferService> memory_transfer_service_;
  std::vector<dawn::native::Adapter> dawn_adapters_;

  bool enable_unsafe_webgpu_ = false;
  WebGPUAdapterName use_webgpu_adapter_ = WebGPUAdapterName::kDefault;
  std::vector<std::string> force_enabled_toggles_;
  std::vector<std::string> force_disabled_toggles_;
  bool allow_unsafe_apis_;

  std::unique_ptr<dawn::wire::WireServer> wire_server_;
  std::unique_ptr<DawnServiceSerializer> wire_serializer_;

  // Helper class whose derived implementations holds a representation
  // and its ScopedAccess, ensuring safe destruction order.
  class SharedImageRepresentationAndAccess {
   public:
    virtual ~SharedImageRepresentationAndAccess() = default;
    // Get an unowned reference to the WGPUTexture for the shared image.
    virtual WGPUTexture texture() const = 0;
  };

  // Wraps a |SharedImageRepresentationDawn| as a WGPUTexture.
  class SharedImageRepresentationAndAccessDawn
      : public SharedImageRepresentationAndAccess {
   public:
    SharedImageRepresentationAndAccessDawn(
        std::unique_ptr<SharedImageRepresentationDawn> representation,
        std::unique_ptr<SharedImageRepresentationDawn::ScopedAccess> access)
        : representation_(std::move(representation)),
          access_(std::move(access)) {}

    WGPUTexture texture() const override { return access_->texture(); }

   private:
    std::unique_ptr<SharedImageRepresentationDawn> representation_;
    std::unique_ptr<SharedImageRepresentationDawn::ScopedAccess> access_;
  };

  // Wraps a |SharedImageRepresentationSkia| and exposes
  // it as a WGPUTexture by performing CPU readbacks/uploads.
  class SharedImageRepresentationAndAccessSkiaFallback
      : public SharedImageRepresentationAndAccess {
   public:
    static std::unique_ptr<SharedImageRepresentationAndAccessSkiaFallback>
    Create(scoped_refptr<SharedContextState> shared_context_state,
           std::unique_ptr<SharedImageRepresentationSkia> representation,
           const DawnProcTable& procs,
           WGPUDevice device,
           WGPUTextureUsage usage) {
      viz::ResourceFormat format = representation->format();
      // Include list of formats this is tested to work with.
      // See gpu/command_buffer/tests/webgpu_mailbox_unittest.cc
      switch (format) {
// TODO(crbug.com/1241369): Handle additional formats.
#if !BUILDFLAG(IS_MAC)
        case viz::ResourceFormat::RGBA_8888:
#endif  // !BUILDFLAG(IS_MAC)
        case viz::ResourceFormat::BGRA_8888:
        case viz::ResourceFormat::RGBA_F16:
          break;
        default:
          return nullptr;
      }

      // Make sure we can create a WebGPU texture for this format
      if (viz::ToWGPUFormat(format) == WGPUTextureFormat_Undefined) {
        return nullptr;
      }

      const bool is_initialized = representation->IsCleared();
      auto result =
          base::WrapUnique(new SharedImageRepresentationAndAccessSkiaFallback(
              std::move(shared_context_state), std::move(representation), procs,
              device, usage));
      if (is_initialized && !result->PopulateFromSkia()) {
        return nullptr;
      }
      return result;
    }

    ~SharedImageRepresentationAndAccessSkiaFallback() override {
      // If we have write access, flush any writes by uploading
      // into the SkSurface.
      if ((usage_ & kAllowedWritableMailboxTextureUsages) != 0) {
        if (UploadContentsToSkia()) {
          // Upload to skia was successful. Mark the contents as initialized.
          representation_->SetCleared();
        } else {
          DLOG(ERROR) << "Failed to write to SkSurface.";
        }
      }

      procs_.textureDestroy(texture_);
      procs_.textureRelease(texture_);
      procs_.deviceRelease(device_);
    }

    WGPUTexture texture() const override { return texture_; }

   private:
    SharedImageRepresentationAndAccessSkiaFallback(
        scoped_refptr<SharedContextState> shared_context_state,
        std::unique_ptr<SharedImageRepresentationSkia> representation,
        const DawnProcTable& procs,
        WGPUDevice device,
        WGPUTextureUsage usage)
        : shared_context_state_(std::move(shared_context_state)),
          representation_(std::move(representation)),
          procs_(procs),
          device_(device),
          usage_(usage) {
      // Create a WGPUTexture to hold the image contents.
      // It should be internally copyable so Chrome can internally perform
      // copies with it, but Javascript cannot (unless |usage| contains copy
      // src/dst).
      WGPUDawnTextureInternalUsageDescriptor internal_usage_desc = {
          .chain = {.sType = WGPUSType_DawnTextureInternalUsageDescriptor},
          .internalUsage =
              static_cast<WGPUTextureUsageFlags>(WGPUTextureUsage_CopyDst) |
              static_cast<WGPUTextureUsageFlags>(WGPUTextureUsage_CopySrc),
      };
      WGPUTextureDescriptor texture_desc = {
          .nextInChain = &internal_usage_desc.chain,
          .usage = static_cast<WGPUTextureUsageFlags>(usage),
          .dimension = WGPUTextureDimension_2D,
          .size = {static_cast<uint32_t>(representation_->size().width()),
                   static_cast<uint32_t>(representation_->size().height()), 1},
          .format = viz::ToWGPUFormat(representation_->format()),
          .mipLevelCount = 1,
          .sampleCount = 1,
      };

      procs_.deviceReference(device_);
      texture_ = procs_.deviceCreateTexture(device, &texture_desc);
      DCHECK(texture_);
    }

    bool ComputeStagingBufferParams(const SkImageInfo& image_info,
                                    uint32_t* bytes_per_row,
                                    size_t* buffer_size) const {
      DCHECK(bytes_per_row);
      DCHECK(buffer_size);

      base::CheckedNumeric<uint32_t> checked_bytes_per_row(
          image_info.bytesPerPixel());
      checked_bytes_per_row *= image_info.width();

      uint32_t packed_bytes_per_row;
      if (!checked_bytes_per_row.AssignIfValid(&packed_bytes_per_row)) {
        return false;
      }

      // Align up to 256, required by WebGPU buffer->texture and texture->buffer
      // copies.
      checked_bytes_per_row = base::bits::AlignUp(packed_bytes_per_row, 256);
      if (!checked_bytes_per_row.AssignIfValid(bytes_per_row)) {
        return false;
      }
      if (*bytes_per_row < packed_bytes_per_row) {
        // Overflow in AlignUp.
        return false;
      }

      base::CheckedNumeric<size_t> checked_buffer_size = checked_bytes_per_row;
      checked_buffer_size *= image_info.height();

      return checked_buffer_size.AssignIfValid(buffer_size);
    }

    bool PopulateFromSkia() {
      std::vector<GrBackendSemaphore> begin_semaphores;
      std::vector<GrBackendSemaphore> end_semaphores;
      auto scoped_read_access = representation_->BeginScopedReadAccess(
          &begin_semaphores, &end_semaphores);
      if (!scoped_read_access) {
        DLOG(ERROR) << "PopulateFromSkia: Couldn't begin shared image access";
        return false;
      }

      // Wait for any work that previously used the image.
      WaitForSemaphores(std::move(begin_semaphores));

      // Make an SkImage to read the image contents
      auto sk_image = scoped_read_access->CreateSkImage(
          shared_context_state_->gr_context());
      if (!sk_image) {
        DLOG(ERROR) << "Couldn't make SkImage";
        return false;
      }

      uint32_t bytes_per_row;
      size_t buffer_size;
      if (!ComputeStagingBufferParams(sk_image->imageInfo(), &bytes_per_row,
                                      &buffer_size)) {
        return false;
      }

      // Create a staging buffer to hold pixel data which will be uploaded into
      // a texture.
      WGPUBufferDescriptor buffer_desc = {
          .usage = WGPUBufferUsage_CopySrc,
          .size = buffer_size,
          .mappedAtCreation = true,
      };
      WGPUBuffer buffer = procs_.deviceCreateBuffer(device_, &buffer_desc);

      // Read back the Skia image contents into the staging buffer.
      void* dst_pointer =
          procs_.bufferGetMappedRange(buffer, 0, WGPU_WHOLE_MAP_SIZE);
      DCHECK(dst_pointer);
      if (!sk_image->readPixels(shared_context_state_->gr_context(),
                                sk_image->imageInfo(), dst_pointer,
                                bytes_per_row, 0, 0)) {
        procs_.bufferRelease(buffer);
        DLOG(ERROR) << "Failed to read from SkImage";
        return false;
      }
      // Unmap the buffer.
      procs_.bufferUnmap(buffer);

      // Transition the image back to the desired end state. This is used for
      // transitioning the image to the external queue for Vulkan/GL interop.
      if (auto end_state = scoped_read_access->TakeEndState()) {
        if (!shared_context_state_->gr_context()->setBackendTextureState(
                scoped_read_access->promise_image_texture()->backendTexture(),
                *end_state)) {
          DLOG(ERROR) << "setBackendTextureState() failed.";
          return false;
        }
      }

      // ReadPixels finished; signal the semaphores.
      SignalSemaphores(std::move(end_semaphores));

      // Copy from the staging WGPUBuffer into the WGPUTexture.
      WGPUDawnEncoderInternalUsageDescriptor internal_usage_desc = {
          .chain = {.sType = WGPUSType_DawnEncoderInternalUsageDescriptor},
          .useInternalUsages = true,
      };
      WGPUCommandEncoderDescriptor command_encoder_desc = {
          .nextInChain = &internal_usage_desc.chain,
      };
      WGPUCommandEncoder encoder =
          procs_.deviceCreateCommandEncoder(device_, &command_encoder_desc);
      WGPUImageCopyBuffer buffer_copy = {
          .layout =
              {
                  .bytesPerRow = bytes_per_row,
                  .rowsPerImage = WGPU_COPY_STRIDE_UNDEFINED,
              },
          .buffer = buffer,
      };
      WGPUImageCopyTexture texture_copy = {
          .texture = texture_,
      };
      WGPUExtent3D extent = {
          static_cast<uint32_t>(representation_->size().width()),
          static_cast<uint32_t>(representation_->size().height()), 1};
      procs_.commandEncoderCopyBufferToTexture(encoder, &buffer_copy,
                                               &texture_copy, &extent);
      WGPUCommandBuffer commandBuffer =
          procs_.commandEncoderFinish(encoder, nullptr);
      procs_.commandEncoderRelease(encoder);

      WGPUQueue queue = procs_.deviceGetQueue(device_);
      procs_.queueSubmit(queue, 1, &commandBuffer);
      procs_.commandBufferRelease(commandBuffer);
      procs_.queueRelease(queue);
      procs_.bufferRelease(buffer);

      return true;
    }

    bool UploadContentsToSkia() {
      std::vector<GrBackendSemaphore> begin_semaphores;
      std::vector<GrBackendSemaphore> end_semaphores;
      auto scoped_write_access = representation_->BeginScopedWriteAccess(
          &begin_semaphores, &end_semaphores,
          SharedImageRepresentation::AllowUnclearedAccess::kYes);
      if (!scoped_write_access) {
        DLOG(ERROR)
            << "UploadContentsToSkia: Couldn't begin shared image access";
        return false;
      }

      auto* surface = scoped_write_access->surface();

      uint32_t bytes_per_row;
      size_t buffer_size;
      if (!ComputeStagingBufferParams(surface->imageInfo(), &bytes_per_row,
                                      &buffer_size)) {
        return false;
      }

      // Create a staging buffer to read back from the texture.
      WGPUBufferDescriptor buffer_desc = {
          .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead,
          .size = buffer_size,
      };
      WGPUBuffer buffer = procs_.deviceCreateBuffer(device_, &buffer_desc);

      WGPUImageCopyTexture texture_copy = {
          .texture = texture_,
      };
      WGPUImageCopyBuffer buffer_copy = {
          .layout =
              {
                  .bytesPerRow = bytes_per_row,
                  .rowsPerImage = WGPU_COPY_STRIDE_UNDEFINED,
              },
          .buffer = buffer,
      };
      WGPUExtent3D extent = {
          static_cast<uint32_t>(representation_->size().width()),
          static_cast<uint32_t>(representation_->size().height()), 1};

      // Copy from the texture into the staging buffer.
      WGPUDawnEncoderInternalUsageDescriptor internal_usage_desc = {
          .chain = {.sType = WGPUSType_DawnEncoderInternalUsageDescriptor},
          .useInternalUsages = true,
      };
      WGPUCommandEncoderDescriptor command_encoder_desc = {
          .nextInChain = &internal_usage_desc.chain,
      };
      WGPUCommandEncoder encoder =
          procs_.deviceCreateCommandEncoder(device_, &command_encoder_desc);
      procs_.commandEncoderCopyTextureToBuffer(encoder, &texture_copy,
                                               &buffer_copy, &extent);
      WGPUCommandBuffer commandBuffer =
          procs_.commandEncoderFinish(encoder, nullptr);
      procs_.commandEncoderRelease(encoder);

      WGPUQueue queue = procs_.deviceGetQueue(device_);
      procs_.queueSubmit(queue, 1, &commandBuffer);
      procs_.commandBufferRelease(commandBuffer);
      procs_.queueRelease(queue);

      struct Userdata {
        bool map_complete = false;
        WGPUBufferMapAsyncStatus status;
      } userdata;

      // Map the staging buffer for read.
      procs_.bufferMapAsync(
          buffer, WGPUMapMode_Read, 0, WGPU_WHOLE_MAP_SIZE,
          [](WGPUBufferMapAsyncStatus status, void* void_userdata) {
            Userdata* userdata = static_cast<Userdata*>(void_userdata);
            userdata->status = status;
            userdata->map_complete = true;
          },
          &userdata);

      // While the map is in flight, wait for any work that previously used the
      // image.
      WaitForSemaphores(std::move(begin_semaphores));

      // Poll for the map to complete.
      while (!userdata.map_complete) {
        base::PlatformThread::Sleep(base::Milliseconds(1));
        procs_.deviceTick(device_);
      }

      if (userdata.status != WGPUBufferMapAsyncStatus_Success) {
        procs_.bufferRelease(buffer);
        return false;
      }
      const void* data =
          procs_.bufferGetConstMappedRange(buffer, 0, WGPU_WHOLE_MAP_SIZE);
      DCHECK(data);
      surface->writePixels(SkPixmap(surface->imageInfo(), data, bytes_per_row),
                           /*x*/ 0, /*y*/ 0);

      procs_.bufferRelease(buffer);

      // Transition the image back to the desired end state. This is used for
      // transitioning the image to the external queue for Vulkan/GL interop.
      if (auto end_state = scoped_write_access->TakeEndState()) {
        // It's ok to pass in empty GrFlushInfo here since SignalSemaphores()
        // will populate it with semaphores and call GrDirectContext::flush.
        scoped_write_access->surface()->flush(/*info=*/{}, end_state.get());
      }

      SignalSemaphores(std::move(end_semaphores));

      return true;
    }

    void WaitForSemaphores(std::vector<GrBackendSemaphore> semaphores) {
      if (semaphores.empty())
        return;

      bool wait_result = shared_context_state_->gr_context()->wait(
          semaphores.size(), semaphores.data(),
          /*deleteSemaphoresAfterWait=*/false);
      DCHECK(wait_result);
    }

    void SignalSemaphores(std::vector<GrBackendSemaphore> semaphores) {
      if (semaphores.empty())
        return;

      GrFlushInfo flush_info = {
          .fNumSemaphores = semaphores.size(),
          .fSignalSemaphores = semaphores.data(),
      };
      // Note: this is a no-op if vk_context_provider is null.
      AddVulkanCleanupTaskForSkiaFlush(
          shared_context_state_->vk_context_provider(), &flush_info);
      auto flush_result =
          shared_context_state_->gr_context()->flush(flush_info);
      DCHECK(flush_result == GrSemaphoresSubmitted::kYes);
      shared_context_state_->gr_context()->submit();
    }

    scoped_refptr<SharedContextState> shared_context_state_;
    std::unique_ptr<SharedImageRepresentationSkia> representation_;
    const DawnProcTable& procs_;
    WGPUDevice device_;
    WGPUTexture texture_;
    WGPUTextureUsage usage_;
  };

  // Map from the <ID, generation> pair for a wire texture to the shared image
  // representation and access for it.
  base::flat_map<std::tuple<uint32_t, uint32_t>,
                 std::unique_ptr<SharedImageRepresentationAndAccess>>
      associated_shared_image_map_;

  // A container of devices that we've seen on the wire, and their associated
  // metadata. Not all of them may be valid, so it gets pruned when
  // iterating through it in PerformPollingWork.
  struct DeviceMetadata {
    WGPUAdapterType adapterType;
    WGPUBackendType backendType;
  };
  std::unordered_map<WGPUDevice, DeviceMetadata> known_device_metadata_;

  bool has_polling_work_ = false;
  bool destroyed_ = false;

  scoped_refptr<gl::GLContext> gl_context_;
  scoped_refptr<gl::GLSurface> gl_surface_;

  base::WeakPtrFactory<WebGPUDecoderImpl> weak_ptr_factory_{this};
};

constexpr WebGPUDecoderImpl::CommandInfo WebGPUDecoderImpl::command_info[] = {
#define WEBGPU_CMD_OP(name)                                \
  {                                                        \
      &WebGPUDecoderImpl::Handle##name,                    \
      cmds::name::kArgFlags,                               \
      cmds::name::cmd_flags,                               \
      sizeof(cmds::name) / sizeof(CommandBufferEntry) - 1, \
  }, /* NOLINT */
    WEBGPU_COMMAND_LIST(WEBGPU_CMD_OP)
#undef WEBGPU_CMD_OP
};

// DawnWireServer is a wrapper around dawn::wire::WireServer which allows
// overriding some of the WGPU procs the server delegates calls to.
// It has a special feature that around HandleDawnCommands, its owning
// WebGPUDecoderImpl is stored in thread-local storage. This enables
// some of the overridden procs to be overridden with a WebGPUDecoderImpl
// member function. The proc will be set to a plain-old C function pointer,
// which loads the WebGPUDecoderImpl from thread-local storage and forwards
// the call to the member function.
class DawnWireServer : public dawn::wire::WireServer {
  // This variable is set to DawnWireServer's parent decoder during
  // execution of HandleCommands. It is cleared to nullptr after.
  thread_local static WebGPUDecoderImpl* tls_parent_decoder;

  // Base template for specialization.
  template <auto Method>
  struct ProcForDecoderMethodImpl;

 public:
  // Helper function for making a Proc from a WebGPUDecoderImpl method.
  template <auto Method>
  static auto ProcForDecoderMethod() {
    return ProcForDecoderMethodImpl<Method>{}();
  }

  template <typename... Procs>
  static std::unique_ptr<DawnWireServer> Create(
      WebGPUDecoderImpl* decoder,
      dawn::wire::CommandSerializer* serializer,
      dawn::wire::server::MemoryTransferService* memory_transfer_service,
      const DawnProcTable& procs) {
    dawn::wire::WireServerDescriptor descriptor = {};
    descriptor.procs = &procs;
    descriptor.serializer = serializer;
    descriptor.memoryTransferService = memory_transfer_service;

    return base::WrapUnique(new DawnWireServer(decoder, descriptor));
  }

  ~DawnWireServer() override = default;

  // Handle Dawn commands. Forward the call to the base class, but
  // set |tls_parent_decoder| around it.
  const volatile char* HandleCommands(const volatile char* commands,
                                      size_t size) override {
    tls_parent_decoder = decoder_;
    const volatile char* rv =
        dawn::wire::WireServer::HandleCommands(commands, size);
    tls_parent_decoder = nullptr;
    return rv;
  }

 private:
  DawnWireServer(WebGPUDecoderImpl* decoder,
                 const dawn::wire::WireServerDescriptor& desc)
      : dawn::wire::WireServer(desc), decoder_(decoder) {}

  // Specialization where |Method| is a WebGPUDecoderImpl member function.
  // Returns a proc which loads the decoder from thread-local storage
  // and forwards the call to the member function.
  template <typename R,
            typename... Args,
            R (WebGPUDecoderImpl::*Method)(Args...)>
  struct ProcForDecoderMethodImpl<Method> {
    using Proc = R (*)(Args...);
    Proc operator()() {
      return [](Args... args) -> R {
        WebGPUDecoderImpl* decoder = tls_parent_decoder;
        DCHECK(decoder);
        return (decoder->*Method)(std::forward<Args>(args)...);
      };
    }
  };

  raw_ptr<WebGPUDecoderImpl> decoder_;
};

thread_local WebGPUDecoderImpl* DawnWireServer::tls_parent_decoder = nullptr;

WebGPUDecoder* CreateWebGPUDecoderImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    SharedImageManager* shared_image_manager,
    MemoryTracker* memory_tracker,
    gles2::Outputter* outputter,
    const GpuPreferences& gpu_preferences,
    scoped_refptr<SharedContextState> shared_context_state) {
  return new WebGPUDecoderImpl(
      client, command_buffer_service, shared_image_manager, memory_tracker,
      outputter, gpu_preferences, std::move(shared_context_state));
}

WebGPUDecoderImpl::WebGPUDecoderImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    SharedImageManager* shared_image_manager,
    MemoryTracker* memory_tracker,
    gles2::Outputter* outputter,
    const GpuPreferences& gpu_preferences,
    scoped_refptr<SharedContextState> shared_context_state)
    : WebGPUDecoder(client, command_buffer_service, outputter),
      shared_context_state_(std::move(shared_context_state)),
      gr_context_type_(gpu_preferences.gr_context_type),
      shared_image_representation_factory_(
          std::make_unique<SharedImageRepresentationFactory>(
              shared_image_manager,
              memory_tracker)),
      dawn_platform_(new DawnPlatform()),
      dawn_instance_(
          DawnInstance::Create(dawn_platform_.get(), gpu_preferences)),
      memory_transfer_service_(new DawnServiceMemoryTransferService(this)),
      wire_serializer_(new DawnServiceSerializer(client)) {
  enable_unsafe_webgpu_ = gpu_preferences.enable_unsafe_webgpu;
  use_webgpu_adapter_ = gpu_preferences.use_webgpu_adapter;
  force_enabled_toggles_ = gpu_preferences.enabled_dawn_features_list;
  force_disabled_toggles_ = gpu_preferences.disabled_dawn_features_list;

  // Only allow unsafe APIs if the disallow_unsafe_apis toggle is explicitly
  // disabled.
  allow_unsafe_apis_ =
      std::find(force_disabled_toggles_.begin(), force_disabled_toggles_.end(),
                "disallow_unsafe_apis") != force_disabled_toggles_.end();

  DawnProcTable wire_procs = dawn::native::GetProcs();
  wire_procs.createInstance =
      [](const WGPUInstanceDescriptor*) -> WGPUInstance {
    CHECK(false);
    return nullptr;
  };
  wire_procs.instanceRequestAdapter = DawnWireServer::ProcForDecoderMethod<
      &WebGPUDecoderImpl::RequestAdapterImpl>();
  wire_procs.adapterHasFeature = DawnWireServer::ProcForDecoderMethod<
      &WebGPUDecoderImpl::AdapterHasFeatureImpl>();
  wire_procs.adapterEnumerateFeatures = DawnWireServer::ProcForDecoderMethod<
      &WebGPUDecoderImpl::AdapterEnumerateFeaturesImpl>();
  wire_procs.adapterRequestDevice = DawnWireServer::ProcForDecoderMethod<
      &WebGPUDecoderImpl::RequestDeviceImpl>();

  wire_server_ = DawnWireServer::Create(
      this, wire_serializer_.get(), memory_transfer_service_.get(), wire_procs);

  wire_server_->InjectInstance(dawn_instance_->Get(), 1, 0);
}

WebGPUDecoderImpl::~WebGPUDecoderImpl() {
  Destroy(false);
}

void WebGPUDecoderImpl::Destroy(bool have_context) {
  associated_shared_image_map_.clear();
  for (auto [device, _] : known_device_metadata_) {
    dawn::native::GetProcs().deviceRelease(device);
  }
  known_device_metadata_.clear();
  wire_server_ = nullptr;

  destroyed_ = true;
}

ContextResult WebGPUDecoderImpl::Initialize(
    const GpuFeatureInfo& gpu_feature_info) {
  if (kGpuFeatureStatusSoftware ==
      gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGPU]) {
    use_webgpu_adapter_ = WebGPUAdapterName::kSwiftShader;
  }

  if (use_webgpu_adapter_ == WebGPUAdapterName::kCompat) {
    gl_surface_ = new gl::SurfacelessEGL(gl::GLSurfaceEGL::GetGLDisplayEGL(),
                                         gfx::Size(1, 1));
    gl::GLContextAttribs attribs;
    attribs.client_major_es_version = 3;
    attribs.client_minor_es_version = 1;
    gl_context_ = new gl::GLContextEGL(nullptr);
    gl_context_->Initialize(gl_surface_.get(), attribs);
    gl_context_->MakeCurrent(gl_surface_.get());
  }
  DiscoverAdapters();
  return ContextResult::kSuccess;
}

bool WebGPUDecoderImpl::IsFeatureExposed(WGPUFeatureName feature) const {
  switch (feature) {
    case WGPUFeatureName_TimestampQuery:
    case WGPUFeatureName_PipelineStatisticsQuery:
    case WGPUFeatureName_ChromiumExperimentalDp4a:
    case WGPUFeatureName_DawnMultiPlanarFormats:
      return allow_unsafe_apis_;
    case WGPUFeatureName_Depth24UnormStencil8:
    case WGPUFeatureName_Depth32FloatStencil8:
    case WGPUFeatureName_TextureCompressionBC:
    case WGPUFeatureName_TextureCompressionETC2:
    case WGPUFeatureName_TextureCompressionASTC:
    case WGPUFeatureName_IndirectFirstInstance:
      return true;
    default:
      return false;
  }
}

void WebGPUDecoderImpl::RequestAdapterImpl(
    WGPUInstance instance,
    const WGPURequestAdapterOptions* options,
    WGPURequestAdapterCallback callback,
    void* userdata) {
  WGPURequestAdapterOptions default_options;
  if (options == nullptr) {
    default_options = {};
    options = &default_options;
  }

  bool force_fallback_adapter = options->forceFallbackAdapter;
  if (use_webgpu_adapter_ == WebGPUAdapterName::kSwiftShader) {
    force_fallback_adapter = true;
  }

  if (gr_context_type_ != GrContextType::kVulkan &&
      use_webgpu_adapter_ != WebGPUAdapterName::kCompat) {
#if BUILDFLAG(IS_LINUX)
    callback(WGPURequestAdapterStatus_Unavailable, nullptr,
             "WebGPU on Linux requires command-line flag "
             "--enable-features=Vulkan",
             userdata);
    return;
#endif  // BUILDFLAG(IS_LINUX)
  }

  int32_t requested_adapter_index = GetPreferredAdapterIndex(
      options->powerPreference, force_fallback_adapter);

  if (requested_adapter_index < 0) {
    // There are no adapters to return since webgpu is not supported here
    callback(WGPURequestAdapterStatus_Unavailable, nullptr,
             "No available adapters.", userdata);
    return;
  }

  // Currently we treat the index of the adapter in
  // dawn_adapters_ as the id of the adapter in the server side.
  DCHECK_LT(static_cast<size_t>(requested_adapter_index),
            dawn_adapters_.size());
  const dawn::native::Adapter& adapter =
      dawn_adapters_[requested_adapter_index];

  // TODO(crbug.com/1266550): Hide CPU adapters until WebGPU fallback adapters
  // are fully tested.
  WGPUAdapterProperties properties = {};
  adapter.GetProperties(&properties);
  if (properties.adapterType == WGPUAdapterType_CPU && !enable_unsafe_webgpu_) {
    callback(WGPURequestAdapterStatus_Unavailable, nullptr,
             "No available adapters.", userdata);
    return;
  }

  // Callback takes ownership of the reference. Add a ref to pass to the
  // callback.
  dawn::native::GetProcs().adapterReference(adapter.Get());
  callback(WGPURequestAdapterStatus_Success, adapter.Get(), nullptr, userdata);
}

bool WebGPUDecoderImpl::AdapterHasFeatureImpl(WGPUAdapter adapter,
                                              WGPUFeatureName feature) {
  if (!dawn::native::GetProcs().adapterHasFeature(adapter, feature)) {
    return false;
  }
  return IsFeatureExposed(feature);
}

size_t WebGPUDecoderImpl::AdapterEnumerateFeaturesImpl(
    WGPUAdapter adapter,
    WGPUFeatureName* features_out) {
  size_t count =
      dawn::native::GetProcs().adapterEnumerateFeatures(adapter, nullptr);
  std::vector<WGPUFeatureName> features(count);
  dawn::native::GetProcs().adapterEnumerateFeatures(adapter, features.data());

  auto it =
      std::partition(features.begin(), features.end(),
                     [&](WGPUFeatureName f) { return IsFeatureExposed(f); });
  count = std::distance(features.begin(), it);

  if (features_out != nullptr) {
    memcpy(features_out, features.data(), sizeof(WGPUFeatureName) * count);
  }
  return count;
}

void WebGPUDecoderImpl::RequestDeviceImpl(
    WGPUAdapter adapter,
    const WGPUDeviceDescriptor* descriptor,
    WGPURequestDeviceCallback callback,
    void* userdata) {
  // Copy the descriptor so we can modify it.
  WGPUDeviceDescriptor desc =
      descriptor != nullptr ? *descriptor : WGPUDeviceDescriptor{};
  DCHECK_EQ(desc.nextInChain, nullptr);

  std::vector<WGPUFeatureName> required_features;
  if (desc.requiredFeaturesCount) {
    required_features = {
        desc.requiredFeatures,
        desc.requiredFeatures + desc.requiredFeaturesCount,
    };

    // Check that no disallowed features were requested. They should be hidden
    // by AdapterEnumerateFeaturesImpl.
    for (const WGPUFeatureName& f : required_features) {
      if (!IsFeatureExposed(f)) {
        callback(WGPURequestDeviceStatus_Error, nullptr,
                 "Disallowed feature requested.", userdata);
        return;
      }
    }
  }

  // We need to request internal usage to be able to do operations with internal
  // SharedImage / interop methods that would need specific usages.
  required_features.push_back(WGPUFeatureName_DawnInternalUsages);

  // Always enable "multi-planar-formats" as long as available.
  if (dawn::native::GetProcs().adapterHasFeature(
          adapter, WGPUFeatureName_DawnMultiPlanarFormats)) {
    required_features.push_back(WGPUFeatureName_DawnMultiPlanarFormats);
  }

  desc.requiredFeatures = required_features.data();
  desc.requiredFeaturesCount = required_features.size();

  // If a new toggle is added here, ForceDawnTogglesForWebGPU() which collects
  // info for about:gpu should be updated as well.
  WGPUDawnTogglesDeviceDescriptor dawn_toggles = {};
  std::vector<const char*> force_enabled_toggles;
  std::vector<const char*> force_disabled_toggles;

  // Disallows usage of SPIR-V by default for security (we only ensure that WGSL
  // is secure), unless --enable-unsafe-webgpu is used.
  if (!enable_unsafe_webgpu_) {
    force_enabled_toggles.push_back("disallow_spirv");
  }
  for (const std::string& toggles : force_enabled_toggles_) {
    force_enabled_toggles.push_back(toggles.c_str());
  }
  for (const std::string& toggles : force_disabled_toggles_) {
    force_disabled_toggles.push_back(toggles.c_str());
  }
  dawn_toggles.forceEnabledToggles = force_enabled_toggles.data();
  dawn_toggles.forceEnabledTogglesCount = force_enabled_toggles.size();
  dawn_toggles.forceDisabledToggles = force_disabled_toggles.data();
  dawn_toggles.forceDisabledTogglesCount = force_disabled_toggles.size();
  dawn_toggles.chain.sType = WGPUSType_DawnTogglesDeviceDescriptor;
  desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&dawn_toggles);

  bool called = false;
  auto* bound_callback = BindWGPUOnceCallback(
      [](
          // bound arguments
          bool* called,                                 //
          WebGPUDecoderImpl* decoder,                   //
          WGPUAdapter adapter,                          //
          WGPURequestDeviceCallback original_callback,  //
          void* original_userdata,                      //
          // callback arguments, minus userdata
          WGPURequestDeviceStatus status,  //
          WGPUDevice device,               //
          const char* message              //
      ) {
        *called = true;
        // Forward to the original callback.
        original_callback(status, device, message, original_userdata);

        if (device) {
          // Intercept the response so we can add a device ref to the list of
          // known devices that we may need to call DeviceTick on.
          WGPUAdapterProperties properties{};
          dawn::native::GetProcs().adapterGetProperties(adapter, &properties);
          dawn::native::GetProcs().deviceReference(device);
          decoder->known_device_metadata_.emplace(
              device,
              DeviceMetadata{properties.adapterType, properties.backendType});
        }
      },
      &called, this, adapter, callback, userdata);

  dawn::native::GetProcs().adapterRequestDevice(
      adapter, &desc, bound_callback->UnboundCallback(),
      bound_callback->AsUserdata());
  // The callback must have been called synchronously. We could allow async
  // here, but it would require careful handling of the decoder lifetime.
  CHECK(called);
}

void WebGPUDecoderImpl::DiscoverAdapters() {
#if BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
  if (use_webgpu_adapter_ == WebGPUAdapterName::kCompat) {
    auto getProc = [](const char* pname) {
      return reinterpret_cast<void*>(eglGetProcAddress(pname));
    };
    dawn::native::opengl::AdapterDiscoveryOptionsES optionsES;
    optionsES.getProc = getProc;
    dawn_instance_->DiscoverAdapters(&optionsES);
  }
#endif
#if BUILDFLAG(IS_WIN)
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device) {
    // In the case where the d3d11 device is nullptr, we want to return a null
    // adapter
    return;
  }
  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device.As(&dxgi_device);
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_device->GetAdapter(&dxgi_adapter);
  dawn::native::d3d12::AdapterDiscoveryOptions options(std::move(dxgi_adapter));
  dawn_instance_->DiscoverAdapters(&options);

  // Also discover the SwiftShader adapter. It will be discovered by default
  // for other OSes in DiscoverDefaultAdapters.
  dawn::native::vulkan::AdapterDiscoveryOptions swiftShaderOptions;
  swiftShaderOptions.forceSwiftShader = true;
  dawn_instance_->DiscoverAdapters(&swiftShaderOptions);
#else
  dawn_instance_->DiscoverDefaultAdapters();
#endif

  std::vector<dawn::native::Adapter> adapters = dawn_instance_->GetAdapters();
  for (dawn::native::Adapter& adapter : adapters) {
    adapter.SetUseTieredLimits(true);

    WGPUAdapterProperties adapterProperties = {};
    adapter.GetProperties(&adapterProperties);

    const bool is_fallback_adapter =
        adapterProperties.adapterType == WGPUAdapterType_CPU &&
        adapterProperties.vendorID == 0x1AE0 &&
        adapterProperties.deviceID == 0xC0DE;

    // The adapter must be able to import external images, or it must be a
    // SwiftShader adapter. For SwiftShader, we will perform a manual
    // upload/readback to/from shared images.
    if (!(adapter.SupportsExternalImages() || is_fallback_adapter)) {
      continue;
    }

    if (use_webgpu_adapter_ == WebGPUAdapterName::kCompat) {
      if (adapterProperties.backendType == WGPUBackendType_OpenGLES) {
        dawn_adapters_.push_back(adapter);
      }
    } else if (adapterProperties.backendType != WGPUBackendType_Null &&
               adapterProperties.backendType != WGPUBackendType_OpenGL) {
      dawn_adapters_.push_back(adapter);
    }
  }
}

int32_t WebGPUDecoderImpl::GetPreferredAdapterIndex(
    WGPUPowerPreference power_preference,
    bool force_fallback) const {
  WGPUAdapterType preferred_adapter_type =
      PowerPreferenceToDawnAdapterType(power_preference);

  int32_t discrete_gpu_adapter_index = -1;
  int32_t integrated_gpu_adapter_index = -1;
  int32_t cpu_adapter_index = -1;
  int32_t unknown_adapter_index = -1;

  for (int32_t i = 0; i < static_cast<int32_t>(dawn_adapters_.size()); ++i) {
    const dawn::native::Adapter& adapter = dawn_adapters_[i];
    WGPUAdapterProperties adapterProperties = {};
    adapter.GetProperties(&adapterProperties);

    if (force_fallback &&
        adapterProperties.adapterType != WGPUAdapterType_CPU) {
      continue;
    }

    if (adapterProperties.adapterType == preferred_adapter_type) {
      return i;
    }
    switch (adapterProperties.adapterType) {
      case WGPUAdapterType_DiscreteGPU:
        discrete_gpu_adapter_index = i;
        break;
      case WGPUAdapterType_IntegratedGPU:
        integrated_gpu_adapter_index = i;
        break;
      case WGPUAdapterType_CPU:
        cpu_adapter_index = i;
        break;
      case WGPUAdapterType_Unknown:
        unknown_adapter_index = i;
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  // For now, we always prefer the discrete GPU
  if (discrete_gpu_adapter_index >= 0) {
    return discrete_gpu_adapter_index;
  }
  if (integrated_gpu_adapter_index >= 0) {
    return integrated_gpu_adapter_index;
  }
  if (cpu_adapter_index >= 0) {
    return cpu_adapter_index;
  }
  if (unknown_adapter_index >= 0) {
    return unknown_adapter_index;
  }
  return -1;
}

const char* WebGPUDecoderImpl::GetCommandName(unsigned int command_id) const {
  if (command_id >= kFirstWebGPUCommand && command_id < kNumCommands) {
    return webgpu::GetCommandName(static_cast<CommandId>(command_id));
  }
  return GetCommonCommandName(static_cast<cmd::CommandId>(command_id));
}

error::Error WebGPUDecoderImpl::DoCommands(unsigned int num_commands,
                                           const volatile void* buffer,
                                           int num_entries,
                                           int* entries_processed) {
  DCHECK(entries_processed);
  int commands_to_process = num_commands;
  error::Error result = error::kNoError;
  const volatile CommandBufferEntry* cmd_data =
      static_cast<const volatile CommandBufferEntry*>(buffer);
  int process_pos = 0;
  CommandId command = static_cast<CommandId>(0);

  while (process_pos < num_entries && result == error::kNoError &&
         commands_to_process--) {
    const unsigned int size = cmd_data->value_header.size;
    command = static_cast<CommandId>(cmd_data->value_header.command);

    if (size == 0) {
      result = error::kInvalidSize;
      break;
    }

    if (static_cast<int>(size) + process_pos > num_entries) {
      result = error::kOutOfBounds;
      break;
    }

    const unsigned int arg_count = size - 1;
    unsigned int command_index = command - kFirstWebGPUCommand;
    if (command_index < std::size(command_info)) {
      // Prevent all further WebGPU commands from being processed if the server
      // is destroyed.
      if (destroyed_) {
        result = error::kLostContext;
        break;
      }
      const CommandInfo& info = command_info[command_index];
      unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
      if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
          (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
        uint32_t immediate_data_size = (arg_count - info_arg_count) *
                                       sizeof(CommandBufferEntry);  // NOLINT
        result = (this->*info.cmd_handler)(immediate_data_size, cmd_data);
      } else {
        result = error::kInvalidArguments;
      }
    } else {
      result = DoCommonCommand(command, arg_count, cmd_data);
    }

    if (result == error::kNoError &&
        current_decoder_error_ != error::kNoError) {
      result = current_decoder_error_;
      current_decoder_error_ = error::kNoError;
    }

    if (result != error::kDeferCommandUntilLater) {
      process_pos += size;
      cmd_data += size;
    }
  }

  *entries_processed = process_pos;

  if (error::IsError(result)) {
    LOG(ERROR) << "Error: " << result << " for Command "
               << GetCommandName(command);
  }

  return result;
}

error::Error WebGPUDecoderImpl::HandleDawnCommands(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile webgpu::cmds::DawnCommands& c =
      *static_cast<const volatile webgpu::cmds::DawnCommands*>(cmd_data);
  uint32_t size = static_cast<uint32_t>(c.size);
  uint32_t commands_shm_id = static_cast<uint32_t>(c.commands_shm_id);
  uint32_t commands_shm_offset = static_cast<uint32_t>(c.commands_shm_offset);

  const volatile char* shm_commands = GetSharedMemoryAs<const volatile char*>(
      commands_shm_id, commands_shm_offset, size);
  if (shm_commands == nullptr) {
    return error::kOutOfBounds;
  }

  TRACE_EVENT_WITH_FLOW0(
      TRACE_DISABLED_BY_DEFAULT("gpu.dawn"), "DawnCommands",
      (static_cast<uint64_t>(commands_shm_id) << 32) + commands_shm_offset,
      TRACE_EVENT_FLAG_FLOW_IN);

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
               "WebGPUDecoderImpl::HandleDawnCommands", "bytes", size);

  if (!wire_server_->HandleCommands(shm_commands, size)) {
    return error::kLostContext;
  }

  // TODO(crbug.com/1174145): This is O(N) where N is the number of devices.
  // Multiple submits would be O(N*M). We should find a way to more
  // intelligently poll for work on only the devices that need it.
  PerformPollingWork();

  return error::kNoError;
}

std::unique_ptr<WebGPUDecoderImpl::SharedImageRepresentationAndAccess>
WebGPUDecoderImpl::AssociateMailboxDawn(const Mailbox& mailbox,
                                        MailboxFlags flags,
                                        WGPUDevice device,
                                        WGPUBackendType backendType,
                                        WGPUTextureUsage usage) {
  std::unique_ptr<SharedImageRepresentationDawn> shared_image =
      shared_image_representation_factory_->ProduceDawn(mailbox, device,
                                                        backendType);

  if (!shared_image) {
    DLOG(ERROR) << "AssociateMailbox: Couldn't produce shared image";
    return nullptr;
  }

#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_CHROMEOS)
  if (usage & WGPUTextureUsage_StorageBinding) {
    DLOG(ERROR) << "AssociateMailbox: WGPUTextureUsage_StorageBinding is NOT "
                   "supported yet.";
    return nullptr;
  }
#endif

  if (flags & WEBGPU_MAILBOX_DISCARD) {
    // Set contents to uncleared.
    shared_image->SetClearedRect(gfx::Rect());
  }

  std::unique_ptr<SharedImageRepresentationDawn::ScopedAccess> scoped_access =
      shared_image->BeginScopedAccess(
          usage, SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!scoped_access) {
    DLOG(ERROR) << "AssociateMailbox: Couldn't begin shared image access";
    return nullptr;
  }

  return std::make_unique<SharedImageRepresentationAndAccessDawn>(
      std::move(shared_image), std::move(scoped_access));
}

std::unique_ptr<WebGPUDecoderImpl::SharedImageRepresentationAndAccess>
WebGPUDecoderImpl::AssociateMailboxUsingSkiaFallback(const Mailbox& mailbox,
                                                     MailboxFlags flags,
                                                     WGPUDevice device,
                                                     WGPUTextureUsage usage) {
  // Produce a Skia image from the mailbox.
  std::unique_ptr<SharedImageRepresentationSkia> shared_image =
      shared_image_representation_factory_->ProduceSkia(
          mailbox, shared_context_state_.get());

  if (!shared_image) {
    DLOG(ERROR) << "AssociateMailbox: Couldn't produce shared image";
    return nullptr;
  }

  if (flags & WEBGPU_MAILBOX_DISCARD) {
    // Set contents to uncleared.
    shared_image->SetClearedRect(gfx::Rect());
  }

  return SharedImageRepresentationAndAccessSkiaFallback::Create(
      shared_context_state_, std::move(shared_image), dawn::native::GetProcs(),
      device, usage);
}

error::Error WebGPUDecoderImpl::HandleAssociateMailboxImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile webgpu::cmds::AssociateMailboxImmediate& c =
      *static_cast<const volatile webgpu::cmds::AssociateMailboxImmediate*>(
          cmd_data);
  uint32_t device_id = static_cast<uint32_t>(c.device_id);
  uint32_t device_generation = static_cast<uint32_t>(c.device_generation);
  uint32_t id = static_cast<uint32_t>(c.id);
  uint32_t generation = static_cast<uint32_t>(c.generation);
  WGPUTextureUsage usage = static_cast<WGPUTextureUsage>(c.usage);
  MailboxFlags flags = static_cast<MailboxFlags>(c.flags);

  // Unpack the mailbox
  if (sizeof(Mailbox) > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLbyte* mailbox_bytes =
      gles2::GetImmediateDataAs<volatile const GLbyte*>(c, sizeof(Mailbox),
                                                        immediate_data_size);
  if (mailbox_bytes == nullptr) {
    return error::kOutOfBounds;
  }
  Mailbox mailbox = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(mailbox_bytes));
  DLOG_IF(ERROR, !mailbox.Verify())
      << "AssociateMailbox was passed an invalid mailbox";

  if (usage & ~kAllowedMailboxTextureUsages) {
    DLOG(ERROR) << "AssociateMailbox: Invalid usage";
    return error::kInvalidArguments;
  }

  WGPUDevice device = wire_server_->GetDevice(device_id, device_generation);
  if (device == nullptr) {
    return error::kInvalidArguments;
  }

  std::unique_ptr<SharedImageRepresentationAndAccess> representation_and_access;
  auto it = known_device_metadata_.find(device);
  DCHECK(it != known_device_metadata_.end());
  if (it->second.adapterType == WGPUAdapterType_CPU) {
    representation_and_access =
        AssociateMailboxUsingSkiaFallback(mailbox, flags, device, usage);
  } else {
    representation_and_access = AssociateMailboxDawn(
        mailbox, flags, device, it->second.backendType, usage);
  }

  if (!representation_and_access) {
    return error::kInvalidArguments;
  }

  // Inject the texture in the dawn::wire::Server and remember which shared
  // image it is associated with.
  if (!wire_server_->InjectTexture(representation_and_access->texture(), id,
                                   generation, device_id, device_generation)) {
    DLOG(ERROR) << "AssociateMailbox: Invalid texture ID";
    return error::kInvalidArguments;
  }

  std::tuple<uint32_t, uint32_t> id_and_generation{id, generation};
  auto insertion = associated_shared_image_map_.emplace(
      id_and_generation, std::move(representation_and_access));

  // InjectTexture already validated that the (ID, generation) can't have been
  // registered before.
  DCHECK(insertion.second);
  return error::kNoError;
}

error::Error WebGPUDecoderImpl::HandleDissociateMailbox(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile webgpu::cmds::DissociateMailbox& c =
      *static_cast<const volatile webgpu::cmds::DissociateMailbox*>(cmd_data);
  uint32_t texture_id = static_cast<uint32_t>(c.texture_id);
  uint32_t texture_generation = static_cast<uint32_t>(c.texture_generation);

  std::tuple<uint32_t, uint32_t> id_and_generation{texture_id,
                                                   texture_generation};
  auto it = associated_shared_image_map_.find(id_and_generation);
  if (it == associated_shared_image_map_.end()) {
    DLOG(ERROR) << "DissociateMailbox: Invalid texture ID";
    return error::kInvalidArguments;
  }

  associated_shared_image_map_.erase(it);
  return error::kNoError;
}

error::Error WebGPUDecoderImpl::HandleDissociateMailboxForPresent(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile webgpu::cmds::DissociateMailboxForPresent& c =
      *static_cast<const volatile webgpu::cmds::DissociateMailboxForPresent*>(
          cmd_data);
  uint32_t device_id = static_cast<uint32_t>(c.device_id);
  uint32_t device_generation = static_cast<uint32_t>(c.device_generation);
  uint32_t texture_id = static_cast<uint32_t>(c.texture_id);
  uint32_t texture_generation = static_cast<uint32_t>(c.texture_generation);

  std::tuple<uint32_t, uint32_t> id_and_generation{texture_id,
                                                   texture_generation};
  auto it = associated_shared_image_map_.find(id_and_generation);
  if (it == associated_shared_image_map_.end()) {
    DLOG(ERROR) << "DissociateMailbox: Invalid texture ID";
    return error::kInvalidArguments;
  }

  WGPUDevice device = wire_server_->GetDevice(device_id, device_generation);
  if (device == nullptr) {
    return error::kInvalidArguments;
  }

  WGPUTexture texture = it->second->texture();
  DCHECK(texture);
  if (!dawn::native::IsTextureSubresourceInitialized(texture, 0, 1, 0, 1)) {
    // The compositor renders uninitialized textures as red. If the texture is
    // not initialized, we need to explicitly clear its contents to black.
    // TODO(crbug.com/1242712): Use the C++ WebGPU API.
    const auto& procs = dawn::native::GetProcs();
    WGPUTextureView view = procs.textureCreateView(texture, nullptr);

    WGPURenderPassColorAttachment color_attachment = {};
    color_attachment.view = view;
    color_attachment.loadOp = WGPULoadOp_Clear;
    color_attachment.storeOp = WGPUStoreOp_Store;
    color_attachment.clearColor = {0.0, 0.0, 0.0, 0.0};

    WGPURenderPassDescriptor render_pass_descriptor = {};
    render_pass_descriptor.colorAttachmentCount = 1;
    render_pass_descriptor.colorAttachments = &color_attachment;

    WGPUDawnEncoderInternalUsageDescriptor internal_usage_desc = {
        .chain = {.sType = WGPUSType_DawnEncoderInternalUsageDescriptor},
        .useInternalUsages = true,
    };
    WGPUCommandEncoderDescriptor command_encoder_desc = {
        .nextInChain = &internal_usage_desc.chain,
    };
    WGPUCommandEncoder encoder =
        procs.deviceCreateCommandEncoder(device, &command_encoder_desc);
    WGPURenderPassEncoder pass =
        procs.commandEncoderBeginRenderPass(encoder, &render_pass_descriptor);
    procs.renderPassEncoderEndPass(pass);
    WGPUCommandBuffer command_buffer =
        procs.commandEncoderFinish(encoder, nullptr);
    WGPUQueue queue = procs.deviceGetQueue(device);
    procs.queueSubmit(queue, 1, &command_buffer);
    procs.queueRelease(queue);
    procs.commandBufferRelease(command_buffer);
    procs.renderPassEncoderRelease(pass);
    procs.commandEncoderRelease(encoder);
    procs.textureViewRelease(view);

    DCHECK(dawn::native::IsTextureSubresourceInitialized(texture, 0, 1, 0, 1));
  }

  associated_shared_image_map_.erase(it);
  return error::kNoError;
}

}  // namespace webgpu
}  // namespace gpu
