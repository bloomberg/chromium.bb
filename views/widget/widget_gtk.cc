// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget_gtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/extensions/shape.h>

#include <set>
#include <vector>

#include "base/auto_reset.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_gtk.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/path.h"
#include "views/focus/view_storage.h"
#include "views/widget/drop_target_gtk.h"
#include "views/widget/gtk_views_fixed.h"
#include "views/widget/gtk_views_window.h"
#include "views/widget/root_view.h"
#include "views/widget/tooltip_manager_gtk.h"
#include "views/widget/widget_delegate.h"
#include "views/widget/widget_utils.h"
#include "views/window/window_gtk.h"

#if defined(TOUCH_UI)
#if defined(HAVE_XINPUT2)
#include <gdk/gdkx.h>

#include "ui/gfx/gtk_util.h"
#include "views/touchui/touch_factory.h"
#endif
#endif

using ui::OSExchangeData;
using ui::OSExchangeDataProviderGtk;
using ui::ActiveWindowWatcherX;

namespace {

// g_object data keys to associate a WidgetGtk object to a GtkWidget.
const char* kWidgetKey = "__VIEWS_WIDGET__";
// A g_object data key to associate a CompositePainter object to a GtkWidget.
const char* kCompositePainterKey = "__VIEWS_COMPOSITE_PAINTER__";
// A g_object data key to associate the flag whether or not the widget
// is composited to a GtkWidget. gtk_widget_is_composited simply tells
// if x11 supports composition and cannot be used to tell if given widget
// is composited.
const char* kCompositeEnabledKey = "__VIEWS_COMPOSITE_ENABLED__";

// CompositePainter draws a composited child widgets image into its
// drawing area. This object is created at most once for a widget and kept
// until the widget is destroyed.
class CompositePainter {
 public:
  explicit CompositePainter(GtkWidget* parent)
      : parent_object_(G_OBJECT(parent)) {
    handler_id_ = g_signal_connect_after(
        parent_object_, "expose_event", G_CALLBACK(OnCompositePaint), NULL);
  }

  static void AddCompositePainter(GtkWidget* widget) {
    CompositePainter* painter = static_cast<CompositePainter*>(
        g_object_get_data(G_OBJECT(widget), kCompositePainterKey));
    if (!painter) {
      g_object_set_data(G_OBJECT(widget), kCompositePainterKey,
                        new CompositePainter(widget));
      g_signal_connect(widget, "destroy",
                       G_CALLBACK(&DestroyPainter), NULL);
    }
  }

  // Set the composition flag.
  static void SetComposited(GtkWidget* widget) {
    g_object_set_data(G_OBJECT(widget), kCompositeEnabledKey,
                      const_cast<char*>(""));
  }

  // Returns true if the |widget| is composited and ready to be drawn.
  static bool IsComposited(GtkWidget* widget) {
    return g_object_get_data(G_OBJECT(widget), kCompositeEnabledKey) != NULL;
  }

 private:
  virtual ~CompositePainter() {}

  // Composes a image from one child.
  static void CompositeChildWidget(GtkWidget* child, gpointer data) {
    GdkEventExpose* event = static_cast<GdkEventExpose*>(data);
    GtkWidget* parent = gtk_widget_get_parent(child);
    DCHECK(parent);
    if (IsComposited(child)) {
      cairo_t* cr = gdk_cairo_create(parent->window);
      gdk_cairo_set_source_pixmap(cr, child->window,
                                  child->allocation.x,
                                  child->allocation.y);
      GdkRegion* region = gdk_region_rectangle(&child->allocation);
      gdk_region_intersect(region, event->region);
      gdk_cairo_region(cr, region);
      cairo_clip(cr);
      cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
      cairo_paint(cr);
      cairo_destroy(cr);
    }
  }

  // Expose-event handler that compose & draws children's image into
  // the |parent|'s drawing area.
  static gboolean OnCompositePaint(GtkWidget* parent, GdkEventExpose* event) {
    gtk_container_foreach(GTK_CONTAINER(parent),
                          CompositeChildWidget,
                          event);
    return false;
  }

  static void DestroyPainter(GtkWidget* object) {
    CompositePainter* painter = reinterpret_cast<CompositePainter*>(
        g_object_get_data(G_OBJECT(object), kCompositePainterKey));
    DCHECK(painter);
    delete painter;
  }

  GObject* parent_object_;
  gulong handler_id_;

  DISALLOW_COPY_AND_ASSIGN(CompositePainter);
};

}  // namespace

namespace views {

// During drag and drop GTK sends a drag-leave during a drop. This means we
// have no way to tell the difference between a normal drag leave and a drop.
// To work around that we listen for DROP_START, then ignore the subsequent
// drag-leave that GTK generates.
class WidgetGtk::DropObserver : public MessageLoopForUI::Observer {
 public:
  DropObserver() {}

  static DropObserver* GetInstance() {
    return Singleton<DropObserver>::get();
  }

  virtual void WillProcessEvent(GdkEvent* event) {
    if (event->type == GDK_DROP_START) {
      WidgetGtk* widget = GetWidgetGtkForEvent(event);
      if (widget)
        widget->ignore_drag_leave_ = true;
    }
  }

  virtual void DidProcessEvent(GdkEvent* event) {
  }

 private:
  WidgetGtk* GetWidgetGtkForEvent(GdkEvent* event) {
    GtkWidget* gtk_widget = gtk_get_event_widget(event);
    if (!gtk_widget)
      return NULL;

    return WidgetGtk::GetViewForNative(gtk_widget);
  }

  DISALLOW_COPY_AND_ASSIGN(DropObserver);
};

// Returns the position of a widget on screen.
static void GetWidgetPositionOnScreen(GtkWidget* widget, int* x, int *y) {
  // First get the root window.
  GtkWidget* root = widget;
  while (root && !GTK_IS_WINDOW(root)) {
    root = gtk_widget_get_parent(root);
  }
  if (!root) {
    // If root is null we're not parented. Return 0x0 and assume the caller will
    // query again when we're parented.
    *x = *y = 0;
    return;
  }
  // Translate the coordinate from widget to root window.
  gtk_widget_translate_coordinates(widget, root, 0, 0, x, y);
  // Then adjust the position with the position of the root window.
  int window_x, window_y;
  gtk_window_get_position(GTK_WINDOW(root), &window_x, &window_y);
  *x += window_x;
  *y += window_y;
}

// "expose-event" handler of drag icon widget that renders drag image pixbuf.
static gboolean DragIconWidgetPaint(GtkWidget* widget,
                                    GdkEventExpose* event,
                                    gpointer data) {
  GdkPixbuf* pixbuf = reinterpret_cast<GdkPixbuf*>(data);

  cairo_t* cr = gdk_cairo_create(widget->window);

  gdk_cairo_region(cr, event->region);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  gdk_cairo_set_source_pixbuf(cr, pixbuf, 0.0, 0.0);
  cairo_paint(cr);

  cairo_destroy(cr);
  return true;
}

// Creates a drag icon widget that draws drag_image.
static GtkWidget* CreateDragIconWidget(GdkPixbuf* drag_image) {
  GdkColormap* rgba_colormap =
      gdk_screen_get_rgba_colormap(gdk_screen_get_default());
  if (!rgba_colormap)
    return NULL;

  GtkWidget* drag_widget = gtk_window_new(GTK_WINDOW_POPUP);

  gtk_widget_set_colormap(drag_widget, rgba_colormap);
  gtk_widget_set_app_paintable(drag_widget, true);
  gtk_widget_set_size_request(drag_widget,
                              gdk_pixbuf_get_width(drag_image),
                              gdk_pixbuf_get_height(drag_image));

  g_signal_connect(G_OBJECT(drag_widget), "expose-event",
                   G_CALLBACK(&DragIconWidgetPaint), drag_image);
  return drag_widget;
}

// static
GtkWidget* WidgetGtk::null_parent_ = NULL;
bool WidgetGtk::debug_paint_enabled_ = false;

////////////////////////////////////////////////////////////////////////////////
// WidgetGtk, public:

WidgetGtk::WidgetGtk(Type type)
    : is_window_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(delegate_(this)),
      type_(type),
      widget_(NULL),
      window_contents_(NULL),
      focus_manager_(NULL),
      is_mouse_down_(false),
      has_capture_(false),
      last_mouse_event_was_move_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_widget_factory_(this)),
      delete_on_destroy_(true),
      transparent_(false),
      ignore_events_(false),
      ignore_drag_leave_(false),
      opacity_(255),
      drag_data_(NULL),
      is_active_(false),
      transient_to_parent_(false),
      got_initial_focus_in_(false),
      has_focus_(false),
      always_on_top_(false),
      is_double_buffered_(false),
      should_handle_menu_key_release_(false),
      dragged_view_(NULL) {
  set_native_widget(this);
  static bool installed_message_loop_observer = false;
  if (!installed_message_loop_observer) {
    installed_message_loop_observer = true;
    MessageLoopForUI* loop = MessageLoopForUI::current();
    if (loop)
      loop->AddObserver(DropObserver::GetInstance());
  }

  if (type_ != TYPE_CHILD)
    focus_manager_ = new FocusManager(this);
}

WidgetGtk::~WidgetGtk() {
  DestroyRootView();
  DCHECK(delete_on_destroy_ || widget_ == NULL);
  if (type_ != TYPE_CHILD)
    ActiveWindowWatcherX::RemoveObserver(this);

  // Defer focus manager's destruction. This is for the case when the
  // focus manager is referenced by a child WidgetGtk (e.g. TabbedPane in a
  // dialog). When gtk_widget_destroy is called on the parent, the destroy
  // signal reaches parent first and then the child. Thus causing the parent
  // WidgetGtk's dtor executed before the child's. If child's view hierarchy
  // references this focus manager, it crashes. This will defer focus manager's
  // destruction after child WidgetGtk's dtor.
  if (focus_manager_)
    MessageLoop::current()->DeleteSoon(FROM_HERE, focus_manager_);
}

GtkWindow* WidgetGtk::GetTransientParent() const {
  return (type_ != TYPE_CHILD && widget_) ?
      gtk_window_get_transient_for(GTK_WINDOW(widget_)) : NULL;
}

bool WidgetGtk::MakeTransparent() {
  // Transparency can only be enabled only if we haven't realized the widget.
  DCHECK(!widget_);

  if (!gdk_screen_is_composited(gdk_screen_get_default())) {
    // Transparency is only supported for compositing window managers.
    // NOTE: there's a race during ChromeOS startup such that X might think
    // compositing isn't supported. We ignore it if the wm says compositing
    // isn't supported.
    DLOG(WARNING) << "compositing not supported; allowing anyway";
  }

  if (!gdk_screen_get_rgba_colormap(gdk_screen_get_default())) {
    // We need rgba to make the window transparent.
    return false;
  }

  transparent_ = true;
  return true;
}

void WidgetGtk::EnableDoubleBuffer(bool enabled) {
  is_double_buffered_ = enabled;
  if (window_contents_) {
    if (is_double_buffered_)
      GTK_WIDGET_SET_FLAGS(window_contents_, GTK_DOUBLE_BUFFERED);
    else
      GTK_WIDGET_UNSET_FLAGS(window_contents_, GTK_DOUBLE_BUFFERED);
  }
}

bool WidgetGtk::MakeIgnoreEvents() {
  // Transparency can only be enabled for windows/popups and only if we haven't
  // realized the widget.
  DCHECK(!widget_ && type_ != TYPE_CHILD);

  ignore_events_ = true;
  return true;
}

void WidgetGtk::AddChild(GtkWidget* child) {
  gtk_container_add(GTK_CONTAINER(window_contents_), child);
}

void WidgetGtk::RemoveChild(GtkWidget* child) {
  // We can be called after the contents widget has been destroyed, e.g. any
  // NativeViewHost not removed from the view hierarchy before the window is
  // closed.
  if (GTK_IS_CONTAINER(window_contents_)) {
    gtk_container_remove(GTK_CONTAINER(window_contents_), child);
    gtk_views_fixed_set_widget_size(child, 0, 0);
  }
}

void WidgetGtk::ReparentChild(GtkWidget* child) {
  gtk_widget_reparent(child, window_contents_);
}

void WidgetGtk::PositionChild(GtkWidget* child, int x, int y, int w, int h) {
  gtk_views_fixed_set_widget_size(child, w, h);
  gtk_fixed_move(GTK_FIXED(window_contents_), child, x, y);
}

void WidgetGtk::DoDrag(const OSExchangeData& data, int operation) {
  const OSExchangeDataProviderGtk& data_provider =
      static_cast<const OSExchangeDataProviderGtk&>(data.provider());
  GtkTargetList* targets = data_provider.GetTargetList();
  GdkEvent* current_event = gtk_get_current_event();
  const OSExchangeDataProviderGtk& provider(
      static_cast<const OSExchangeDataProviderGtk&>(data.provider()));

  GdkDragContext* context = gtk_drag_begin(
      window_contents_,
      targets,
      static_cast<GdkDragAction>(
          ui::DragDropTypes::DragOperationToGdkDragAction(operation)),
      1,
      current_event);

  GtkWidget* drag_icon_widget = NULL;

  // Set the drag image if one was supplied.
  if (provider.drag_image()) {
    drag_icon_widget = CreateDragIconWidget(provider.drag_image());
    if (drag_icon_widget) {
      // Use a widget as the drag icon when compositing is enabled for proper
      // transparency handling.
      g_object_ref(provider.drag_image());
      gtk_drag_set_icon_widget(context,
                               drag_icon_widget,
                               provider.cursor_offset().x(),
                               provider.cursor_offset().y());
    } else {
      gtk_drag_set_icon_pixbuf(context,
                               provider.drag_image(),
                               provider.cursor_offset().x(),
                               provider.cursor_offset().y());
    }
  }

  if (current_event)
    gdk_event_free(current_event);
  gtk_target_list_unref(targets);

  drag_data_ = &data_provider;

  // Block the caller until drag is done by running a nested message loop.
  MessageLoopForUI::current()->Run(NULL);

  drag_data_ = NULL;

  if (drag_icon_widget) {
    gtk_widget_destroy(drag_icon_widget);
    g_object_unref(provider.drag_image());
  }
}

void WidgetGtk::IsActiveChanged() {
  if (GetWidgetDelegate())
    GetWidgetDelegate()->IsActiveChanged(IsActive());
}

// static
WidgetGtk* WidgetGtk::GetViewForNative(GtkWidget* widget) {
  return static_cast<WidgetGtk*>(GetWidgetFromNativeView(widget));
}

void WidgetGtk::ResetDropTarget() {
  ignore_drag_leave_ = false;
  drop_target_.reset(NULL);
}

// static
RootView* WidgetGtk::GetRootViewForWidget(GtkWidget* widget) {
  gpointer user_data = g_object_get_data(G_OBJECT(widget), "root-view");
  return static_cast<RootView*>(user_data);
}

void WidgetGtk::GetRequestedSize(gfx::Size* out) const {
  int width, height;
  if (GTK_IS_VIEWS_FIXED(widget_) &&
      gtk_views_fixed_get_widget_size(GetNativeView(), &width, &height)) {
    out->SetSize(width, height);
  } else {
    GtkRequisition requisition;
    gtk_widget_get_child_requisition(GetNativeView(), &requisition);
    out->SetSize(requisition.width, requisition.height);
  }
}

////////////////////////////////////////////////////////////////////////////////
// WidgetGtk, ActiveWindowWatcherX::Observer implementation:

void WidgetGtk::ActiveWindowChanged(GdkWindow* active_window) {
  if (!GetNativeView())
    return;

  bool was_active = IsActive();
  is_active_ = (active_window == GTK_WIDGET(GetNativeView())->window);
  if (!is_active_ && active_window && type_ != TYPE_CHILD) {
    // We're not active, but the force the window to be rendered as active if
    // a child window is transient to us.
    gpointer data = NULL;
    gdk_window_get_user_data(active_window, &data);
    GtkWidget* widget = reinterpret_cast<GtkWidget*>(data);
    is_active_ =
        (widget && GTK_IS_WINDOW(widget) &&
         gtk_window_get_transient_for(GTK_WINDOW(widget)) == GTK_WINDOW(
             widget_));
  }
  if (was_active != IsActive())
    IsActiveChanged();
}

////////////////////////////////////////////////////////////////////////////////
// WidgetGtk, Widget implementation:

void WidgetGtk::InitWithWidget(Widget* parent,
                               const gfx::Rect& bounds) {
  WidgetGtk* parent_gtk = static_cast<WidgetGtk*>(parent);
  GtkWidget* native_parent = NULL;
  if (parent != NULL) {
    if (type_ != TYPE_CHILD) {
      // window's parent has to be window.
      native_parent = parent_gtk->GetNativeView();
    } else {
      native_parent = parent_gtk->window_contents();
    }
  }
  Init(native_parent, bounds);
}

void WidgetGtk::Init(GtkWidget* parent,
                     const gfx::Rect& bounds) {
  Widget::Init(parent, bounds);
  if (type_ != TYPE_CHILD)
    ActiveWindowWatcherX::AddObserver(this);

  // Make container here.
  CreateGtkWidget(parent, bounds);

  if (opacity_ != 255)
    SetOpacity(opacity_);

  // Make sure we receive our motion events.

  // In general we register most events on the parent of all widgets. At a
  // minimum we need painting to happen on the parent (otherwise painting
  // doesn't work at all), and similarly we need mouse release events on the
  // parent as windows don't get mouse releases.
  gtk_widget_add_events(window_contents_,
                        GDK_ENTER_NOTIFY_MASK |
                        GDK_LEAVE_NOTIFY_MASK |
                        GDK_BUTTON_PRESS_MASK |
                        GDK_BUTTON_RELEASE_MASK |
                        GDK_POINTER_MOTION_MASK |
                        GDK_KEY_PRESS_MASK |
                        GDK_KEY_RELEASE_MASK);
  SetRootViewForWidget(widget_, GetRootView());

  g_signal_connect_after(G_OBJECT(window_contents_), "size_request",
                         G_CALLBACK(&OnSizeRequestThunk), this);
  g_signal_connect_after(G_OBJECT(window_contents_), "size_allocate",
                         G_CALLBACK(&OnSizeAllocateThunk), this);
  gtk_widget_set_app_paintable(window_contents_, true);
  g_signal_connect(window_contents_, "expose_event",
                   G_CALLBACK(&OnPaintThunk), this);
  g_signal_connect(window_contents_, "enter_notify_event",
                   G_CALLBACK(&OnEnterNotifyThunk), this);
  g_signal_connect(window_contents_, "leave_notify_event",
                   G_CALLBACK(&OnLeaveNotifyThunk), this);
  g_signal_connect(window_contents_, "motion_notify_event",
                   G_CALLBACK(&OnMotionNotifyThunk), this);
  g_signal_connect(window_contents_, "button_press_event",
                   G_CALLBACK(&OnButtonPressThunk), this);
  g_signal_connect(window_contents_, "button_release_event",
                   G_CALLBACK(&OnButtonReleaseThunk), this);
  g_signal_connect(window_contents_, "grab_broken_event",
                   G_CALLBACK(&OnGrabBrokeEventThunk), this);
  g_signal_connect(window_contents_, "grab_notify",
                   G_CALLBACK(&OnGrabNotifyThunk), this);
  g_signal_connect(window_contents_, "scroll_event",
                   G_CALLBACK(&OnScrollThunk), this);
  g_signal_connect(window_contents_, "visibility_notify_event",
                   G_CALLBACK(&OnVisibilityNotifyThunk), this);

  // In order to receive notification when the window is no longer the front
  // window, we need to install these on the widget.
  // NOTE: this doesn't work with focus follows mouse.
  g_signal_connect(widget_, "focus_in_event",
                   G_CALLBACK(&OnFocusInThunk), this);
  g_signal_connect(widget_, "focus_out_event",
                   G_CALLBACK(&OnFocusOutThunk), this);
  g_signal_connect(widget_, "destroy",
                   G_CALLBACK(&OnDestroyThunk), this);
  g_signal_connect(widget_, "show",
                   G_CALLBACK(&OnShowThunk), this);
  g_signal_connect(widget_, "hide",
                   G_CALLBACK(&OnHideThunk), this);

  // Views/FocusManager (re)sets the focus to the root window,
  // so we need to connect signal handlers to the gtk window.
  // See views::Views::Focus and views::FocusManager::ClearNativeFocus
  // for more details.
  g_signal_connect(widget_, "key_press_event",
                   G_CALLBACK(&OnKeyEventThunk), this);
  g_signal_connect(widget_, "key_release_event",
                   G_CALLBACK(&OnKeyEventThunk), this);

  // Drag and drop.
  gtk_drag_dest_set(window_contents_, static_cast<GtkDestDefaults>(0),
                    NULL, 0, GDK_ACTION_COPY);
  g_signal_connect(window_contents_, "drag_motion",
                   G_CALLBACK(&OnDragMotionThunk), this);
  g_signal_connect(window_contents_, "drag_data_received",
                   G_CALLBACK(&OnDragDataReceivedThunk), this);
  g_signal_connect(window_contents_, "drag_drop",
                   G_CALLBACK(&OnDragDropThunk), this);
  g_signal_connect(window_contents_, "drag_leave",
                   G_CALLBACK(&OnDragLeaveThunk), this);
  g_signal_connect(window_contents_, "drag_data_get",
                   G_CALLBACK(&OnDragDataGetThunk), this);
  g_signal_connect(window_contents_, "drag_end",
                   G_CALLBACK(&OnDragEndThunk), this);
  g_signal_connect(window_contents_, "drag_failed",
                   G_CALLBACK(&OnDragFailedThunk), this);

  tooltip_manager_.reset(new TooltipManagerGtk(this));

  // Register for tooltips.
  g_object_set(G_OBJECT(window_contents_), "has-tooltip", TRUE, NULL);
  g_signal_connect(window_contents_, "query_tooltip",
                   G_CALLBACK(&OnQueryTooltipThunk), this);

  if (type_ == TYPE_CHILD) {
    if (parent) {
      SetBounds(bounds);
    }
  } else {
    if (bounds.width() > 0 && bounds.height() > 0)
      gtk_window_resize(GTK_WINDOW(widget_), bounds.width(), bounds.height());
    gtk_window_move(GTK_WINDOW(widget_), bounds.x(), bounds.y());
  }
}

void WidgetGtk::GetBounds(gfx::Rect* out, bool including_frame) const {
  if (!widget_) {
    // Due to timing we can get a request for the bounds after Close.
    *out = gfx::Rect(gfx::Point(0, 0), size_);
    return;
  }

  int x = 0, y = 0, w, h;
  if (GTK_IS_WINDOW(widget_)) {
    gtk_window_get_position(GTK_WINDOW(widget_), &x, &y);
    // NOTE: this doesn't include frame decorations, but it should be good
    // enough for our uses.
    gtk_window_get_size(GTK_WINDOW(widget_), &w, &h);
  } else {
    GetWidgetPositionOnScreen(widget_, &x, &y);
    w = widget_->allocation.width;
    h = widget_->allocation.height;
  }

  return out->SetRect(x, y, w, h);
}

void WidgetGtk::SetBounds(const gfx::Rect& bounds) {
  if (type_ == TYPE_CHILD) {
    GtkWidget* parent = gtk_widget_get_parent(widget_);
    if (GTK_IS_VIEWS_FIXED(parent)) {
      WidgetGtk* parent_widget = GetViewForNative(parent);
      parent_widget->PositionChild(widget_, bounds.x(), bounds.y(),
                                   bounds.width(), bounds.height());
    } else {
      DCHECK(GTK_IS_FIXED(parent))
          << "Parent of WidgetGtk has to be Fixed or ViewsFixed";
      // Just request the size if the parent is not WidgetGtk but plain
      // GtkFixed. WidgetGtk does not know the minimum size so we assume
      // the caller of the SetBounds knows exactly how big it wants to be.
      gtk_widget_set_size_request(widget_, bounds.width(), bounds.height());
      if (parent != null_parent_)
        gtk_fixed_move(GTK_FIXED(parent), widget_, bounds.x(), bounds.y());
    }
  } else {
    if (GTK_WIDGET_MAPPED(widget_)) {
      // If the widget is mapped (on screen), we can move and resize with one
      // call, which avoids two separate window manager steps.
      gdk_window_move_resize(widget_->window, bounds.x(), bounds.y(),
                             bounds.width(), bounds.height());
    }

    // Always call gtk_window_move and gtk_window_resize so that GtkWindow's
    // geometry info is up-to-date.
    GtkWindow* gtk_window = GTK_WINDOW(widget_);
    // TODO: this may need to set an initial size if not showing.
    // TODO: need to constrain based on screen size.
    if (!bounds.IsEmpty()) {
      gtk_window_resize(gtk_window, bounds.width(), bounds.height());
    }
    gtk_window_move(gtk_window, bounds.x(), bounds.y());
  }
}

void WidgetGtk::MoveAbove(Widget* widget) {
  DCHECK(widget_);
  DCHECK(widget_->window);
  // TODO(oshima): gdk_window_restack is not available in gtk2.0, so
  // we're simply raising the window to the top. We should switch to
  // gdk_window_restack when we upgrade gtk to 2.18 or up.
  gdk_window_raise(widget_->window);
}

void WidgetGtk::SetShape(gfx::NativeRegion region) {
  DCHECK(widget_);
  DCHECK(widget_->window);
  gdk_window_shape_combine_region(widget_->window, region, 0, 0);
  gdk_region_destroy(region);
}

void WidgetGtk::Close() {
  if (!widget_)
    return;  // No need to do anything.

  // Hide first.
  Hide();
  if (close_widget_factory_.empty()) {
    // And we delay the close just in case we're on the stack.
    MessageLoop::current()->PostTask(FROM_HERE,
        close_widget_factory_.NewRunnableMethod(
            &WidgetGtk::CloseNow));
  }
}

void WidgetGtk::CloseNow() {
  if (widget_) {
    gtk_widget_destroy(widget_);  // Triggers OnDestroy().
  }
}

void WidgetGtk::Show() {
  if (widget_) {
    gtk_widget_show(widget_);
    if (widget_->window)
      gdk_window_raise(widget_->window);
  }
}

void WidgetGtk::Hide() {
  if (widget_) {
    gtk_widget_hide(widget_);
    if (widget_->window)
      gdk_window_lower(widget_->window);
  }
}

gfx::NativeView WidgetGtk::GetNativeView() const {
  return widget_;
}

void WidgetGtk::SetOpacity(unsigned char opacity) {
  opacity_ = opacity;
  if (widget_) {
    // We can only set the opacity when the widget has been realized.
    gdk_window_set_opacity(widget_->window, static_cast<gdouble>(opacity) /
                           static_cast<gdouble>(255));
  }
}

void WidgetGtk::SetAlwaysOnTop(bool on_top) {
  DCHECK(type_ != TYPE_CHILD);
  always_on_top_ = on_top;
  if (widget_)
    gtk_window_set_keep_above(GTK_WINDOW(widget_), on_top);
}

Widget* WidgetGtk::GetRootWidget() const {
  GtkWidget* parent = widget_;
  GtkWidget* last_parent = parent;
  while (parent) {
    last_parent = parent;
    parent = gtk_widget_get_parent(parent);
  }
  return last_parent ? GetViewForNative(last_parent) : NULL;
}

bool WidgetGtk::IsVisible() const {
  return GTK_WIDGET_VISIBLE(widget_);
}

bool WidgetGtk::IsActive() const {
  DCHECK(type_ != TYPE_CHILD);
  return is_active_;
}

bool WidgetGtk::IsAccessibleWidget() const {
  return false;
}

void WidgetGtk::GenerateMousePressedForView(View* view,
                                            const gfx::Point& point) {
  NOTIMPLEMENTED();
}

TooltipManager* WidgetGtk::GetTooltipManager() {
  return tooltip_manager_.get();
}

bool WidgetGtk::GetAccelerator(int cmd_id, ui::Accelerator* accelerator) {
  NOTIMPLEMENTED();
  return false;
}

Window* WidgetGtk::GetWindow() {
  return GetWindowImpl(widget_);
}

const Window* WidgetGtk::GetWindow() const {
  return GetWindowImpl(widget_);
}

void WidgetGtk::SetNativeWindowProperty(const char* name, void* value) {
  g_object_set_data(G_OBJECT(widget_), name, value);
}

void* WidgetGtk::GetNativeWindowProperty(const char* name) {
  return g_object_get_data(G_OBJECT(widget_), name);
}

ThemeProvider* WidgetGtk::GetThemeProvider() const {
  return GetWidgetThemeProvider(this);
}

FocusManager* WidgetGtk::GetFocusManager() {
  if (focus_manager_)
    return focus_manager_;

  Widget* root = GetRootWidget();
  if (root && root != this) {
    // Widget subclasses may override GetFocusManager(), for example for
    // dealing with cases where the widget has been unparented.
    return root->GetFocusManager();
  }
  return NULL;
}

void WidgetGtk::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (drop_target_.get())
    drop_target_->ResetTargetViewIfEquals(child);

  if (!is_add) {
    if (child == dragged_view_)
      dragged_view_ = NULL;

    FocusManager* focus_manager = GetFocusManager();
    if (focus_manager) {
      if (focus_manager->GetFocusedView() == child)
        focus_manager->SetFocusedView(NULL);
      focus_manager->ViewRemoved(parent, child);
    }
    ViewStorage::GetInstance()->ViewRemoved(parent, child);
  }
}

bool WidgetGtk::ContainsNativeView(gfx::NativeView native_view) {
  // TODO(port)  See implementation in WidgetWin::ContainsNativeView.
  NOTREACHED() << "WidgetGtk::ContainsNativeView is not implemented.";
  return false;
}

void WidgetGtk::StartDragForViewFromMouseEvent(
    View* view,
    const OSExchangeData& data,
    int operation) {
  // NOTE: view may be null.
  dragged_view_ = view;
  DoDrag(data, operation);
  // If the view is removed during the drag operation, dragged_view_ is set to
  // NULL.
  if (view && dragged_view_ == view) {
    dragged_view_ = NULL;
    view->OnDragDone();
  }
}

View* WidgetGtk::GetDraggedView() {
  return dragged_view_;
}

void WidgetGtk::SchedulePaintInRect(const gfx::Rect& rect) {
  if (widget_ && GTK_WIDGET_DRAWABLE(widget_)) {
    gtk_widget_queue_draw_area(widget_, rect.x(), rect.y(), rect.width(),
                               rect.height());
  }
}

void WidgetGtk::SetCursor(gfx::NativeCursor cursor) {
#if defined(TOUCH_UI) && defined(HAVE_XINPUT2)
  if (!TouchFactory::GetInstance()->is_cursor_visible())
    cursor = gfx::GetCursor(GDK_BLANK_CURSOR);
#endif
  if (widget_)
    gdk_window_set_cursor(widget_->window, cursor);
}

void WidgetGtk::ClearNativeFocus() {
  DCHECK(type_ != TYPE_CHILD);
  if (!GetNativeView()) {
    NOTREACHED();
    return;
  }
  gtk_window_set_focus(GTK_WINDOW(GetNativeView()), NULL);
}

bool WidgetGtk::HandleKeyboardEvent(GdkEventKey* event) {
  if (!focus_manager_)
    return false;

  KeyEvent key(reinterpret_cast<NativeEvent>(event));
  int key_code = key.key_code();
  bool handled = false;

  // Always reset |should_handle_menu_key_release_| unless we are handling a
  // VKEY_MENU key release event. It ensures that VKEY_MENU accelerator can only
  // be activated when handling a VKEY_MENU key release event which is preceded
  // by an unhandled VKEY_MENU key press event.
  if (key_code != ui::VKEY_MENU || event->type != GDK_KEY_RELEASE)
    should_handle_menu_key_release_ = false;

  if (event->type == GDK_KEY_PRESS) {
    // VKEY_MENU is triggered by key release event.
    // FocusManager::OnKeyEvent() returns false when the key has been consumed.
    if (key_code != ui::VKEY_MENU)
      handled = !focus_manager_->OnKeyEvent(key);
    else
      should_handle_menu_key_release_ = true;
  } else if (key_code == ui::VKEY_MENU && should_handle_menu_key_release_ &&
             (key.flags() & ~ui::EF_ALT_DOWN) == 0) {
    // Trigger VKEY_MENU when only this key is pressed and released, and both
    // press and release events are not handled by others.
    Accelerator accelerator(ui::VKEY_MENU, false, false, false);
    handled = focus_manager_->ProcessAccelerator(accelerator);
  }

  return handled;
}

// static
int WidgetGtk::GetFlagsForEventButton(const GdkEventButton& event) {
  int flags = Event::GetFlagsFromGdkState(event.state);
  switch (event.button) {
    case 1:
      flags |= ui::EF_LEFT_BUTTON_DOWN;
      break;
    case 2:
      flags |= ui::EF_MIDDLE_BUTTON_DOWN;
      break;
    case 3:
      flags |= ui::EF_RIGHT_BUTTON_DOWN;
      break;
    default:
      // We only deal with 1-3.
      break;
  }
  if (event.type == GDK_2BUTTON_PRESS)
    flags |= ui::EF_IS_DOUBLE_CLICK;
  return flags;
}

// static
void WidgetGtk::EnableDebugPaint() {
  debug_paint_enabled_ = true;
}

////////////////////////////////////////////////////////////////////////////////
// WidgetGtk, protected:

void WidgetGtk::OnSizeRequest(GtkWidget* widget, GtkRequisition* requisition) {
  // Do only return the preferred size for child windows. GtkWindow interprets
  // the requisition as a minimum size for top level windows, returning a
  // preferred size for these would prevents us from setting smaller window
  // sizes.
  if (type_ == TYPE_CHILD) {
    gfx::Size size(GetRootView()->GetPreferredSize());
    requisition->width = size.width();
    requisition->height = size.height();
  }
}

void WidgetGtk::OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation) {
  // See comment next to size_ as to why we do this. Also note, it's tempting
  // to put this in the static method so subclasses don't need to worry about
  // it, but if a subclasses needs to set a shape then they need to always
  // reset the shape in this method regardless of whether the size changed.
  gfx::Size new_size(allocation->width, allocation->height);
  if (new_size == size_)
    return;
  size_ = new_size;
  GetRootView()->SetBounds(0, 0, allocation->width, allocation->height);
  GetRootView()->SchedulePaint();
}

gboolean WidgetGtk::OnPaint(GtkWidget* widget, GdkEventExpose* event) {
  if (transparent_ && type_ == TYPE_CHILD) {
    // Clear the background before drawing any view and native components.
    DrawTransparentBackground(widget, event);
    if (!CompositePainter::IsComposited(widget_) &&
        gdk_screen_is_composited(gdk_screen_get_default())) {
      // Let the parent draw the content only after something is drawn on
      // the widget.
      CompositePainter::SetComposited(widget_);
    }
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

  gfx::CanvasSkiaPaint canvas(event);
  if (!canvas.is_empty()) {
    canvas.set_composite_alpha(is_transparent());
    GetRootView()->Paint(&canvas);
  }
  return false;  // False indicates other widgets should get the event as well.
}

void WidgetGtk::OnDragDataGet(GtkWidget* widget,
                              GdkDragContext* context,
                              GtkSelectionData* data,
                              guint info,
                              guint time) {
  if (!drag_data_) {
    NOTREACHED();
    return;
  }
  drag_data_->WriteFormatToSelection(info, data);
}

void WidgetGtk::OnDragDataReceived(GtkWidget* widget,
                                   GdkDragContext* context,
                                   gint x,
                                   gint y,
                                   GtkSelectionData* data,
                                   guint info,
                                   guint time) {
  if (drop_target_.get())
    drop_target_->OnDragDataReceived(context, x, y, data, info, time);
}

gboolean WidgetGtk::OnDragDrop(GtkWidget* widget,
                               GdkDragContext* context,
                               gint x,
                               gint y,
                               guint time) {
  if (drop_target_.get()) {
    return drop_target_->OnDragDrop(context, x, y, time);
  }
  return FALSE;
}

void WidgetGtk::OnDragEnd(GtkWidget* widget, GdkDragContext* context) {
  if (!drag_data_) {
    // This indicates we didn't start a drag operation, and should never
    // happen.
    NOTREACHED();
    return;
  }
  // Quit the nested message loop we spawned in DoDrag.
  MessageLoop::current()->Quit();
}

gboolean WidgetGtk::OnDragFailed(GtkWidget* widget,
                                 GdkDragContext* context,
                                 GtkDragResult result) {
  return FALSE;
}

void WidgetGtk::OnDragLeave(GtkWidget* widget,
                            GdkDragContext* context,
                            guint time) {
  if (ignore_drag_leave_) {
    ignore_drag_leave_ = false;
    return;
  }
  if (drop_target_.get()) {
    drop_target_->OnDragLeave(context, time);
    drop_target_.reset(NULL);
  }
}

gboolean WidgetGtk::OnDragMotion(GtkWidget* widget,
                                 GdkDragContext* context,
                                 gint x,
                                 gint y,
                                 guint time) {
  if (!drop_target_.get())
    drop_target_.reset(new DropTargetGtk(GetRootView(), context));
  return drop_target_->OnDragMotion(context, x, y, time);
}

gboolean WidgetGtk::OnEnterNotify(GtkWidget* widget, GdkEventCrossing* event) {
  if (last_mouse_event_was_move_ && last_mouse_move_x_ == event->x_root &&
      last_mouse_move_y_ == event->y_root) {
    // Don't generate a mouse event for the same location as the last.
    return false;
  }

  if (has_capture_ && event->mode == GDK_CROSSING_GRAB) {
    // Doing a grab results an async enter event, regardless of where the mouse
    // is. We don't want to generate a mouse move in this case.
    return false;
  }

  if (!last_mouse_event_was_move_ && !is_mouse_down_) {
    // When a mouse button is pressed gtk generates a leave, enter, press.
    // RootView expects to get a mouse move before a press, otherwise enter is
    // not set. So we generate a move here.
    last_mouse_move_x_ = event->x_root;
    last_mouse_move_y_ = event->y_root;
    last_mouse_event_was_move_ = true;

    int x = 0, y = 0;
    GetContainedWidgetEventCoordinates(event, &x, &y);

    // If this event is the result of pressing a button then one of the button
    // modifiers is set. Unset it as we're compensating for the leave generated
    // when you press a button.
    int flags = (Event::GetFlagsFromGdkState(event->state) &
                 ~(ui::EF_LEFT_BUTTON_DOWN |
                   ui::EF_MIDDLE_BUTTON_DOWN |
                   ui::EF_RIGHT_BUTTON_DOWN));
    MouseEvent mouse_move(ui::ET_MOUSE_MOVED, x, y, flags);
    GetRootView()->OnMouseMoved(mouse_move);
  }

  return false;
}

gboolean WidgetGtk::OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event) {
  last_mouse_event_was_move_ = false;
  if (!has_capture_ && !is_mouse_down_)
    GetRootView()->ProcessOnMouseExited();
  return false;
}

gboolean WidgetGtk::OnMotionNotify(GtkWidget* widget, GdkEventMotion* event) {
  int x = 0, y = 0;
  GetContainedWidgetEventCoordinates(event, &x, &y);

  if (has_capture_ && is_mouse_down_) {
    last_mouse_event_was_move_ = false;
    int flags = Event::GetFlagsFromGdkState(event->state);
    MouseEvent mouse_drag(ui::ET_MOUSE_DRAGGED, x, y, flags);
    GetRootView()->OnMouseDragged(mouse_drag);
    return true;
  }
  gfx::Point screen_loc(event->x_root, event->y_root);
  if (last_mouse_event_was_move_ && last_mouse_move_x_ == screen_loc.x() &&
      last_mouse_move_y_ == screen_loc.y()) {
    // Don't generate a mouse event for the same location as the last.
    return true;
  }
  last_mouse_move_x_ = screen_loc.x();
  last_mouse_move_y_ = screen_loc.y();
  last_mouse_event_was_move_ = true;
  int flags = Event::GetFlagsFromGdkState(event->state);
  MouseEvent mouse_move(ui::ET_MOUSE_MOVED, x, y, flags);
  GetRootView()->OnMouseMoved(mouse_move);
  return true;
}

gboolean WidgetGtk::OnButtonPress(GtkWidget* widget, GdkEventButton* event) {
  return ProcessMousePressed(event);
}

gboolean WidgetGtk::OnButtonRelease(GtkWidget* widget, GdkEventButton* event) {
  ProcessMouseReleased(event);
  return true;
}

gboolean WidgetGtk::OnScroll(GtkWidget* widget, GdkEventScroll* event) {
  return ProcessScroll(event);
}

gboolean WidgetGtk::OnFocusIn(GtkWidget* widget, GdkEventFocus* event) {
  if (has_focus_)
    return false;  // This is the second focus-in event in a row, ignore it.
  has_focus_ = true;

  should_handle_menu_key_release_ = false;

  if (type_ == TYPE_CHILD)
    return false;

  // See description of got_initial_focus_in_ for details on this.
  if (!got_initial_focus_in_) {
    got_initial_focus_in_ = true;
    SetInitialFocus();
  } else {
    focus_manager_->RestoreFocusedView();
  }
  return false;
}

gboolean WidgetGtk::OnFocusOut(GtkWidget* widget, GdkEventFocus* event) {
  if (!has_focus_)
    return false;  // This is the second focus-out event in a row, ignore it.
  has_focus_ = false;

  if (type_ == TYPE_CHILD)
    return false;

  // The top-level window lost focus, store the focused view.
  focus_manager_->StoreFocusedView();
  return false;
}

gboolean WidgetGtk::OnKeyEvent(GtkWidget* widget, GdkEventKey* event) {
  KeyEvent key(reinterpret_cast<NativeEvent>(event));

  // Always reset |should_handle_menu_key_release_| unless we are handling a
  // VKEY_MENU key release event. It ensures that VKEY_MENU accelerator can only
  // be activated when handling a VKEY_MENU key release event which is preceded
  // by an unhandled VKEY_MENU key press event. See also HandleKeyboardEvent().
  if (key.key_code() != ui::VKEY_MENU || event->type != GDK_KEY_RELEASE)
    should_handle_menu_key_release_ = false;

  bool handled = false;

  // Dispatch the key event to View hierarchy first.
  handled = GetRootView()->ProcessKeyEvent(key);

  // Dispatch the key event to native GtkWidget hierarchy.
  // To prevent GtkWindow from handling the key event as a keybinding, we need
  // to bypass GtkWindow's default key event handler and dispatch the event
  // here.
  if (!handled && GTK_IS_WINDOW(widget))
    handled = gtk_window_propagate_key_event(GTK_WINDOW(widget), event);

  // On Linux, in order to handle VKEY_MENU (Alt) accelerator key correctly and
  // avoid issues like: http://crbug.com/40966 and http://crbug.com/49701, we
  // should only send the key event to the focus manager if it's not handled by
  // any View or native GtkWidget.
  // The flow is different when the focus is in a RenderWidgetHostViewGtk, which
  // always consumes the key event and send it back to us later by calling
  // HandleKeyboardEvent() directly, if it's not handled by webkit.
  if (!handled)
    handled = HandleKeyboardEvent(event);

  // Dispatch the key event for bindings processing.
  if (!handled && GTK_IS_WINDOW(widget))
    handled = gtk_bindings_activate_event(GTK_OBJECT(widget), event);

  // Always return true for toplevel window to prevents GtkWindow's default key
  // event handler.
  return GTK_IS_WINDOW(widget) ? true : handled;
}

gboolean WidgetGtk::OnQueryTooltip(GtkWidget* widget,
                                   gint x,
                                   gint y,
                                   gboolean keyboard_mode,
                                   GtkTooltip* tooltip) {
  return tooltip_manager_->ShowTooltip(x, y, keyboard_mode, tooltip);
}

gboolean WidgetGtk::OnVisibilityNotify(GtkWidget* widget,
                                       GdkEventVisibility* event) {
  return false;
}

gboolean WidgetGtk::OnGrabBrokeEvent(GtkWidget* widget, GdkEvent* event) {
  HandleGrabBroke();
  return false;  // To let other widgets get the event.
}

void WidgetGtk::OnGrabNotify(GtkWidget* widget, gboolean was_grabbed) {
  if (!window_contents_)
    return;  // Grab broke after window destroyed, don't try processing it.

  gtk_grab_remove(window_contents_);
  HandleGrabBroke();
}

void WidgetGtk::OnDestroy(GtkWidget* object) {
  // Note that this handler is hooked to GtkObject::destroy.
  // NULL out pointers here since we might still be in an observerer list
  // until delstion happens.
  widget_ = window_contents_ = NULL;
  if (delete_on_destroy_) {
    // Delays the deletion of this WidgetGtk as we want its children to have
    // access to it when destroyed.
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }
}

void WidgetGtk::OnShow(GtkWidget* widget) {
}

void WidgetGtk::OnHide(GtkWidget* widget) {
}

void WidgetGtk::DoGrab() {
  has_capture_ = true;
  gtk_grab_add(window_contents_);
}

void WidgetGtk::ReleaseGrab() {
  if (has_capture_) {
    has_capture_ = false;
    gtk_grab_remove(window_contents_);
  }
}

void WidgetGtk::HandleGrabBroke() {
  if (has_capture_) {
    if (is_mouse_down_)
      GetRootView()->ProcessMouseDragCanceled();
    is_mouse_down_ = false;
    has_capture_ = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
// WidgetGtk, private:

RootView* WidgetGtk::CreateRootView() {
  return new RootView(this);
}

gboolean WidgetGtk::OnWindowPaint(GtkWidget* widget, GdkEventExpose* event) {
  // Clear the background to be totally transparent. We don't need to
  // paint the root view here as that is done by OnPaint.
  DCHECK(transparent_);
  DrawTransparentBackground(widget, event);
  return false;
}

bool WidgetGtk::ProcessMousePressed(GdkEventButton* event) {
  if (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS) {
    // The sequence for double clicks is press, release, press, 2press, release.
    // This means that at the time we get the second 'press' we don't know
    // whether it corresponds to a double click or not. For now we're completely
    // ignoring the 2press/3press events as they are duplicate. To make this
    // work right we need to write our own code that detects if the press is a
    // double/triple. For now we're completely punting, which means we always
    // get single clicks.
    // TODO: fix this.
    return true;
  }

  // An event may come from a contained widget which has its own gdk window.
  // Translate it to the widget's coordinates.
  int x = 0;
  int y = 0;
  GetContainedWidgetEventCoordinates(event, &x, &y);
  last_mouse_event_was_move_ = false;
  MouseEvent mouse_pressed(ui::ET_MOUSE_PRESSED, x, y,
                           GetFlagsForEventButton(*event));

  if (GetRootView()->OnMousePressed(mouse_pressed)) {
    is_mouse_down_ = true;
    if (!has_capture_)
      DoGrab();
    return true;
  }

  // Returns true to consume the event when widget is not transparent.
  return !transparent_;
}

void WidgetGtk::ProcessMouseReleased(GdkEventButton* event) {
  int x = 0, y = 0;
  GetContainedWidgetEventCoordinates(event, &x, &y);

  last_mouse_event_was_move_ = false;
  MouseEvent mouse_up(ui::ET_MOUSE_RELEASED, x, y,
                      GetFlagsForEventButton(*event));
  // Release the capture first, that way we don't get confused if
  // OnMouseReleased blocks.
  if (has_capture_ && ReleaseCaptureOnMouseReleased())
    ReleaseGrab();
  is_mouse_down_ = false;
  // GTK generates a mouse release at the end of dnd. We need to ignore it.
  if (!drag_data_)
    GetRootView()->OnMouseReleased(mouse_up, false);
}

bool WidgetGtk::ProcessScroll(GdkEventScroll* event) {
  // An event may come from a contained widget which has its own gdk window.
  // Translate it to the widget's coordinates.
  int x = 0, y = 0;
  GetContainedWidgetEventCoordinates(event, &x, &y);
  GdkEventScroll translated_event = *event;
  translated_event.x = x;
  translated_event.y = y;

  MouseWheelEvent wheel_event(reinterpret_cast<GdkEvent*>(&translated_event));
  return GetRootView()->OnMouseWheel(wheel_event);
}

// static
void WidgetGtk::SetRootViewForWidget(GtkWidget* widget, RootView* root_view) {
  g_object_set_data(G_OBJECT(widget), "root-view", root_view);
}

// static
Window* WidgetGtk::GetWindowImpl(GtkWidget* widget) {
  GtkWidget* parent = widget;
  while (parent) {
    WidgetGtk* widget_gtk = GetViewForNative(parent);
    if (widget_gtk && widget_gtk->is_window_)
      return static_cast<WindowGtk*>(widget_gtk);
    parent = gtk_widget_get_parent(parent);
  }
  return NULL;
}

void WidgetGtk::CreateGtkWidget(GtkWidget* parent, const gfx::Rect& bounds) {
  // We turn off double buffering for two reasons:
  // 1. We draw to a canvas then composite to the screen, which means we're
  //    doing our own double buffering already.
  // 2. GTKs double buffering clips to the dirty region. RootView occasionally
  //    needs to expand the paint region (see RootView::OnPaint). This means
  //    that if we use GTK's double buffering and we tried to expand the dirty
  //    region, it wouldn't get painted.
  if (type_ == TYPE_CHILD) {
    window_contents_ = widget_ = gtk_views_fixed_new();
    gtk_widget_set_name(widget_, "views-gtkwidget-child-fixed");
    if (!is_double_buffered_)
      GTK_WIDGET_UNSET_FLAGS(widget_, GTK_DOUBLE_BUFFERED);
    gtk_fixed_set_has_window(GTK_FIXED(widget_), true);
    if (!parent && !null_parent_) {
      GtkWidget* popup = gtk_window_new(GTK_WINDOW_POPUP);
      null_parent_ = gtk_fixed_new();
      gtk_widget_set_name(widget_, "views-gtkwidget-null-parent");
      gtk_container_add(GTK_CONTAINER(popup), null_parent_);
      gtk_widget_realize(null_parent_);
    }
    if (transparent_) {
      // transparency has to be configured before widget is realized.
      DCHECK(parent) << "Transparent widget must have parent when initialized";
      ConfigureWidgetForTransparentBackground(parent);
    }
    gtk_container_add(GTK_CONTAINER(parent ? parent : null_parent_), widget_);
    gtk_widget_realize(widget_);
    if (transparent_) {
      // The widget has to be realized to set composited flag.
      // I tried "realize" signal to set this flag, but it did not work
      // when the top level is popup.
      DCHECK(GTK_WIDGET_REALIZED(widget_));
      gdk_window_set_composited(widget_->window, true);
    }
    if (parent && !bounds.size().IsEmpty()) {
      // Make sure that an widget is given it's initial size before
      // we're done initializing, to take care of some potential
      // corner cases when programmatically arranging hierarchies as
      // seen in
      // http://code.google.com/p/chromium-os/issues/detail?id=5987

      // This can't be done without a parent present, or stale data
      // might show up on the screen as seen in
      // http://code.google.com/p/chromium/issues/detail?id=53870
      GtkAllocation alloc = { 0, 0, bounds.width(), bounds.height() };
      gtk_widget_size_allocate(widget_, &alloc);
    }
  } else {
    // Use our own window class to override GtkWindow's move_focus method.
    widget_ = gtk_views_window_new(
        (type_ == TYPE_WINDOW || type_ == TYPE_DECORATED_WINDOW) ?
        GTK_WINDOW_TOPLEVEL : GTK_WINDOW_POPUP);
    gtk_widget_set_name(widget_, "views-gtkwidget-window");
    if (transient_to_parent_)
      gtk_window_set_transient_for(GTK_WINDOW(widget_), GTK_WINDOW(parent));
    GTK_WIDGET_UNSET_FLAGS(widget_, GTK_DOUBLE_BUFFERED);

    // Gtk determines the size for windows based on the requested size of the
    // child. For WidgetGtk the child is a fixed. If the fixed ends up with a
    // child widget it's possible the child widget will drive the requested size
    // of the widget, which we don't want. We explicitly set a value of 1x1 here
    // so that gtk doesn't attempt to resize the window if we end up with a
    // situation where the requested size of a child of the fixed is greater
    // than the size of the window. By setting the size in this manner we're
    // also allowing users of WidgetGtk to change the requested size at any
    // time.
    gtk_widget_set_size_request(widget_, 1, 1);

    if (!bounds.size().IsEmpty()) {
      // When we realize the window, the window manager is given a size. If we
      // don't specify a size before then GTK defaults to 200x200. Specify
      // a size now so that the window manager sees the requested size.
      GtkAllocation alloc = { 0, 0, bounds.width(), bounds.height() };
      gtk_widget_size_allocate(widget_, &alloc);
    }
    if (type_ != TYPE_DECORATED_WINDOW) {
      gtk_window_set_decorated(GTK_WINDOW(widget_), false);
      // We'll take care of positioning our window.
      gtk_window_set_position(GTK_WINDOW(widget_), GTK_WIN_POS_NONE);
    }

    window_contents_ = gtk_views_fixed_new();
    gtk_widget_set_name(window_contents_, "views-gtkwidget-window-fixed");
    if (!is_double_buffered_)
      GTK_WIDGET_UNSET_FLAGS(window_contents_, GTK_DOUBLE_BUFFERED);
    gtk_fixed_set_has_window(GTK_FIXED(window_contents_), true);
    gtk_container_add(GTK_CONTAINER(widget_), window_contents_);
    gtk_widget_show(window_contents_);
    g_object_set_data(G_OBJECT(window_contents_), kWidgetKey,
                      static_cast<Widget*>(this));

    if (transparent_)
      ConfigureWidgetForTransparentBackground(NULL);

    if (ignore_events_)
      ConfigureWidgetForIgnoreEvents();

    SetAlwaysOnTop(always_on_top_);
    // The widget needs to be realized before handlers like size-allocate can
    // function properly.
    gtk_widget_realize(widget_);
  }
  // Setting the WidgetKey property to widget_, which is used by
  // GetWidgetFromNativeWindow.
  SetNativeWindowProperty(kWidgetKey, this);
}

void WidgetGtk::ConfigureWidgetForTransparentBackground(GtkWidget* parent) {
  DCHECK(widget_ && window_contents_);

  GdkColormap* rgba_colormap =
      gdk_screen_get_rgba_colormap(gtk_widget_get_screen(widget_));
  if (!rgba_colormap) {
    transparent_ = false;
    return;
  }
  // To make the background transparent we need to install the RGBA colormap
  // on both the window and fixed. In addition we need to make sure no
  // decorations are drawn. The last bit is to make sure the widget doesn't
  // attempt to draw a pixmap in it's background.
  if (type_ != TYPE_CHILD) {
    DCHECK(parent == NULL);
    gtk_widget_set_colormap(widget_, rgba_colormap);
    gtk_widget_set_app_paintable(widget_, true);
    g_signal_connect(widget_, "expose_event",
                     G_CALLBACK(&OnWindowPaintThunk), this);
    gtk_widget_realize(widget_);
    gdk_window_set_decorations(widget_->window,
                               static_cast<GdkWMDecoration>(0));
  } else {
    DCHECK(parent);
    CompositePainter::AddCompositePainter(parent);
  }
  DCHECK(!GTK_WIDGET_REALIZED(window_contents_));
  gtk_widget_set_colormap(window_contents_, rgba_colormap);
}

void WidgetGtk::ConfigureWidgetForIgnoreEvents() {
  gtk_widget_realize(widget_);
  GdkWindow* gdk_window = widget_->window;
  Display* display = GDK_WINDOW_XDISPLAY(gdk_window);
  XID win = GDK_WINDOW_XID(gdk_window);

  // This sets the clickable area to be empty, allowing all events to be
  // passed to any windows behind this one.
  XShapeCombineRectangles(
      display,
      win,
      ShapeInput,
      0,  // x offset
      0,  // y offset
      NULL,  // rectangles
      0,  // num rectangles
      ShapeSet,
      0);
}

void WidgetGtk::DrawTransparentBackground(GtkWidget* widget,
                                          GdkEventExpose* event) {
  cairo_t* cr = gdk_cairo_create(widget->window);
  cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
  gdk_cairo_region(cr, event->region);
  cairo_fill(cr);
  cairo_destroy(cr);
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

// static
Widget* Widget::CreatePopupWidget(TransparencyParam transparent,
                                  EventsParam accept_events,
                                  DeleteParam delete_on_destroy,
                                  MirroringParam mirror_in_rtl) {
  WidgetGtk* popup = new WidgetGtk(WidgetGtk::TYPE_POPUP);
  popup->set_delete_on_destroy(delete_on_destroy == DeleteOnDestroy);
  if (transparent == Transparent)
    popup->MakeTransparent();
  if (accept_events == NotAcceptEvents)
    popup->MakeIgnoreEvents();
  return popup;
}

// Callback from gtk_container_foreach. Locates the first root view of widget
// or one of it's descendants.
static void RootViewLocatorCallback(GtkWidget* widget,
                                    gpointer root_view_p) {
  RootView** root_view = static_cast<RootView**>(root_view_p);
  if (!*root_view) {
    *root_view = WidgetGtk::GetRootViewForWidget(widget);
    if (!*root_view && GTK_IS_CONTAINER(widget)) {
      // gtk_container_foreach only iterates over children, not all descendants,
      // so we have to recurse here to get all descendants.
      gtk_container_foreach(GTK_CONTAINER(widget), RootViewLocatorCallback,
                            root_view_p);
    }
  }
}

// static
RootView* Widget::FindRootView(GtkWindow* window) {
  RootView* root_view = WidgetGtk::GetRootViewForWidget(GTK_WIDGET(window));
  if (root_view)
    return root_view;

  // Enumerate all children and check if they have a RootView.
  gtk_container_foreach(GTK_CONTAINER(window), RootViewLocatorCallback,
                        static_cast<gpointer>(&root_view));
  return root_view;
}

static void AllRootViewsLocatorCallback(GtkWidget* widget,
                                        gpointer root_view_p) {
  std::set<RootView*>* root_views_set =
      reinterpret_cast<std::set<RootView*>*>(root_view_p);
  RootView *root_view = WidgetGtk::GetRootViewForWidget(widget);
  if (!root_view && GTK_IS_CONTAINER(widget)) {
    // gtk_container_foreach only iterates over children, not all descendants,
    // so we have to recurse here to get all descendants.
    gtk_container_foreach(GTK_CONTAINER(widget), AllRootViewsLocatorCallback,
        root_view_p);
  } else {
    if (root_view)
      root_views_set->insert(root_view);
  }
}

// static
void Widget::FindAllRootViews(GtkWindow* window,
                              std::vector<RootView*>* root_views) {
  RootView* root_view = WidgetGtk::GetRootViewForWidget(GTK_WIDGET(window));
  if (root_view)
    root_views->push_back(root_view);

  std::set<RootView*> root_views_set;

  // Enumerate all children and check if they have a RootView.
  gtk_container_foreach(GTK_CONTAINER(window), AllRootViewsLocatorCallback,
      reinterpret_cast<gpointer>(&root_views_set));
  root_views->clear();
  root_views->reserve(root_views_set.size());
  for (std::set<RootView*>::iterator it = root_views_set.begin();
      it != root_views_set.end();
      ++it)
    root_views->push_back(*it);
}

// static
Widget* Widget::GetWidgetFromNativeView(gfx::NativeView native_view) {
  gpointer raw_widget = g_object_get_data(G_OBJECT(native_view), kWidgetKey);
  if (raw_widget)
    return reinterpret_cast<Widget*>(raw_widget);
  return NULL;
}

// static
Widget* Widget::GetWidgetFromNativeWindow(gfx::NativeWindow native_window) {
  gpointer raw_widget = g_object_get_data(G_OBJECT(native_window), kWidgetKey);
  if (raw_widget)
    return reinterpret_cast<Widget*>(raw_widget);
  return NULL;
}

// static
void Widget::NotifyLocaleChanged() {
  GList *window_list = gtk_window_list_toplevels();
  for (GList* element = window_list; element; element = g_list_next(element)) {
    Widget* widget = GetWidgetFromNativeWindow(GTK_WINDOW(element->data));
    if (widget)
      widget->LocaleChanged();
  }
  g_list_free(window_list);
}

}  // namespace views
