// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_VIZ_UTILS_H_
#define COMPONENTS_VIZ_COMMON_VIZ_UTILS_H_

#include "base/timer/elapsed_timer.h"
#include "components/viz/common/viz_common_export.h"

#include "build/build_config.h"

namespace gfx {
class Rect;
class RRectF;
class QuadF;
}  // namespace gfx

namespace viz {

VIZ_COMMON_EXPORT bool PreferRGB565ResourcesForDisplay();

#if defined(OS_ANDROID)
VIZ_COMMON_EXPORT bool AlwaysUseWideColorGamut();
#endif

// This takes a gfx::Rect and a clip region quad in the same space,
// and returns a quad with the same proportions in the space -0.5->0.5.
VIZ_COMMON_EXPORT bool GetScaledRegion(const gfx::Rect& rect,
                                       const gfx::QuadF* clip,
                                       gfx::QuadF* scaled_region);
// This takes a rounded rect and a rect that it lives in, and returns an
// equivalent rounded rect in the space -0.5->0.5.
VIZ_COMMON_EXPORT bool GetScaledRRectF(const gfx::Rect& space,
                                       const gfx::RRectF& rect,
                                       gfx::RRectF* scaled_rect);
// This takes a gfx::Rect and a clip region quad in the same space,
// and returns the proportional uv's in the space 0->1.
VIZ_COMMON_EXPORT bool GetScaledUVs(const gfx::Rect& rect,
                                    const gfx::QuadF* clip,
                                    float uvs[8]);

// Returns File Descriptor (FD) stats for current process.
// Rendering resources can consume FDs. This this function can be used to
// determine if the process is low on FDs or find an FD leak.
VIZ_COMMON_EXPORT bool GatherFDStats(base::TimeDelta* delta_time_taken,
                                     int* fd_max,
                                     int* active_fd_count,
                                     int* rlim_cur);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_VIZ_UTILS_H_
