// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/root_view.h"

#include <gtk/gtk.h>
#include <X11/Xlib.h>

#include "base/logging.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "views/widget/widget_gtk.h"

namespace views {

void RootView::OnPaint(GdkEventExpose* event) {

  WidgetGtk* widget = static_cast<WidgetGtk*>(GetWidget());
  if (!widget) {
    NOTREACHED();
    return;
  }
  if (debug_paint_enabled_) {
    // Using cairo directly because using skia didn't have immediate effect.
    cairo_t* cr = gdk_cairo_create(event->window);
    gdk_cairo_region(cr, event->region);
    cairo_set_source_rgb(cr, 1, 0, 0);  // red
    cairo_rectangle(cr,
                    event->area.x, event->area.y,
                    event->area.width, event->area.height);
    cairo_fill(cr);
    cairo_destroy(cr);
    // Make sure that users see the red flash.
    XSync(ui::GetXDisplay(), false /* don't discard events */);
  }
  gfx::Rect scheduled_dirty_rect = GetScheduledPaintRectConstrainedToSize();
  gfx::Rect expose_rect = gfx::Rect(event->area);
  gfx::CanvasSkiaPaint canvas(event);
  bool invoked_process_paint = false;
  if (!canvas.is_empty()) {
    canvas.set_composite_alpha(widget->is_transparent());
    SchedulePaintInRect(gfx::Rect(canvas.rectangle()), false);
    if (NeedsPainting(false)) {
      Paint(&canvas);
      invoked_process_paint = true;
    }
  }

  if (invoked_process_paint && !scheduled_dirty_rect.IsEmpty() &&
      !expose_rect.Contains(scheduled_dirty_rect) && widget &&
      !widget->in_paint_now()) {
    // We're painting as the result of Gtk wanting us to paint (not from views)
    // and there was a region scheduled by views to be painted that is not
    // contained in the region gtk wants us to paint. As a result of the
    // Paint() call above views no longer thinks it needs to be painted.
    // We have to invoke SchedulePaint here to be sure we end up painting the
    // region views wants to paint, otherwise we'll drop the views paint region
    // on the floor.
    //
    // NOTE: We don't expand the region to paint to include
    // scheduled_dirty_rect as that results in us drawing on top of any GTK
    // widgets that don't have a window. We have to schedule the paint through
    // GTK so that such widgets are painted.
    SchedulePaintInRect(scheduled_dirty_rect, false);
  }
}

}
