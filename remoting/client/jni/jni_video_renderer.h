// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_VIDEO_RENDERER_H_
#define REMOTING_CLIENT_JNI_JNI_VIDEO_RENDERER_H_

#include "base/memory/ref_counted.h"
#include "remoting/protocol/video_renderer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

namespace protocol {
class PerformanceTracker;
}  // namespace protocol

// An extension of VideoRenderer that allows renderer to be initialized after
// it is constructed.
class JniVideoRenderer : public protocol::VideoRenderer {
 public:
  ~JniVideoRenderer() override {}
  virtual void Initialize(
      scoped_refptr<base::SingleThreadTaskRunner> decode_task_runner,
      protocol::PerformanceTracker* perf_tracker) = 0;
 protected:
  JniVideoRenderer() {}
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_JNI_JNI_VIDEO_RENDERER_H_
