// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_ANDROID_ANDROID_IMAGE_READER_UTILS_H_
#define GPU_IPC_COMMON_ANDROID_ANDROID_IMAGE_READER_UTILS_H_

#include <android/hardware_buffer.h>

#include "base/android/android_image_reader_compat.h"
#include "base/files/scoped_file.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

// Delete the AImage asynchronously by inserting an android native fence sync.
bool DeleteAImageAsync(AImage* image,
                       base::android::AndroidImageReader* loader);

// Create and insert an EGL fence using the provided fence fd.
bool InsertEglFenceAndWait(base::ScopedFD acquire_fence_fd);

// Create an EGL image from the AImage via AHardwarebuffer. Bind this EGL image
// to the texture target target_id. This changes the texture binding on the
// current context.
bool CreateAndBindEglImage(const AImage* image,
                           GLuint texture_id,
                           base::android::AndroidImageReader* loader);

}  // namespace gpu

#endif  // GPU_IPC_COMMON_ANDROID_ANDROID_IMAGE_READER_UTILS_H_
