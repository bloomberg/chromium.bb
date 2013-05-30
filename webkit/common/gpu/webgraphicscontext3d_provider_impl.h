// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_GPU_WEBGRAPHICSCONTEXT3D_PROVIDER_IMPL_H_
#define WEBKIT_COMMON_GPU_WEBGRAPHICSCONTEXT3D_PROVIDER_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3DProvider.h"
#include "webkit/common/gpu/webkit_gpu_export.h"

namespace cc { class ContextProvider; }

namespace webkit {
namespace gpu {

class WEBKIT_GPU_EXPORT WebGraphicsContext3DProviderImpl
    : public NON_EXPORTED_BASE(WebKit::WebGraphicsContext3DProvider) {
 public:
  explicit WebGraphicsContext3DProviderImpl(
      scoped_refptr<cc::ContextProvider> provider);
  virtual ~WebGraphicsContext3DProviderImpl();

  // WebGraphicsContext3DProvider implementation.
  virtual WebKit::WebGraphicsContext3D* context3d() OVERRIDE;
  virtual GrContext* grContext() OVERRIDE;

 private:
  scoped_refptr<cc::ContextProvider> provider_;
};

}  // namespace gpu
}  // namespace webkit

#endif  // WEBKIT_COMMON_GPU_WEBGRAPHICSCONTEXT3D_PROVIDER_IMPL_H_
