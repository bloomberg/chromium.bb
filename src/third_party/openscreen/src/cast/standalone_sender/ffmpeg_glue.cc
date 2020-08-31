// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/ffmpeg_glue.h"

#include "util/osp_logging.h"

namespace openscreen {
namespace cast {
namespace internal {

AVFormatContext* CreateAVFormatContextForFile(const char* path) {
  AVFormatContext* format_context = nullptr;
  int result = avformat_open_input(&format_context, path, nullptr, nullptr);
  if (result < 0) {
    OSP_LOG_ERROR << "Cannot open " << path << ": " << av_err2str(result);
    return nullptr;
  }
  result = avformat_find_stream_info(format_context, nullptr);
  if (result < 0) {
    avformat_close_input(&format_context);
    OSP_LOG_ERROR << "Cannot find stream info in " << path << ": "
                  << av_err2str(result);
    return nullptr;
  }
  return format_context;
}

}  // namespace internal
}  // namespace cast
}  // namespace openscreen
