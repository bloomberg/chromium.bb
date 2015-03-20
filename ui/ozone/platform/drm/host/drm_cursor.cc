// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_cursor.h"

#include "base/thread_task_runner_handle.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/drm/host/drm_window_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host_manager.h"

#if defined(OS_CHROMEOS)
#include "ui/events/ozone/chromeos/cursor_controller.h"
#endif

namespace ui {

DrmCursor::DrmCursor(DrmWindowHostManager* window_manager)
    : window_manager_(window_manager) {
}

DrmCursor::~DrmCursor() {
}

void DrmCursor::SetCursor(gfx::AcceleratedWidget window,
                          PlatformCursor platform_cursor) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(window, gfx::kNullAcceleratedWidget);

  scoped_refptr<BitmapCursorOzone> bitmap =
      BitmapCursorFactoryOzone::GetBitmapCursor(platform_cursor);

  base::AutoLock lock(state_.lock);
  if (state_.window != window || state_.bitmap == bitmap)
    return;

  state_.bitmap = bitmap;

  SendCursorShowLocked();
}

void DrmCursor::OnWindowAdded(gfx::AcceleratedWidget window,
                              const gfx::Rect& bounds_in_screen,
                              const gfx::Rect& cursor_confined_bounds) {
#if DCHECK_IS_ON()
  if (!ui_task_runner_)
    ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
#endif
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(state_.lock);

  if (state_.window == gfx::kNullAcceleratedWidget) {
    // First window added & cursor is not placed. Place it.
    state_.window = window;
    state_.display_bounds_in_screen = bounds_in_screen;
    state_.confined_bounds = cursor_confined_bounds;
    SetCursorLocationLocked(cursor_confined_bounds.CenterPoint());
  }
}

void DrmCursor::OnWindowRemoved(gfx::AcceleratedWidget window) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(state_.lock);

  if (state_.window == window) {
    // Try to find a new location for the cursor.
    DrmWindowHost* dest_window = window_manager_->GetPrimaryWindow();

    if (dest_window) {
      state_.window = dest_window->GetAcceleratedWidget();
      state_.display_bounds_in_screen = dest_window->GetBounds();
      state_.confined_bounds = dest_window->GetCursorConfinedBounds();
      SetCursorLocationLocked(state_.confined_bounds.CenterPoint());
      SendCursorShowLocked();
    } else {
      state_.window = gfx::kNullAcceleratedWidget;
      state_.display_bounds_in_screen = gfx::Rect();
      state_.confined_bounds = gfx::Rect();
      state_.location = gfx::Point();
    }
  }
}

void DrmCursor::PrepareForBoundsChange(gfx::AcceleratedWidget window) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(state_.lock);

  // Bounds changes can reparent the window to a different display, so
  // we hide prior to the change so that we avoid leaving a cursor
  // behind on the old display.
  // TODO(spang): The GPU-side code should handle this.
  if (state_.window == window)
    SendCursorHideLocked();

  // The cursor will be shown and moved in CommitBoundsChange().
}

void DrmCursor::CommitBoundsChange(
    gfx::AcceleratedWidget window,
    const gfx::Rect& new_display_bounds_in_screen,
    const gfx::Rect& new_confined_bounds) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(state_.lock);
  if (state_.window == window) {
    state_.display_bounds_in_screen = new_display_bounds_in_screen;
    state_.confined_bounds = new_confined_bounds;
    SetCursorLocationLocked(state_.location);
    SendCursorShowLocked();
  }
}

void DrmCursor::ConfineCursorToBounds(gfx::AcceleratedWidget window,
                                      const gfx::Rect& bounds) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(state_.lock);
  if (state_.window == window) {
    state_.confined_bounds = bounds;
    SetCursorLocationLocked(state_.location);
    SendCursorShowLocked();
  }
}

void DrmCursor::MoveCursorTo(gfx::AcceleratedWidget window,
                             const gfx::PointF& location) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(state_.lock);
  gfx::AcceleratedWidget old_window = state_.window;

  if (window != old_window) {
    // When moving between displays, hide the cursor on the old display
    // prior to showing it on the new display.
    if (old_window != gfx::kNullAcceleratedWidget)
      SendCursorHideLocked();

    DrmWindowHost* drm_window_host = window_manager_->GetWindow(window);
    state_.display_bounds_in_screen = drm_window_host->GetBounds();
    state_.confined_bounds = drm_window_host->GetCursorConfinedBounds();
    state_.window = window;
  }

  SetCursorLocationLocked(location);

  if (window != old_window)
    SendCursorShowLocked();
  else
    SendCursorMoveLocked();
}

void DrmCursor::MoveCursorTo(const gfx::PointF& screen_location) {
  base::AutoLock lock(state_.lock);

  // TODO(spang): Moving between windows doesn't work here, but
  // is not needed for current uses.

  SetCursorLocationLocked(screen_location -
                          state_.display_bounds_in_screen.OffsetFromOrigin());

  SendCursorMoveLocked();
}

void DrmCursor::MoveCursor(const gfx::Vector2dF& delta) {
  base::AutoLock lock(state_.lock);
  if (state_.window == gfx::kNullAcceleratedWidget)
    return;

  gfx::Point location;
#if defined(OS_CHROMEOS)
  gfx::Vector2dF transformed_delta = delta;
  ui::CursorController::GetInstance()->ApplyCursorConfigForWindow(
      state_.window, &transformed_delta);
  SetCursorLocationLocked(state_.location + transformed_delta);
#else
  SetCursorLocationLocked(state_.location + delta);
#endif

  SendCursorMoveLocked();
}

bool DrmCursor::IsCursorVisible() {
  base::AutoLock lock(state_.lock);
  return state_.bitmap;
}

gfx::PointF DrmCursor::GetLocation() {
  base::AutoLock lock(state_.lock);
  return state_.location + state_.display_bounds_in_screen.OffsetFromOrigin();
}

gfx::Rect DrmCursor::GetCursorConfinedBounds() {
  base::AutoLock lock(state_.lock);
  return state_.confined_bounds +
         state_.display_bounds_in_screen.OffsetFromOrigin();
}

void DrmCursor::OnChannelEstablished(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    const base::Callback<void(IPC::Message*)>& send_callback) {
#if DCHECK_IS_ON()
  if (!ui_task_runner_)
    ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
#endif
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(state_.lock);
  state_.host_id = host_id;
  state_.send_runner = send_runner;
  state_.send_callback = send_callback;
  // Initial set for this GPU process will happen after the window
  // initializes, in CommitBoundsChange().
}

void DrmCursor::OnChannelDestroyed(int host_id) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(state_.lock);
  if (state_.host_id == host_id) {
    state_.host_id = -1;
    state_.send_runner = NULL;
    state_.send_callback.Reset();
  }
}

bool DrmCursor::OnMessageReceived(const IPC::Message& message) {
  return false;
}

void DrmCursor::SetCursorLocationLocked(const gfx::PointF& location) {
  state_.lock.AssertAcquired();

  gfx::PointF clamped_location = location;
  clamped_location.SetToMax(state_.confined_bounds.origin());
  // Right and bottom edges are exclusive.
  clamped_location.SetToMin(gfx::PointF(state_.confined_bounds.right() - 1,
                                        state_.confined_bounds.bottom() - 1));

  state_.location = clamped_location;
}

gfx::Point DrmCursor::GetBitmapLocationLocked() {
  return gfx::ToFlooredPoint(state_.location) -
         state_.bitmap->hotspot().OffsetFromOrigin();
}

bool DrmCursor::IsConnectedLocked() {
  return !state_.send_callback.is_null();
}

void DrmCursor::SendCursorShowLocked() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (!state_.bitmap) {
    SendCursorHideLocked();
    return;
  }
  SendLocked(new OzoneGpuMsg_CursorSet(state_.window, state_.bitmap->bitmaps(),
                                       GetBitmapLocationLocked(),
                                       state_.bitmap->frame_delay_ms()));
}

void DrmCursor::SendCursorHideLocked() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  SendLocked(new OzoneGpuMsg_CursorSet(state_.window, std::vector<SkBitmap>(),
                                       gfx::Point(), 0));
}

void DrmCursor::SendCursorMoveLocked() {
  if (!state_.bitmap)
    return;
  SendLocked(
      new OzoneGpuMsg_CursorMove(state_.window, GetBitmapLocationLocked()));
}

void DrmCursor::SendLocked(IPC::Message* message) {
  state_.lock.AssertAcquired();

  if (IsConnectedLocked() &&
      state_.send_runner->PostTask(FROM_HERE,
                                   base::Bind(state_.send_callback, message)))
    return;

  // Drop disconnected updates. DrmWindowHost will call CommitBoundsChange()
  // when
  // we connect to initialize the cursor location.
  delete message;
}

DrmCursor::CursorState::CursorState()
    : window(gfx::kNullAcceleratedWidget), host_id(-1) {
}

DrmCursor::CursorState::~CursorState() {
}

}  // namespace ui
