// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_ANDROID_SURFACE_OWNER_ANDROID_H_
#define GPU_IPC_COMMON_ANDROID_SURFACE_OWNER_ANDROID_H_

#include <memory>

#include "base/android/android_image_reader_compat.h"
#include "base/memory/ref_counted.h"
#include "gpu/gpu_export.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace gpu {

// This interface wraps the usage of surface and the associated OpenGL
// texture_id. Surface owned by this class is used by OpenGL, MediaPlayer or
// CameraDevice to draw into. Once a frame is drawn into the surface, the
// associated callback function is run. The drawn frame can be used to update
// the GL texture at target texture_id.
class GPU_EXPORT SurfaceOwner {
 public:
  virtual ~SurfaceOwner();

  // Creates a new SurfaceOwner with the provided texture id. This texture_id
  // target will be updated when UpdateTexImage() is called.
  static std::unique_ptr<SurfaceOwner> Create(uint32_t texture_id);

  // Set the callback function to run when a new frame is available.
  virtual void SetFrameAvailableCallback(
      const base::RepeatingClosure& callback) = 0;

  // Create a java surface for the SurfaceOwner.
  virtual gl::ScopedJavaSurface CreateJavaSurface() const = 0;

  // Transformation matrix if any associated with the texture image.
  virtual void GetTransformMatrix(float mtx[16]) = 0;

  // Update the texture image using the latest available image data.
  virtual void UpdateTexImage() = 0;

 protected:
  SurfaceOwner();

 private:
  DISALLOW_COPY_AND_ASSIGN(SurfaceOwner);
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_ANDROID_SURFACE_OWNER_ANDROID_H_
