// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_cursor.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/cursor/mojom/cursor_type.mojom.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/ozone/common/bitmap_cursor.h"
#include "ui/ozone/platform/drm/host/drm_window_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host_manager.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/events/ozone/chromeos/cursor_controller.h"
#endif

namespace ui {
namespace {

using mojom::CursorType;

class NullProxy : public DrmCursorProxy {
 public:
  NullProxy() {}

  NullProxy(const NullProxy&) = delete;
  NullProxy& operator=(const NullProxy&) = delete;

  ~NullProxy() override {}

  void CursorSet(gfx::AcceleratedWidget window,
                 const std::vector<SkBitmap>& bitmaps,
                 const gfx::Point& point,
                 base::TimeDelta frame_delay) override {}
  void Move(gfx::AcceleratedWidget window, const gfx::Point& point) override {}
  void InitializeOnEvdevIfNecessary() override {}
};

}  // namespace

DrmCursor::DrmCursor(DrmWindowHostManager* window_manager)
    : window_(gfx::kNullAcceleratedWidget),
      window_manager_(window_manager),
      proxy_(new NullProxy()) {
  evdev_thread_checker_.DetachFromThread();
}

DrmCursor::~DrmCursor() {}

void DrmCursor::SetDrmCursorProxy(std::unique_ptr<DrmCursorProxy> proxy) {
  TRACE_EVENT0("drmcursor", "DrmCursor::SetDrmCursorProxy");
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  proxy_ = std::move(proxy);
  if (window_ != gfx::kNullAcceleratedWidget)
    SendCursorShowLocked();
}

void DrmCursor::ResetDrmCursorProxy() {
  TRACE_EVENT0("drmcursor", "DrmCursor::ResetDrmCursorProxy");
  DCHECK(thread_checker_.CalledOnValidThread());

  NullProxy* np = new NullProxy();
  base::AutoLock lock(lock_);
  proxy_.reset(np);
}

gfx::Point DrmCursor::GetBitmapLocationLocked() {
  return gfx::ToFlooredPoint(location_) - cursor_->hotspot().OffsetFromOrigin();
}

void DrmCursor::SetCursor(gfx::AcceleratedWidget window,
                          scoped_refptr<BitmapCursor> platform_cursor) {
  TRACE_EVENT0("drmcursor", "DrmCursor::SetCursor");
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(window, gfx::kNullAcceleratedWidget);
  DCHECK(platform_cursor);

  base::AutoLock lock(lock_);

  if (window_ != window || cursor_ == platform_cursor)
    return;

  cursor_ = platform_cursor;

  SendCursorShowLocked();
}

void DrmCursor::OnWindowAdded(gfx::AcceleratedWidget window,
                              const gfx::Rect& bounds_in_screen,
                              const gfx::Rect& cursor_confined_bounds) {
  TRACE_EVENT0("drmcursor", "DrmCursor::OnWindowAdded");
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);

  if (window_ == gfx::kNullAcceleratedWidget) {
    // First window added & cursor is not placed. Place it.
    window_ = window;
    display_bounds_in_screen_ = bounds_in_screen;
    confined_bounds_ = cursor_confined_bounds;
    SetCursorLocationLocked(gfx::PointF(cursor_confined_bounds.CenterPoint()));
  }
}

void DrmCursor::OnWindowRemoved(gfx::AcceleratedWidget window) {
  TRACE_EVENT0("drmcursor", "DrmCursor::OnWindowRemoved");
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);

  if (window_ == window) {
    // Try to find a new location for the cursor.
    DrmWindowHost* dest_window = window_manager_->GetPrimaryWindow();

    if (dest_window) {
      window_ = dest_window->GetAcceleratedWidget();
      display_bounds_in_screen_ = dest_window->GetBounds();
      confined_bounds_ = dest_window->GetCursorConfinedBounds();
      SetCursorLocationLocked(gfx::PointF(confined_bounds_.CenterPoint()));
      SendCursorShowLocked();
    } else {
      window_ = gfx::kNullAcceleratedWidget;
      display_bounds_in_screen_ = gfx::Rect();
      confined_bounds_ = gfx::Rect();
      location_ = gfx::PointF();
    }
  }
}

void DrmCursor::CommitBoundsChange(
    gfx::AcceleratedWidget window,
    const gfx::Rect& new_display_bounds_in_screen,
    const gfx::Rect& new_confined_bounds) {
  TRACE_EVENT0("drmcursor", "DrmCursor::CommitBoundsChange");
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);

  if (window_ == window) {
    display_bounds_in_screen_ = new_display_bounds_in_screen;
    confined_bounds_ = new_confined_bounds;
    SetCursorLocationLocked(location_);
    SendCursorShowLocked();
  }
}

void DrmCursor::MoveCursorTo(gfx::AcceleratedWidget window,
                             const gfx::PointF& location) {
  TRACE_EVENT0("drmcursor", "DrmCursor::MoveCursorTo (window)");
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  gfx::AcceleratedWidget old_window = window_;

  if (window != old_window) {
    // When moving between displays, hide the cursor on the old display
    // prior to showing it on the new display.
    if (old_window != gfx::kNullAcceleratedWidget)
      SendCursorHideLocked();

    // TODO(rjk): pass this in?
    DrmWindowHost* drm_window_host = window_manager_->GetWindow(window);
    display_bounds_in_screen_ = drm_window_host->GetBounds();
    confined_bounds_ = drm_window_host->GetCursorConfinedBounds();
    window_ = window;
  }

  SetCursorLocationLocked(location);
  if (window != old_window)
    SendCursorShowLocked();
  else
    SendCursorMoveLocked();
}

void DrmCursor::MoveCursorTo(const gfx::PointF& screen_location) {
  TRACE_EVENT0("drmcursor", "DrmCursor::MoveCursorTo");
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);

  // TODO(spang): Moving between windows doesn't work here, but
  // is not needed for current uses.
  SetCursorLocationLocked(screen_location -
                          display_bounds_in_screen_.OffsetFromOrigin());

  SendCursorMoveLocked();
}

void DrmCursor::MoveCursor(const gfx::Vector2dF& delta) {
  DCHECK(evdev_thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("drmcursor", "DrmCursor::MoveCursor");
  base::AutoLock lock(lock_);

  if (window_ == gfx::kNullAcceleratedWidget)
    return;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  gfx::Vector2dF transformed_delta = delta;
  ui::CursorController::GetInstance()->ApplyCursorConfigForWindow(
      window_, &transformed_delta);
  SetCursorLocationLocked(location_ + transformed_delta);
#else
  SetCursorLocationLocked(location_ + delta);
#endif
  SendCursorMoveLocked();
}

bool DrmCursor::IsCursorVisible() {
  base::AutoLock lock(lock_);
  return cursor_ != nullptr && cursor_->type() != CursorType::kNone;
}

gfx::PointF DrmCursor::GetLocation() {
  base::AutoLock lock(lock_);
  return location_ + display_bounds_in_screen_.OffsetFromOrigin();
}

gfx::Rect DrmCursor::GetCursorConfinedBounds() {
  base::AutoLock lock(lock_);
  return confined_bounds_ + display_bounds_in_screen_.OffsetFromOrigin();
}

void DrmCursor::InitializeOnEvdev() {
  DCHECK(evdev_thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  proxy_->InitializeOnEvdevIfNecessary();
}

void DrmCursor::SetCursorLocationLocked(const gfx::PointF& location) {
  gfx::PointF clamped_location = location;
  clamped_location.SetToMax(gfx::PointF(confined_bounds_.origin()));
  // Right and bottom edges are exclusive.
  clamped_location.SetToMin(
      gfx::PointF(confined_bounds_.right() - 1, confined_bounds_.bottom() - 1));

  location_ = clamped_location;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ui::CursorController::GetInstance()->SetCursorLocation(location_);
#endif
}

void DrmCursor::SendCursorShowLocked() {
  if (!cursor_ || cursor_->type() == CursorType::kNone) {
    SendCursorHideLocked();
    return;
  }

  CursorSetLockTested(window_, cursor_->bitmaps(), GetBitmapLocationLocked(),
                      cursor_->frame_delay());
}

void DrmCursor::SendCursorHideLocked() {
  CursorSetLockTested(window_, std::vector<SkBitmap>(), gfx::Point(),
                      base::TimeDelta());
}

void DrmCursor::SendCursorMoveLocked() {
  if (!cursor_ || cursor_->type() == CursorType::kNone)
    return;

  MoveLockTested(window_, GetBitmapLocationLocked());
}

// Lock-testing helpers.
void DrmCursor::CursorSetLockTested(gfx::AcceleratedWidget window,
                                    const std::vector<SkBitmap>& bitmaps,
                                    const gfx::Point& point,
                                    base::TimeDelta frame_delay) {
  lock_.AssertAcquired();
  proxy_->CursorSet(window, bitmaps, point, frame_delay);
}

void DrmCursor::MoveLockTested(gfx::AcceleratedWidget window,
                               const gfx::Point& point) {
  lock_.AssertAcquired();
  proxy_->Move(window, point);
}


}  // namespace ui
