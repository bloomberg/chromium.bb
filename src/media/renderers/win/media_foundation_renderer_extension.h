// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERER_EXTENSION_H_
#define MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERER_EXTENSION_H_

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/media_export.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

// C++ interface equivalent to mojom::MediaFoundationRendererExtension.
// This interface allows MediaFoundationRenderer to support video rendering
// using Direct Compositon.
class MEDIA_EXPORT MediaFoundationRendererExtension {
 public:
  virtual ~MediaFoundationRendererExtension() = default;

  // TODO(frankli): naming: Change DComp into DirectComposition for interface
  // method names in a separate CL.

  // Enable Direct Composition video rendering.
  using SetDCompModeCB = base::OnceCallback<void(bool)>;
  virtual void SetDCompMode(bool enabled, SetDCompModeCB callback) = 0;

  // Get a Direct Composition Surface handle.
  using GetDCompSurfaceCB = base::OnceCallback<void(HANDLE)>;
  virtual void GetDCompSurface(GetDCompSurfaceCB callback) = 0;

  // Notify renderer whether video is enabled.
  virtual void SetVideoStreamEnabled(bool enabled) = 0;

  // Provide a unique identifier which maps to a specific playback element.
  virtual void SetPlaybackElementId(uint64_t playback_element_id) = 0;

  // Notify renderer of output composition parameters.
  virtual void SetOutputParams(const ::gfx::Rect& rect) = 0;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERER_EXTENSION_H_
