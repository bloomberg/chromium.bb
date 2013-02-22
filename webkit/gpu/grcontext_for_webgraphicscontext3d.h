// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GPU_GRCONTEXT_FOR_WEBGRAPHICSCONTEXT3D_H_
#define WEBKIT_GPU_GRCONTEXT_FOR_WEBGRAPHICSCONTEXT3D_H_

#include "skia/ext/refptr.h"
#include "webkit/gpu/webkit_gpu_export.h"

class GrContext;
namespace WebKit { class WebGraphicsContext3D; }

namespace webkit {
namespace gpu {

// This class binds an offscreen GrContext to an offscreen context3d. The
// context3d is used by the GrContext so must be valid as long as this class
// is alive.
class WEBKIT_GPU_EXPORT GrContextForWebGraphicsContext3D {
 public:
  explicit GrContextForWebGraphicsContext3D(
      WebKit::WebGraphicsContext3D* context3d);
  virtual ~GrContextForWebGraphicsContext3D();

  GrContext* get() { return gr_context_.get(); }

  void SetMemoryLimit(bool nonzero_allocation);

 private:
  skia::RefPtr<class GrContext> gr_context_;
};

}  // namespace gpu
}  // namespace webkit

#endif  // WEBKIT_GPU_GRCONTEXT_FOR_WEBGRAPHICSCONTEXT3D_H_
