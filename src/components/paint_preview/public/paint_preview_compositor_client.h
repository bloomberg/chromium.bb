// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_PUBLIC_PAINT_PREVIEW_COMPOSITOR_CLIENT_H_
#define COMPONENTS_PAINT_PREVIEW_PUBLIC_PAINT_PREVIEW_COMPOSITOR_CLIENT_H_

#include "base/callback_forward.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "url/gurl.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace paint_preview {

// An instance of a paint preview compositor that is running in a utility
// process service. The class' lifetime is tied to that of the compositor
// running in the utility process (unless there is some kind of IPC disconnect
// that occurs).
class PaintPreviewCompositorClient {
 public:
  virtual ~PaintPreviewCompositorClient() = default;

  // Returns the token associated with the client. Will be null if the client
  // isn't started.
  virtual const base::Optional<base::UnguessableToken>& Token() const = 0;

  // Adds |closure| as a disconnect handler.
  virtual void SetDisconnectHandler(base::OnceClosure closure) = 0;

  // mojom::PaintPreviewCompositor API
  virtual void BeginComposite(
      mojom::PaintPreviewBeginCompositeRequestPtr request,
      mojom::PaintPreviewCompositor::BeginCompositeCallback callback) = 0;
  virtual void BitmapForFrame(
      const base::UnguessableToken& frame_guid,
      const gfx::Rect& clip_rect,
      float scale_factor,
      mojom::PaintPreviewCompositor::BitmapForFrameCallback callback) = 0;
  virtual void SetRootFrameUrl(const GURL& url) = 0;

  PaintPreviewCompositorClient(const PaintPreviewCompositorClient&) = delete;
  PaintPreviewCompositorClient& operator=(const PaintPreviewCompositorClient&) =
      delete;

 protected:
  PaintPreviewCompositorClient() = default;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_PUBLIC_PAINT_PREVIEW_COMPOSITOR_CLIENT_H_
