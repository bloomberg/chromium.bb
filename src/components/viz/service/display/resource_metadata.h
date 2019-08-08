// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_RESOURCE_METADATA_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_RESOURCE_METADATA_H_

#include "base/optional.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

// Metadata for a resource named by a ResourceId in DisplayResourceProvider.
// Used to construct a promise SkImage for a ResourceId.
struct VIZ_SERVICE_EXPORT ResourceMetadata {
  ResourceMetadata();
  ResourceMetadata(const ResourceMetadata& other);
  ~ResourceMetadata();
  ResourceMetadata& operator=(const ResourceMetadata& other);

  // Resource Id.
  ResourceId resource_id = kInvalidResourceId;

  // A mailbox holder for the resource texture.
  gpu::MailboxHolder mailbox_holder;

  // The resource size.
  gfx::Size size;

  // ResourceFormat from the resource texture.
  ResourceFormat resource_format = RGBA_8888;

  // The color space for the resource texture.
  gfx::ColorSpace color_space;

  // Whether resource is premultiplied.
  SkAlphaType alpha_type;

  // If the SkImage should use top-left or bottom-left for (0,0) uv
  GrSurfaceOrigin origin;

  // Sampler conversion information which is used in vulkan context for android
  // video.
  base::Optional<gpu::VulkanYCbCrInfo> ycbcr_info;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_RESOURCE_METADATA_H_
