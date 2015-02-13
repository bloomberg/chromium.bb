// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mouse_shape_pump.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/cursor_shape_stub.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting {

// Poll mouse shape 10 times a second.
static const int kCursorCaptureIntervalMs = 100;

class MouseShapePump::Core : public webrtc::MouseCursorMonitor::Callback {
 public:
  Core(base::WeakPtr<MouseShapePump> proxy,
       scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
       scoped_ptr<webrtc::MouseCursorMonitor> mouse_cursor_monitor);
  ~Core() override;

  void Start();
  void Capture();

 private:
  // webrtc::MouseCursorMonitor::Callback implementation.
  void OnMouseCursor(webrtc::MouseCursor* mouse_cursor) override;
  void OnMouseCursorPosition(webrtc::MouseCursorMonitor::CursorState state,
                             const webrtc::DesktopVector& position) override;

  base::ThreadChecker thread_checker_;

  base::WeakPtr<MouseShapePump> proxy_;
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  scoped_ptr<webrtc::MouseCursorMonitor> mouse_cursor_monitor_;

  base::Timer capture_timer_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

MouseShapePump::Core::Core(
    base::WeakPtr<MouseShapePump> proxy,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_ptr<webrtc::MouseCursorMonitor> mouse_cursor_monitor)
    : proxy_(proxy),
      caller_task_runner_(caller_task_runner),
      mouse_cursor_monitor_(mouse_cursor_monitor.Pass()),
      capture_timer_(true, true) {
  thread_checker_.DetachFromThread();
}

MouseShapePump::Core::~Core() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MouseShapePump::Core::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());

  mouse_cursor_monitor_->Init(this, webrtc::MouseCursorMonitor::SHAPE_ONLY);

  capture_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kCursorCaptureIntervalMs),
      base::Bind(&MouseShapePump::Core::Capture, base::Unretained(this)));
}

void MouseShapePump::Core::Capture() {
  DCHECK(thread_checker_.CalledOnValidThread());

  mouse_cursor_monitor_->Capture();
}

void MouseShapePump::Core::OnMouseCursor(webrtc::MouseCursor* cursor) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<webrtc::MouseCursor> owned_cursor(cursor);

  scoped_ptr<protocol::CursorShapeInfo> cursor_proto(
      new protocol::CursorShapeInfo());
  cursor_proto->set_width(cursor->image()->size().width());
  cursor_proto->set_height(cursor->image()->size().height());
  cursor_proto->set_hotspot_x(cursor->hotspot().x());
  cursor_proto->set_hotspot_y(cursor->hotspot().y());

  cursor_proto->set_data(std::string());
  uint8_t* current_row = cursor->image()->data();
  for (int y = 0; y < cursor->image()->size().height(); ++y) {
    cursor_proto->mutable_data()->append(
        current_row,
        current_row + cursor->image()->size().width() *
            webrtc::DesktopFrame::kBytesPerPixel);
    current_row += cursor->image()->stride();
  }

  caller_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MouseShapePump::OnCursorShape, proxy_,
                            base::Passed(&cursor_proto)));
}

void MouseShapePump::Core::OnMouseCursorPosition(
    webrtc::MouseCursorMonitor::CursorState state,
    const webrtc::DesktopVector& position) {
  // We're not subscribing to mouse position changes.
  NOTREACHED();
}

MouseShapePump::MouseShapePump(
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    scoped_ptr<webrtc::MouseCursorMonitor> mouse_cursor_monitor,
    protocol::CursorShapeStub* cursor_shape_stub)
    : capture_task_runner_(capture_task_runner),
      cursor_shape_stub_(cursor_shape_stub),
      weak_factory_(this) {
  core_.reset(new Core(weak_factory_.GetWeakPtr(),
                       base::ThreadTaskRunnerHandle::Get(),
                       mouse_cursor_monitor.Pass()));
  capture_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Core::Start, base::Unretained(core_.get())));
}

MouseShapePump::~MouseShapePump() {
  capture_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void MouseShapePump::OnCursorShape(
    scoped_ptr<protocol::CursorShapeInfo> cursor) {
  DCHECK(thread_checker_.CalledOnValidThread());

  cursor_shape_stub_->SetCursorShape(*cursor);
}

}  // namespace remoting
