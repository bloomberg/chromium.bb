// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_cursor.h"

#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/drm/host/drm_window_host_manager.h"

#if defined(OS_CHROMEOS)
#include "ui/events/ozone/chromeos/cursor_controller.h"
#endif

namespace ui {

DrmCursor::DrmCursor(DrmWindowHostManager* window_manager)
    : ipc_(&lock_), core_(new DrmCursorCore(&ipc_, window_manager)) {}

DrmCursor::~DrmCursor() {
}

void DrmCursor::SetCursor(gfx::AcceleratedWidget window,
                          PlatformCursor platform_cursor) {
  TRACE_EVENT0("drmcursor", "DrmCursor::SetCursor");
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(window, gfx::kNullAcceleratedWidget);

  scoped_refptr<BitmapCursorOzone> bitmap =
      BitmapCursorFactoryOzone::GetBitmapCursor(platform_cursor);

  base::AutoLock lock(lock_);
  core_->SetCursor(window, bitmap);
}

void DrmCursor::OnWindowAdded(gfx::AcceleratedWidget window,
                              const gfx::Rect& bounds_in_screen,
                              const gfx::Rect& cursor_confined_bounds) {
  TRACE_EVENT0("drmcursor", "DrmCursor::OnWindowAdded");
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);

  core_->OnWindowAdded(window, bounds_in_screen, cursor_confined_bounds);
}

void DrmCursor::OnWindowRemoved(gfx::AcceleratedWidget window) {
  TRACE_EVENT0("drmcursor", "DrmCursor::OnWindowRemoved");
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  core_->OnWindowRemoved(window);
}

void DrmCursor::CommitBoundsChange(
    gfx::AcceleratedWidget window,
    const gfx::Rect& new_display_bounds_in_screen,
    const gfx::Rect& new_confined_bounds) {
  TRACE_EVENT0("drmcursor", "DrmCursor::CommitBoundsChange");
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  core_->CommitBoundsChange(window, new_display_bounds_in_screen,
                            new_confined_bounds);
}

void DrmCursor::MoveCursorTo(gfx::AcceleratedWidget window,
                             const gfx::PointF& location) {
  TRACE_EVENT0("drmcursor", "DrmCursor::MoveCursorTo (window)");
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  core_->MoveCursorTo(window, location);
}

void DrmCursor::MoveCursorTo(const gfx::PointF& screen_location) {
  TRACE_EVENT0("drmcursor", "DrmCursor::MoveCursorTo");
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  core_->MoveCursorTo(screen_location);
}

void DrmCursor::MoveCursor(const gfx::Vector2dF& delta) {
  TRACE_EVENT0("drmcursor", "DrmCursor::MoveCursor");
  base::AutoLock lock(lock_);
  core_->MoveCursor(delta);
}

bool DrmCursor::IsCursorVisible() {
  base::AutoLock lock(lock_);
  return core_->IsCursorVisible();
}

gfx::PointF DrmCursor::GetLocation() {
  base::AutoLock lock(lock_);
  return core_->GetLocation();
}

gfx::Rect DrmCursor::GetCursorConfinedBounds() {
  base::AutoLock lock(lock_);
  return core_->GetCursorConfinedBounds();
}

void DrmCursor::OnChannelEstablished(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    const base::Callback<void(IPC::Message*)>& send_callback) {
  TRACE_EVENT0("drmcursor", "DrmCursor::OnChannelEstablished");
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  ipc_.host_id = host_id;
  ipc_.send_runner = send_runner;
  ipc_.send_callback = send_callback;
  // Initial set for this GPU process will happen after the window
  // initializes, in CommitBoundsChange().
}

void DrmCursor::OnChannelDestroyed(int host_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  if (ipc_.host_id == host_id) {
    ipc_.host_id = -1;
    ipc_.send_runner = NULL;
    ipc_.send_callback.Reset();
  }
}

bool DrmCursor::OnMessageReceived(const IPC::Message& message) {
  return false;
}

bool DrmCursor::CursorIPC::IsConnectedLocked() {
  return !send_callback.is_null();
}

void DrmCursor::CursorIPC::CursorSet(gfx::AcceleratedWidget window,
                                     const std::vector<SkBitmap>& bitmaps,
                                     gfx::Point point,
                                     int frame_delay_ms) {
  SendLocked(new OzoneGpuMsg_CursorSet(window, bitmaps, point, frame_delay_ms));
}

void DrmCursor::CursorIPC::Move(gfx::AcceleratedWidget window,
                                gfx::Point point) {
  SendLocked(new OzoneGpuMsg_CursorMove(window, point));
}

void DrmCursor::CursorIPC::SendLocked(IPC::Message* message) {
  lock->AssertAcquired();

  if (IsConnectedLocked() &&
      send_runner->PostTask(FROM_HERE, base::Bind(send_callback, message)))
    return;

  // Drop disconnected updates. DrmWindowHost will call
  // CommitBoundsChange() when we connect to initialize the cursor
  // location.
  delete message;
}

DrmCursor::CursorIPC::CursorIPC(base::Lock* lock) : host_id(-1), lock(lock) {}

DrmCursor::CursorIPC::~CursorIPC() {}

}  // namespace ui
