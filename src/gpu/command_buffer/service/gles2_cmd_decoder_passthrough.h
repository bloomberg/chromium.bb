// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the GLES2DecoderPassthroughImpl class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_PASSTHROUGH_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_PASSTHROUGH_H_

#include <algorithm>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/common/debug_marker_manager.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/client_service_map.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/passthrough_abstract_texture_impl.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface.h"

namespace gl {
class GLFence;
class ProgressReporter;
}

namespace gpu {
class SharedImageRepresentationGLTexturePassthrough;

namespace gles2 {

class ContextGroup;
class GPUTracer;
class MultiDrawManager;
class PassthroughAbstractTextureImpl;

struct MappedBuffer {
  GLsizeiptr size;
  GLbitfield original_access;
  GLbitfield filtered_access;
  uint8_t* map_ptr;
  int32_t data_shm_id;
  uint32_t data_shm_offset;
};

struct PassthroughResources {
  PassthroughResources();
  ~PassthroughResources();

  // api is null if we don't have a context (e.g. lost).
  void Destroy(gl::GLApi* api, gl::ProgressReporter* progress_reporter);

  // Resources stores a shared list of textures pending deletion.
  // If we have don't context when this function is called, we can mark
  // these textures as lost context and drop all references to them.
  void DestroyPendingTextures(bool has_context);

  // If there are any textures pending destruction.
  bool HasTexturesPendingDestruction() const;

  // Mappings from client side IDs to service side IDs.
  ClientServiceMap<GLuint, GLuint> texture_id_map;
  ClientServiceMap<GLuint, GLuint> buffer_id_map;
  ClientServiceMap<GLuint, GLuint> renderbuffer_id_map;
  ClientServiceMap<GLuint, GLuint> sampler_id_map;
  ClientServiceMap<GLuint, GLuint> program_id_map;
  ClientServiceMap<GLuint, GLuint> shader_id_map;

  static_assert(sizeof(uintptr_t) == sizeof(GLsync),
                "GLsync not the same size as uintptr_t");
  ClientServiceMap<GLuint, uintptr_t> sync_id_map;

  // Mapping of client texture IDs to TexturePassthrough objects used to make
  // sure all textures used by mailboxes are not deleted until all textures
  // using the mailbox are deleted
  ClientServiceMap<GLuint, scoped_refptr<TexturePassthrough>>
      texture_object_map;

  class SharedImageData {
   public:
    SharedImageData();
    explicit SharedImageData(
        std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
            representation);
    SharedImageData(SharedImageData&& other);
    ~SharedImageData();
    SharedImageData& operator=(SharedImageData&& other);

    SharedImageRepresentationGLTexturePassthrough* representation() const {
      return representation_.get();
    }

    bool BeginAccess(GLenum mode, gl::GLApi* api);

    void EndAccess() {
      DCHECK(is_being_accessed());
      scoped_access_.reset();
    }

    bool is_being_accessed() const { return !!scoped_access_; }

   private:
    std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
        representation_;
    std::unique_ptr<SharedImageRepresentationGLTexturePassthrough::ScopedAccess>
        scoped_access_;
    DISALLOW_COPY_AND_ASSIGN(SharedImageData);
  };
  // Mapping of client texture IDs to
  // SharedImageRepresentationGLTexturePassthroughs.
  // TODO(ericrk): Remove this once TexturePassthrough holds a reference to
  // the SharedImageRepresentationGLTexturePassthrough itself.
  base::flat_map<GLuint, SharedImageData> texture_shared_image_map;

  // A set of yet-to-be-deleted TexturePassthrough, which should be tossed
  // whenever a context switch happens or the resources is destroyed.
  base::flat_set<scoped_refptr<TexturePassthrough>>
      textures_pending_destruction;

  // Mapping of client buffer IDs that are mapped to the shared memory used to
  // back the mapping so that it can be flushed when the buffer is unmapped
  std::unordered_map<GLuint, MappedBuffer> mapped_buffer_map;
};

class GPU_GLES2_EXPORT GLES2DecoderPassthroughImpl : public GLES2Decoder {
 public:
  GLES2DecoderPassthroughImpl(DecoderClient* client,
                              CommandBufferServiceBase* command_buffer_service,
                              Outputter* outputter,
                              ContextGroup* group);
  ~GLES2DecoderPassthroughImpl() override;

  Error DoCommands(unsigned int num_commands,
                   const volatile void* buffer,
                   int num_entries,
                   int* entries_processed) override;

  template <bool DebugImpl>
  Error DoCommandsImpl(unsigned int num_commands,
                       const volatile void* buffer,
                       int num_entries,
                       int* entries_processed);

  base::WeakPtr<DecoderContext> AsWeakPtr() override;

  gpu::ContextResult Initialize(
      const scoped_refptr<gl::GLSurface>& surface,
      const scoped_refptr<gl::GLContext>& context,
      bool offscreen,
      const DisallowedFeatures& disallowed_features,
      const ContextCreationAttribs& attrib_helper) override;

  // Destroys the graphics context.
  void Destroy(bool have_context) override;

  // Set the surface associated with the default FBO.
  void SetSurface(const scoped_refptr<gl::GLSurface>& surface) override;

  // Releases the surface associated with the GL context.
  // The decoder should not be used until a new surface is set.
  void ReleaseSurface() override;

  void TakeFrontBuffer(const Mailbox& mailbox) override;

  void ReturnFrontBuffer(const Mailbox& mailbox, bool is_lost) override;

  // Resize an offscreen frame buffer.
  bool ResizeOffscreenFramebuffer(const gfx::Size& size) override;

  // Make this decoder's GL context current.
  bool MakeCurrent() override;

  gl::GLApi* api() const { return api_; }

  // Gets the GLES2 Util which holds info.
  GLES2Util* GetGLES2Util() override;

  // Gets the associated GLContext and GLSurface.
  gl::GLContext* GetGLContext() override;
  gl::GLSurface* GetGLSurface() override;

  // Gets the associated ContextGroup
  ContextGroup* GetContextGroup() override;

  const FeatureInfo* GetFeatureInfo() const override;

  Capabilities GetCapabilities() override;

  // Restores all of the decoder GL state.
  void RestoreState(const ContextState* prev_state) override;

  // Restore States.
  void RestoreActiveTexture() const override;
  void RestoreAllTextureUnitAndSamplerBindings(
      const ContextState* prev_state) const override;
  void RestoreActiveTextureUnitBinding(unsigned int target) const override;
  void RestoreBufferBinding(unsigned int target) override;
  void RestoreBufferBindings() const override;
  void RestoreFramebufferBindings() const override;
  void RestoreRenderbufferBindings() override;
  void RestoreGlobalState() const override;
  void RestoreProgramBindings() const override;
  void RestoreTextureState(unsigned service_id) override;
  void RestoreTextureUnitBindings(unsigned unit) const override;
  void RestoreVertexAttribArray(unsigned index) override;
  void RestoreAllExternalTextureBindingsIfNeeded() override;
  void RestoreDeviceWindowRectangles() const override;

  void ClearAllAttributes() const override;
  void RestoreAllAttributes() const override;

  void SetIgnoreCachedStateForTest(bool ignore) override;
  void SetForceShaderNameHashingForTest(bool force) override;
  size_t GetSavedBackTextureCountForTest() override;
  size_t GetCreatedBackTextureCountForTest() override;

  // Gets the QueryManager for this context.
  QueryManager* GetQueryManager() override;

  // Set a callback to be called when a query is complete.
  void SetQueryCallback(unsigned int query_client_id,
                        base::OnceClosure callback) override;

  // Gets the GpuFenceManager for this context.
  GpuFenceManager* GetGpuFenceManager() override;

  // Gets the FramebufferManager for this context.
  FramebufferManager* GetFramebufferManager() override;

  // Gets the TransformFeedbackManager for this context.
  TransformFeedbackManager* GetTransformFeedbackManager() override;

  // Gets the VertexArrayManager for this context.
  VertexArrayManager* GetVertexArrayManager() override;

  // Gets the ImageManager for this context.
  ImageManager* GetImageManagerForTest() override;

  // Returns false if there are no pending queries.
  bool HasPendingQueries() const override;

  // Process any pending queries.
  void ProcessPendingQueries(bool did_finish) override;

  // Returns false if there is no idle work to be made.
  bool HasMoreIdleWork() const override;

  // Perform any idle work that needs to be made.
  void PerformIdleWork() override;

  bool HasPollingWork() const override;
  void PerformPollingWork() override;

  bool GetServiceTextureId(uint32_t client_texture_id,
                           uint32_t* service_texture_id) override;
  TextureBase* GetTextureBase(uint32_t client_id) override;

  // Provides detail about a lost context if one occurred.
  // Clears a level sub area of a texture
  // Returns false if a GL error should be generated.
  bool ClearLevel(Texture* texture,
                  unsigned target,
                  int level,
                  unsigned format,
                  unsigned type,
                  int xoffset,
                  int yoffset,
                  int width,
                  int height) override;

  // Clears a level sub area of a compressed 2D texture.
  // Returns false if a GL error should be generated.
  bool ClearCompressedTextureLevel(Texture* texture,
                                   unsigned target,
                                   int level,
                                   unsigned format,
                                   int width,
                                   int height) override;

  // Clears a level sub area of a compressed 3D texture.
  // Returns false if a GL error should be generated.
  bool ClearCompressedTextureLevel3D(Texture* texture,
                                     unsigned target,
                                     int level,
                                     unsigned format,
                                     int width,
                                     int height,
                                     int depth) override;

  // Indicates whether a given internal format is one for a compressed
  // texture.
  bool IsCompressedTextureFormat(unsigned format) override;

  // Clears a level of a 3D texture.
  // Returns false if a GL error should be generated.
  bool ClearLevel3D(Texture* texture,
                    unsigned target,
                    int level,
                    unsigned format,
                    unsigned type,
                    int width,
                    int height,
                    int depth) override;

  ErrorState* GetErrorState() override;

  std::unique_ptr<AbstractTexture> CreateAbstractTexture(
      unsigned target,
      unsigned internal_format,
      int width,
      int height,
      int depth,
      int border,
      unsigned format,
      unsigned type) override;

  void WaitForReadPixels(base::OnceClosure callback) override;

  // Returns true if the context was lost either by GL_ARB_robustness, forced
  // context loss or command buffer parse error.
  bool WasContextLost() const override;

  // Returns true if the context was lost specifically by GL_ARB_robustness.
  bool WasContextLostByRobustnessExtension() const override;

  // Lose this context.
  void MarkContextLost(error::ContextLostReason reason) override;

  // Update lost context state for use when making calls to the GL context
  // directly, and needing to know if they failed due to loss.
  bool CheckResetStatus() override;

  Logger* GetLogger() override;

  void BeginDecoding() override;
  void EndDecoding() override;

  const ContextState* GetContextState() override;
  scoped_refptr<ShaderTranslatorInterface> GetTranslator(GLenum type) override;

  void BindImage(uint32_t client_texture_id,
                 uint32_t texture_target,
                 gl::GLImage* image,
                 bool can_bind_to_sampler) override;

  void OnDebugMessage(GLenum source,
                      GLenum type,
                      GLuint id,
                      GLenum severity,
                      GLsizei length,
                      const GLchar* message);

  void SetCopyTextureResourceManagerForTest(
      CopyTextureCHROMIUMResourceManager* copy_texture_resource_manager)
      override;
  void SetCopyTexImageBlitterForTest(
      CopyTexImageResourceManager* copy_tex_image_blit) override;

  void OnAbstractTextureDestroyed(PassthroughAbstractTextureImpl*,
                                  scoped_refptr<TexturePassthrough>);

 private:
  // Allow unittests to inspect internal state tracking
  friend class GLES2DecoderPassthroughTestBase;

  const char* GetCommandName(unsigned int command_id) const;

  void SetOptionalExtensionsRequestedForTesting(bool request_extensions);

  void InitializeFeatureInfo(ContextType context_type,
                             const DisallowedFeatures& disallowed_features,
                             bool force_reinitialize);

  template <typename T, typename GLGetFunction>
  error::Error GetNumericHelper(GLenum pname,
                                GLsizei bufsize,
                                GLsizei* length,
                                T* params,
                                GLGetFunction get_call) {
    get_call(pname, bufsize, length, params);

    // Update the results of the query, if needed
    const error::Error error = PatchGetNumericResults(pname, *length, params);
    if (error != error::kNoError) {
      *length = 0;
      return error;
    }

    return error::kNoError;
  }

  template <typename T>
  error::Error PatchGetNumericResults(GLenum pname, GLsizei length, T* params);
  error::Error PatchGetFramebufferAttachmentParameter(GLenum target,
                                                      GLenum attachment,
                                                      GLenum pname,
                                                      GLsizei length,
                                                      GLint* params);

  template <typename T>
  error::Error PatchGetBufferResults(GLenum target,
                                     GLenum pname,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     T* params);

  void InsertError(GLenum error, const std::string& message);
  GLenum PopError();
  bool FlushErrors();

  bool IsEmulatedQueryTarget(GLenum target) const;
  error::Error ProcessQueries(bool did_finish);
  void RemovePendingQuery(GLuint service_id);

  struct BufferShadowUpdate;
  // BufferShadowUpdateMap's key is a buffer client id.
  using BufferShadowUpdateMap = base::flat_map<GLuint, BufferShadowUpdate>;
  void ReadBackBuffersIntoShadowCopies(const BufferShadowUpdateMap& updates);

  error::Error ProcessReadPixels(bool did_finish);

  // Checks to see if the inserted fence has completed.
  void ProcessDescheduleUntilFinished();

  void UpdateTextureBinding(GLenum target,
                            GLuint client_id,
                            TexturePassthrough* texture);
  void RebindTexture(TexturePassthrough* texture);

  void UpdateTextureSizeFromTexturePassthrough(TexturePassthrough* texture,
                                               GLuint client_id);
  void UpdateTextureSizeFromTarget(GLenum target);
  void UpdateTextureSizeFromClientID(GLuint client_id);

  error::Error BindTexImage2DCHROMIUMImpl(GLenum target,
                                          GLenum internalformat,
                                          GLint image_id);

  void VerifyServiceTextureObjectsExist();

  bool IsEmulatedFramebufferBound(GLenum target) const;

  const FeatureInfo::FeatureFlags& features() const {
    return feature_info_->feature_flags();
  }

  void ExitCommandProcessingEarly() override;

  void CheckSwapBuffersAsyncResult(const char* function_name,
                                   uint64_t swap_id,
                                   gfx::SwapResult result,
                                   std::unique_ptr<gfx::GpuFence> gpu_fence);
  error::Error CheckSwapBuffersResult(gfx::SwapResult result,
                                      const char* function_name);

  // Issue BindTexImage / CopyTexImage calls for |passthrough_texture|, if
  // they're pending.
  void BindOnePendingImage(GLenum target, TexturePassthrough* texture);

  // Issue BindTexImage / CopyTexImage calls for any GLImages that
  // requested it in BindImage, and are currently bound to textures that
  // are bound to samplers (i.e., are in |textures_pending_binding_|).
  void BindPendingImagesForSamplers();

  // Fail-fast inline version of BindPendingImagesForSamplers.
  inline void BindPendingImagesForSamplersIfNeeded() {
    if (!textures_pending_binding_.empty())
      BindPendingImagesForSamplers();
  }

  // Fail-fast version of BindPendingImages that operates on a single texture
  // that's specified by |client_id|.
  inline void BindPendingImageForClientIDIfNeeded(int client_id) {
    scoped_refptr<TexturePassthrough> texture = nullptr;

    // We could keep track of the number of |is_bind_pending| textures in
    // |resources_|, and elide all of this if it's zero.
    if (!resources_->texture_object_map.GetServiceID(client_id, &texture))
      return;

    if (texture && texture->is_bind_pending())
      BindOnePendingImage(texture->target(), texture.get());
  }

  inline void RemovePendingBindingTexture(GLenum target, GLuint unit) {
    // Note that this code was found to be faster than running base::EraseIf.
    size_t num_pending = textures_pending_binding_.size();
    for (size_t index = 0; index < num_pending; ++index) {
      TexturePendingBinding& pending = textures_pending_binding_[index];
      if (pending.target == target && pending.unit == unit) {
        textures_pending_binding_.erase(textures_pending_binding_.begin() +
                                        index);
        return;
      }
    }
  }

  bool OnlyHasPendingProgramCompletionQueries();

  // A set of raw pointers to currently living PassthroughAbstractTextures
  // which allow us to properly signal to them when we are destroyed.
  base::flat_set<PassthroughAbstractTextureImpl*> abstract_textures_;

  int commands_to_process_;

  DebugMarkerManager debug_marker_manager_;
  Logger logger_;

#define GLES2_CMD_OP(name) \
  Error Handle##name(uint32_t immediate_data_size, const volatile void* data);

  GLES2_COMMAND_LIST(GLES2_CMD_OP)
#undef GLES2_CMD_OP

  using CmdHandler =
      Error (GLES2DecoderPassthroughImpl::*)(uint32_t immediate_data_size,
                                             const volatile void* data);

  // A struct to hold info about each command.
  struct CommandInfo {
    CmdHandler cmd_handler;
    uint8_t arg_flags;   // How to handle the arguments for this scommand
    uint8_t cmd_flags;   // How to handle this command
    uint16_t arg_count;  // How many arguments are expected for this command.
  };

  // A table of CommandInfo for all the commands.
  static const CommandInfo command_info[kNumCommands - kFirstGLES2Command];

  // The GLApi to make the gl calls on.
  gl::GLApi* api_ = nullptr;

  // The GL context this decoder renders to on behalf of the client.
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  bool offscreen_;

  // The ContextGroup for this decoder uses to track resources.
  scoped_refptr<ContextGroup> group_;
  scoped_refptr<FeatureInfo> feature_info_;

  // By default, all requestable extensions should be loaded at initialization
  // time. Can be disabled for testing with only specific extensions enabled.
  bool request_optional_extensions_ = true;

  // Some objects may generate resources when they are bound even if they were
  // not generated yet: texture, buffer, renderbuffer, framebuffer, transform
  // feedback, vertex array
  bool bind_generates_resource_;

  // Mappings from client side IDs to service side IDs for shared objects
  PassthroughResources* resources_ = nullptr;

  // Mappings from client side IDs to service side IDs for per-context objects
  ClientServiceMap<GLuint, GLuint> framebuffer_id_map_;
  ClientServiceMap<GLuint, GLuint> transform_feedback_id_map_;
  ClientServiceMap<GLuint, GLuint> query_id_map_;
  ClientServiceMap<GLuint, GLuint> vertex_array_id_map_;

  // Mailboxes
  MailboxManager* mailbox_manager_ = nullptr;

  std::unique_ptr<GpuFenceManager> gpu_fence_manager_;

  std::unique_ptr<MultiDrawManager> multi_draw_manager_;

  // State tracking of currently bound 2D textures (client IDs)
  size_t active_texture_unit_;

  enum class TextureTarget : uint8_t {
    k2D = 0,
    kCubeMap = 1,
    k2DArray = 2,
    k3D = 3,
    k2DMultisample = 4,
    kExternal = 5,
    kRectangle = 6,

    kUnkown = 7,
    kCount = kUnkown,
  };
  static TextureTarget GLenumToTextureTarget(GLenum target);

  struct BoundTexture {
    BoundTexture();
    ~BoundTexture();
    BoundTexture(const BoundTexture&);
    BoundTexture(BoundTexture&&);
    BoundTexture& operator=(const BoundTexture&);
    BoundTexture& operator=(BoundTexture&&);

    GLuint client_id = 0;
    scoped_refptr<TexturePassthrough> texture;
  };

  // Tracked viewport and scissor state for surface offset
  GLint viewport_[4] = {0, 0, 0, 0};
  GLint scissor_[4] = {0, 0, 0, 0};
  gfx::Vector2d GetSurfaceDrawOffset() const;
  void ApplySurfaceDrawOffset();

  // Use a limit that is at least ANGLE's IMPLEMENTATION_MAX_ACTIVE_TEXTURES
  // constant
  static constexpr size_t kMaxTextureUnits = 64;
  static constexpr size_t kNumTextureTypes =
      static_cast<size_t>(TextureTarget::kCount);
  std::array<std::array<BoundTexture, kMaxTextureUnits>, kNumTextureTypes>
      bound_textures_;

  // [target, texture unit, texture] where texture has a bound GLImage that
  // requires bind / copy before draw.
  struct TexturePendingBinding {
    TexturePendingBinding(GLenum target,
                          GLuint unit,
                          base::WeakPtr<TexturePassthrough> texture);
    TexturePendingBinding(const TexturePendingBinding& other);
    TexturePendingBinding(TexturePendingBinding&& other);
    ~TexturePendingBinding();

    TexturePendingBinding& operator=(const TexturePendingBinding& other);
    TexturePendingBinding& operator=(TexturePendingBinding&& other);

    GLenum target;
    GLuint unit;
    base::WeakPtr<TexturePassthrough> texture;
  };
  std::vector<TexturePendingBinding> textures_pending_binding_;

  // State tracking of currently bound buffers
  std::unordered_map<GLenum, GLuint> bound_buffers_;

  // Track the service-id to type of all queries for validation
  struct QueryInfo {
    GLenum type = GL_NONE;
  };
  std::unordered_map<GLuint, QueryInfo> query_info_map_;

  // All queries that are waiting for their results to be ready
  struct PendingQuery {
    PendingQuery();
    ~PendingQuery();
    PendingQuery(const PendingQuery&) = delete;
    PendingQuery(PendingQuery&&);
    PendingQuery& operator=(const PendingQuery&) = delete;
    PendingQuery& operator=(PendingQuery&&);

    GLenum target = GL_NONE;
    GLuint service_id = 0;

    scoped_refptr<gpu::Buffer> shm;
    QuerySync* sync = nullptr;
    base::subtle::Atomic32 submit_count = 0;

    std::unique_ptr<gl::GLFence> commands_completed_fence;
    base::TimeDelta commands_issued_time;
    base::TimeTicks commands_issued_timestamp;

    std::vector<base::OnceClosure> callbacks;
    std::unique_ptr<gl::GLFence> buffer_shadow_update_fence = nullptr;
    BufferShadowUpdateMap buffer_shadow_updates;
    GLuint program_service_id = 0u;
  };
  base::circular_deque<PendingQuery> pending_queries_;

  // Currently active queries
  struct ActiveQuery {
    ActiveQuery();
    ~ActiveQuery();
    ActiveQuery(const ActiveQuery&) = delete;
    ActiveQuery(ActiveQuery&&);
    ActiveQuery& operator=(const ActiveQuery&) = delete;
    ActiveQuery& operator=(ActiveQuery&&);

    GLuint service_id = 0;
    scoped_refptr<gpu::Buffer> shm;
    QuerySync* sync = nullptr;

    // Time at which the commands for this query started processing. This is
    // used to ensure we only include the time when the decoder is scheduled in
    // the |active_time|. Used for GL_COMMANDS_ISSUED_CHROMIUM type query.
    base::TimeTicks command_processing_start_time;
    base::TimeDelta active_time;
  };
  std::unordered_map<GLenum, ActiveQuery> active_queries_;

  // Pending async ReadPixels calls
  struct PendingReadPixels {
    PendingReadPixels();
    ~PendingReadPixels();
    PendingReadPixels(PendingReadPixels&&);
    PendingReadPixels& operator=(PendingReadPixels&&);

    std::unique_ptr<gl::GLFence> fence = nullptr;
    GLuint buffer_service_id = 0;
    uint32_t pixels_size = 0;
    uint32_t pixels_shm_id = 0;
    uint32_t pixels_shm_offset = 0;
    uint32_t result_shm_id = 0;
    uint32_t result_shm_offset = 0;

    // Service IDs of GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM queries waiting for
    // this read pixels operation to complete
    base::flat_set<GLuint> waiting_async_pack_queries;

    DISALLOW_COPY_AND_ASSIGN(PendingReadPixels);
  };
  base::circular_deque<PendingReadPixels> pending_read_pixels_;

  struct BufferShadowUpdate {
    BufferShadowUpdate();
    ~BufferShadowUpdate();
    BufferShadowUpdate(BufferShadowUpdate&&);
    BufferShadowUpdate& operator=(BufferShadowUpdate&&);

    scoped_refptr<gpu::Buffer> shm;
    GLuint shm_offset = 0;
    GLuint size = 0;
    DISALLOW_COPY_AND_ASSIGN(BufferShadowUpdate);
  };
  BufferShadowUpdateMap buffer_shadow_updates_;

  // Error state
  std::set<GLenum> errors_;

  // Checks if an error has been generated since the last call to
  // CheckErrorCallbackState
  bool CheckErrorCallbackState();
  bool had_error_callback_ = false;

  // Default framebuffer emulation
  struct EmulatedDefaultFramebufferFormat {
    GLenum color_renderbuffer_internal_format = GL_NONE;
    GLenum color_texture_internal_format = GL_NONE;
    GLenum color_texture_format = GL_NONE;
    GLenum color_texture_type = GL_NONE;
    GLenum depth_stencil_internal_format = GL_NONE;
    GLenum depth_internal_format = GL_NONE;
    GLenum stencil_internal_format = GL_NONE;
    GLint samples = 0;
  };

  struct EmulatedColorBuffer {
    explicit EmulatedColorBuffer(
        gl::GLApi* api,
        const EmulatedDefaultFramebufferFormat& format_in);
    ~EmulatedColorBuffer();

    void Resize(const gfx::Size& new_size);
    void Destroy(bool have_context);

    gl::GLApi* api;

    scoped_refptr<TexturePassthrough> texture;

    gfx::Size size;
    EmulatedDefaultFramebufferFormat format;

    DISALLOW_COPY_AND_ASSIGN(EmulatedColorBuffer);
  };

  struct EmulatedDefaultFramebuffer {
    EmulatedDefaultFramebuffer(
        gl::GLApi* api,
        const EmulatedDefaultFramebufferFormat& format_in,
        const FeatureInfo* feature_info,
        bool supports_separate_fbo_bindings);
    ~EmulatedDefaultFramebuffer();

    // Set a new color buffer, return the old one
    std::unique_ptr<EmulatedColorBuffer> SetColorBuffer(
        std::unique_ptr<EmulatedColorBuffer> new_color_buffer);

    // Blit this framebuffer into another same-sized color buffer
    void Blit(EmulatedColorBuffer* target);

    bool Resize(const gfx::Size& new_size, const FeatureInfo* feature_info);
    void Destroy(bool have_context);

    gl::GLApi* api;
    bool supports_separate_fbo_bindings = false;

    // Service ID of the framebuffer
    GLuint framebuffer_service_id = 0;

    // Service ID of the color renderbuffer (if multisampled)
    GLuint color_buffer_service_id = 0;

    // Color buffer texture (if not multisampled)
    std::unique_ptr<EmulatedColorBuffer> color_texture;

    // Service ID of the depth stencil renderbuffer
    GLuint depth_stencil_buffer_service_id = 0;

    // Service ID of the depth renderbuffer
    GLuint depth_buffer_service_id = 0;

    // Service ID of the stencil renderbuffer (
    GLuint stencil_buffer_service_id = 0;

    gfx::Size size;
    EmulatedDefaultFramebufferFormat format;

    DISALLOW_COPY_AND_ASSIGN(EmulatedDefaultFramebuffer);
  };
  EmulatedDefaultFramebufferFormat emulated_default_framebuffer_format_;
  std::unique_ptr<EmulatedDefaultFramebuffer> emulated_back_buffer_;
  std::unique_ptr<EmulatedColorBuffer> emulated_front_buffer_;
  bool offscreen_single_buffer_;
  bool offscreen_target_buffer_preserved_;
  std::vector<std::unique_ptr<EmulatedColorBuffer>> in_use_color_textures_;
  std::vector<std::unique_ptr<EmulatedColorBuffer>> available_color_textures_;
  size_t create_color_buffer_count_for_test_;

  // Maximum 2D resource sizes for limiting offscreen framebuffer sizes
  GLint max_2d_texture_size_ = 0;
  GLint max_renderbuffer_size_ = 0;
  GLint max_offscreen_framebuffer_size_ = 0;

  // State tracking of currently bound draw and read framebuffers (client IDs)
  GLuint bound_draw_framebuffer_;
  GLuint bound_read_framebuffer_;

  // If this context supports both read and draw framebuffer bindings
  bool supports_separate_fbo_bindings_ = false;

  // Tracing
  std::unique_ptr<GPUTracer> gpu_tracer_;
  const unsigned char* gpu_decoder_category_ = nullptr;
  int gpu_trace_level_;
  bool gpu_trace_commands_;
  bool gpu_debug_commands_;

  // Context lost state
  bool context_lost_;
  bool reset_by_robustness_extension_;
  bool lose_context_when_out_of_memory_;

  // After a second fence is inserted, both the GpuChannelMessageQueue and
  // CommandExecutor are descheduled. Once the first fence has completed, both
  // get rescheduled.
  std::vector<std::unique_ptr<gl::GLFence>> deschedule_until_finished_fences_;

  GLuint linking_program_service_id_ = 0u;

  // CA Layer state
  std::unique_ptr<CALayerSharedState> ca_layer_shared_state_;

  base::WeakPtrFactory<GLES2DecoderPassthroughImpl> weak_ptr_factory_{this};

  class ScopedEnableTextureRectangleInShaderCompiler;

// Include the prototypes of all the doer functions from a separate header to
// keep this file clean.
#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough_doer_prototypes.h"
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_PASSTHROUGH_H_
