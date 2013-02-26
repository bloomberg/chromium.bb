// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEB_RENDERING_STATS_IMPL_H_
#define WEB_RENDERING_STATS_IMPL_H_

#include "cc/rendering_stats.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRenderingStats.h"

namespace WebKit {

struct WebRenderingStatsImpl : public WebRenderingStats {
  WebRenderingStatsImpl() {}

  cc::RenderingStats rendering_stats;
};

}  // namespace WebKit

#endif  // WEB_RENDERING_STATS_IMPL_H_
