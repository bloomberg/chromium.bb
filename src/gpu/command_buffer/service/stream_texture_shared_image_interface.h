// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_SHARED_IMAGE_INTERFACE_H_
#define GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_SHARED_IMAGE_INTERFACE_H_

#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"

namespace gpu {
class TextureOwner;
class TextureBase;

// This class is a specialized GLImage that lets SharedImageVideo draw video
// frames.
class GPU_GLES2_EXPORT StreamTextureSharedImageInterface : public gl::GLImage {
 public:
  enum class BindingsMode {
    // Binds image to the texture with service id. Doesn't alter current gl
    // bindings.
    kBindImage,

    // Updates the current image but does not bind it.
    kDontBindImage
  };

  // Release the underlying resources. This should be called when the image is
  // not longer valid or the context is lost.
  virtual void ReleaseResources() = 0;

  // Whether the StreamTextureSharedImageInterface is accounting for gpu memory
  // or not.
  virtual bool IsUsingGpuMemory() const = 0;

  // Update texture image to the most recent frame and bind it to the provided
  // texture |service_id| if TextureOwner does not implicitly binds texture
  // during the update.
  // If TextureOwner() always binds texture implicitly during the update, then
  // it will always bind it to TextureOwner's texture id and not to the
  // |service_id|.
  virtual void UpdateAndBindTexImage(GLuint service_id) = 0;

  virtual bool HasTextureOwner() const = 0;
  virtual TextureBase* GetTextureBase() const = 0;

  // Notify the texture of overlay decision, When overlay promotion is true,
  // this also sets the bounds of where the overlay is.
  virtual void NotifyOverlayPromotion(bool promotion,
                                      const gfx::Rect& bounds) = 0;
  // Render the video frame into an overlay plane. Should only be called after
  // the overlay promotion. Return true if it could render to overlay correctly.
  virtual bool RenderToOverlay() = 0;

  // Whether TextureOwner's implementation binds texture to TextureOwner owned
  // texture_id during the texture update.
  virtual bool TextureOwnerBindsTextureOnUpdate() = 0;

 protected:
  ~StreamTextureSharedImageInterface() override = default;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_SHARED_IMAGE_INTERFACE_H_
