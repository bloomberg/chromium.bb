// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_FRAME_MEDIA_PLAYBACK_OPTIONS_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_FRAME_MEDIA_PLAYBACK_OPTIONS_H_

#include "build/build_config.h"

namespace content {

// Default value for is_background_suspend_enabled is determined statically in
// Chromium, but some content embedders (e.g. Cast) may need to change it at
// runtime.
#if defined(OS_ANDROID)
const bool kIsBackgroundMediaSuspendEnabled = true;
#else
const bool kIsBackgroundMediaSuspendEnabled = false;
#endif

struct RenderFrameMediaPlaybackOptions {
  // Whether the renderer should automatically suspend media playback on
  // background tabs for given |render_frame|.
  bool is_background_suspend_enabled = kIsBackgroundMediaSuspendEnabled;

  // Whether background video is allowed to play for given |render_frame|.
  bool is_background_video_playback_enabled = true;

  // Whether background video optimization is supported on current platform.
  bool is_background_video_track_optimization_supported = true;

  // Whether MojoRenderer should be used for given |render_frame|.
  bool is_mojo_renderer_enabled = true;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_FRAME_MEDIA_PLAYBACK_OPTIONS_H_
