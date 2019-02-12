// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/drm_render_node_path_finder.h"

#include <fcntl.h>

#include "base/files/scoped_file.h"
#include "base/strings/stringprintf.h"

namespace ui {

namespace {

// Drm render node path template.
constexpr char kDriRenderNodeTemplate[] = "/dev/dri/renderD%u";

// Number of files to look for when discovering DRM devices.
constexpr uint32_t kDrmMaxMinor = 15;
constexpr uint32_t kRenderNodeStart = 128;
constexpr uint32_t kRenderNodeEnd = kRenderNodeStart + kDrmMaxMinor + 1;

}  // namespace

DrmRenderNodePathFinder::DrmRenderNodePathFinder() {
  FindDrmRenderNodePath();
}

DrmRenderNodePathFinder::~DrmRenderNodePathFinder() = default;

base::FilePath DrmRenderNodePathFinder::GetDrmRenderNodePath() const {
  return drm_render_node_path_;
}

void DrmRenderNodePathFinder::FindDrmRenderNodePath() {
  for (uint32_t i = kRenderNodeStart; i < kRenderNodeEnd; i++) {
    std::string dri_render_node(base::StringPrintf(kDriRenderNodeTemplate, i));
    base::ScopedFD drm_fd(open(dri_render_node.c_str(), O_RDWR));
    if (drm_fd.get() < 0)
      continue;

    drm_render_node_path_ = base::FilePath(dri_render_node);
    break;
  }
}

}  // namespace ui
