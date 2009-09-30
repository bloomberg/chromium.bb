// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/root_view.h"

#include <gtk/gtk.h>

#include "app/gfx/canvas_paint.h"
#include "base/logging.h"
#include "views/widget/widget_gtk.h"

namespace views {

void RootView::OnPaint(GdkEventExpose* event) {
  gfx::Rect scheduled_dirty_rect = GetScheduledPaintRectConstrainedToSize();
  gfx::Rect expose_rect = gfx::Rect(event->area);
  gfx::CanvasPaint canvas(event);
  bool invoked_process_paint = false;
  if (!canvas.is_empty()) {
    canvas.set_composite_alpha(
        static_cast<WidgetGtk*>(GetWidget())->is_transparent());
    SchedulePaint(gfx::Rect(canvas.rectangle()), false);
    if (NeedsPainting(false)) {
      ProcessPaint(&canvas);
      invoked_process_paint = true;
    }
  }

  if (invoked_process_paint && !scheduled_dirty_rect.IsEmpty() &&
      !expose_rect.Contains(scheduled_dirty_rect)) {
    // The region Views needs to paint (scheduled_dirty_rect) isn't contained
    // within the region GTK wants us to paint. ProccessPaint clears out the
    // dirty region, so that at this point views thinks everything has painted
    // correctly, but we haven't. Invoke schedule paint again to make sure we
    // paint everything we need to.
    //
    // NOTE: We don't expand the region to paint to include
    // scheduled_dirty_rect as that results in us drawing on top of any GTK
    // widgets that don't have a window. We have to schedule the paint through
    // GTK so that such widgets are painted.
    SchedulePaint(scheduled_dirty_rect, false);
  }
}

void RootView::StartDragForViewFromMouseEvent(
    View* view,
    const OSExchangeData& data,
    int operation) {
  // NOTE: view may be null.
  drag_view_ = view;
  static_cast<WidgetGtk*>(GetWidget())->DoDrag(data, operation);
  // If the view is removed during the drag operation, drag_view_ is set to
  // NULL.
  if (view && drag_view_ == view) {
    View* drag_view = drag_view_;
    drag_view_ = NULL;
    drag_view->OnDragDone();
  }
}

}
