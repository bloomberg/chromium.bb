// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANDROID_SURFACE_TEXTURE_H_
#define UI_GL_ANDROID_SURFACE_TEXTURE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "ui/gl/gl_export.h"

struct ANativeWindow;

namespace gfx {

// This class serves as a bridge for native code to call java functions inside
// android SurfaceTexture class.
class GL_EXPORT SurfaceTexture
    : public base::RefCountedThreadSafe<SurfaceTexture>{
 public:
  explicit SurfaceTexture(int texture_id);

  // Set the listener callback, which will be invoked on the same thread that
  // is being called from here for registration.
  // Note: Since callbacks come in from Java objects that might outlive objects
  // being referenced from the callback, the only robust way here is to create
  // the callback from a weak pointer to your object.
  void SetFrameAvailableCallback(const base::Closure& callback);

  // Update the texture image to the most recent frame from the image stream.
  void UpdateTexImage();

  // Retrieve the 4x4 texture coordinate transform matrix associated with the
  // texture image set by the most recent call to updateTexImage.
  void GetTransformMatrix(float mtx[16]);

  // Set the default size of the image buffers.
  void SetDefaultBufferSize(int width, int height);

  // Attach the SurfaceTexture to the texture currently bound to
  // GL_TEXTURE_EXTERNAL_OES.
  void AttachToGLContext();

  // Detaches the SurfaceTexture from the context that owns its current GL
  // texture. Must be called with that context current on the calling thread.
  void DetachFromGLContext();

  // Creates a native render surface for this surface texture.
  // The caller must release the underlying reference when done with the handle
  // by calling ANativeWindow_release().
  ANativeWindow* CreateSurface();

  const base::android::JavaRef<jobject>& j_surface_texture() const {
    return j_surface_texture_;
  }

  static bool RegisterSurfaceTexture(JNIEnv* env);

 private:
  friend class base::RefCountedThreadSafe<SurfaceTexture>;
  ~SurfaceTexture();

  // Java SurfaceTexture instance.
  base::android::ScopedJavaGlobalRef<jobject> j_surface_texture_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceTexture);
};

}  // namespace gfx

#endif  // UI_GL_ANDROID_SURFACE_TEXTURE_H_
