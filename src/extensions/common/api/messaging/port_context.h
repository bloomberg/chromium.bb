// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_MESSAGING_PORT_CONTEXT_H_
#define EXTENSIONS_COMMON_API_MESSAGING_PORT_CONTEXT_H_

#include <stddef.h>

#include <string>

#include "base/optional.h"

namespace extensions {

// Specifies the renderer context that is tied to a message Port.
// A port can refer to a RenderFrame (FrameContext) or a Service Worker
// (WorkerContext).
struct PortContext {
 public:
  PortContext();
  ~PortContext();
  PortContext(const PortContext& other);

  struct FrameContext {
    FrameContext();
    explicit FrameContext(int routing_id);

    // The routing id of the frame context.
    // This may be MSG_ROUTING_NONE if the context is process-wide and isn't
    // tied to a specific RenderFrame.
    int routing_id;
  };

  struct WorkerContext {
    WorkerContext();
    WorkerContext(int thread_id,
                  int64_t version_id,
                  const std::string& extension_id);

    int thread_id;
    int64_t version_id;
    std::string extension_id;
  };

  static PortContext ForFrame(int routing_id);
  static PortContext ForWorker(int thread_id,
                               int64_t version_id,
                               const std::string& extension_id);

  bool is_for_render_frame() const { return frame.has_value(); }
  bool is_for_service_worker() const { return worker.has_value(); }

  base::Optional<FrameContext> frame;
  base::Optional<WorkerContext> worker;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_MESSAGING_PORT_CONTEXT_H_
