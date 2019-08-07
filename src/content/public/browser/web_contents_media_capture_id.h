// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_MEDIA_CAPTURE_ID_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_MEDIA_CAPTURE_ID_H_

#include <string>

#include "content/common/content_export.h"
#include "ipc/ipc_message.h"

namespace content {

struct CONTENT_EXPORT WebContentsMediaCaptureId {
 public:
  WebContentsMediaCaptureId() = default;
  WebContentsMediaCaptureId(int render_process_id, int main_render_frame_id)
      : render_process_id(render_process_id),
        main_render_frame_id(main_render_frame_id) {}

  WebContentsMediaCaptureId(int render_process_id,
                            int main_render_frame_id,
                            bool enable_auto_throttling,
                            bool disable_local_echo)
      : render_process_id(render_process_id),
        main_render_frame_id(main_render_frame_id),
        enable_auto_throttling(enable_auto_throttling),
        disable_local_echo(disable_local_echo) {}

  bool operator<(const WebContentsMediaCaptureId& other) const;
  bool operator==(const WebContentsMediaCaptureId& other) const;

  // Return true if render_process_id or main_render_frame_id is invalid.
  bool is_null() const;

  std::string ToString() const;

  // Tab video and audio capture need render process id and render frame id.
  int render_process_id = MSG_ROUTING_NONE;
  int main_render_frame_id = MSG_ROUTING_NONE;

  bool enable_auto_throttling = false;
  bool disable_local_echo = false;

  // TODO(qiangchen): Pass structured ID along code paths, instead of doing
  // string conversion back and forth. See crbug/648666.
  // Create WebContentsMediaCaptureId based on a string.
  // Return false if the input string does not represent a
  // WebContentsMediaCaptureId.
  static bool Parse(const std::string& str,
                    WebContentsMediaCaptureId* output_id);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_MEDIA_CAPTURE_ID_H_
