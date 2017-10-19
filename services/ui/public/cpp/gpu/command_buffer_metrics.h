// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_GPU_COMMAND_BUFFER_METRICS_H_
#define SERVICES_UI_PUBLIC_CPP_GPU_COMMAND_BUFFER_METRICS_H_

#include <string>

#include "gpu/command_buffer/common/constants.h"

namespace ui {
namespace command_buffer_metrics {

enum ContextType {
  DISPLAY_COMPOSITOR_ONSCREEN_CONTEXT,
  BROWSER_OFFSCREEN_MAINTHREAD_CONTEXT,
  BROWSER_WORKER_CONTEXT,
  RENDER_COMPOSITOR_CONTEXT,
  RENDER_WORKER_CONTEXT,
  RENDERER_MAINTHREAD_CONTEXT,
  GPU_VIDEO_ACCELERATOR_CONTEXT,
  OFFSCREEN_VIDEO_CAPTURE_CONTEXT,
  OFFSCREEN_CONTEXT_FOR_WEBGL,
  CONTEXT_TYPE_UNKNOWN,
  MEDIA_CONTEXT,
  MUS_CLIENT_CONTEXT,
  UI_COMPOSITOR_CONTEXT,
  OFFSCREEN_CONTEXT_FOR_TESTING = CONTEXT_TYPE_UNKNOWN,
};

std::string ContextTypeToString(ContextType type);

void UmaRecordContextInitFailed(ContextType type);

void UmaRecordContextLost(ContextType type,
                          gpu::error::Error error,
                          gpu::error::ContextLostReason reason);

}  // namespace command_buffer_metrics
}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_GPU_COMMAND_BUFFER_METRICS_H_
