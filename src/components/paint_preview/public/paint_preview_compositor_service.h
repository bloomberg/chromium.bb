// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_PUBLIC_PAINT_PREVIEW_COMPOSITOR_SERVICE_H_
#define COMPONENTS_PAINT_PREVIEW_PUBLIC_PAINT_PREVIEW_COMPOSITOR_SERVICE_H_

#include <memory>

#include "base/callback_forward.h"
#include "components/paint_preview/public/paint_preview_compositor_client.h"

namespace paint_preview {

// An instance of a paint preview compositor utility process service. This
// class' lifetime is tied to that of the utility process to which it is
// connected. As long as the instance of this class is alive the utility process
// should be as well*. When this class is destructed then the utility process
// will also exit shortly.
//
// * If the utility process is killed by the OS or disconnected via Mojo then
//   this isn't necessarily the case; however, in the general it is true.
class PaintPreviewCompositorService {
 public:
  virtual ~PaintPreviewCompositorService() = default;

  // Creates a compositor instance tied to the service. |connected_closure| is
  // run once the compositor is started.
  virtual std::unique_ptr<PaintPreviewCompositorClient> CreateCompositor(
      base::OnceClosure connected_closure) = 0;

  // Returns whether there are any active clients. This can be used to
  // check if killing this service is safe (i.e. won't drop messages).
  virtual bool HasActiveClients() const = 0;

  PaintPreviewCompositorService(const PaintPreviewCompositorService&) = delete;
  PaintPreviewCompositorService& operator=(
      const PaintPreviewCompositorService&) = delete;

 protected:
  PaintPreviewCompositorService() = default;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_PUBLIC_PAINT_PREVIEW_COMPOSITOR_SERVICE_H_
