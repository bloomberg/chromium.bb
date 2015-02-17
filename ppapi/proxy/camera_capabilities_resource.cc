// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "ppapi/proxy/camera_capabilities_resource.h"

namespace ppapi {
namespace proxy {

CameraCapabilitiesResource::CameraCapabilitiesResource(
    PP_Instance instance,
    const std::vector<PP_Size>& preview_sizes)
    : Resource(OBJECT_IS_PROXY, instance),
      num_preview_sizes_(static_cast<int32_t>(preview_sizes.size())),
      preview_sizes_(new PP_Size[num_preview_sizes_]) {
  std::copy(preview_sizes.begin(), preview_sizes.end(), preview_sizes_.get());
}

CameraCapabilitiesResource::~CameraCapabilitiesResource() {
}

thunk::PPB_CameraCapabilities_API*
CameraCapabilitiesResource::AsPPB_CameraCapabilities_API() {
  return this;
}

void CameraCapabilitiesResource::GetSupportedPreviewSizes(
    int32_t* array_size,
    PP_Size** preview_sizes) {
  *array_size = num_preview_sizes_;
  *preview_sizes = preview_sizes_.get();
}

}  // namespace proxy
}  // namespace ppapi
