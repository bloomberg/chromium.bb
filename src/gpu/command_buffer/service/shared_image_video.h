// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_VIDEO_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_VIDEO_H_

#include <memory>

#include "gpu/command_buffer/service/shared_image_backing_android.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gpu {
struct Mailbox;
struct VulkanYCbCrInfo;
class RefCountedLock;
class StreamTextureSharedImageInterface;
class SharedContextState;
class TextureOwner;

namespace gles2 {
class AbstractTexture;
}  // namespace gles2

// Implementation of SharedImageBacking that renders MediaCodec buffers to a
// TextureOwner or overlay as needed in order to draw them.
class GPU_GLES2_EXPORT SharedImageVideo : public SharedImageBackingAndroid {
 public:
  static std::unique_ptr<SharedImageVideo> Create(
      const Mailbox& mailbox,
      const gfx::Size& size,
      const gfx::ColorSpace color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
      scoped_refptr<SharedContextState> context_state,
      scoped_refptr<RefCountedLock> drdc_lock);

  // Returns ycbcr information. This is only valid in vulkan context and
  // nullopt for other context.
  static absl::optional<VulkanYCbCrInfo> GetYcbcrInfo(
      TextureOwner* texture_owner,
      scoped_refptr<SharedContextState> context_state);

  ~SharedImageVideo() override;

  // Disallow copy and assign.
  SharedImageVideo(const SharedImageVideo&) = delete;
  SharedImageVideo& operator=(const SharedImageVideo&) = delete;

  // SharedImageBacking implementation.
  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;

 protected:
  SharedImageVideo(const Mailbox& mailbox,
                   const gfx::Size& size,
                   const gfx::ColorSpace color_space,
                   GrSurfaceOrigin surface_origin,
                   SkAlphaType alpha_type,
                   bool is_thread_safe);

  std::unique_ptr<gles2::AbstractTexture> GenAbstractTexture(
      const bool passthrough);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_VIDEO_H_
