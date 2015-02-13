// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOUSE_SHAPE_PUMP_H_
#define REMOTING_HOST_MOUSE_SHAPE_PUMP_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class MouseCursorMonitor;
}  // namespace webrtc

namespace remoting {

namespace protocol {
class CursorShapeStub;
class CursorShapeInfo;
}  // namespace protocol

// MouseShapePump is responsible for capturing mouse shape using
// MouseCursorMonitor and sending it to a CursorShapeStub.
class MouseShapePump {
 public:
  MouseShapePump(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      scoped_ptr<webrtc::MouseCursorMonitor> mouse_cursor_monitor,
      protocol::CursorShapeStub* cursor_shape_stub);
  ~MouseShapePump();

 private:
  class Core;

  void OnCursorShape(scoped_ptr<protocol::CursorShapeInfo> cursor);

  base::ThreadChecker thread_checker_;

  scoped_ptr<Core> core_;
  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  protocol::CursorShapeStub* cursor_shape_stub_;

  base::WeakPtrFactory<MouseShapePump> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MouseShapePump);
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOUSE_SHAPE_PUMP_H_
