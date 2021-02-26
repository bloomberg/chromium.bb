// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/paint_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "pdf/paint_ready_rect.h"
#include "pdf/ppapi_migration/callback.h"
#include "pdf/ppapi_migration/geometry_conversions.h"
#include "pdf/ppapi_migration/graphics.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/module.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace chrome_pdf {

PaintManager::PaintManager(Client* client) : client_(client) {
  DCHECK(client_);
}

PaintManager::~PaintManager() = default;

// static
gfx::Size PaintManager::GetNewContextSize(const gfx::Size& current_context_size,
                                          const gfx::Size& plugin_size) {
  // The amount of additional space in pixels to allocate to the right/bottom of
  // the context.
  constexpr int kBufferSize = 50;

  // Default to returning the same size.
  gfx::Size result = current_context_size;

  // The minimum size of the plugin before resizing the context to ensure we
  // aren't wasting too much memory. We deduct twice the kBufferSize from the
  // current context size which gives a threshhold that is kBufferSize below
  // the plugin size when the context size was last computed.
  gfx::Size min_size(
      std::max(current_context_size.width() - 2 * kBufferSize, 0),
      std::max(current_context_size.height() - 2 * kBufferSize, 0));

  // If the plugin size is bigger than the current context size, we need to
  // resize the context. If the plugin size is smaller than the current
  // context size by a given threshhold then resize the context so that we
  // aren't wasting too much memory.
  if (plugin_size.width() > current_context_size.width() ||
      plugin_size.height() > current_context_size.height() ||
      plugin_size.width() < min_size.width() ||
      plugin_size.height() < min_size.height()) {
    // Create a larger context than needed so that if we only resize by a
    // small margin, we don't need a new context.
    result = gfx::Size(plugin_size.width() + kBufferSize,
                       plugin_size.height() + kBufferSize);
  }

  return result;
}

void PaintManager::SetSize(const gfx::Size& new_size, float device_scale) {
  if (GetEffectiveSize() == new_size &&
      GetEffectiveDeviceScale() == device_scale) {
    return;
  }

  has_pending_resize_ = true;
  pending_size_ = new_size;
  pending_device_scale_ = device_scale;

  view_size_changed_waiting_for_paint_ = true;

  Invalidate();
}

void PaintManager::SetTransform(float scale,
                                const gfx::Point& origin,
                                const gfx::Vector2d& translate,
                                bool schedule_flush) {
  if (!graphics_)
    return;

  graphics_->SetLayerTransform(scale, origin, translate);

  if (!schedule_flush)
    return;

  if (flush_pending_) {
    flush_requested_ = true;
    return;
  }
  Flush();
}

void PaintManager::ClearTransform() {
  SetTransform(1.f, gfx::Point(), gfx::Vector2d(), false);
}

void PaintManager::Invalidate() {
  if (!graphics_ && !has_pending_resize_)
    return;

  EnsureCallbackPending();
  aggregator_.InvalidateRect(gfx::Rect(GetEffectiveSize()));
}

void PaintManager::InvalidateRect(const gfx::Rect& rect) {
  DCHECK(!in_paint_);

  if (!graphics_ && !has_pending_resize_)
    return;

  // Clip the rect to the device area.
  gfx::Rect clipped_rect =
      gfx::IntersectRects(rect, gfx::Rect(GetEffectiveSize()));
  if (clipped_rect.IsEmpty())
    return;  // Nothing to do.

  EnsureCallbackPending();
  aggregator_.InvalidateRect(clipped_rect);
}

void PaintManager::ScrollRect(const gfx::Rect& clip_rect,
                              const gfx::Vector2d& amount) {
  DCHECK(!in_paint_);

  if (!graphics_ && !has_pending_resize_)
    return;

  EnsureCallbackPending();

  aggregator_.ScrollRect(clip_rect, amount);
}

gfx::Size PaintManager::GetEffectiveSize() const {
  return has_pending_resize_ ? pending_size_ : plugin_size_;
}

float PaintManager::GetEffectiveDeviceScale() const {
  return has_pending_resize_ ? pending_device_scale_ : device_scale_;
}

void PaintManager::EnsureCallbackPending() {
  // The best way for us to do the next update is to get a notification that
  // a previous one has completed. So if we're already waiting for one, we
  // don't have to do anything differently now.
  if (flush_pending_)
    return;

  // If no flush is pending, we need to do a manual call to get back to the
  // main thread. We may have one already pending, or we may need to schedule.
  if (manual_callback_pending_)
    return;

  pp::Module::Get()->core()->CallOnMainThread(
      0,
      PPCompletionCallbackFromResultCallback(base::BindOnce(
          &PaintManager::OnManualCallbackComplete, weak_factory_.GetWeakPtr())),
      0);
  manual_callback_pending_ = true;
}

void PaintManager::DoPaint() {
  base::AutoReset<bool> auto_reset_in_paint(&in_paint_, true);

  std::vector<PaintReadyRect> ready_rects;
  std::vector<gfx::Rect> pending_rects;

  DCHECK(aggregator_.HasPendingUpdate());

  // Apply any pending resize. Setting the graphics to this class must happen
  // before asking the plugin to paint in case it requests invalides or resizes.
  // However, the bind must not happen until afterward since we don't want to
  // have an unpainted device bound. The needs_binding flag tells us whether to
  // do this later.
  //
  // Note that |has_pending_resize_| will always be set on the first DoPaint().
  DCHECK(graphics_ || has_pending_resize_);
  if (has_pending_resize_) {
    plugin_size_ = pending_size_;
    // Only create a new graphics context if the current context isn't big
    // enough or if it is far too big. This avoids creating a new context if
    // we only resize by a small amount.
    gfx::Size old_size = graphics_ ? graphics_->size() : gfx::Size();
    gfx::Size new_size = GetNewContextSize(old_size, pending_size_);
    if (old_size != new_size || !graphics_) {
      graphics_ = client_->CreatePaintGraphics(new_size);
      graphics_need_to_be_bound_ = true;

      // Since we're binding a new one, all of the callbacks have been canceled.
      manual_callback_pending_ = false;
      flush_pending_ = false;
      weak_factory_.InvalidateWeakPtrs();
    }

    if (pending_device_scale_ != 1.0)
      graphics_->SetScale(1.0 / pending_device_scale_);
    device_scale_ = pending_device_scale_;

    // This must be cleared before calling into the plugin since it may do
    // additional invalidation or sizing operations.
    has_pending_resize_ = false;
    pending_size_ = gfx::Size();
  }

  PaintAggregator::PaintUpdate update = aggregator_.GetPendingUpdate();
  client_->OnPaint(update.paint_rects, &ready_rects, &pending_rects);

  if (ready_rects.empty() && pending_rects.empty())
    return;  // Nothing was painted, don't schedule a flush.

  std::vector<PaintReadyRect> ready_now;
  if (pending_rects.empty()) {
    aggregator_.SetIntermediateResults(ready_rects, pending_rects);
    ready_now = aggregator_.GetReadyRects();
    aggregator_.ClearPendingUpdate();

    // Apply any scroll first.
    if (update.has_scroll)
      graphics_->Scroll(update.scroll_rect, update.scroll_delta);

    view_size_changed_waiting_for_paint_ = false;
  } else {
    std::vector<PaintReadyRect> ready_later;
    for (const auto& ready_rect : ready_rects) {
      // Don't flush any part (i.e. scrollbars) if we're resizing the browser,
      // as that'll lead to flashes.  Until we flush, the browser will use the
      // previous image, but if we flush, it'll revert to using the blank image.
      // We make an exception for the first paint since we want to show the
      // default background color instead of the pepper default of black.
      if (ready_rect.flush_now() &&
          (!view_size_changed_waiting_for_paint_ || first_paint_)) {
        ready_now.push_back(ready_rect);
      } else {
        ready_later.push_back(ready_rect);
      }
    }
    // Take the rectangles, except the ones that need to be flushed right away,
    // and save them so that everything is flushed at once.
    aggregator_.SetIntermediateResults(ready_later, pending_rects);

    if (ready_now.empty()) {
      EnsureCallbackPending();
      return;
    }
  }

  for (const auto& ready_rect : ready_now) {
    graphics_->PaintImage(ready_rect.image(), ready_rect.rect());
  }

  Flush();

  first_paint_ = false;

  if (graphics_need_to_be_bound_) {
    client_->BindPaintGraphics(*graphics_);
    graphics_need_to_be_bound_ = false;
  }
}

void PaintManager::Flush() {
  flush_requested_ = false;
  flush_pending_ = graphics_->Flush(base::BindOnce(
      &PaintManager::OnFlushComplete, weak_factory_.GetWeakPtr()));
  DCHECK(flush_pending_);
}

void PaintManager::OnFlushComplete(int32_t) {
  DCHECK(flush_pending_);
  flush_pending_ = false;

  // If more paints were enqueued while we were waiting for the flush to
  // complete, execute them now.
  if (aggregator_.HasPendingUpdate())
    DoPaint();

  // If there was another flush request while flushing we flush again.
  if (flush_requested_) {
    Flush();
  }
}

void PaintManager::OnManualCallbackComplete(int32_t) {
  DCHECK(manual_callback_pending_);
  manual_callback_pending_ = false;

  // Just because we have a manual callback doesn't mean there are actually any
  // invalid regions. Even though we only schedule this callback when something
  // is pending, a Flush callback could have come in before this callback was
  // executed and that could have cleared the queue.
  if (aggregator_.HasPendingUpdate())
    DoPaint();
}

}  // namespace chrome_pdf
