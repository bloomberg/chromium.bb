// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/raster_decoder.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/color_space_transfer_cache_entry.h"
#include "cc/paint/paint_cache.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/transfer_cache_entry.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/debug_marker_manager.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/common/raster_cmd_ids.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gl_stream_texture_image.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_tex_image.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/raster_cmd_validation.h"
#include "gpu/command_buffer/service/raster_decoder_context_state.h"
#include "gpu/command_buffer/service/service_font_manager.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/wrapped_sk_image.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpaceXformCanvas.h"
#include "third_party/skia/include/core/SkDeferredDisplayListRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"

// Local versions of the SET_GL_ERROR macros
#define LOCAL_SET_GL_ERROR(error, function_name, msg) \
  ERRORSTATE_SET_GL_ERROR(state_.GetErrorState(), error, function_name, msg)
#define LOCAL_SET_GL_ERROR_INVALID_ENUM(function_name, value, label)          \
  ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(state_.GetErrorState(), function_name, \
                                       static_cast<uint32_t>(value), label)
#define LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(function_name)         \
  ERRORSTATE_COPY_REAL_GL_ERRORS_TO_WRAPPER(state_.GetErrorState(), \
                                            function_name)
#define LOCAL_PEEK_GL_ERROR(function_name) \
  ERRORSTATE_PEEK_GL_ERROR(state_.GetErrorState(), function_name)
#define LOCAL_CLEAR_REAL_GL_ERRORS(function_name) \
  ERRORSTATE_CLEAR_REAL_GL_ERRORS(state_.GetErrorState(), function_name)
#define LOCAL_PERFORMANCE_WARNING(msg) \
  PerformanceWarning(__FILE__, __LINE__, msg)
#define LOCAL_RENDER_WARNING(msg) RenderWarning(__FILE__, __LINE__, msg)

namespace gpu {
namespace raster {

namespace {

base::AtomicSequenceNumber g_raster_decoder_id;

class TextureMetadata {
 public:
  TextureMetadata(bool use_buffer,
                  gfx::BufferUsage buffer_usage,
                  viz::ResourceFormat format,
                  const Capabilities& caps)
      : use_buffer_(use_buffer),
        buffer_usage_(buffer_usage),
        format_(format),
        target_(CalcTarget(use_buffer, buffer_usage, format, caps)) {}
  TextureMetadata(const TextureMetadata& tmd) = default;

  bool use_buffer() const { return use_buffer_; }
  gfx::BufferUsage buffer_usage() const { return buffer_usage_; }
  viz::ResourceFormat format() const { return format_; }
  GLenum target() const { return target_; }

 private:
  static GLenum CalcTarget(bool use_buffer,
                           gfx::BufferUsage buffer_usage,
                           viz::ResourceFormat format,
                           const Capabilities& caps) {
    if (use_buffer) {
      gfx::BufferFormat buffer_format = viz::BufferFormat(format);
      return GetBufferTextureTarget(buffer_usage, buffer_format, caps);
    } else {
      return GL_TEXTURE_2D;
    }
  }

  const bool use_buffer_;
  const gfx::BufferUsage buffer_usage_;
  const viz::ResourceFormat format_;
  const GLenum target_;
};

// This class prevents any GL errors that occur when it is in scope from
// being reported to the client.
class ScopedGLErrorSuppressor {
 public:
  ScopedGLErrorSuppressor(const char* function_name,
                          gles2::ErrorState* error_state)
      : function_name_(function_name), error_state_(error_state) {
    ERRORSTATE_COPY_REAL_GL_ERRORS_TO_WRAPPER(error_state_, function_name_);
  }
  ~ScopedGLErrorSuppressor() {
    ERRORSTATE_CLEAR_REAL_GL_ERRORS(error_state_, function_name_);
  }

 private:
  const char* function_name_;
  gles2::ErrorState* error_state_;
  DISALLOW_COPY_AND_ASSIGN(ScopedGLErrorSuppressor);
};

// Temporarily changes a decoder's bound texture and restore it when this
// object goes out of scope. Also temporarily switches to using active texture
// unit zero in case the client has changed that to something invalid.
class ScopedTextureBinder {
 public:
  ScopedTextureBinder(gles2::ContextState* state,
                      gles2::TextureManager* texture_manager,
                      gles2::TextureRef* texture_ref,
                      GLenum target,
                      GrContext* gr_context,
                      bool state_is_dirty)
      : state_(state),
        target_(target),
        gr_context_(gr_context),
        state_is_dirty_(state_is_dirty) {
    auto* api = state->api();
    api->glActiveTextureFn(GL_TEXTURE0);
    gles2::Texture* texture = texture_ref->texture();
    if (texture->target() == 0)
      texture_manager->SetTarget(texture_ref, target);
    DCHECK_EQ(texture->target(), target_)
        << "Texture bound to more than 1 target.";
    api->glBindTextureFn(target_, texture_ref->service_id());
  }

  ~ScopedTextureBinder() {
    if (!state_is_dirty_)
      state_->api()->glBindTextureFn(target_, 0);
    if (gr_context_)
      gr_context_->resetContext(kTextureBinding_GrGLBackendState);
  }

 private:
  gles2::ContextState* state_;
  GLenum target_;
  GrContext* gr_context_;
  const bool state_is_dirty_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTextureBinder);
};

// Temporarily changes a decoder's PIXEL_UNPACK_BUFFER to 0 and set pixel
// unpack params to default, and restore them when this object goes out of
// scope.
class ScopedPixelUnpackState {
 public:
  explicit ScopedPixelUnpackState(gles2::ContextState* state) : state_(state) {
    DCHECK(state_);
    state_->PushTextureUnpackState();
  }
  ~ScopedPixelUnpackState() { state_->RestoreUnpackState(); }

 private:
  gles2::ContextState* state_;
  DISALLOW_COPY_AND_ASSIGN(ScopedPixelUnpackState);
};

// Commands that are whitelisted as OK to occur between BeginRasterCHROMIUM
// and EndRasterCHROMIUM. They do not invalidate GrContext state tracking.
bool AllowedBetweenBeginEndRaster(CommandId command) {
  switch (command) {
    case kCreateTransferCacheEntryINTERNAL:
    case kDeleteTransferCacheEntryINTERNAL:
    case kEndRasterCHROMIUM:
    case kFinish:
    case kFlush:
    case kGetError:
    case kRasterCHROMIUM:
    case kUnlockTransferCacheEntryINTERNAL:
      return true;
    default:
      return false;
  }
}

// Commands that do not require that GL state matches ContextState. Some
// are completely indifferent to GL state. Others require that GL state
// matches GrContext state tracking.
bool PermitsInconsistentContextState(CommandId command) {
  // Note that state restoration is expensive. If you're adding any new
  // command which is frequently used between multiple RasterCHROMIUMs for
  // tiled rasterization, make sure to add it to the whitelist below for
  // commands which don't need consistent GL state.
  switch (command) {
    case kBeginQueryEXT:
    case kBeginRasterCHROMIUMImmediate:
    case kCreateAndConsumeTextureINTERNALImmediate:
    case kCreateTransferCacheEntryINTERNAL:
    case kDeleteQueriesEXTImmediate:
    case kDeleteTexturesImmediate:
    case kDeleteTransferCacheEntryINTERNAL:
    case kEndQueryEXT:
    case kEndRasterCHROMIUM:
    case kFinish:
    case kFlush:
    case kGenQueriesEXTImmediate:
    case kGetError:
    case kInsertFenceSyncCHROMIUM:
    case kRasterCHROMIUM:
    case kSetActiveURLCHROMIUM:
    case kUnlockTransferCacheEntryINTERNAL:
    case kWaitSyncTokenCHROMIUM:
    case kTraceBeginCHROMIUM:
    case kTraceEndCHROMIUM:
    case kDeletePaintCacheTextBlobsINTERNALImmediate:
    case kDeletePaintCachePathsINTERNALImmediate:
    case kClearPaintCacheINTERNAL:
      return true;
    case kLoseContextCHROMIUM:
    case kCopySubTexture:
      return false;
    case kNumCommands:
    case kOneBeforeStartPoint:
      NOTREACHED();
      return false;
  }

  NOTREACHED();
  return false;
}

}  // namespace

// RasterDecoderImpl uses two separate state trackers (gpu::gles2::ContextState
// and GrContext) that cache the current GL driver state. Each class sees a
// fraction of the GL calls issued and can easily become inconsistent with GL
// state. We guard against that by resetting. But resetting is expensive, so we
// avoid it as much as possible. The argument for correctness is as follows:
//
// - GLES2Decoder: The GL state matches the ContextState before and after a
//   command is executed here. The interesting case is making a GLES2Decoder
//   current. If using a virtual context, we will restore state appropriately
//   when the GLES2Decoder is made current because of the call to
//   RasterDecoderImpl::GetContextState.
//
// - RasterDecoder: There are two cases to consider
//
//   Case 1: Making a RasterDecoder current. If we are using virtual contexts,
//     we will restore to |state_| and GrContext::resetContext because of
//     RasterDecoderImpl::{GetContextState,RestoreState}. If not, we will
//     restore to the previous GL state (either |state_| or GrContext consistent
//     with previous GL state).
//
//   Case 2a: Executing a PermitsInconsistentContextState command: Either the
//     command doesn't inspect/modify GL state (InsertSyncPoint,
//     CreateAndConsumeTexture) or it requires and maintains that GrContext
//     state tracking matches GL context state (e.g. *RasterCHROMIUM --- see
//     raster_decoder_context_state_->PessimisticallyResetGrContext).
//
//   Case 2b: Executing a command that is not whitelisted: We force GL state to
//     match |state_| as necessary (see |need_context_state_reset|) in
//     DoCommandsImpl with RestoreState(nullptr). This will call
//     GrContext::resetContext.
class RasterDecoderImpl final : public RasterDecoder,
                                public gles2::ErrorStateClient,
                                public ServiceFontManager::Client {
 public:
  RasterDecoderImpl(
      DecoderClient* client,
      CommandBufferServiceBase* command_buffer_service,
      gles2::Outputter* outputter,
      gles2::ContextGroup* group,
      scoped_refptr<RasterDecoderContextState> raster_decoder_context_state);
  ~RasterDecoderImpl() override;

  gles2::GLES2Util* GetGLES2Util() override { return &util_; }

  // DecoderContext implementation.
  base::WeakPtr<DecoderContext> AsWeakPtr() override;
  ContextResult Initialize(
      const scoped_refptr<gl::GLSurface>& surface,
      const scoped_refptr<gl::GLContext>& context,
      bool offscreen,
      const gles2::DisallowedFeatures& disallowed_features,
      const ContextCreationAttribs& attrib_helper) override;
  void Destroy(bool have_context) override;
  bool MakeCurrent() override;
  gl::GLContext* GetGLContext() override;
  gl::GLSurface* GetGLSurface() override;
  const gles2::FeatureInfo* GetFeatureInfo() const override {
    return feature_info_.get();
  }
  Capabilities GetCapabilities() override;
  const gles2::ContextState* GetContextState() override;
  void RestoreGlobalState() const override;
  void ClearAllAttributes() const override;
  void RestoreAllAttributes() const override;
  void RestoreState(const gles2::ContextState* prev_state) override;
  void RestoreActiveTexture() const override;
  void RestoreAllTextureUnitAndSamplerBindings(
      const gles2::ContextState* prev_state) const override;
  void RestoreActiveTextureUnitBinding(unsigned int target) const override;
  void RestoreBufferBinding(unsigned int target) override;
  void RestoreBufferBindings() const override;
  void RestoreFramebufferBindings() const override;
  void RestoreRenderbufferBindings() override;
  void RestoreProgramBindings() const override;
  void RestoreTextureState(unsigned service_id) const override;
  void RestoreTextureUnitBindings(unsigned unit) const override;
  void RestoreVertexAttribArray(unsigned index) override;
  void RestoreAllExternalTextureBindingsIfNeeded() override;
  QueryManager* GetQueryManager() override;
  void SetQueryCallback(unsigned int query_client_id,
                        base::OnceClosure callback) override;
  gles2::GpuFenceManager* GetGpuFenceManager() override;
  bool HasPendingQueries() const override;
  void ProcessPendingQueries(bool did_finish) override;
  bool HasMoreIdleWork() const override;
  void PerformIdleWork() override;
  bool HasPollingWork() const override;
  void PerformPollingWork() override;
  TextureBase* GetTextureBase(uint32_t client_id) override;
  void SetLevelInfo(uint32_t client_id,
                    int level,
                    unsigned internal_format,
                    unsigned width,
                    unsigned height,
                    unsigned depth,
                    unsigned format,
                    unsigned type,
                    const gfx::Rect& cleared_rect) override;
  bool WasContextLost() const override;
  bool WasContextLostByRobustnessExtension() const override;
  void MarkContextLost(error::ContextLostReason reason) override;
  bool CheckResetStatus() override;
  void BeginDecoding() override;
  void EndDecoding() override;
  const char* GetCommandName(unsigned int command_id) const;
  error::Error DoCommands(unsigned int num_commands,
                          const volatile void* buffer,
                          int num_entries,
                          int* entries_processed) override;
  base::StringPiece GetLogPrefix() override;
  void BindImage(uint32_t client_texture_id,
                 uint32_t texture_target,
                 gl::GLImage* image,
                 bool can_bind_to_sampler) override;
  gles2::ContextGroup* GetContextGroup() override;
  gles2::ErrorState* GetErrorState() override;
  std::unique_ptr<gles2::AbstractTexture> CreateAbstractTexture(
      GLenum target,
      GLenum internal_format,
      GLsizei width,
      GLsizei height,
      GLsizei depth,
      GLint border,
      GLenum format,
      GLenum type) override;
  bool IsCompressedTextureFormat(unsigned format) override;
  bool ClearLevel(gles2::Texture* texture,
                  unsigned target,
                  int level,
                  unsigned format,
                  unsigned type,
                  int xoffset,
                  int yoffset,
                  int width,
                  int height) override;
  bool ClearCompressedTextureLevel(gles2::Texture* texture,
                                   unsigned target,
                                   int level,
                                   unsigned format,
                                   int width,
                                   int height) override;
  bool ClearLevel3D(gles2::Texture* texture,
                    unsigned target,
                    int level,
                    unsigned format,
                    unsigned type,
                    int width,
                    int height,
                    int depth) override {
    NOTIMPLEMENTED();
    return false;
  }
  int DecoderIdForTest() override;
  ServiceTransferCache* GetTransferCacheForTest() override;
  void SetUpForRasterCHROMIUMForTest() override;
  void SetOOMErrorForTest() override;

  // ErrorClientState implementation.
  void OnContextLostError() override;
  void OnOutOfMemoryError() override;

  gles2::Logger* GetLogger() override;

  void SetIgnoreCachedStateForTest(bool ignore) override;
  gles2::ImageManager* GetImageManagerForTest() override;

  void SetCopyTextureResourceManagerForTest(
      gles2::CopyTextureCHROMIUMResourceManager* copy_texture_resource_manager)
      override;

  // ServiceFontManager::Client implementation.
  scoped_refptr<Buffer> GetShmBuffer(uint32_t shm_id) override;

 private:
  std::unordered_map<GLuint, TextureMetadata> texture_metadata_;
  TextureMetadata* GetTextureMetadata(GLuint client_id) {
    auto it = texture_metadata_.find(client_id);
    DCHECK(it != texture_metadata_.end()) << "Undefined texture id";
    return &it->second;
  }

  gl::GLApi* api() const { return state_.api(); }
  GrContext* gr_context() const {
    return raster_decoder_context_state_->gr_context;
  }
  ServiceTransferCache* transfer_cache() {
    return raster_decoder_context_state_->transfer_cache.get();
  }

  const gles2::FeatureInfo::FeatureFlags& features() const {
    return feature_info_->feature_flags();
  }

  const GpuDriverBugWorkarounds& workarounds() const {
    return feature_info_->workarounds();
  }

  bool IsRobustnessSupported() {
    return has_robustness_extension_ &&
           context_->WasAllocatedUsingRobustnessExtension();
  }

  const gl::GLVersionInfo& gl_version_info() {
    return feature_info_->gl_version_info();
  }

  MemoryTracker* memory_tracker() { return group_->memory_tracker(); }

  gles2::BufferManager* buffer_manager() { return group_->buffer_manager(); }

  const gles2::TextureManager* texture_manager() const {
    return group_->texture_manager();
  }

  gles2::TextureManager* texture_manager() { return group_->texture_manager(); }

  gles2::ImageManager* image_manager() { return group_->image_manager(); }

  // Creates a Texture for the given texture.
  gles2::TextureRef* CreateTexture(GLuint client_id, GLuint service_id) {
    return texture_manager()->CreateTexture(client_id, service_id);
  }

  // Gets the texture info for the given texture. Returns nullptr if none
  // exists.
  gles2::TextureRef* GetTexture(GLuint client_id) const {
    return texture_manager()->GetTexture(client_id);
  }

  // Deletes the texture info for the given texture.
  void RemoveTexture(GLuint client_id) {
    texture_manager()->RemoveTexture(client_id);

    auto texture_iter = texture_metadata_.find(client_id);
    DCHECK(texture_iter != texture_metadata_.end());

    texture_metadata_.erase(texture_iter);
  }

  // Set remaining commands to process to 0 to force DoCommands to return
  // and allow context preemption and GPU watchdog checks in
  // CommandExecutor().
  void ExitCommandProcessingEarly() { commands_to_process_ = 0; }

  template <bool DebugImpl>
  error::Error DoCommandsImpl(unsigned int num_commands,
                              const volatile void* buffer,
                              int num_entries,
                              int* entries_processed);

  void DoCreateAndConsumeTextureINTERNAL(GLuint client_id,
                                         bool use_buffer,
                                         gfx::BufferUsage buffer_usage,
                                         viz::ResourceFormat resource_format,
                                         const volatile GLbyte* key);
  void DeleteTexturesHelper(GLsizei n, const volatile GLuint* client_ids);
  bool GenQueriesEXTHelper(GLsizei n, const GLuint* client_ids);
  void DeleteQueriesEXTHelper(GLsizei n, const volatile GLuint* client_ids);
  void DoFinish();
  void DoFlush();
  void DoGetIntegerv(GLenum pname, GLint* params, GLsizei params_size);
  void DoTraceEndCHROMIUM();
  bool InitializeCopyTexImageBlitter();
  bool InitializeCopyTextureCHROMIUM();
  void DoCopySubTexture(GLuint source_id,
                        GLuint dest_id,
                        GLint xoffset,
                        GLint yoffset,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height);
  // If the texture has an image but that image is not bound or copied to the
  // texture, this will first attempt to bind it, and if that fails
  // CopyTexImage on it.
  void DoBindOrCopyTexImageIfNeeded(gles2::Texture* texture, GLenum textarget);
  void DoLoseContextCHROMIUM(GLenum current, GLenum other) { NOTIMPLEMENTED(); }
  void DoBeginRasterCHROMIUM(GLuint sk_color,
                             GLuint msaa_sample_count,
                             GLboolean can_use_lcd_text,
                             GLint color_type,
                             GLuint color_space_transfer_cache_id,
                             const volatile GLbyte* key);
  void DoRasterCHROMIUM(GLuint raster_shm_id,
                        GLuint raster_shm_offset,
                        GLsizeiptr raster_shm_size,
                        GLuint font_shm_id,
                        GLuint font_shm_offset,
                        GLsizeiptr font_shm_size);
  void DoEndRasterCHROMIUM();
  void DoCreateTransferCacheEntryINTERNAL(GLuint entry_type,
                                          GLuint entry_id,
                                          GLuint handle_shm_id,
                                          GLuint handle_shm_offset,
                                          GLuint data_shm_id,
                                          GLuint data_shm_offset,
                                          GLuint data_size);
  void DoUnlockTransferCacheEntryINTERNAL(GLuint entry_type, GLuint entry_id);
  void DoDeleteTransferCacheEntryINTERNAL(GLuint entry_type, GLuint entry_id);
  void RestoreStateForAttrib(GLuint attrib, bool restore_array_binding);
  void DeletePaintCacheTextBlobsINTERNALHelper(
      GLsizei n,
      const volatile GLuint* paint_cache_ids);
  void DeletePaintCachePathsINTERNALHelper(
      GLsizei n,
      const volatile GLuint* paint_cache_ids);
  void DoClearPaintCacheINTERNAL();

#if defined(NDEBUG)
  void LogClientServiceMapping(const char* /* function_name */,
                               GLuint /* client_id */,
                               GLuint /* service_id */) {}
  template <typename T>
  void LogClientServiceForInfo(T* /* info */,
                               GLuint /* client_id */,
                               const char* /* function_name */) {}
#else
  void LogClientServiceMapping(const char* function_name,
                               GLuint client_id,
                               GLuint service_id) {
    if (service_logging_) {
      VLOG(1) << "[" << logger_.GetLogPrefix() << "] " << function_name
              << ": client_id = " << client_id
              << ", service_id = " << service_id;
    }
  }
  template <typename T>
  void LogClientServiceForInfo(T* info,
                               GLuint client_id,
                               const char* function_name) {
    if (info) {
      LogClientServiceMapping(function_name, client_id, info->service_id());
    }
  }
#endif

// Generate a member function prototype for each command in an automated and
// typesafe way.
#define RASTER_CMD_OP(name) \
  Error Handle##name(uint32_t immediate_data_size, const volatile void* data);

  RASTER_COMMAND_LIST(RASTER_CMD_OP)
#undef RASTER_CMD_OP

  typedef error::Error (RasterDecoderImpl::*CmdHandler)(
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
  static const CommandInfo command_info[kNumCommands - kFirstRasterCommand];

  const int raster_decoder_id_;

  // Number of commands remaining to be processed in DoCommands().
  int commands_to_process_ = 0;

  bool supports_oop_raster_ = false;
  bool use_ddl_ = false;

  bool has_robustness_extension_ = false;
  bool context_was_lost_ = false;
  bool reset_by_robustness_extension_ = false;

  // The current decoder error communicates the decoder error through command
  // processing functions that do not return the error value. Should be set
  // only if not returning an error.
  error::Error current_decoder_error_ = error::kNoError;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;

  DecoderClient* client_;

  gles2::DebugMarkerManager debug_marker_manager_;
  gles2::Logger logger_;

  // The ContextGroup for this decoder uses to track resources.
  scoped_refptr<gles2::ContextGroup> group_;
  scoped_refptr<RasterDecoderContextState> raster_decoder_context_state_;
  std::unique_ptr<Validators> validators_;
  scoped_refptr<gles2::FeatureInfo> feature_info_;

  std::unique_ptr<QueryManager> query_manager_;

  // All the state for this context.
  gles2::ContextState state_;

  gles2::GLES2Util util_;

  // States related to each manager.
  gles2::DecoderTextureState texture_state_;
  gles2::DecoderFramebufferState framebuffer_state_;

  // An optional behaviour to lose the context and group when OOM.
  bool lose_context_when_out_of_memory_ = false;

  // Log extra info.
  bool service_logging_;

  std::unique_ptr<gles2::CopyTexImageResourceManager> copy_tex_image_blit_;
  std::unique_ptr<gles2::CopyTextureCHROMIUMResourceManager>
      copy_texture_chromium_;

  std::unique_ptr<gles2::GPUTracer> gpu_tracer_;
  const unsigned char* gpu_decoder_category_;
  static constexpr int gpu_trace_level_ = 2;
  bool gpu_trace_commands_ = false;
  bool gpu_debug_commands_ = false;

  // Raster helpers.
  scoped_refptr<ServiceFontManager> font_manager_;
  std::unique_ptr<SharedImageRepresentationSkia> shared_image_;
  sk_sp<SkSurface> sk_surface_;
  std::unique_ptr<cc::ServicePaintCache> paint_cache_;

  std::unique_ptr<SkDeferredDisplayListRecorder> recorder_;
  std::unique_ptr<SkCanvas> raster_canvas_;
  uint32_t raster_color_space_id_;
  std::vector<SkDiscardableHandleId> locked_handles_;

  // Tracing helpers.
  int raster_chromium_id_ = 0;

  base::WeakPtrFactory<DecoderContext> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RasterDecoderImpl);
};

constexpr RasterDecoderImpl::CommandInfo RasterDecoderImpl::command_info[] = {
#define RASTER_CMD_OP(name)                                \
  {                                                        \
      &RasterDecoderImpl::Handle##name,                    \
      cmds::name::kArgFlags,                               \
      cmds::name::cmd_flags,                               \
      sizeof(cmds::name) / sizeof(CommandBufferEntry) - 1, \
  }, /* NOLINT */
    RASTER_COMMAND_LIST(RASTER_CMD_OP)
#undef RASTER_CMD_OP
};

// static
RasterDecoder* RasterDecoder::Create(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    gles2::Outputter* outputter,
    gles2::ContextGroup* group,
    scoped_refptr<RasterDecoderContextState> raster_decoder_context_state) {
  return new RasterDecoderImpl(client, command_buffer_service, outputter, group,
                               std::move(raster_decoder_context_state));
}

RasterDecoder::RasterDecoder(CommandBufferServiceBase* command_buffer_service,
                             gles2::Outputter* outputter)
    : CommonDecoder(command_buffer_service), outputter_(outputter) {}

RasterDecoder::~RasterDecoder() {}

bool RasterDecoder::initialized() const {
  return initialized_;
}

TextureBase* RasterDecoder::GetTextureBase(uint32_t client_id) {
  return nullptr;
}

void RasterDecoder::SetLevelInfo(uint32_t client_id,
                                 int level,
                                 unsigned internal_format,
                                 unsigned width,
                                 unsigned height,
                                 unsigned depth,
                                 unsigned format,
                                 unsigned type,
                                 const gfx::Rect& cleared_rect) {}

void RasterDecoder::BeginDecoding() {}

void RasterDecoder::EndDecoding() {}

void RasterDecoder::SetLogCommands(bool log_commands) {
  log_commands_ = log_commands;
}

gles2::Outputter* RasterDecoder::outputter() const {
  return outputter_;
}

base::StringPiece RasterDecoder::GetLogPrefix() {
  return GetLogger()->GetLogPrefix();
}

RasterDecoderImpl::RasterDecoderImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    gles2::Outputter* outputter,
    gles2::ContextGroup* group,
    scoped_refptr<RasterDecoderContextState> raster_decoder_context_state)
    : RasterDecoder(command_buffer_service, outputter),
      raster_decoder_id_(g_raster_decoder_id.GetNext() + 1),
      client_(client),
      logger_(&debug_marker_manager_,
              base::BindRepeating(&DecoderClient::OnConsoleMessage,
                                  base::Unretained(client_),
                                  0)),
      group_(group),
      raster_decoder_context_state_(std::move(raster_decoder_context_state)),
      validators_(new Validators),
      feature_info_(group_->feature_info()),
      state_(group_->feature_info(),
             this,
             &logger_,
             false /* track_texture_and_sampler_units */),
      texture_state_(group_->feature_info()->workarounds()),
      service_logging_(
          group_->gpu_preferences().enable_gpu_service_logging_gpu),
      gpu_decoder_category_(TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("gpu.decoder"))),
      font_manager_(base::MakeRefCounted<ServiceFontManager>(this)),
      weak_ptr_factory_(this) {
  DCHECK(raster_decoder_context_state_);
}

RasterDecoderImpl::~RasterDecoderImpl() {
  if (supports_oop_raster_)
    transfer_cache()->DeleteAllEntriesForDecoder(raster_decoder_id_);
}

base::WeakPtr<DecoderContext> RasterDecoderImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

ContextResult RasterDecoderImpl::Initialize(
    const scoped_refptr<gl::GLSurface>& surface,
    const scoped_refptr<gl::GLContext>& context,
    bool offscreen,
    const gles2::DisallowedFeatures& disallowed_features,
    const ContextCreationAttribs& attrib_helper) {
  TRACE_EVENT0("gpu", "RasterDecoderImpl::Initialize");
  DCHECK(context->IsCurrent(surface.get()));
  DCHECK(!context_.get());

  state_.set_api(gl::g_current_gl_context);

  set_initialized();

  if (!offscreen) {
    return ContextResult::kFatalFailure;
  }

  if (group_->gpu_preferences().enable_gpu_debugging)
    set_debug(true);

  if (group_->gpu_preferences().enable_gpu_command_logging)
    SetLogCommands(true);

  surface_ = surface;
  context_ = context;

  // Create GPU Tracer for timing values.
  gpu_tracer_.reset(new gles2::GPUTracer(this));

  // Save the loseContextWhenOutOfMemory context creation attribute.
  lose_context_when_out_of_memory_ =
      attrib_helper.lose_context_when_out_of_memory;

  auto result =
      group_->Initialize(this, attrib_helper.context_type, disallowed_features);
  if (result != ContextResult::kSuccess) {
    group_ =
        nullptr;  // Must not destroy ContextGroup if it is not initialized.
    Destroy(true);
    return result;
  }
  CHECK_GL_ERROR();

  // Support for CHROMIUM_texture_storage_image depends on the underlying
  // ImageFactory's ability to create anonymous images.
  gpu::ImageFactory* image_factory = group_->image_factory();
  if (image_factory && image_factory->SupportsCreateAnonymousImage())
    feature_info_->EnableCHROMIUMTextureStorageImage();

  state_.InitGenericAttribs(group_->max_vertex_attribs());

  query_manager_.reset(new QueryManager());

  has_robustness_extension_ = features().arb_robustness ||
                              features().khr_robustness ||
                              features().ext_robustness;

  // Set all the default state because some GL drivers get it wrong.
  // TODO(backer): Not all of this state needs to be initialized. Reduce the set
  // if perf becomes a problem.
  state_.InitCapabilities(nullptr);
  state_.InitState(nullptr);

  if (attrib_helper.enable_oop_rasterization) {
    if (!features().chromium_raster_transport) {
      LOG(ERROR) << "ContextResult::kFatalFailure: "
                    "chromium_raster_transport not present";
      Destroy(true);
      return ContextResult::kFatalFailure;
    }

    supports_oop_raster_ = !!raster_decoder_context_state_->gr_context;
    if (supports_oop_raster_)
      paint_cache_ = std::make_unique<cc::ServicePaintCache>();
    use_ddl_ = group_->gpu_preferences().enable_oop_rasterization_ddl;
  }

  return ContextResult::kSuccess;
}

void RasterDecoderImpl::Destroy(bool have_context) {
  if (!initialized())
    return;

  DCHECK(!have_context || context_->IsCurrent(nullptr));

  if (have_context) {
    if (copy_tex_image_blit_.get()) {
      copy_tex_image_blit_->Destroy();
      copy_tex_image_blit_.reset();
    }

    if (copy_texture_chromium_.get()) {
      copy_texture_chromium_->Destroy();
      copy_texture_chromium_.reset();
    }

    // Make sure we flush any pending skia work on this context.
    if (sk_surface_) {
      sk_surface_->flush();
      sk_surface_.reset();
    }
    if (gr_context()) {
      gr_context()->flush();
    }
  } else {
    if (group_ && group_->texture_manager()) {
      group_->texture_manager()->MarkContextLost();
    }

    state_.MarkContextLost();
  }

  // Unbind everything.
  state_.bound_pixel_pack_buffer = nullptr;
  state_.bound_pixel_unpack_buffer = nullptr;

  copy_tex_image_blit_.reset();
  copy_texture_chromium_.reset();

  if (query_manager_.get()) {
    query_manager_->Destroy(have_context);
    query_manager_.reset();
  }

  if (group_.get()) {
    group_->Destroy(this, have_context);
    group_ = nullptr;
  }

  // Destroy the surface before the context, some surface destructors make GL
  // calls.
  surface_ = nullptr;

  if (context_.get()) {
    context_->ReleaseCurrent(nullptr);
    context_ = nullptr;
  }

  font_manager_->Destroy();
  font_manager_.reset();
}

// Make this decoder's GL context current.
bool RasterDecoderImpl::MakeCurrent() {
  DCHECK(surface_);
  if (!context_.get())
    return false;

  if (WasContextLost()) {
    LOG(ERROR) << "  RasterDecoderImpl: Trying to make lost context current.";
    return false;
  }

  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "  RasterDecoderImpl: Context lost during MakeCurrent.";
    MarkContextLost(error::kMakeCurrentFailed);
    group_->LoseContexts(error::kUnknown);
    return false;
  }
  DCHECK_EQ(api(), gl::g_current_gl_context);

  if (CheckResetStatus()) {
    LOG(ERROR)
        << "  RasterDecoderImpl: Context reset detected after MakeCurrent.";
    group_->LoseContexts(error::kUnknown);
    return false;
  }

  // Rebind textures if the service ids may have changed.
  RestoreAllExternalTextureBindingsIfNeeded();

  return true;
}

gl::GLContext* RasterDecoderImpl::GetGLContext() {
  return context_.get();
}

gl::GLSurface* RasterDecoderImpl::GetGLSurface() {
  return surface_.get();
}

Capabilities RasterDecoderImpl::GetCapabilities() {
  Capabilities caps;
  caps.gpu_rasterization =
      group_->gpu_feature_info()
          .status_values[GPU_FEATURE_TYPE_GPU_RASTERIZATION] ==
      kGpuFeatureStatusEnabled;
  caps.supports_oop_raster = supports_oop_raster_;
  caps.gpu_memory_buffer_formats =
      feature_info_->feature_flags().gpu_memory_buffer_formats;
  caps.texture_target_exception_list =
      group_->gpu_preferences().texture_target_exception_list;
  caps.texture_format_bgra8888 =
      feature_info_->feature_flags().ext_texture_format_bgra8888;
  caps.texture_storage_image =
      feature_info_->feature_flags().chromium_texture_storage_image;
  caps.texture_storage = feature_info_->feature_flags().ext_texture_storage;
  caps.max_texture_size = texture_manager()->MaxSizeForTarget(GL_TEXTURE_2D);
  caps.sync_query = feature_info_->feature_flags().chromium_sync_query;

  if (gr_context()) {
    caps.context_supports_distance_field_text =
        gr_context()->supportsDistanceFieldText();
    caps.glyph_cache_max_texture_bytes =
        raster_decoder_context_state_->glyph_cache_max_texture_bytes;
  }
  return caps;
}

const gles2::ContextState* RasterDecoderImpl::GetContextState() {
  if (raster_decoder_context_state_->need_context_state_reset) {
    // Returning nullptr to force full state restoration by the caller.  We do
    // this because GrContext changes to GL state are untracked in our state_.
    return nullptr;
  }

  return &state_;
}

void RasterDecoderImpl::RestoreGlobalState() const {
  raster_decoder_context_state_->PessimisticallyResetGrContext();
  state_.RestoreGlobalState(nullptr);
}

void RasterDecoderImpl::ClearAllAttributes() const {}

void RasterDecoderImpl::RestoreAllAttributes() const {
  raster_decoder_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreState(const gles2::ContextState* prev_state) {
  TRACE_EVENT1("gpu", "RasterDecoderImpl::RestoreState", "context",
               logger_.GetLogPrefix());
  state_.RestoreState(prev_state);
  raster_decoder_context_state_->need_context_state_reset = false;
  raster_decoder_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreActiveTexture() const {
  raster_decoder_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreAllTextureUnitAndSamplerBindings(
    const gles2::ContextState* prev_state) const {
  raster_decoder_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreActiveTextureUnitBinding(
    unsigned int target) const {
  raster_decoder_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreBufferBinding(unsigned int target) {
  raster_decoder_context_state_->PessimisticallyResetGrContext();
  if (target == GL_PIXEL_PACK_BUFFER) {
    state_.UpdatePackParameters();
  } else if (target == GL_PIXEL_UNPACK_BUFFER) {
    state_.UpdateUnpackParameters();
  }
  gles2::Buffer* bound_buffer =
      buffer_manager()->GetBufferInfoForTarget(&state_, target);
  api()->glBindBufferFn(target, bound_buffer ? bound_buffer->service_id() : 0);
}

void RasterDecoderImpl::RestoreBufferBindings() const {
  raster_decoder_context_state_->PessimisticallyResetGrContext();
  state_.RestoreBufferBindings();
}

void RasterDecoderImpl::RestoreFramebufferBindings() const {
  raster_decoder_context_state_->PessimisticallyResetGrContext();
  state_.fbo_binding_for_scissor_workaround_dirty = true;
  state_.stencil_state_changed_since_validation = true;

  if (workarounds().flush_on_framebuffer_change)
    api()->glFlushFn();
}

void RasterDecoderImpl::RestoreRenderbufferBindings() {
  raster_decoder_context_state_->PessimisticallyResetGrContext();
  state_.RestoreRenderbufferBindings();
}

void RasterDecoderImpl::RestoreProgramBindings() const {
  raster_decoder_context_state_->PessimisticallyResetGrContext();
  state_.RestoreProgramSettings(nullptr, false);
}

void RasterDecoderImpl::RestoreTextureState(unsigned service_id) const {
  raster_decoder_context_state_->PessimisticallyResetGrContext();
  gles2::Texture* texture =
      texture_manager()->GetTextureForServiceId(service_id);
  if (texture) {
    GLenum target = texture->target();
    api()->glBindTextureFn(target, service_id);
    api()->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, texture->wrap_s());
    api()->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, texture->wrap_t());
    api()->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER,
                             texture->min_filter());
    api()->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER,
                             texture->mag_filter());
    if (feature_info_->IsWebGL2OrES3Context()) {
      api()->glTexParameteriFn(target, GL_TEXTURE_BASE_LEVEL,
                               texture->base_level());
    }
  }
}

void RasterDecoderImpl::RestoreTextureUnitBindings(unsigned unit) const {
  raster_decoder_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreVertexAttribArray(unsigned index) {
  NOTIMPLEMENTED();
}

void RasterDecoderImpl::RestoreAllExternalTextureBindingsIfNeeded() {
  raster_decoder_context_state_->PessimisticallyResetGrContext();
}

QueryManager* RasterDecoderImpl::GetQueryManager() {
  return query_manager_.get();
}

void RasterDecoderImpl::SetQueryCallback(unsigned int query_client_id,
                                         base::OnceClosure callback) {
  QueryManager::Query* query = query_manager_->GetQuery(query_client_id);
  if (query) {
    query->AddCallback(std::move(callback));
  } else {
    VLOG(1) << "RasterDecoderImpl::SetQueryCallback: No query with ID "
            << query_client_id << ". Running the callback immediately.";
    std::move(callback).Run();
  }
}

gles2::GpuFenceManager* RasterDecoderImpl::GetGpuFenceManager() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool RasterDecoderImpl::HasPendingQueries() const {
  return query_manager_.get() && query_manager_->HavePendingQueries();
}

void RasterDecoderImpl::ProcessPendingQueries(bool did_finish) {
  if (!query_manager_.get())
    return;
  query_manager_->ProcessPendingQueries(did_finish);
}

bool RasterDecoderImpl::HasMoreIdleWork() const {
  return gpu_tracer_->HasTracesToProcess();
}

void RasterDecoderImpl::PerformIdleWork() {
  gpu_tracer_->ProcessTraces();
}

bool RasterDecoderImpl::HasPollingWork() const {
  return false;
}

void RasterDecoderImpl::PerformPollingWork() {}

TextureBase* RasterDecoderImpl::GetTextureBase(uint32_t client_id) {
  NOTIMPLEMENTED();
  return nullptr;
}

void RasterDecoderImpl::SetLevelInfo(uint32_t client_id,
                                     int level,
                                     unsigned internal_format,
                                     unsigned width,
                                     unsigned height,
                                     unsigned depth,
                                     unsigned format,
                                     unsigned type,
                                     const gfx::Rect& cleared_rect) {
  NOTIMPLEMENTED();
}

bool RasterDecoderImpl::WasContextLost() const {
  return context_was_lost_;
}

bool RasterDecoderImpl::WasContextLostByRobustnessExtension() const {
  return WasContextLost() && reset_by_robustness_extension_;
}

void RasterDecoderImpl::MarkContextLost(error::ContextLostReason reason) {
  // Only lose the context once.
  if (WasContextLost())
    return;

  // Don't make GL calls in here, the context might not be current.
  command_buffer_service()->SetContextLostReason(reason);
  current_decoder_error_ = error::kLostContext;
  context_was_lost_ = true;

  state_.MarkContextLost();
  raster_decoder_context_state_->context_lost = true;

  if (gr_context())
    gr_context()->abandonContext();
}

bool RasterDecoderImpl::CheckResetStatus() {
  DCHECK(!WasContextLost());
  DCHECK(context_->IsCurrent(nullptr));

  if (IsRobustnessSupported()) {
    // If the reason for the call was a GL error, we can try to determine the
    // reset status more accurately.
    GLenum driver_status = api()->glGetGraphicsResetStatusARBFn();
    if (driver_status == GL_NO_ERROR)
      return false;

    LOG(ERROR) << "RasterDecoder context lost via ARB/EXT_robustness. Reset "
                  "status = "
               << gles2::GLES2Util::GetStringEnum(driver_status);

    // Don't pretend we know which client was responsible.
    if (workarounds().use_virtualized_gl_contexts)
      driver_status = GL_UNKNOWN_CONTEXT_RESET_ARB;

    switch (driver_status) {
      case GL_GUILTY_CONTEXT_RESET_ARB:
        MarkContextLost(error::kGuilty);
        break;
      case GL_INNOCENT_CONTEXT_RESET_ARB:
        MarkContextLost(error::kInnocent);
        break;
      case GL_UNKNOWN_CONTEXT_RESET_ARB:
        MarkContextLost(error::kUnknown);
        break;
      default:
        NOTREACHED();
        return false;
    }
    reset_by_robustness_extension_ = true;
    return true;
  }
  return false;
}

gles2::Logger* RasterDecoderImpl::GetLogger() {
  return &logger_;
}

void RasterDecoderImpl::SetIgnoreCachedStateForTest(bool ignore) {
  state_.SetIgnoreCachedStateForTest(ignore);
}

gles2::ImageManager* RasterDecoderImpl::GetImageManagerForTest() {
  return group_->image_manager();
}

void RasterDecoderImpl::SetCopyTextureResourceManagerForTest(
    gles2::CopyTextureCHROMIUMResourceManager* copy_texture_resource_manager) {
  copy_texture_chromium_.reset(copy_texture_resource_manager);
}

void RasterDecoderImpl::BeginDecoding() {
  gpu_tracer_->BeginDecoding();
  gpu_trace_commands_ = gpu_tracer_->IsTracing() && *gpu_decoder_category_;
  gpu_debug_commands_ = log_commands() || debug() || gpu_trace_commands_;
  query_manager_->BeginProcessingCommands();
}

void RasterDecoderImpl::EndDecoding() {
  gpu_tracer_->EndDecoding();
  query_manager_->EndProcessingCommands();
}

const char* RasterDecoderImpl::GetCommandName(unsigned int command_id) const {
  if (command_id >= kFirstRasterCommand && command_id < kNumCommands) {
    return raster::GetCommandName(static_cast<CommandId>(command_id));
  }
  return GetCommonCommandName(static_cast<cmd::CommandId>(command_id));
}

template <bool DebugImpl>
error::Error RasterDecoderImpl::DoCommandsImpl(unsigned int num_commands,
                                               const volatile void* buffer,
                                               int num_entries,
                                               int* entries_processed) {
  DCHECK(entries_processed);
  commands_to_process_ = num_commands;
  error::Error result = error::kNoError;
  const volatile CommandBufferEntry* cmd_data =
      static_cast<const volatile CommandBufferEntry*>(buffer);
  int process_pos = 0;
  CommandId command = static_cast<CommandId>(0);

  while (process_pos < num_entries && result == error::kNoError &&
         commands_to_process_--) {
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

    if (DebugImpl && log_commands()) {
      LOG(ERROR) << "[" << logger_.GetLogPrefix() << "]"
                 << "cmd: " << GetCommandName(command);
    }

    const unsigned int arg_count = size - 1;
    unsigned int command_index = command - kFirstRasterCommand;
    if (command_index < arraysize(command_info)) {
      const CommandInfo& info = command_info[command_index];
      if (sk_surface_) {
        if (!AllowedBetweenBeginEndRaster(command)) {
          LOCAL_SET_GL_ERROR(
              GL_INVALID_OPERATION, GetCommandName(command),
              "Unexpected command between BeginRasterCHROMIUM and "
              "EndRasterCHROMIUM");
          process_pos += size;
          cmd_data += size;
          continue;
        }
      }
      if (!PermitsInconsistentContextState(command)) {
        if (raster_decoder_context_state_->need_context_state_reset) {
          RestoreState(nullptr);
        }
      }
      unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
      if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
          (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
        bool doing_gpu_trace = false;
        if (DebugImpl && gpu_trace_commands_) {
          if (CMD_FLAG_GET_TRACE_LEVEL(info.cmd_flags) <= gpu_trace_level_) {
            doing_gpu_trace = true;
            gpu_tracer_->Begin(TRACE_DISABLED_BY_DEFAULT("gpu.decoder"),
                               GetCommandName(command), gles2::kTraceDecoder);
          }
        }

        uint32_t immediate_data_size = (arg_count - info_arg_count) *
                                       sizeof(CommandBufferEntry);  // NOLINT
        result = (this->*info.cmd_handler)(immediate_data_size, cmd_data);

        if (DebugImpl && doing_gpu_trace)
          gpu_tracer_->End(gles2::kTraceDecoder);

        if (DebugImpl && debug() && !WasContextLost()) {
          GLenum error;
          while ((error = api()->glGetErrorFn()) != GL_NO_ERROR) {
            LOG(ERROR) << "[" << logger_.GetLogPrefix() << "] "
                       << "GL ERROR: " << gles2::GLES2Util::GetStringEnum(error)
                       << " : " << GetCommandName(command);
            LOCAL_SET_GL_ERROR(error, "DoCommand", "GL error from driver");
          }
        }
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

  if (supports_oop_raster_)
    client_->ScheduleGrContextCleanup();

  return result;
}

error::Error RasterDecoderImpl::DoCommands(unsigned int num_commands,
                                           const volatile void* buffer,
                                           int num_entries,
                                           int* entries_processed) {
  if (gpu_debug_commands_) {
    return DoCommandsImpl<true>(num_commands, buffer, num_entries,
                                entries_processed);
  } else {
    return DoCommandsImpl<false>(num_commands, buffer, num_entries,
                                 entries_processed);
  }
}

base::StringPiece RasterDecoderImpl::GetLogPrefix() {
  return logger_.GetLogPrefix();
}

void RasterDecoderImpl::BindImage(uint32_t client_texture_id,
                                  uint32_t texture_target,
                                  gl::GLImage* image,
                                  bool can_bind_to_sampler) {
  NOTIMPLEMENTED();
}

gles2::ContextGroup* RasterDecoderImpl::GetContextGroup() {
  return group_.get();
}

gles2::ErrorState* RasterDecoderImpl::GetErrorState() {
  return state_.GetErrorState();
}

std::unique_ptr<gles2::AbstractTexture>
RasterDecoderImpl::CreateAbstractTexture(GLenum target,
                                         GLenum internal_format,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLint border,
                                         GLenum format,
                                         GLenum type) {
  return nullptr;
}

bool RasterDecoderImpl::IsCompressedTextureFormat(unsigned format) {
  return feature_info_->validators()->compressed_texture_format.IsValid(format);
}

bool RasterDecoderImpl::ClearLevel(gles2::Texture* texture,
                                   unsigned target,
                                   int level,
                                   unsigned format,
                                   unsigned type,
                                   int xoffset,
                                   int yoffset,
                                   int width,
                                   int height) {
  DCHECK(target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY &&
         target != GL_TEXTURE_EXTERNAL_OES);
  uint32_t channels = gles2::GLES2Util::GetChannelsForFormat(format);
  if (channels & gles2::GLES2Util::kDepth) {
    DCHECK(false) << "depth not supported";
    return false;
  }

  static constexpr uint32_t kMaxZeroSize = 1024 * 1024 * 4;

  uint32_t size;
  uint32_t padded_row_size;
  if (!gles2::GLES2Util::ComputeImageDataSizes(width, height, 1, format, type,
                                               state_.unpack_alignment, &size,
                                               nullptr, &padded_row_size)) {
    return false;
  }

  TRACE_EVENT1("gpu", "RasterDecoderImpl::ClearLevel", "size", size);

  int tile_height;

  if (size > kMaxZeroSize) {
    if (kMaxZeroSize < padded_row_size) {
      // That'd be an awfully large texture.
      return false;
    }
    // We should never have a large total size with a zero row size.
    DCHECK_GT(padded_row_size, 0U);
    tile_height = kMaxZeroSize / padded_row_size;
    if (!gles2::GLES2Util::ComputeImageDataSizes(width, tile_height, 1, format,
                                                 type, state_.unpack_alignment,
                                                 &size, nullptr, nullptr)) {
      return false;
    }
  } else {
    tile_height = height;
  }

  api()->glBindTextureFn(texture->target(), texture->service_id());
  {
    // Add extra scope to destroy zero and the object it owns right
    // after its usage.
    // Assumes the size has already been checked.
    std::unique_ptr<char[]> zero(new char[size]);
    memset(zero.get(), 0, size);

    ScopedPixelUnpackState reset_restore(&state_);
    GLint y = 0;
    while (y < height) {
      GLint h = y + tile_height > height ? height - y : tile_height;
      api()->glTexSubImage2DFn(
          target, level, xoffset, yoffset + y, width, h,
          gles2::TextureManager::AdjustTexFormat(feature_info_.get(), format),
          type, zero.get());
      y += tile_height;
    }
  }
  DCHECK(glGetError() == GL_NO_ERROR);

  if (gr_context()) {
    gr_context()->resetContext(kPixelStore_GrGLBackendState |
                               kTextureBinding_GrGLBackendState);
  }

  return true;
}

bool RasterDecoderImpl::ClearCompressedTextureLevel(gles2::Texture* texture,
                                                    unsigned target,
                                                    int level,
                                                    unsigned format,
                                                    int width,
                                                    int height) {
  DCHECK(target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY);
  // This code path can only be called if the texture was originally
  // allocated via TexStorage2D. Note that TexStorage2D is exposed
  // internally for ES 2.0 contexts, but compressed texture support is
  // not part of that exposure.
  DCHECK(feature_info_->IsWebGL2OrES3Context());

  GLsizei bytes_required = 0;
  std::string error_str;
  if (!GetCompressedTexSizeInBytes("ClearCompressedTextureLevel", width, height,
                                   1, format, &bytes_required,
                                   state_.GetErrorState())) {
    return false;
  }

  TRACE_EVENT1("gpu", "RasterDecoderImpl::ClearCompressedTextureLevel",
               "bytes_required", bytes_required);

  api()->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, 0);
  {
    // Add extra scope to destroy zero and the object it owns right
    // after its usage.
    std::unique_ptr<char[]> zero(new char[bytes_required]);
    memset(zero.get(), 0, bytes_required);
    api()->glBindTextureFn(texture->target(), texture->service_id());
    api()->glCompressedTexSubImage2DFn(target, level, 0, 0, width, height,
                                       format, bytes_required, zero.get());
  }
  gles2::TextureRef* bound_texture =
      texture_manager()->GetTextureInfoForTarget(&state_, texture->target());
  api()->glBindTextureFn(texture->target(),
                         bound_texture ? bound_texture->service_id() : 0);
  gles2::Buffer* bound_buffer =
      buffer_manager()->GetBufferInfoForTarget(&state_, GL_PIXEL_UNPACK_BUFFER);
  if (bound_buffer) {
    api()->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, bound_buffer->service_id());
  }

  if (gr_context()) {
    gr_context()->resetContext(kTextureBinding_GrGLBackendState);
  }
  return true;
}

int RasterDecoderImpl::DecoderIdForTest() {
  return raster_decoder_id_;
}

ServiceTransferCache* RasterDecoderImpl::GetTransferCacheForTest() {
  return raster_decoder_context_state_->transfer_cache.get();
}

void RasterDecoderImpl::SetUpForRasterCHROMIUMForTest() {
  // Some tests use mock GL which doesn't work with skia. Just use a bitmap
  // backed surface for OOP raster commands.
  sk_surface_ = SkSurface::MakeRaster(SkImageInfo::MakeN32Premul(10, 10));
  raster_canvas_ = SkCreateColorSpaceXformCanvas(sk_surface_->getCanvas(),
                                                 SkColorSpace::MakeSRGB());
}

void RasterDecoderImpl::SetOOMErrorForTest() {
  LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, "SetOOMErrorForTest",
                     "synthetic out of memory");
}

void RasterDecoderImpl::OnContextLostError() {
  if (!WasContextLost()) {
    // Need to lose current context before broadcasting!
    CheckResetStatus();
    group_->LoseContexts(error::kUnknown);
    reset_by_robustness_extension_ = true;
  }
}

void RasterDecoderImpl::OnOutOfMemoryError() {
  if (lose_context_when_out_of_memory_ && !WasContextLost()) {
    error::ContextLostReason other = error::kOutOfMemory;
    if (CheckResetStatus()) {
      other = error::kUnknown;
    } else {
      // Need to lose current context before broadcasting!
      MarkContextLost(error::kOutOfMemory);
    }
    group_->LoseContexts(other);
  }
}

error::Error RasterDecoderImpl::HandleWaitSyncTokenCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::WaitSyncTokenCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::WaitSyncTokenCHROMIUM*>(
          cmd_data);

  static constexpr CommandBufferNamespace kMinNamespaceId =
      CommandBufferNamespace::INVALID;
  static constexpr CommandBufferNamespace kMaxNamespaceId =
      CommandBufferNamespace::NUM_COMMAND_BUFFER_NAMESPACES;

  CommandBufferNamespace namespace_id =
      static_cast<CommandBufferNamespace>(c.namespace_id);
  if ((namespace_id < static_cast<int32_t>(kMinNamespaceId)) ||
      (namespace_id >= static_cast<int32_t>(kMaxNamespaceId))) {
    namespace_id = CommandBufferNamespace::INVALID;
  }
  const CommandBufferId command_buffer_id =
      CommandBufferId::FromUnsafeValue(c.command_buffer_id());
  const uint64_t release = c.release_count();

  SyncToken sync_token;
  sync_token.Set(namespace_id, command_buffer_id, release);
  return client_->OnWaitSyncToken(sync_token) ? error::kDeferCommandUntilLater
                                              : error::kNoError;
}

error::Error RasterDecoderImpl::HandleBeginQueryEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::BeginQueryEXT& c =
      *static_cast<const volatile raster::cmds::BeginQueryEXT*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint client_id = static_cast<GLuint>(c.id);
  int32_t sync_shm_id = static_cast<int32_t>(c.sync_data_shm_id);
  uint32_t sync_shm_offset = static_cast<uint32_t>(c.sync_data_shm_offset);

  switch (target) {
    case GL_COMMANDS_ISSUED_CHROMIUM:
    case GL_COMMANDS_COMPLETED_CHROMIUM:
      break;
    default:
      LOCAL_SET_GL_ERROR(GL_INVALID_ENUM, "glBeginQueryEXT",
                         "unknown query target");
      return error::kNoError;
  }

  if (query_manager_->GetActiveQuery(target)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT",
                       "query already in progress");
    return error::kNoError;
  }

  if (client_id == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT", "id is 0");
    return error::kNoError;
  }

  scoped_refptr<Buffer> buffer = GetSharedMemoryBuffer(sync_shm_id);
  if (!buffer)
    return error::kInvalidArguments;
  QuerySync* sync = static_cast<QuerySync*>(
      buffer->GetDataAddress(sync_shm_offset, sizeof(QuerySync)));
  if (!sync)
    return error::kOutOfBounds;

  QueryManager::Query* query = query_manager_->GetQuery(client_id);
  if (!query) {
    if (!query_manager_->IsValidQuery(client_id)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT",
                         "id not made by glGenQueriesEXT");
      return error::kNoError;
    }

    query =
        query_manager_->CreateQuery(target, client_id, std::move(buffer), sync);
  } else {
    if (query->target() != target) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT",
                         "target does not match");
      return error::kNoError;
    } else if (query->sync() != sync) {
      DLOG(ERROR) << "Shared memory used by query not the same as before";
      return error::kInvalidArguments;
    }
  }

  query_manager_->BeginQuery(query);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleEndQueryEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::EndQueryEXT& c =
      *static_cast<const volatile raster::cmds::EndQueryEXT*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  uint32_t submit_count = static_cast<GLuint>(c.submit_count);

  QueryManager::Query* query = query_manager_->GetActiveQuery(target);
  if (!query) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glEndQueryEXT",
                       "No active query");
    return error::kNoError;
  }

  query_manager_->EndQuery(query, submit_count);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleInsertFenceSyncCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::InsertFenceSyncCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::InsertFenceSyncCHROMIUM*>(
          cmd_data);

  const uint64_t release_count = c.release_count();
  client_->OnFenceSyncRelease(release_count);
  // Exit inner command processing loop so that we check the scheduling state
  // and yield if necessary as we may have unblocked a higher priority
  // context.
  ExitCommandProcessingEarly();
  return error::kNoError;
}

void RasterDecoderImpl::DoFinish() {
  api()->glFinishFn();
  ProcessPendingQueries(true);
}

void RasterDecoderImpl::DoFlush() {
  api()->glFlushFn();
  ProcessPendingQueries(false);
}

void RasterDecoderImpl::DoCreateAndConsumeTextureINTERNAL(
    GLuint client_id,
    bool use_buffer,
    gfx::BufferUsage buffer_usage,
    viz::ResourceFormat resource_format,
    const volatile GLbyte* key) {
  TRACE_EVENT2("gpu", "RasterDecoderImpl::DoCreateAndConsumeTextureINTERNAL",
               "context", logger_.GetLogPrefix(), "key[0]",
               static_cast<unsigned char>(key[0]));
  Mailbox mailbox =
      Mailbox::FromVolatile(*reinterpret_cast<const volatile Mailbox*>(key));
  DLOG_IF(ERROR, !mailbox.Verify()) << "CreateAndConsumeTexture was "
                                       "passed a mailbox that was not "
                                       "generated by ProduceTextureCHROMIUM.";
  if (!client_id) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glCreateAndConsumeTextureCHROMIUM",
                       "invalid client id");
    return;
  }

  gles2::TextureRef* texture_ref = GetTexture(client_id);
  if (texture_ref) {
    // No need to create texture here, the client_id already has an associated
    // texture.
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glCreateAndConsumeTextureCHROMIUM",
                       "client id already in use");
    return;
  }

  texture_metadata_.emplace(std::make_pair(
      client_id, TextureMetadata(use_buffer, buffer_usage, resource_format,
                                 GetCapabilities())));

  gles2::Texture* texture = gles2::Texture::CheckedCast(
      group_->mailbox_manager()->ConsumeTexture(mailbox));
  if (!texture) {
    // Create texture to handle invalid mailbox (see http://crbug.com/472465).
    GLuint service_id = 0;
    api()->glGenTexturesFn(1, &service_id);
    DCHECK(service_id);
    texture_manager()->CreateTexture(client_id, service_id);

    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glCreateAndConsumeTextureCHROMIUM",
                       "invalid mailbox name");
    return;
  }

  texture_ref = texture_manager()->Consume(client_id, texture);

  // TODO(backer): Validate that the consumed texture is consistent with
  // TextureMetadata.
}

void RasterDecoderImpl::DeleteTexturesHelper(
    GLsizei n,
    const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    gles2::TextureRef* texture_ref = GetTexture(client_id);
    if (texture_ref) {
      RemoveTexture(client_id);
    }
  }
}

bool RasterDecoderImpl::GenQueriesEXTHelper(GLsizei n,
                                            const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (query_manager_->IsValidQuery(client_ids[ii])) {
      return false;
    }
  }
  query_manager_->GenQueries(n, client_ids);
  return true;
}

void RasterDecoderImpl::DeleteQueriesEXTHelper(
    GLsizei n,
    const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    query_manager_->RemoveQuery(client_id);
  }
}

error::Error RasterDecoderImpl::HandleTraceBeginCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TraceBeginCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::TraceBeginCHROMIUM*>(cmd_data);
  Bucket* category_bucket = GetBucket(c.category_bucket_id);
  Bucket* name_bucket = GetBucket(c.name_bucket_id);
  static constexpr size_t kMaxStrLen = 256;
  if (!category_bucket || category_bucket->size() == 0 ||
      category_bucket->size() > kMaxStrLen || !name_bucket ||
      name_bucket->size() == 0 || name_bucket->size() > kMaxStrLen) {
    return error::kInvalidArguments;
  }

  std::string category_name;
  std::string trace_name;
  if (!category_bucket->GetAsString(&category_name) ||
      !name_bucket->GetAsString(&trace_name)) {
    return error::kInvalidArguments;
  }

  debug_marker_manager_.PushGroup(trace_name);
  if (!gpu_tracer_->Begin(category_name, trace_name, gles2::kTraceCHROMIUM)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glTraceBeginCHROMIUM",
                       "unable to create begin trace");
    return error::kNoError;
  }
  return error::kNoError;
}

void RasterDecoderImpl::DoTraceEndCHROMIUM() {
  debug_marker_manager_.PopGroup();
  if (!gpu_tracer_->End(gles2::kTraceCHROMIUM)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glTraceEndCHROMIUM",
                       "no trace begin found");
    return;
  }
}

error::Error RasterDecoderImpl::HandleSetActiveURLCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile cmds::SetActiveURLCHROMIUM& c =
      *static_cast<const volatile cmds::SetActiveURLCHROMIUM*>(cmd_data);
  Bucket* url_bucket = GetBucket(c.url_bucket_id);
  static constexpr size_t kMaxStrLen = 1024;
  if (!url_bucket || url_bucket->size() == 0 ||
      url_bucket->size() > kMaxStrLen) {
    return error::kInvalidArguments;
  }

  size_t size = url_bucket->size();
  const char* url_str = url_bucket->GetDataAs<const char*>(0, size);
  if (!url_str)
    return error::kInvalidArguments;

  GURL url(base::StringPiece(url_str, size));
  client_->SetActiveURL(std::move(url));
  return error::kNoError;
}

bool RasterDecoderImpl::InitializeCopyTexImageBlitter() {
  if (!copy_tex_image_blit_.get()) {
    LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("glCopySubTexture");
    copy_tex_image_blit_.reset(
        new gles2::CopyTexImageResourceManager(feature_info_.get()));
    copy_tex_image_blit_->Initialize(this);
    if (LOCAL_PEEK_GL_ERROR("glCopySubTexture") != GL_NO_ERROR)
      return false;
  }
  return true;
}

bool RasterDecoderImpl::InitializeCopyTextureCHROMIUM() {
  // Defer initializing the CopyTextureCHROMIUMResourceManager until it is
  // needed because it takes 10s of milliseconds to initialize.
  if (!copy_texture_chromium_.get()) {
    LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("glCopySubTexture");
    copy_texture_chromium_.reset(
        gles2::CopyTextureCHROMIUMResourceManager::Create());
    copy_texture_chromium_->Initialize(this, features());
    if (LOCAL_PEEK_GL_ERROR("glCopySubTexture") != GL_NO_ERROR)
      return false;

    // On the desktop core profile this also needs emulation of
    // CopyTex{Sub}Image2D for luminance, alpha, and luminance_alpha
    // textures.
    if (gles2::CopyTexImageResourceManager::CopyTexImageRequiresBlit(
            feature_info_.get(), GL_LUMINANCE)) {
      if (!InitializeCopyTexImageBlitter())
        return false;
    }
  }
  return true;
}

void RasterDecoderImpl::DoCopySubTexture(GLuint source_id,
                                         GLuint dest_id,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height) {
  gles2::TextureRef* source_texture_ref = GetTexture(source_id);
  gles2::TextureRef* dest_texture_ref = GetTexture(dest_id);
  if (!source_texture_ref || !dest_texture_ref) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                       "unknown texture id");
    return;
  }

  gles2::Texture* source_texture = source_texture_ref->texture();
  gles2::Texture* dest_texture = dest_texture_ref->texture();
  if (source_texture == dest_texture) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glCopySubTexture",
                       "source and destination textures are the same");
    return;
  }

  TextureMetadata* source_texture_metadata = GetTextureMetadata(source_id);
  if (!source_texture_metadata) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture", "unknown texture");
    return;
  }

  TextureMetadata* dest_texture_metadata = GetTextureMetadata(dest_id);
  if (!dest_texture_metadata) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture", "unknown texture");
    return;
  }

  GLenum source_target = source_texture_metadata->target();
  GLint source_level = 0;
  GLenum dest_target = dest_texture_metadata->target();
  GLint dest_level = 0;

  ScopedTextureBinder binder(
      &state_, texture_manager(), dest_texture_ref, dest_target, gr_context(),
      raster_decoder_context_state_->need_context_state_reset);

  int source_width = 0;
  int source_height = 0;
  gl::GLImage* image =
      source_texture->GetLevelImage(source_target, 0 /* level */);
  if (image) {
    gfx::Size size = image->GetSize();
    source_width = size.width();
    source_height = size.height();
    if (source_width <= 0 || source_height <= 0) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                         "invalid image size");
      return;
    }

    // Ideally we should not need to check that the sub-texture copy rectangle
    // is valid in two different ways, here and below. However currently there
    // is no guarantee that a texture backed by a GLImage will have sensible
    // level info. If this synchronization were to be enforced then this and
    // other functions in this file could be cleaned up.
    // See: https://crbug.com/586476
    int32_t max_x;
    int32_t max_y;
    if (!gles2::SafeAddInt32(x, width, &max_x) ||
        !gles2::SafeAddInt32(y, height, &max_y) || x < 0 || y < 0 ||
        max_x > source_width || max_y > source_height) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                         "source texture bad dimensions");
      return;
    }
  } else {
    if (!source_texture->GetLevelSize(source_target, 0 /* level */,
                                      &source_width, &source_height, nullptr)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                         "source texture has no data for level");
      return;
    }

    // Check that this type of texture is allowed.
    if (!texture_manager()->ValidForTarget(source_target, 0 /* level */,
                                           source_width, source_height, 1)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                         "source texture bad dimensions");
      return;
    }

    if (!source_texture->ValidForTexture(source_target, 0 /* level */, x, y, 0,
                                         width, height, 1)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                         "source texture bad dimensions.");
      return;
    }
  }

  GLenum source_type = 0;
  GLenum source_internal_format = 0;
  source_texture->GetLevelType(source_target, 0 /* level */, &source_type,
                               &source_internal_format);

  GLenum dest_type = 0;
  GLenum dest_internal_format = 0;
  bool dest_level_defined = dest_texture->GetLevelType(
      dest_target, 0 /* level */, &dest_type, &dest_internal_format);
  if (!dest_level_defined) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glCopySubTexture",
                       "destination texture is not defined");
    return;
  }
  if (!dest_texture->ValidForTexture(dest_target, 0 /* level */, xoffset,
                                     yoffset, 0, width, height, 1)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                       "destination texture bad dimensions.");
    return;
  }
  std::string output_error_msg;
  if (!ValidateCopyTextureCHROMIUMInternalFormats(
          GetFeatureInfo(), source_internal_format, dest_internal_format,
          &output_error_msg)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glCopySubTexture",
                       output_error_msg.c_str());
    return;
  }

  if (feature_info_->feature_flags().desktop_srgb_support) {
    bool enable_framebuffer_srgb =
        gles2::GLES2Util::GetColorEncodingFromInternalFormat(
            source_internal_format) == GL_SRGB ||
        gles2::GLES2Util::GetColorEncodingFromInternalFormat(
            dest_internal_format) == GL_SRGB;
    state_.EnableDisableFramebufferSRGB(enable_framebuffer_srgb);
  }

  // Clear the source texture if necessary.
  if (!texture_manager()->ClearTextureLevel(this, source_texture_ref,
                                            source_target, 0 /* level */)) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, "glCopySubTexture",
                       "source texture dimensions too big");
    return;
  }

  int dest_width = 0;
  int dest_height = 0;
  bool ok = dest_texture->GetLevelSize(dest_target, dest_level, &dest_width,
                                       &dest_height, nullptr);
  DCHECK(ok);
  if (xoffset != 0 || yoffset != 0 || width != dest_width ||
      height != dest_height) {
    gfx::Rect cleared_rect;
    if (gles2::TextureManager::CombineAdjacentRects(
            dest_texture->GetLevelClearedRect(dest_target, dest_level),
            gfx::Rect(xoffset, yoffset, width, height), &cleared_rect)) {
      DCHECK_GE(cleared_rect.size().GetArea(),
                dest_texture->GetLevelClearedRect(dest_target, dest_level)
                    .size()
                    .GetArea());
      texture_manager()->SetLevelClearedRect(dest_texture_ref, dest_target,
                                             dest_level, cleared_rect);
    } else {
      // Otherwise clear part of texture level that is not already cleared.
      if (!texture_manager()->ClearTextureLevel(this, dest_texture_ref,
                                                dest_target, dest_level)) {
        LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, "glCopySubTexture",
                           "destination texture dimensions too big");
        return;
      }
    }
  } else {
    texture_manager()->SetLevelCleared(dest_texture_ref, dest_target,
                                       dest_level, true);
  }

  // TODO(qiankun.miao@intel.com): Support level > 0 for CopyTexSubImage.
  if (image && dest_internal_format == source_internal_format &&
      dest_level == 0) {
    if (image->CopyTexSubImage(dest_target, gfx::Point(xoffset, yoffset),
                               gfx::Rect(x, y, width, height))) {
      return;
    }
  }

  if (!InitializeCopyTextureCHROMIUM())
    return;

  DoBindOrCopyTexImageIfNeeded(source_texture, source_target);

  // GL_TEXTURE_EXTERNAL_OES texture requires apply a transform matrix
  // before presenting.
  if (source_target == GL_TEXTURE_EXTERNAL_OES) {
    if (gles2::GLStreamTextureImage* image =
            source_texture->GetLevelStreamTextureImage(GL_TEXTURE_EXTERNAL_OES,
                                                       source_level)) {
      GLfloat transform_matrix[16];
      image->GetTextureMatrix(transform_matrix);

      copy_texture_chromium_->DoCopySubTextureWithTransform(
          this, source_target, source_texture->service_id(), source_level,
          source_internal_format, dest_target, dest_texture->service_id(),
          dest_level, dest_internal_format, xoffset, yoffset, x, y, width,
          height, dest_width, dest_height, source_width, source_height,
          false /* unpack_flip_y */, false /* unpack_premultiply_alpha */,
          false /* unpack_unmultiply_alpha */, false /* dither */,
          transform_matrix, copy_tex_image_blit_.get());
      return;
    }
  }

  gles2::CopyTextureMethod method = GetCopyTextureCHROMIUMMethod(
      GetFeatureInfo(), source_target, source_level, source_internal_format,
      source_type, dest_target, dest_level, dest_internal_format,
      false /* unpack_flip_y */, false /* unpack_premultiply_alpha */,
      false /* unpack_unmultiply_alpha */, false /* dither */);
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
  // glDrawArrays is faster than glCopyTexSubImage2D on IA Mesa driver,
  // although opposite in Android.
  // TODO(dshwang): After Mesa fixes this issue, remove this hack.
  // https://bugs.freedesktop.org/show_bug.cgi?id=98478,
  // https://crbug.com/535198.
  if (gles2::Texture::ColorRenderable(GetFeatureInfo(), dest_internal_format,
                                      dest_texture->IsImmutable()) &&
      method == gles2::CopyTextureMethod::DIRECT_COPY) {
    method = gles2::CopyTextureMethod::DIRECT_DRAW;
  }
#endif

  copy_texture_chromium_->DoCopySubTexture(
      this, source_target, source_texture->service_id(), source_level,
      source_internal_format, dest_target, dest_texture->service_id(),
      dest_level, dest_internal_format, xoffset, yoffset, x, y, width, height,
      dest_width, dest_height, source_width, source_height,
      false /* unpack_flip_y */, false /* unpack_premultiply_alpha */,
      false /* unpack_unmultiply_alpha */, false /* dither */, method,
      copy_tex_image_blit_.get());
}

void RasterDecoderImpl::DoBindOrCopyTexImageIfNeeded(gles2::Texture* texture,
                                                     GLenum textarget) {
  // Image is already in use if texture is attached to a framebuffer.
  if (texture && !texture->IsAttachedToFramebuffer()) {
    gles2::Texture::ImageState image_state;
    gl::GLImage* image = texture->GetLevelImage(textarget, 0, &image_state);
    if (image && image_state == gles2::Texture::UNBOUND) {
      ScopedGLErrorSuppressor suppressor(
          "RasterDecoderImpl::DoBindOrCopyTexImageIfNeeded", GetErrorState());
      api()->glBindTextureFn(textarget, texture->service_id());
      if (!image->BindTexImage(textarget)) {
        // Note: We update the state to COPIED prior to calling CopyTexImage()
        // as that allows the GLImage implemenatation to set it back to
        // UNBOUND and ensure that CopyTexImage() is called each time the
        // texture is used.
        texture->SetLevelImageState(textarget, 0, gles2::Texture::COPIED);
        bool rv = image->CopyTexImage(textarget);
        DCHECK(rv) << "CopyTexImage() failed";
      }
    }
  }
}

namespace {

// Helper to read client data from transfer cache.
class TransferCacheDeserializeHelperImpl final
    : public cc::TransferCacheDeserializeHelper {
 public:
  explicit TransferCacheDeserializeHelperImpl(
      int raster_decoder_id,
      ServiceTransferCache* transfer_cache)
      : raster_decoder_id_(raster_decoder_id), transfer_cache_(transfer_cache) {
    DCHECK(transfer_cache_);
  }
  ~TransferCacheDeserializeHelperImpl() override = default;

  void CreateLocalEntry(
      uint32_t id,
      std::unique_ptr<cc::ServiceTransferCacheEntry> entry) override {
    auto type = entry->Type();
    transfer_cache_->CreateLocalEntry(
        ServiceTransferCache::EntryKey(raster_decoder_id_, type, id),
        std::move(entry));
  }

 private:
  cc::ServiceTransferCacheEntry* GetEntryInternal(
      cc::TransferCacheEntryType entry_type,
      uint32_t entry_id) override {
    return transfer_cache_->GetEntry(ServiceTransferCache::EntryKey(
        raster_decoder_id_, entry_type, entry_id));
  }

  const int raster_decoder_id_;
  ServiceTransferCache* const transfer_cache_;

  DISALLOW_COPY_AND_ASSIGN(TransferCacheDeserializeHelperImpl);
};

}  // namespace

void RasterDecoderImpl::DeletePaintCacheTextBlobsINTERNALHelper(
    GLsizei n,
    const volatile GLuint* paint_cache_ids) {
  if (!supports_oop_raster_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glDeletePaintCacheEntriesINTERNAL",
                       "No chromium raster support");
    return;
  }

  paint_cache_->Purge(cc::PaintCacheDataType::kTextBlob, n, paint_cache_ids);
}

void RasterDecoderImpl::DeletePaintCachePathsINTERNALHelper(
    GLsizei n,
    const volatile GLuint* paint_cache_ids) {
  if (!supports_oop_raster_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glDeletePaintCacheEntriesINTERNAL",
                       "No chromium raster support");
    return;
  }

  paint_cache_->Purge(cc::PaintCacheDataType::kPath, n, paint_cache_ids);
}

void RasterDecoderImpl::DoClearPaintCacheINTERNAL() {
  if (!supports_oop_raster_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glClearPaintCacheINTERNAL",
                       "No chromium raster support");
    return;
  }

  paint_cache_->PurgeAll();
}

void RasterDecoderImpl::DoBeginRasterCHROMIUM(
    GLuint sk_color,
    GLuint msaa_sample_count,
    GLboolean can_use_lcd_text,
    GLint color_type,
    GLuint color_space_transfer_cache_id,
    const volatile GLbyte* key) {
  if (!gr_context()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginRasterCHROMIUM",
                       "chromium_raster_transport not enabled via attribs");
    return;
  }
  if (sk_surface_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginRasterCHROMIUM",
                       "BeginRasterCHROMIUM without EndRasterCHROMIUM");
    return;
  }

  Mailbox mailbox =
      Mailbox::FromVolatile(*reinterpret_cast<const volatile Mailbox*>(key));
  DLOG_IF(ERROR, !mailbox.Verify()) << "BeginRasterCHROMIUM was "
                                       "passed a mailbox that was not "
                                       "generated by ProduceTextureCHROMIUM.";

  DCHECK(!shared_image_);
  shared_image_ =
      group_->shared_image_representation_factory()->ProduceSkia(mailbox);
  if (!shared_image_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glBeginRasterCHROMIUM",
                       "passed invalid mailbox.");
    return;
  }

  DCHECK(locked_handles_.empty());
  DCHECK(!raster_canvas_);
  raster_decoder_context_state_->need_context_state_reset = true;

  // Use unknown pixel geometry to disable LCD text.
  uint32_t flags = 0;
  SkSurfaceProps surface_props(flags, kUnknown_SkPixelGeometry);
  if (can_use_lcd_text) {
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    surface_props =
        SkSurfaceProps(flags, SkSurfaceProps::kLegacyFontHost_InitType);
  }

  SkColorType sk_color_type = static_cast<SkColorType>(color_type);
  // If we can't match requested MSAA samples, don't use MSAA.
  int final_msaa_count = std::max(static_cast<int>(msaa_sample_count), 0);
  if (final_msaa_count >
      gr_context()->maxSurfaceSampleCountForColorType(sk_color_type))
    final_msaa_count = 0;

  sk_surface_ = shared_image_->BeginWriteAccess(gr_context(), final_msaa_count,
                                                sk_color_type, surface_props);
  if (!sk_surface_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginRasterCHROMIUM",
                       "failed to create surface");
    shared_image_.reset();
    return;
  }

  TransferCacheDeserializeHelperImpl transfer_cache_deserializer(
      raster_decoder_id_, transfer_cache());
  auto* color_space_entry =
      transfer_cache_deserializer
          .GetEntryAs<cc::ServiceColorSpaceTransferCacheEntry>(
              color_space_transfer_cache_id);
  if (!color_space_entry || !color_space_entry->color_space().IsValid()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginRasterCHROMIUM",
                       "failed to find valid color space");
    shared_image_->EndWriteAccess(std::move(sk_surface_));
    shared_image_.reset();
    return;
  }

  SkCanvas* canvas = nullptr;
  if (use_ddl_) {
    SkSurfaceCharacterization characterization;
    bool result = sk_surface_->characterize(&characterization);
    DCHECK(result) << "Failed to characterize raster SkSurface.";
    recorder_ =
        std::make_unique<SkDeferredDisplayListRecorder>(characterization);
    canvas = recorder_->getCanvas();
  } else {
    canvas = sk_surface_->getCanvas();
  }

  raster_canvas_ = SkCreateColorSpaceXformCanvas(
      canvas, color_space_entry->color_space().ToSkColorSpace());
  raster_color_space_id_ = color_space_transfer_cache_id;

  // All or nothing clearing, as no way to validate the client's input on what
  // is the "used" part of the texture.
  // TODO(enne): This doesn't handle the case where the background color
  // changes and so any extra pixels outside the raster area that get
  // sampled may be incorrect.
  if (shared_image_->IsCleared())
    return;

  raster_canvas_->drawColor(sk_color);
  shared_image_->SetCleared();
}

scoped_refptr<Buffer> RasterDecoderImpl::GetShmBuffer(uint32_t shm_id) {
  return GetSharedMemoryBuffer(shm_id);
}

void RasterDecoderImpl::DoRasterCHROMIUM(GLuint raster_shm_id,
                                         GLuint raster_shm_offset,
                                         GLsizeiptr raster_shm_size,
                                         GLuint font_shm_id,
                                         GLuint font_shm_offset,
                                         GLsizeiptr font_shm_size) {
  TRACE_EVENT1("gpu", "RasterDecoderImpl::DoRasterCHROMIUM", "raster_id",
               ++raster_chromium_id_);

  if (!sk_surface_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glRasterCHROMIUM",
                       "RasterCHROMIUM without BeginRasterCHROMIUM");
    return;
  }
  DCHECK(transfer_cache());
  raster_decoder_context_state_->need_context_state_reset = true;

  if (font_shm_size > 0) {
    // Deserialize fonts before raster.
    volatile char* font_buffer_memory =
        GetSharedMemoryAs<char*>(font_shm_id, font_shm_offset, font_shm_size);
    if (!font_buffer_memory) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRasterCHROMIUM",
                         "Can not read font buffer.");
      return;
    }

    std::vector<SkDiscardableHandleId> new_locked_handles;
    if (!font_manager_->Deserialize(font_buffer_memory, font_shm_size,
                                    &new_locked_handles)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRasterCHROMIUM",
                         "Invalid font buffer.");
      return;
    }
    locked_handles_.insert(locked_handles_.end(), new_locked_handles.begin(),
                           new_locked_handles.end());
  }

  char* paint_buffer_memory = GetSharedMemoryAs<char*>(
      raster_shm_id, raster_shm_offset, raster_shm_size);
  if (!paint_buffer_memory) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRasterCHROMIUM",
                       "Can not read paint buffer.");
    return;
  }

  alignas(
      cc::PaintOpBuffer::PaintOpAlign) char data[sizeof(cc::LargestPaintOp)];

  SkCanvas* canvas = raster_canvas_.get();
  cc::PlaybackParams playback_params(nullptr, SkMatrix::I());
  TransferCacheDeserializeHelperImpl impl(raster_decoder_id_, transfer_cache());
  cc::PaintOp::DeserializeOptions options(&impl, paint_cache_.get(),
                                          font_manager_->strike_client());
  options.crash_dump_on_failure = true;

  size_t paint_buffer_size = raster_shm_size;
  while (paint_buffer_size > 0) {
    size_t skip = 0;
    cc::PaintOp* deserialized_op = cc::PaintOp::Deserialize(
        paint_buffer_memory, paint_buffer_size, &data[0],
        sizeof(cc::LargestPaintOp), &skip, options);
    if (!deserialized_op) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glRasterCHROMIUM",
                         "RasterCHROMIUM: serialization failure");
      return;
    }

    deserialized_op->Raster(canvas, playback_params);
    deserialized_op->DestroyThis();

    paint_buffer_size -= skip;
    paint_buffer_memory += skip;
  }
}

void RasterDecoderImpl::DoEndRasterCHROMIUM() {
  TRACE_EVENT0("gpu", "RasterDecoderImpl::DoEndRasterCHROMIUM");
  if (!sk_surface_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glEndRasterCHROMIUM",
                       "EndRasterCHROMIUM without BeginRasterCHROMIUM");
    return;
  }

  raster_decoder_context_state_->need_context_state_reset = true;

  raster_canvas_.reset();

  if (use_ddl_) {
    auto ddl = recorder_->detach();
    recorder_ = nullptr;
    sk_surface_->draw(ddl.get());
  }
  sk_surface_->prepareForExternalIO();
  if (!shared_image_) {
    // Test only path for  SetUpForRasterCHROMIUMForTest.
    sk_surface_.reset();
  } else {
    shared_image_->EndWriteAccess(std::move(sk_surface_));
    shared_image_.reset();
  }

  // Unlock all font handles. This needs to be deferred until
  // SkSurface::prepareForExternalIO since that flushes batched Gr operations
  // in skia that access the glyph data.
  // TODO(khushalsagar): We just unlocked a bunch of handles, do we need to
  // give a call to skia to attempt to purge any unlocked handles?
  if (!font_manager_->Unlock(locked_handles_)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRasterCHROMIUM",
                       "Invalid font discardable handle.");
  }
  locked_handles_.clear();

  // We just flushed a tile's worth of GPU work from the SkSurface in
  // prepareForExternalIO above. Use kDeferLaterCommands to ensure we yield to
  // the Scheduler before processing more commands.
  current_decoder_error_ = error::kDeferLaterCommands;

#if defined(OS_MACOSX)
  // Aggressively call glFlush on macOS to determine if this is sufficient to
  // avoid GL driver crashes.
  // TODO(ccameron): If this is not sufficient, then add a flush to
  // DoRasterCHROMIUM as well. Also add crash report data to indicate which
  // sequence of commands result in the crash, and formalize this as a GPU
  // bug workaround.
  // https://crbug.com/906453
  if (gr_context())
    gr_context()->flush();
  api()->glFlushFn();
#endif
}

void RasterDecoderImpl::DoCreateTransferCacheEntryINTERNAL(
    GLuint raw_entry_type,
    GLuint entry_id,
    GLuint handle_shm_id,
    GLuint handle_shm_offset,
    GLuint data_shm_id,
    GLuint data_shm_offset,
    GLuint data_size) {
  if (!supports_oop_raster_) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glCreateTransferCacheEntryINTERNAL",
        "Attempt to use OOP transfer cache on a context without OOP raster.");
    return;
  }
  DCHECK(gr_context());
  DCHECK(transfer_cache());

  // Validate the type we are about to create.
  cc::TransferCacheEntryType entry_type;
  if (!cc::ServiceTransferCacheEntry::SafeConvertToType(raw_entry_type,
                                                        &entry_type)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCreateTransferCacheEntryINTERNAL",
                       "Attempt to use OOP transfer cache with an invalid "
                       "cache entry type.");
    return;
  }

  uint8_t* data_memory =
      GetSharedMemoryAs<uint8_t*>(data_shm_id, data_shm_offset, data_size);
  if (!data_memory) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCreateTransferCacheEntryINTERNAL",
                       "Can not read transfer cache entry data.");
    return;
  }

  scoped_refptr<Buffer> handle_buffer = GetSharedMemoryBuffer(handle_shm_id);
  if (!DiscardableHandleBase::ValidateParameters(handle_buffer.get(),
                                                 handle_shm_offset)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCreateTransferCacheEntryINTERNAL",
                       "Invalid shm for discardable handle.");
    return;
  }
  ServiceDiscardableHandle handle(std::move(handle_buffer), handle_shm_offset,
                                  handle_shm_id);

  // If the entry is going to use skia during deserialization, make sure we
  // mark the context state dirty.
  GrContext* context_for_entry =
      cc::ServiceTransferCacheEntry::UsesGrContext(entry_type) ? gr_context()
                                                               : nullptr;
  if (context_for_entry)
    raster_decoder_context_state_->need_context_state_reset = true;

  if (!transfer_cache()->CreateLockedEntry(
          ServiceTransferCache::EntryKey(raster_decoder_id_, entry_type,
                                         entry_id),
          handle, context_for_entry, base::make_span(data_memory, data_size))) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCreateTransferCacheEntryINTERNAL",
                       "Failure to deserialize transfer cache entry.");
    return;
  }
}

void RasterDecoderImpl::DoUnlockTransferCacheEntryINTERNAL(
    GLuint raw_entry_type,
    GLuint entry_id) {
  if (!supports_oop_raster_) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glUnlockTransferCacheEntryINTERNAL",
        "Attempt to use OOP transfer cache on a context without OOP raster.");
    return;
  }
  DCHECK(transfer_cache());
  cc::TransferCacheEntryType entry_type;
  if (!cc::ServiceTransferCacheEntry::SafeConvertToType(raw_entry_type,
                                                        &entry_type)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUnlockTransferCacheEntryINTERNAL",
                       "Attempt to use OOP transfer cache with an invalid "
                       "cache entry type.");
    return;
  }

  if (!transfer_cache()->UnlockEntry(ServiceTransferCache::EntryKey(
          raster_decoder_id_, entry_type, entry_id))) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUnlockTransferCacheEntryINTERNAL",
                       "Attempt to unlock an invalid ID");
  }
}

void RasterDecoderImpl::DoDeleteTransferCacheEntryINTERNAL(
    GLuint raw_entry_type,
    GLuint entry_id) {
  if (!supports_oop_raster_) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glDeleteTransferCacheEntryINTERNAL",
        "Attempt to use OOP transfer cache on a context without OOP raster.");
    return;
  }
  DCHECK(transfer_cache());
  cc::TransferCacheEntryType entry_type;
  if (!cc::ServiceTransferCacheEntry::SafeConvertToType(raw_entry_type,
                                                        &entry_type)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glDeleteTransferCacheEntryINTERNAL",
                       "Attempt to use OOP transfer cache with an invalid "
                       "cache entry type.");
    return;
  }

  if (!transfer_cache()->DeleteEntry(ServiceTransferCache::EntryKey(
          raster_decoder_id_, entry_type, entry_id))) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glDeleteTransferCacheEntryINTERNAL",
                       "Attempt to delete an invalid ID");
  }
}

void RasterDecoderImpl::RestoreStateForAttrib(GLuint attrib_index,
                                              bool restore_array_binding) {
  raster_decoder_context_state_->PessimisticallyResetGrContext();
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "base/macros.h"
#include "gpu/command_buffer/service/raster_decoder_autogen.h"

}  // namespace raster
}  // namespace gpu
