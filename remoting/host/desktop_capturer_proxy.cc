// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_capturer_proxy.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "remoting/proto/control.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

class DesktopCapturerProxy::Core : public webrtc::DesktopCapturer::Callback {
 public:
  Core(base::WeakPtr<DesktopCapturerProxy> proxy,
       scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
       scoped_ptr<webrtc::DesktopCapturer> capturer);
  ~Core() override;

  void Start();
  void SetSharedMemoryFactory(
      scoped_ptr<webrtc::SharedMemoryFactory> shared_memory_factory);
  void Capture(const webrtc::DesktopRegion& rect);

 private:
  // webrtc::DesktopCapturer::Callback implementation.
  void OnCaptureCompleted(webrtc::DesktopFrame* frame) override;

  base::ThreadChecker thread_checker_;

  base::WeakPtr<DesktopCapturerProxy> proxy_;
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  scoped_ptr<webrtc::DesktopCapturer> capturer_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

DesktopCapturerProxy::Core::Core(
    base::WeakPtr<DesktopCapturerProxy> proxy,
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    scoped_ptr<webrtc::DesktopCapturer> capturer)
    : proxy_(proxy),
      caller_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      capturer_(std::move(capturer)) {
  thread_checker_.DetachFromThread();
}

DesktopCapturerProxy::Core::~Core() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void DesktopCapturerProxy::Core::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  capturer_->Start(this);
}

void DesktopCapturerProxy::Core::SetSharedMemoryFactory(
    scoped_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {
  DCHECK(thread_checker_.CalledOnValidThread());
  capturer_->SetSharedMemoryFactory(
      rtc_make_scoped_ptr(shared_memory_factory.release()));
}

void DesktopCapturerProxy::Core::Capture(const webrtc::DesktopRegion& rect) {
  DCHECK(thread_checker_.CalledOnValidThread());
  capturer_->Capture(rect);
}

void DesktopCapturerProxy::Core::OnCaptureCompleted(
    webrtc::DesktopFrame* frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  caller_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DesktopCapturerProxy::OnFrameCaptured, proxy_,
                            base::Passed(make_scoped_ptr(frame))));
}

DesktopCapturerProxy::DesktopCapturerProxy(
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    scoped_ptr<webrtc::DesktopCapturer> capturer)
    : capture_task_runner_(capture_task_runner), weak_factory_(this) {
  core_.reset(new Core(weak_factory_.GetWeakPtr(), capture_task_runner,
                       std::move(capturer)));
}

DesktopCapturerProxy::~DesktopCapturerProxy() {
  capture_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void DesktopCapturerProxy::Start(Callback* callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  callback_ = callback;

  capture_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Core::Start, base::Unretained(core_.get())));
}

void DesktopCapturerProxy::SetSharedMemoryFactory(
    rtc::scoped_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {
  DCHECK(thread_checker_.CalledOnValidThread());

  capture_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &Core::SetSharedMemoryFactory, base::Unretained(core_.get()),
          base::Passed(make_scoped_ptr(shared_memory_factory.release()))));
}

void DesktopCapturerProxy::Capture(const webrtc::DesktopRegion& rect) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Start() must be called before Capture().
  DCHECK(callback_);

  capture_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Core::Capture, base::Unretained(core_.get()), rect));
}

void DesktopCapturerProxy::OnFrameCaptured(
    scoped_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  callback_->OnCaptureCompleted(frame.release());
}

}  // namespace remoting
