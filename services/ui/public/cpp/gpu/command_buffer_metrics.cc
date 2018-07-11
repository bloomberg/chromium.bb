// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/gpu/command_buffer_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace ui {
namespace command_buffer_metrics {

namespace {

enum CommandBufferContextLostReason {
  // Don't add new values here.
  CONTEXT_INIT_FAILED,
  CONTEXT_LOST_GPU_CHANNEL_ERROR,
  CONTEXT_PARSE_ERROR_INVALID_SIZE,
  CONTEXT_PARSE_ERROR_OUT_OF_BOUNDS,
  CONTEXT_PARSE_ERROR_UNKNOWN_COMMAND,
  CONTEXT_PARSE_ERROR_INVALID_ARGS,
  CONTEXT_PARSE_ERROR_GENERIC_ERROR,
  CONTEXT_LOST_GUILTY,
  CONTEXT_LOST_INNOCENT,
  CONTEXT_LOST_UNKNOWN,
  CONTEXT_LOST_OUT_OF_MEMORY,
  CONTEXT_LOST_MAKECURRENT_FAILED,
  CONTEXT_LOST_INVALID_GPU_MESSAGE,
  // Add new values here and update kMaxValue.
  // Also update //tools/metrics/histograms/histograms.xml
  kMaxValue = CONTEXT_LOST_INVALID_GPU_MESSAGE
};

CommandBufferContextLostReason GetContextLostReason(
    gpu::error::Error error,
    gpu::error::ContextLostReason reason) {
  if (error == gpu::error::kLostContext) {
    switch (reason) {
      case gpu::error::kGuilty:
        return CONTEXT_LOST_GUILTY;
      case gpu::error::kInnocent:
        return CONTEXT_LOST_INNOCENT;
      case gpu::error::kUnknown:
        return CONTEXT_LOST_UNKNOWN;
      case gpu::error::kOutOfMemory:
        return CONTEXT_LOST_OUT_OF_MEMORY;
      case gpu::error::kMakeCurrentFailed:
        return CONTEXT_LOST_MAKECURRENT_FAILED;
      case gpu::error::kGpuChannelLost:
        return CONTEXT_LOST_GPU_CHANNEL_ERROR;
      case gpu::error::kInvalidGpuMessage:
        return CONTEXT_LOST_INVALID_GPU_MESSAGE;
    }
  }
  switch (error) {
    case gpu::error::kInvalidSize:
      return CONTEXT_PARSE_ERROR_INVALID_SIZE;
    case gpu::error::kOutOfBounds:
      return CONTEXT_PARSE_ERROR_OUT_OF_BOUNDS;
    case gpu::error::kUnknownCommand:
      return CONTEXT_PARSE_ERROR_UNKNOWN_COMMAND;
    case gpu::error::kInvalidArguments:
      return CONTEXT_PARSE_ERROR_INVALID_ARGS;
    case gpu::error::kGenericError:
      return CONTEXT_PARSE_ERROR_GENERIC_ERROR;
    case gpu::error::kDeferCommandUntilLater:
    case gpu::error::kDeferLaterCommands:
    case gpu::error::kNoError:
    case gpu::error::kLostContext:
      NOTREACHED();
      return CONTEXT_LOST_UNKNOWN;
  }
  NOTREACHED();
  return CONTEXT_LOST_UNKNOWN;
}

void RecordContextLost(ContextType type,
                       CommandBufferContextLostReason reason) {
  switch (type) {
    case ContextType::BROWSER_COMPOSITOR:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.BrowserCompositor", reason);
      break;
    case ContextType::BROWSER_MAIN_THREAD:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.BrowserMainThread", reason);
      break;
    case ContextType::BROWSER_WORKER:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.BrowserWorker", reason);
      break;
    case ContextType::RENDER_COMPOSITOR:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.RenderCompositor", reason);
      break;
    case ContextType::RENDER_WORKER:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.RenderWorker", reason);
      break;
    case ContextType::RENDERER_MAIN_THREAD:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.RenderMainThread", reason);
      break;
    case ContextType::VIDEO_ACCELERATOR:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.VideoAccelerator", reason);
      break;
    case ContextType::VIDEO_CAPTURE:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.VideoCapture", reason);
      break;
    case ContextType::WEBGL:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.WebGL", reason);
      break;
    case ContextType::MEDIA:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.Media", reason);
      break;
    case ContextType::MUS_CLIENT:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.MusClient", reason);
      break;
    case ContextType::UNKNOWN:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.Unknown", reason);
      break;
    case ContextType::FOR_TESTING:
      // Don't record UMA, this is just for tests.
      break;
  }
}

}  // anonymous namespace

std::string ContextTypeToString(ContextType type) {
  switch (type) {
    case ContextType::BROWSER_COMPOSITOR:
      return "BrowserCompositor";
    case ContextType::BROWSER_MAIN_THREAD:
      return "BrowserMainThread";
    case ContextType::BROWSER_WORKER:
      return "BrowserWorker";
    case ContextType::RENDER_COMPOSITOR:
      return "RenderCompositor";
    case ContextType::RENDER_WORKER:
      return "RenderWorker";
    case ContextType::RENDERER_MAIN_THREAD:
      return "RendererMainThread";
    case ContextType::VIDEO_ACCELERATOR:
      return "VideoAccelerator";
    case ContextType::VIDEO_CAPTURE:
      return "VideoCapture";
    case ContextType::WEBGL:
      return "WebGL";
    case ContextType::MEDIA:
      return "Media";
    case ContextType::MUS_CLIENT:
      return "MusClient";
    case ContextType::UNKNOWN:
      return "Unknown";
    case ContextType::FOR_TESTING:
      return "ForTesting";
  }
}

void UmaRecordContextInitFailed(ContextType type) {
  RecordContextLost(type, CONTEXT_INIT_FAILED);
}

void UmaRecordContextLost(ContextType type,
                          gpu::error::Error error,
                          gpu::error::ContextLostReason reason) {
  CommandBufferContextLostReason converted_reason =
      GetContextLostReason(error, reason);
  RecordContextLost(type, converted_reason);
}

}  // namespace command_buffer_metrics
}  // namespace ui
