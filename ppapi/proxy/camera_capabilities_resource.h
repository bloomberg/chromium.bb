// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_CAMERA_CAPABILITIES_RESOURCE_H_
#define PPAPI_PROXY_CAMERA_CAPABILITIES_RESOURCE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_camera_capabilities_api.h"

namespace ppapi {
namespace proxy {

class ImageCaptureResource;

class PPAPI_PROXY_EXPORT CameraCapabilitiesResource
    : public Resource,
      public thunk::PPB_CameraCapabilities_API {
 public:
  CameraCapabilitiesResource(PP_Instance instance,
                             const std::vector<PP_Size>& preview_sizes);

  ~CameraCapabilitiesResource() override;

  // Resource overrides.
  thunk::PPB_CameraCapabilities_API* AsPPB_CameraCapabilities_API() override;

  // PPB_CameraCapabilities_API implementation.
  void GetSupportedPreviewSizes(int32_t* array_size,
                                PP_Size** preview_sizes) override;

 private:
  int32_t num_preview_sizes_;
  scoped_ptr<PP_Size[]> preview_sizes_;

  DISALLOW_COPY_AND_ASSIGN(CameraCapabilitiesResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_CAMERA_CAPABILITIES_RESOURCE_H_
