// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_gpu_platform_support_host.h"

#include <stddef.h>

#include "base/trace_event/trace_event.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/platform/drm/host/gpu_thread_observer.h"

namespace ui {

namespace {

// Helper class that provides DrmCursor with a mechanism to send messages
// to the GPU process.
class CursorIPC : public DrmCursorProxy {
 public:
  CursorIPC(scoped_refptr<base::SingleThreadTaskRunner> send_runner,
            const base::Callback<void(IPC::Message*)>& send_callback);
  ~CursorIPC() override;

  // DrmCursorProxy implementation.
  void CursorSet(gfx::AcceleratedWidget window,
                 const std::vector<SkBitmap>& bitmaps,
                 gfx::Point point,
                 int frame_delay_ms) override;
  void Move(gfx::AcceleratedWidget window, gfx::Point point) override;

 private:
  bool IsConnected();
  void Send(IPC::Message* message);

  scoped_refptr<base::SingleThreadTaskRunner> send_runner_;
  base::Callback<void(IPC::Message*)> send_callback_;

  DISALLOW_COPY_AND_ASSIGN(CursorIPC);
};

CursorIPC::CursorIPC(scoped_refptr<base::SingleThreadTaskRunner> send_runner,
                     const base::Callback<void(IPC::Message*)>& send_callback)
    : send_runner_(send_runner), send_callback_(send_callback) {}

CursorIPC::~CursorIPC() {}

bool CursorIPC::IsConnected() {
  return !send_callback_.is_null();
}

void CursorIPC::CursorSet(gfx::AcceleratedWidget window,
                          const std::vector<SkBitmap>& bitmaps,
                          gfx::Point point,
                          int frame_delay_ms) {
  Send(new OzoneGpuMsg_CursorSet(window, bitmaps, point, frame_delay_ms));
}

void CursorIPC::Move(gfx::AcceleratedWidget window, gfx::Point point) {
  Send(new OzoneGpuMsg_CursorMove(window, point));
}

void CursorIPC::Send(IPC::Message* message) {
  if (IsConnected() &&
      send_runner_->PostTask(FROM_HERE, base::Bind(send_callback_, message)))
    return;

  // Drop disconnected updates. DrmWindowHost will call
  // CommitBoundsChange() when we connect to initialize the cursor
  // location.
  delete message;
}

}  // namespace

DrmGpuPlatformSupportHost::DrmGpuPlatformSupportHost(DrmCursor* cursor)
    : cursor_(cursor) {
}

DrmGpuPlatformSupportHost::~DrmGpuPlatformSupportHost() {
}

void DrmGpuPlatformSupportHost::RegisterHandler(
    GpuPlatformSupportHost* handler) {
  handlers_.push_back(handler);

  if (IsConnected())
    handler->OnChannelEstablished(host_id_, send_runner_, send_callback_);
}

void DrmGpuPlatformSupportHost::UnregisterHandler(
    GpuPlatformSupportHost* handler) {
  std::vector<GpuPlatformSupportHost*>::iterator it =
      std::find(handlers_.begin(), handlers_.end(), handler);
  if (it != handlers_.end())
    handlers_.erase(it);
}

void DrmGpuPlatformSupportHost::AddGpuThreadObserver(
    GpuThreadObserver* observer) {
  gpu_thread_observers_.AddObserver(observer);

  if (IsConnected())
    observer->OnGpuThreadReady();
}

void DrmGpuPlatformSupportHost::RemoveGpuThreadObserver(
    GpuThreadObserver* observer) {
  gpu_thread_observers_.RemoveObserver(observer);
}

bool DrmGpuPlatformSupportHost::IsConnected() {
  return host_id_ >= 0;
}

void DrmGpuPlatformSupportHost::OnChannelEstablished(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    const base::Callback<void(IPC::Message*)>& send_callback) {
  TRACE_EVENT1("drm", "DrmGpuPlatformSupportHost::OnChannelEstablished",
               "host_id", host_id);
  host_id_ = host_id;
  send_runner_ = send_runner;
  send_callback_ = send_callback;

  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->OnChannelEstablished(host_id, send_runner_, send_callback_);

  FOR_EACH_OBSERVER(GpuThreadObserver, gpu_thread_observers_,
                    OnGpuThreadReady());

  // The cursor is special since it will process input events on the IO thread
  // and can by-pass the UI thread. This means that we need to special case it
  // and notify it after all other observers/handlers are notified such that the
  // (windowing) state on the GPU can be initialized before the cursor is
  // allowed to IPC messages (which are targeted to a specific window).
  cursor_->SetDrmCursorProxy(new CursorIPC(send_runner_, send_callback_));
}

void DrmGpuPlatformSupportHost::OnChannelDestroyed(int host_id) {
  TRACE_EVENT1("drm", "DrmGpuPlatformSupportHost::OnChannelDestroyed",
               "host_id", host_id);

  if (host_id_ == host_id) {
    cursor_->ResetDrmCursorProxy();
    host_id_ = -1;
    send_runner_ = nullptr;
    send_callback_.Reset();
    FOR_EACH_OBSERVER(GpuThreadObserver, gpu_thread_observers_,
                      OnGpuThreadRetired());
  }

  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->OnChannelDestroyed(host_id);
}

bool DrmGpuPlatformSupportHost::OnMessageReceived(const IPC::Message& message) {
  for (size_t i = 0; i < handlers_.size(); ++i)
    if (handlers_[i]->OnMessageReceived(message))
      return true;

  return false;
}

bool DrmGpuPlatformSupportHost::Send(IPC::Message* message) {
  if (IsConnected() &&
      send_runner_->PostTask(FROM_HERE, base::Bind(send_callback_, message)))
    return true;

  delete message;
  return false;
}

}  // namespace ui
