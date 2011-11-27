// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_gtk.h"

#include <gtk/gtk.h>

#include <algorithm>

#include "base/logging.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/gtk_views_fixed.h"
#include "ui/views/widget/native_widget_gtk.h"
#include "ui/views/widget/widget.h"
#include "views/views_delegate.h"

namespace views {

namespace {
static bool signal_id_initialized_ = false;
static guint focus_in_event_signal_id_;
static guint focus_out_event_signal_id_;

////////////////////////////////////////////////////////////////////////////////
// Utility functions to block focus signals while re-creating
// Fixed widget.

void InitSignalIds() {
  if (!signal_id_initialized_) {
    signal_id_initialized_ = true;
    focus_in_event_signal_id_ =
        g_signal_lookup("focus-in-event", GTK_TYPE_WIDGET);
    focus_out_event_signal_id_ =
        g_signal_lookup("focus-out-event", GTK_TYPE_WIDGET);
  }
}

// Blocks a |signal_id| on the given |widget| if any.
void BlockSignal(GtkWidget* widget, guint signal_id) {
  gulong handler_id = g_signal_handler_find(G_OBJECT(widget),
                                            G_SIGNAL_MATCH_ID,
                                            signal_id,
                                            0, NULL, NULL, NULL);
  if (handler_id) {
    g_signal_handler_block(G_OBJECT(widget), handler_id);
  }
}

// Unblocks a |signal_id| on the given |widget| if any.
void UnblockSignal(GtkWidget* widget, guint signal_id) {
  gulong handler_id = g_signal_handler_find(G_OBJECT(widget),
                                            G_SIGNAL_MATCH_ID,
                                            signal_id,
                                            0, NULL, NULL, NULL);
  if (handler_id) {
    g_signal_handler_unblock(G_OBJECT(widget), handler_id);
  }
}

// Blocks focus in/out signals of the widget and its descendent
// children.
// Note: Due to the limiation of Gtk API, this only blocks the 1st
// handler found and won't block the rest if there is more than one handlers.
// See bug http://crbug.com/33236.
void BlockFocusSignals(GtkWidget* widget, gpointer data) {
  if (!widget)
    return;
  InitSignalIds();
  BlockSignal(widget, focus_in_event_signal_id_);
  BlockSignal(widget, focus_out_event_signal_id_);
  if (GTK_IS_CONTAINER(widget))
    gtk_container_foreach(GTK_CONTAINER(widget), BlockFocusSignals, data);
}

// Unlocks focus in/out signals of the widget and its descendent children.
void UnblockFocusSignals(GtkWidget* widget, gpointer data) {
  if (!widget)
    return;
  InitSignalIds();
  UnblockSignal(widget, focus_in_event_signal_id_);
  UnblockSignal(widget, focus_out_event_signal_id_);
  if (GTK_IS_CONTAINER(widget))
    gtk_container_foreach(GTK_CONTAINER(widget), UnblockFocusSignals, data);
}

// Removes |child| from |parent|.
void RemoveFromParent(GtkWidget* child, gpointer parent) {
  gtk_container_remove(GTK_CONTAINER(parent), child);
}

// Reparents |child| to be a child of |parent|.
void Reparent(GtkWidget* child, gpointer parent) {
  gtk_widget_reparent(child, GTK_WIDGET(parent));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostGtk, public:

NativeViewHostGtk::NativeViewHostGtk(NativeViewHost* host)
    : host_(host),
      installed_clip_(false),
      destroy_signal_id_(0),
      focus_signal_id_(0),
      fixed_(NULL) {
  CreateFixed(false);
}

NativeViewHostGtk::~NativeViewHostGtk() {
  if (fixed_) {
    gtk_container_foreach(GTK_CONTAINER(fixed_), RemoveFromParent, fixed_);
    gtk_widget_destroy(fixed_);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostGtk, NativeViewHostWrapper implementation:

void NativeViewHostGtk::NativeViewAttached() {
  AttachHostWidget();

  GtkWidget* host_widget = host_->native_view();

  // Let the widget know that the native component has been painted.
  views::NativeWidgetGtk::RegisterChildExposeHandler(host_widget);

  if (!destroy_signal_id_) {
    destroy_signal_id_ = g_signal_connect(host_widget,
                                          "destroy", G_CALLBACK(CallDestroy),
                                          this);
  }

  if (!focus_signal_id_) {
    focus_signal_id_ = g_signal_connect(host_widget,
                                        "focus-in-event",
                                        G_CALLBACK(CallFocusIn), this);
  }

  // Always layout though.
  host_->Layout();

  // TODO(port): figure out focus.
}

void NativeViewHostGtk::NativeViewDetaching(bool destroyed) {
  GtkWidget* host_widget = host_->native_view();
  DCHECK(host_widget);

  g_signal_handler_disconnect(G_OBJECT(host_widget), destroy_signal_id_);
  destroy_signal_id_ = 0;

  g_signal_handler_disconnect(G_OBJECT(host_widget), focus_signal_id_);
  focus_signal_id_ = 0;

  installed_clip_ = false;
}

void NativeViewHostGtk::AddedToWidget() {
  if (!fixed_)
    CreateFixed(false);
  if (gtk_widget_get_parent(fixed_))
    GetHostWidget()->ReparentChild(fixed_);
  else
    GetHostWidget()->AddChild(fixed_);

  if (!host_->native_view())
    return;

  AttachHostWidget();

  if (host_->IsVisibleInRootView()) {
    gtk_widget_show(host_->native_view());
    gtk_widget_show(fixed_);
  } else {
    gtk_widget_hide(fixed_);
  }
  host_->Layout();
}

void NativeViewHostGtk::RemovedFromWidget() {
  if (!host_->native_view())
    return;
  DestroyFixed();
}

void NativeViewHostGtk::InstallClip(int x, int y, int w, int h) {
  DCHECK(w > 0 && h > 0);
  installed_clip_bounds_.SetRect(x, y, w, h);
  if (!installed_clip_) {
    installed_clip_ = true;

    // We only re-create the fixed with a window when a cliprect is installed.
    // Because the presence of a X Window will prevent transparency from working
    // properly, we only want it to be active for the duration of a clip
    // (typically during animations and scrolling.)
    CreateFixed(true);
  }
}

bool NativeViewHostGtk::HasInstalledClip() {
  return installed_clip_;
}

void NativeViewHostGtk::UninstallClip() {
  installed_clip_ = false;
  // We now re-create the fixed without a X Window so transparency works again.
  CreateFixed(false);
}

void NativeViewHostGtk::ShowWidget(int x, int y, int w, int h) {
  // x and y are the desired position of host_ in NativeWidgetGtk coordinates.
  int fixed_x = x;
  int fixed_y = y;
  int fixed_w = w;
  int fixed_h = h;
  int child_x = 0;
  int child_y = 0;
  int child_w = w;
  int child_h = h;
  if (installed_clip_) {
    child_x = -installed_clip_bounds_.x();
    child_y = -installed_clip_bounds_.y();
    fixed_x += -child_x;
    fixed_y += -child_y;
    fixed_w = std::min(installed_clip_bounds_.width(), w);
    fixed_h = std::min(installed_clip_bounds_.height(), h);
  }

  GtkWidget* host_widget = host_->native_view();
  // Don't call gtk_widget_size_allocate now, as we're possibly in the
  // middle of a re-size, and it kicks off another re-size, and you
  // get flashing.  Instead, we'll set the desired size as properties
  // on the widget and queue the re-size.
  gtk_views_fixed_set_widget_size(host_widget, child_w, child_h);
  gtk_fixed_move(GTK_FIXED(fixed_), host_widget, child_x, child_y);

  // Size and place the fixed_.
  GetHostWidget()->PositionChild(fixed_, fixed_x, fixed_y, fixed_w, fixed_h);

  gtk_widget_show(host_widget);
  gtk_widget_show(fixed_);
}

void NativeViewHostGtk::HideWidget() {
  if (fixed_)
    gtk_widget_hide(fixed_);
}

void NativeViewHostGtk::SetFocus() {
  GtkWidget* host_widget = host_->native_view();
  DCHECK(host_widget);
  gtk_widget_grab_focus(host_widget);
}

gfx::NativeViewAccessible NativeViewHostGtk::GetNativeViewAccessible() {
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostGtk, private:

void NativeViewHostGtk::CreateFixed(bool needs_window) {
  GtkWidget* focused_widget = GetFocusedDescendant();

  bool focus_event_blocked = false;
  // We move focus around and do not want focus events to be emitted
  // during this process.
  if (fixed_) {
    BlockFocusSignals(GetHostWidget()->GetNativeView(), NULL);
    focus_event_blocked = true;
  }

  if (focused_widget) {
    // A descendant of our fixed has focus. When we destroy the fixed focus is
    // automatically moved. Temporarily move focus to our host widget, then
    // restore focus after we create the new fixed_. This way focus hasn't
    // really moved.
    gtk_widget_grab_focus(GetHostWidget()->GetNativeView());
  }

  // Move all the contained widgets to the new fixed.
  GtkWidget* new_fixed = gtk_views_fixed_new();
  if (fixed_) {
    gtk_container_foreach(GTK_CONTAINER(fixed_), Reparent, new_fixed);
    DestroyFixed();
  }
  fixed_ = new_fixed;

  gtk_widget_set_name(fixed_, "views-native-view-host-fixed");
  gtk_fixed_set_has_window(GTK_FIXED(fixed_), needs_window);

  // Defeat refcounting. We need to own the fixed.
  gtk_widget_ref(fixed_);

  NativeWidgetGtk* widget_gtk = GetHostWidget();
  if (widget_gtk) {
    widget_gtk->AddChild(fixed_);
    // Clear the background so we don't get flicker.
    gtk_widget_realize(fixed_);
    gdk_window_set_back_pixmap(fixed_->window, NULL, false);
  }

  if (host_->native_view())
    AttachHostWidget();

  if (widget_gtk && host_->native_view() && focused_widget)
    gtk_widget_grab_focus(focused_widget);

  if (focus_event_blocked) {
    // Unblocking a signal handler that is not blocked fails.
    // Unblock only when it's unblocked.
    UnblockFocusSignals(GetHostWidget()->GetNativeView(), NULL);
  }
}

void NativeViewHostGtk::DestroyFixed() {
  if (!fixed_)
    return;

  gtk_widget_hide(fixed_);
  gtk_container_foreach(GTK_CONTAINER(fixed_), RemoveFromParent, fixed_);
  GetHostWidget()->RemoveChild(fixed_);

  // fixed_ should not have any children this point.
  DCHECK_EQ(0U,
            g_list_length(gtk_container_get_children(GTK_CONTAINER(fixed_))));
  gtk_widget_destroy(fixed_);
  fixed_ = NULL;
}

NativeWidgetGtk* NativeViewHostGtk::GetHostWidget() const {
  return static_cast<NativeWidgetGtk*>(host_->GetWidget()->native_widget());
}

GtkWidget* NativeViewHostGtk::GetFocusedDescendant() {
  if (!fixed_)
    return NULL;
  NativeWidgetGtk* host = GetHostWidget();
  if (!host)
    return NULL;
  GtkWidget* top_level = gtk_widget_get_toplevel(host->GetNativeView());
  if (!top_level || !GTK_IS_WINDOW(top_level))
    return NULL;
  GtkWidget* focused = gtk_window_get_focus(GTK_WINDOW(top_level));
  if (!focused)
    return NULL;
  return (focused == fixed_ || gtk_widget_is_ancestor(focused, fixed_)) ?
      focused : NULL;
}

void NativeViewHostGtk::AttachHostWidget() {
  GtkWidget* host_widget = host_->native_view();
  DCHECK(host_widget);

  GtkWidget* host_parent = gtk_widget_get_parent(host_widget);
  bool parent_changed = true;
  if (host_parent) {
    if (host_parent != fixed_)
      gtk_widget_reparent(host_widget, fixed_);
    else
      parent_changed = false;
  } else {
    gtk_container_add(GTK_CONTAINER(fixed_), host_widget);
  }

  if (parent_changed) {
    // We need to clear the background so we don't get flicker on tab switching.
    // To do that we must realize the widget if it's not already.
    if (!GTK_WIDGET_REALIZED(host_widget))
      gtk_widget_realize(host_widget);
    gdk_window_set_back_pixmap(host_widget->window, NULL, false);
  }
}

// static
void NativeViewHostGtk::CallDestroy(GtkObject* object,
                                    NativeViewHostGtk* host) {
  host->host_->NativeViewDestroyed();
}

// static
gboolean NativeViewHostGtk::CallFocusIn(GtkWidget* gtk_widget,
                                        GdkEventFocus* event,
                                        NativeViewHostGtk* host) {
  Widget* widget = Widget::GetWidgetForNativeView(gtk_widget);
  FocusManager* focus_manager = widget ? widget->GetFocusManager() : NULL;
  if (!focus_manager) {
    // TODO(jcampan): http://crbug.com/21378 Reenable this NOTREACHED() when the
    // options page is only based on views.
    // NOTREACHED();
    NOTIMPLEMENTED();
    return false;
  }
  focus_manager->SetFocusedView(host->host_->focus_view());
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostWrapper, public:

// static
NativeViewHostWrapper* NativeViewHostWrapper::CreateWrapper(
    NativeViewHost* host) {
  return new NativeViewHostGtk(host);
}

}  // namespace views
