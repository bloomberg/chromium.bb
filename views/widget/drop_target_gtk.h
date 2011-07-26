// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_DROP_TARGET_GTK_H_
#define VIEWS_WIDGET_DROP_TARGET_GTK_H_
#pragma once

#include <gtk/gtk.h>
#include <set>

#include "base/memory/scoped_ptr.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "views/widget/drop_helper.h"

namespace ui {
class OSExchangeDataProviderGtk;
}
using ui::OSExchangeData;
using ui::OSExchangeDataProviderGtk;

namespace views {

class View;
namespace internal {
class RootView;
}

// DropTarget implementation for Gtk.
//
// The data for a drop is not immediately available on X. As such we lazily
// ask for data as necessary. Some Views require data before they can determine
// if the drop is going to be allowed. When such a View is encountered the
// relevant data is requested from the drag source. When the data is available
// the target is notified. Similarly if the drop completes and the data has
// not yet been fetched, it is fetched and the target then notified.
//
// When a drop finishes this class calls back to the containing NativeWidgetGtk
// which results in deleting the DropTargetGtk.
class DropTargetGtk {
 public:
  DropTargetGtk(internal::RootView* root_view, GdkDragContext* context);
  ~DropTargetGtk();

  // If a drag and drop is underway and |view| is the current drop target, the
  // drop target is set to null.
  // This is invoked when a View is removed from the RootView to make sure
  // we don't target a view that was removed during dnd.
  void ResetTargetViewIfEquals(View* view);

  // Drop methods from Gtk. These are forwarded from the containing
  // NativeWidgetGtk.
  void OnDragDataReceived(GdkDragContext* context,
                          gint x,
                          gint y,
                          GtkSelectionData* data,
                          guint info,
                          guint time);
  gboolean OnDragDrop(GdkDragContext* context,
                      gint x,
                      gint y,
                      guint time);
  void OnDragLeave(GdkDragContext* context, guint time);
  gboolean OnDragMotion(GdkDragContext* context,
                        gint x,
                        gint y,
                        guint time);

 private:
  // Invoked when the drop finishes AND all the data is available.
  void FinishDrop(GdkDragContext* context, gint x, gint y, guint time);

  // Returns in |f2| and |cf2| the intersection of |f1| |f2| and
  // |cf1|, |cf2|.
  void IntersectFormats(int f1, const std::set<GdkAtom>& cf1,
                        int* f2, std::set<GdkAtom>* cf2);

  // Requests the formats in |formats| and the custom formats in
  // |custom_formats|.
  void RequestFormats(GdkDragContext* context,
                      int formats,
                      const std::set<GdkAtom>& custom_formats,
                      guint time);

  // Reutrns the Provider of the OSExchangeData we created.
  OSExchangeDataProviderGtk& data_provider() const;

  // Manages sending the appropriate drop methods to the view the drop is over.
  DropHelper helper_;

  // The formats we've requested from the drag source.
  //
  // NOTE: these formats are the intersection of the formats requested by the
  // drop target and the formats provided by the source.
  int requested_formats_;
  std::set<GdkAtom> requested_custom_formats_;

  // The data.
  scoped_ptr<OSExchangeData> data_;

  // Are we waiting for data from the source before we can notify the view?
  // This is set in two distinct ways: when the view requires the data before
  // it can answer Can Drop (that is, AreDropTypesRequired returns true) and
  // when the user dropped the data but we didn't get it all yet.
  bool waiting_for_data_;

  // Has OnDragDrop been invoked?
  bool received_drop_;

  // The view under the mouse. This is not necessarily the same as
  // helper_.target_view(). The two differ if the view under the mouse requires
  // the data.
  View* pending_view_;

  DISALLOW_COPY_AND_ASSIGN(DropTargetGtk);
};

}  // namespace views

#endif  // VIEWS_WIDGET_DROP_TARGET_GTK_H_
