// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_GL_RENDERER_COPIER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_GL_RENDERER_COPIER_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_runner.h"
#include "base/unguessable_token.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class ColorSpace;
class Rect;
class Vector2d;
}  // namespace gfx

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace viz {

class ContextProvider;
class CopyOutputRequest;
class GLI420Converter;
class GLNV12Converter;
class GLScaler;
class TextureDeleter;

namespace copy_output {
struct RenderPassGeometry;
}  // namespace copy_output

// Helper class for GLRenderer that executes CopyOutputRequests using GL, to
// perform texture copies/transformations and read back bitmaps. Also manages
// the caching of resources needed to ensure efficient video performance.
//
// GLRenderer calls CopyFromTextureOrFramebuffer() to execute a
// CopyOutputRequest. GLRendererCopier will examine the request and determine
// the minimal amount of work needed to satisfy all the requirements of the
// request.
//
// In many cases, interim GL objects (textures, framebuffers, etc.) must be
// created as part of a multi-step process. When considering video performance
// (i.e., a series of CopyOutputRequests from the same "source"), these interim
// objects must be cached to prevent a significant performance penalty on some
// GPU/drivers. GLRendererCopier manages such a cache and automatically frees
// the objects when it detects that a stream of CopyOutputRequests from a given
// "source" has ended.
class VIZ_SERVICE_EXPORT GLRendererCopier {
 public:
  // Define types to avoid pulling in command buffer GL headers, which conflict
  // the ui/gl/gl_bindings.h
  using GLuint = unsigned int;
  using GLenum = unsigned int;

  // |context_provider| and |texture_deleter| must outlive this instance.
  GLRendererCopier(ContextProvider* context_provider,
                   TextureDeleter* texture_deleter);

  GLRendererCopier(const GLRendererCopier&) = delete;
  GLRendererCopier& operator=(const GLRendererCopier&) = delete;

  ~GLRendererCopier();

  // Executes the |request|, copying from the currently-bound framebuffer of the
  // given |internal_format|. |output_rect| is the RenderPass's output Rect in
  // draw space, and is used to translate and clip the result selection Rect in
  // the request. |framebuffer_texture| and |framebuffer_texture_size| are
  // optional, but desired for performance: If provided, the texture might be
  // used as the source, to avoid having to make a copy of the framebuffer.
  // |flipped_source| is true (common case) if the framebuffer content is
  // vertically flipped (bottom-up row order). |framebuffer_color_space|
  // specifies the color space of the pixels in the framebuffer.
  //
  // This implementation may change a wide variety of GL state, such as texture
  // and framebuffer bindings, shader programs, and related attributes; and so
  // the caller must not make any assumptions about the state of the GL context
  // after this call.
  void CopyFromTextureOrFramebuffer(
      std::unique_ptr<CopyOutputRequest> request,
      const copy_output::RenderPassGeometry& geometry,
      GLenum internal_format,
      GLuint framebuffer_texture,
      const gfx::Size& framebuffer_texture_size,
      bool flipped_source,
      const gfx::ColorSpace& framebuffer_color_space);

  // Checks whether cached resources should be freed because recent copy
  // activity is no longer using them. This should be called after a frame has
  // finished drawing (after all copy requests have been executed).
  void FreeUnusedCachedResources();

 private:
  friend class GLRendererCopierTest;

  // The collection of resources that might be cached over multiple copy
  // requests from the same source. While executing a CopyOutputRequest, this
  // struct is also used to pass around intermediate objects between operations.
  struct VIZ_SERVICE_EXPORT ReusableThings {
    // This is used to determine whether these things aren't being used anymore.
    uint32_t purge_count_at_last_use = 0;

    // Texture containing a copy of the source framebuffer, if the source
    // framebuffer cannot be used directly.
    GLuint fb_copy_texture = 0;
    GLenum fb_copy_texture_internal_format = static_cast<GLenum>(0 /*GL_NONE*/);
    gfx::Size fb_copy_texture_size;

    // RGBA requests: Scaling, and texture/framebuffer for readback.
    std::unique_ptr<GLScaler> scaler;
    GLuint result_texture = 0;
    gfx::Size result_texture_size;
    GLuint readback_framebuffer = 0;

    // I420_PLANES & NV12_PLANES requests: I420, NV12 scaling and format
    // conversion, and textures+framebuffers for readback.
    std::unique_ptr<GLI420Converter> i420_converter;
    std::unique_ptr<GLNV12Converter> nv12_converter;
    std::array<GLuint, 3> yuv_textures = {0, 0, 0};
    std::array<gfx::Size, 3> texture_sizes;
    std::array<GLuint, 3> yuv_readback_framebuffers = {0, 0, 0};

    ReusableThings();

    ReusableThings(const ReusableThings&) = delete;
    ReusableThings& operator=(const ReusableThings&) = delete;

    ~ReusableThings();

    // Frees all the GL objects and scalers. This is in-lieu of a ReusableThings
    // destructor because a valid GL context is required to free some of the
    // objects.
    void Free(gpu::gles2::GLES2Interface* gl);
  };

  // Manages the execution of one asynchronous framebuffer readback and contains
  // all the relevant state needed to complete a copy request. The constructor
  // initiates the operation, and the destructor cleans up all the GL objects
  // created for this workflow. This class is owned by the GLRendererCopier, and
  // GLRendererCopier is responsible for deleting this either after the workflow
  // is finished, or when the GLRendererCopier is being destroyed.
  struct ReadPixelsWorkflow {
   public:
    // Saves all revelant state and initiates the GL asynchronous read-pixels
    // workflow.
    ReadPixelsWorkflow(std::unique_ptr<CopyOutputRequest> copy_request,
                       const gfx::Vector2d& readback_offset,
                       bool flipped_source,
                       bool swap_red_and_blue,
                       const gfx::Rect& result_rect,
                       const gfx::ColorSpace& color_space,
                       ContextProvider* context_provider,
                       GLenum readback_format);
    ReadPixelsWorkflow(const ReadPixelsWorkflow&) = delete;

    // The destructor is by the GLRendererCopier, either called after the
    // workflow is finished or when GLRendererCopier is being destoryed.
    ~ReadPixelsWorkflow();

    GLuint query() const { return query_; }

    const std::unique_ptr<CopyOutputRequest> copy_request;
    const bool flipped_source;
    const bool swap_red_and_blue;
    const gfx::Rect result_rect;
    const gfx::ColorSpace color_space;
    GLuint transfer_buffer = 0;

   private:
    const raw_ptr<ContextProvider> context_provider_;
    GLuint query_ = 0;
  };

  // Renders a scaled/transformed copy of a source texture according to the
  // |request| parameters and other source characteristics. |result_texture|
  // must be allocated/sized by the caller. For RGBA requests with destination
  // set to system memory, the image content will be rendered in top-down row
  // order and maybe red-blue swapped, to support efficient readback later on.
  // For RGBA requests with ResultDestination::kNativeTextures set, the image
  // content is always rendered Y-flipped (bottom-up row order).
  void RenderResultTexture(const CopyOutputRequest& request,
                           bool flipped_source,
                           const gfx::ColorSpace& source_color_space,
                           const gfx::ColorSpace& dest_color_space,
                           GLuint source_texture,
                           const gfx::Size& source_texture_size,
                           const gfx::Rect& sampling_rect,
                           const gfx::Rect& result_rect,
                           GLuint result_texture,
                           ReusableThings* things);

  // Like the ReadPixelsWorkflow, except for I420 planes readback. Because there
  // are three separate glReadPixels operations that may complete in any order,
  // a ReadI420PlanesWorkflow will receive notifications from three separate "GL
  // query" callbacks. It is only after all three operations have completed that
  // a fully-assembled CopyOutputResult can be sent.
  //
  // See class comments for GLI420Converter for an explanation of how
  // planar data is packed into RGBA textures.
  struct ReadI420PlanesWorkflow {
   public:
    ReadI420PlanesWorkflow(std::unique_ptr<CopyOutputRequest> copy_request,
                           const gfx::Rect& aligned_rect,
                           const gfx::Rect& result_rect,
                           base::WeakPtr<GLRendererCopier> copier_weak_ptr,
                           ContextProvider* context_provider);

    void BindTransferBuffer();
    void StartPlaneReadback(int plane, GLenum readback_format);
    void UnbindTransferBuffer();

    ~ReadI420PlanesWorkflow();

    const std::unique_ptr<CopyOutputRequest> copy_request;
    const gfx::Rect aligned_rect;
    const gfx::Rect result_rect;
    GLuint transfer_buffer;
    std::array<GLuint, 3> queries;

   private:
    gfx::Size y_texture_size() const;
    gfx::Size chroma_texture_size() const;

    base::WeakPtr<GLRendererCopier> copier_weak_ptr_;
    const raw_ptr<ContextProvider> context_provider_;
    std::array<int, 3> data_offsets_;
  };

  // Like the ReadPixelsWorkflow, except for NV12 planes readback. Because there
  // are two separate glReadPixels operations that may complete in any order,
  // a ReadNV12PlanesWorkflow will receive notifications from two separate "GL
  // query" callbacks. It is only after all two operations have completed that
  // a fully-assembled CopyOutputResult can be sent.
  //
  // See class comments for GLNV12Converter for an explanation of how planar
  // data is packed into RGBA textures.
  class ReadNV12PlanesWorkflow {
   public:
    ReadNV12PlanesWorkflow(std::unique_ptr<CopyOutputRequest> copy_request,
                           const gfx::Rect& aligned_rect,
                           const gfx::Rect& result_rect,
                           base::WeakPtr<GLRendererCopier> copier_weak_ptr,
                           ContextProvider* context_provider);
    ~ReadNV12PlanesWorkflow();

    void BindTransferBuffer();
    void StartPlaneReadback(int plane, GLenum readback_format);
    void UnbindTransferBuffer();

    gfx::Rect aligned_rect() const { return aligned_rect_; }

    gfx::Rect result_rect() const { return result_rect_; }

    std::unique_ptr<CopyOutputRequest> TakeRequest() {
      DCHECK(copy_request_);

      return std::move(copy_request_);
    }

    GLuint TakeTransferBuffer() {
      DCHECK(transfer_buffer_);

      GLuint result = transfer_buffer_;
      transfer_buffer_ = 0;
      return result;
    }

    // Returns true if the workflow has completed (i.e. readback requests for
    // all planes have finished).
    bool IsCompleted() const {
      return queries_ == std::array<GLuint, 2>{{0, 0}};
    }

    GLuint query(int plane) { return queries_[plane]; }

    // Marks that a readback has completed for a given plane.
    void MarkQueryCompleted(int plane) { queries_[plane] = 0; }

   private:
    gfx::Size y_texture_size() const;
    gfx::Size chroma_texture_size() const;

    std::unique_ptr<CopyOutputRequest> copy_request_;
    const gfx::Rect aligned_rect_;
    const gfx::Rect result_rect_;
    GLuint transfer_buffer_;
    std::array<GLuint, 2> queries_;

    base::WeakPtr<GLRendererCopier> copier_weak_ptr_;
    const raw_ptr<ContextProvider> context_provider_;
    std::array<int, 2> data_offsets_;
  };

  // Similar to RenderResultTexture(), except also transform the image into I420
  // format (a popular video format). Three textures, representing each of the
  // Y/U/V planes (as described in GLI420Converter), are populated and their GL
  // references placed in |things|. The image content is always rendered in
  // top-down row order and swizzled (if needed), to support efficient readback
  // later on.
  //
  // For alignment reasons, sometimes a slightly larger result will be provided,
  // and the return Rect will indicate the actual bounds that were rendered
  // (|result_rect|'s coordinate system). See StartI420ReadbackFromTextures()
  // for more details.
  gfx::Rect RenderI420Textures(const CopyOutputRequest& request,
                               bool flipped_source,
                               const gfx::ColorSpace& source_color_space,
                               GLuint source_texture,
                               const gfx::Size& source_texture_size,
                               const gfx::Rect& sampling_rect,
                               const gfx::Rect& result_rect,
                               ReusableThings* things);

  // Similar to RenderResultTexture(), except also transform the image into NV12
  // format (a popular video format). Two textures, representing each of the
  // Y/UV planes (as described in GLNV12Converter), are populated and their GL
  // references placed in |things|. The image content is always rendered in
  // top-down row order and swizzled (if needed), to support efficient readback
  // later on.
  //
  // For alignment reasons, sometimes a slightly larger result will be provided,
  // and the return Rect will indicate the actual bounds that were rendered
  // (|result_rect|'s coordinate system). See StartNV12ReadbackFromTextures()
  // for more details.
  gfx::Rect RenderNV12Textures(const CopyOutputRequest& request,
                               bool flipped_source,
                               const gfx::ColorSpace& source_color_space,
                               GLuint source_texture,
                               const gfx::Size& source_texture_size,
                               const gfx::Rect& sampling_rect,
                               const gfx::Rect& result_rect,
                               ReusableThings* things);

  // Binds the |things->result_texture| to a framebuffer and calls
  // StartReadbackFromFramebuffer(). This is only for RGBA requests with
  // destination set to kSystemMemory.
  // Assumes the image content is in top-down row order (and is red-blue swapped
  // iff RenderResultTexture() would have done that).
  void StartReadbackFromTexture(std::unique_ptr<CopyOutputRequest> request,
                                const gfx::Rect& result_rect,
                                const gfx::ColorSpace& color_space,
                                ReusableThings* things);

  // Processes the next phase of the copy request by starting readback from the
  // currently-bound framebuffer into a pixel transfer buffer. |readback_offset|
  // is the origin of the readback rect within the framebuffer, with
  // |result_rect| providing the size of the readback rect. |flipped_source| is
  // true if the framebuffer content is in bottom-up row order, and
  // |swapped_red_and_blue| specifies whether the red and blue channels have
  // been swapped. This method kicks-off an asynchronous glReadPixels()
  // workflow.
  void StartReadbackFromFramebuffer(std::unique_ptr<CopyOutputRequest> request,
                                    const gfx::Vector2d& readback_offset,
                                    bool flipped_source,
                                    bool swapped_red_and_blue,
                                    const gfx::Rect& result_rect,
                                    const gfx::ColorSpace& color_space);

  // Renders a scaled/transformed copy of a source texture similarly to
  // RenderResultTexture, but packages up the result in a mailbox and sends it
  // as the result to the CopyOutputRequest.
  void RenderAndSendTextureResult(std::unique_ptr<CopyOutputRequest> request,
                                  bool flipped_source,
                                  const gfx::ColorSpace& source_color_space,
                                  const gfx::ColorSpace& dest_color_space,
                                  GLuint source_texture,
                                  const gfx::Size& source_texture_size,
                                  const gfx::Rect& sampling_rect,
                                  const gfx::Rect& result_rect,
                                  ReusableThings* things);

  // Like StartReadbackFromTexture(), except that this processes the three Y/U/V
  // result textures in |things| by using three framebuffers and three
  // asynchronous readback operations. A single pixel transfer buffer is used to
  // hold the results of all three readbacks (i.e., each plane starts at a
  // different offset in the transfer buffer).
  //
  // |aligned_rect| is the Rect returned from the RenderI420Textures() call, and
  // is required so that the CopyOutputResult sent at the end of this workflow
  // will access the correct region of pixels.
  void StartI420ReadbackFromTextures(std::unique_ptr<CopyOutputRequest> request,
                                     const gfx::Rect& aligned_rect,
                                     const gfx::Rect& result_rect,
                                     ReusableThings* things);

  // Like StartReadbackFromTexture(), except that this processes the two Y/UV
  // result textures in |things| by using two framebuffers and two asynchronous
  // readback operations. A single pixel transfer buffer is used to hold the
  // results of both readbacks (i.e., each plane starts at a different offset in
  // the transfer buffer).
  //
  // |aligned_rect| is the Rect returned from the RenderNV12Textures() call, and
  // is required so that the CopyOutputResult sent at the end of this workflow
  // will access the correct region of pixels.
  void StartNV12ReadbackFromTextures(std::unique_ptr<CopyOutputRequest> request,
                                     const gfx::Rect& aligned_rect,
                                     const gfx::Rect& result_rect,
                                     ReusableThings* things);

  // Retrieves a cached ReusableThings instance for the given CopyOutputRequest
  // source, or creates a new instance.
  std::unique_ptr<ReusableThings> TakeReusableThingsOrCreate(
      const base::UnguessableToken& requester);

  // If |requester| is a valid UnguessableToken, this stashes the given
  // ReusableThings instance in the cache for use in future CopyOutputRequests
  // from the same requester. Otherwise, |things| is freed.
  void StashReusableThingsOrDelete(const base::UnguessableToken& requester,
                                   std::unique_ptr<ReusableThings> things);

  // Queries the GL implementation to determine which is the more performance-
  // optimal supported readback format: GL_RGBA or GL_BGRA_EXT, and memoizes the
  // result for all future calls.
  //
  // Precondition: The GL context has a complete, bound framebuffer ready for
  // readback.
  GLenum GetOptimalReadbackFormat();

  // Returns true if the red and blue channels should be swapped within the GPU,
  // where such an operation has negligible cost, so that later the red-blue
  // swap does not need to happen on the CPU (non-negligible cost).
  bool ShouldSwapRedAndBlueForBitmapReadback();

  void FinishReadPixelsWorkflow(ReadPixelsWorkflow*);
  void FinishReadI420PlanesWorkflow(ReadI420PlanesWorkflow*, int plane);
  void FinishReadNV12PlanesWorkflow(ReadNV12PlanesWorkflow* workflow,
                                    int plane);

  // Injected dependencies.
  const raw_ptr<ContextProvider> context_provider_;
  const raw_ptr<TextureDeleter> texture_deleter_;

  // This increments by one for every call to FreeUnusedCachedResources(). It
  // is meant to determine when cached resources should be freed because they
  // are unlikely to see further use.
  uint32_t purge_counter_ = 0;

  // A cache of resources recently used in the execution of a stream of copy
  // requests from the same source. Since this reflects the number of active
  // video captures, it is expected to almost always be zero or one entry in
  // size.
  base::flat_map<base::UnguessableToken, std::unique_ptr<ReusableThings>>
      cache_;

  // This specifies whether the GPU+driver combination executes readback more
  // efficiently using GL_RGBA or GL_BGRA_EXT format. This starts out as
  // GL_NONE, which means "unknown," and will be determined at the time the
  // first readback request is made.
  GLenum optimal_readback_format_ = static_cast<GLenum>(0 /*GL_NONE*/);

  // Purge cache entries that have not been used after this many calls to
  // FreeUnusedCachedResources(). The choice of 60 is arbitrary, but on most
  // platforms means that a somewhat-to-fully active compositor will cause
  // things to be auto-purged after approx. 1-2 seconds of not being used.
  static constexpr int kKeepalivePeriod = 60;

  std::vector<std::unique_ptr<ReadPixelsWorkflow>> read_pixels_workflows_;
  std::vector<std::unique_ptr<ReadI420PlanesWorkflow>> read_i420_workflows_;
  std::vector<std::unique_ptr<ReadNV12PlanesWorkflow>> read_nv12_workflows_;

  // Weak ptr to this class.
  base::WeakPtrFactory<GLRendererCopier> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_GL_RENDERER_COPIER_H_
