// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget_gtk.h"

#include "app/drag_drop_types.h"
#include "app/gfx/path.h"
#include "app/os_exchange_data.h"
#include "app/os_exchange_data_provider_gtk.h"
#include "base/auto_reset.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "views/widget/default_theme_provider.h"
#include "views/widget/drop_target_gtk.h"
#include "views/widget/root_view.h"
#include "views/widget/tooltip_manager_gtk.h"
#include "views/window/window_gtk.h"

namespace views {

// During drag and drop GTK sends a drag-leave during a drop. This means we
// have no way to tell the difference between a normal drag leave and a drop.
// To work around that we listen for DROP_START, then ignore the subsequent
// drag-leave that GTK generates.
class WidgetGtk::DropObserver : public MessageLoopForUI::Observer {
 public:
  DropObserver() {}

  static DropObserver* Get() {
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

static const char* kWidgetKey = "__VIEWS_WIDGET__";
static const wchar_t* kWidgetWideKey = L"__VIEWS_WIDGET__";

// Returns the position of a widget on screen.
static void GetWidgetPositionOnScreen(GtkWidget* widget, int* x, int *y) {
  // First get the root window.
  GtkWidget* root = widget;
  while (root && !GTK_IS_WINDOW(root)) {
    root = gtk_widget_get_parent(root);
  }
  DCHECK(root);
  // Translate the coordinate from widget to root window.
  gtk_widget_translate_coordinates(widget, root, 0, 0, x, y);
  // Then adjust the position with the position of the root window.
  int window_x, window_y;
  gtk_window_get_position(GTK_WINDOW(root), &window_x, &window_y);
  *x += window_x;
  *y += window_y;
}

// static
GtkWidget* WidgetGtk::null_parent_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// WidgetGtk, public:

WidgetGtk::WidgetGtk(Type type)
    : is_window_(false),
      type_(type),
      widget_(NULL),
      window_contents_(NULL),
      is_mouse_down_(false),
      has_capture_(false),
      last_mouse_event_was_move_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_widget_factory_(this)),
      delete_on_destroy_(true),
      transparent_(false),
      ignore_drag_leave_(false),
      opacity_(255),
      drag_data_(NULL),
      in_paint_now_(false),
      is_active_(false),
      transient_to_parent_(false) {
  static bool installed_message_loop_observer = false;
  if (!installed_message_loop_observer) {
    installed_message_loop_observer = true;
    MessageLoopForUI* loop = MessageLoopForUI::current();
    if (loop)
      loop->AddObserver(DropObserver::Get());
  }

  if (type_ != TYPE_CHILD)
    focus_manager_.reset(new FocusManager(this));
}

WidgetGtk::~WidgetGtk() {
  if (type_ != TYPE_CHILD)
    ActiveWindowWatcherX::RemoveObserver(this);
  MessageLoopForUI::current()->RemoveObserver(this);
}

GtkWindow* WidgetGtk::GetTransientParent() const {
  return (type_ != TYPE_CHILD && widget_) ?
      gtk_window_get_transient_for(GTK_WINDOW(widget_)) : NULL;
}

bool WidgetGtk::MakeTransparent() {
  // Transparency can only be enabled for windows/popups and only if we haven't
  // realized the widget.
  DCHECK(!widget_ && type_ != TYPE_CHILD);

  if (!gdk_screen_is_composited(gdk_screen_get_default())) {
    // Transparency is only supported for compositing window managers.
    DLOG(WARNING) << "compositing not supported";
    return false;
  }

  if (!gdk_screen_get_rgba_colormap(gdk_screen_get_default())) {
    // We need rgba to make the window transparent.
    return false;
  }

  transparent_ = true;
  return true;
}

void WidgetGtk::AddChild(GtkWidget* child) {
  gtk_container_add(GTK_CONTAINER(window_contents_), child);
}

void WidgetGtk::RemoveChild(GtkWidget* child) {
  // We can be called after the contents widget has been destroyed, e.g. any
  // NativeViewHost not removed from the view hierarchy before the window is
  // closed.
  if (GTK_IS_CONTAINER(window_contents_))
    gtk_container_remove(GTK_CONTAINER(window_contents_), child);
}

void WidgetGtk::ReparentChild(GtkWidget* child) {
  gtk_widget_reparent(child, window_contents_);
}

void WidgetGtk::PositionChild(GtkWidget* child, int x, int y, int w, int h) {
  GtkAllocation alloc = { x, y, w, h };
  // For some reason we need to do both of these to size a widget.
  gtk_widget_size_allocate(child, &alloc);
  gtk_widget_set_size_request(child, w, h);
  gtk_fixed_move(GTK_FIXED(window_contents_), child, x, y);
}

void WidgetGtk::DoDrag(const OSExchangeData& data, int operation) {
  const OSExchangeDataProviderGtk& data_provider =
      static_cast<const OSExchangeDataProviderGtk&>(data.provider());
  GtkTargetList* targets = data_provider.GetTargetList();
  GdkEvent* current_event = gtk_get_current_event();
  DCHECK(current_event);
  const OSExchangeDataProviderGtk& provider(
      static_cast<const OSExchangeDataProviderGtk&>(data.provider()));

  GdkDragContext* context = gtk_drag_begin(
      window_contents_,
      targets,
      static_cast<GdkDragAction>(
          DragDropTypes::DragOperationToGdkDragAction(operation)),
      1,
      current_event);

  // Set the drag image if one was supplied.
  if (provider.drag_image())
    gtk_drag_set_icon_pixbuf(context,
                             provider.drag_image(),
                             provider.cursor_offset_x(),
                             provider.cursor_offset_y());
  gdk_event_free(current_event);
  gtk_target_list_unref(targets);

  drag_data_ = &data_provider;

  // Block the caller until drag is done by running a nested message loop.
  MessageLoopForUI::current()->Run(NULL);

  drag_data_ = NULL;
}

void WidgetGtk::SetFocusTraversableParent(FocusTraversable* parent) {
  root_view_->SetFocusTraversableParent(parent);
}

void WidgetGtk::SetFocusTraversableParentView(View* parent_view) {
  root_view_->SetFocusTraversableParentView(parent_view);
}

// static
WidgetGtk* WidgetGtk::GetViewForNative(GtkWidget* widget) {
  return static_cast<WidgetGtk*>(GetWidgetFromNativeView(widget));
}

// static
WindowGtk* WidgetGtk::GetWindowForNative(GtkWidget* widget) {
  gpointer user_data = g_object_get_data(G_OBJECT(widget), "chrome-window");
  return static_cast<WindowGtk*>(user_data);
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

void WidgetGtk::Init(GtkWidget* parent,
                     const gfx::Rect& bounds) {
  if (type_ != TYPE_CHILD)
    ActiveWindowWatcherX::AddObserver(this);
  // Force creation of the RootView if it hasn't been created yet.
  GetRootView();

  // TODO(sky): nuke this once toolkit_views becomes chromeos.
#if !defined(CHROMEOS_TRANSITIONAL)
  default_theme_provider_.reset(new DefaultThemeProvider());
#endif

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
  SetRootViewForWidget(widget_, root_view_.get());

  MessageLoopForUI::current()->AddObserver(this);

  // TODO(beng): make these take this rather than NULL.
  g_signal_connect_after(G_OBJECT(window_contents_), "size_allocate",
                         G_CALLBACK(CallSizeAllocate), NULL);
  g_signal_connect(G_OBJECT(window_contents_), "expose_event",
                   G_CALLBACK(CallPaint), NULL);
  g_signal_connect(G_OBJECT(window_contents_), "enter_notify_event",
                   G_CALLBACK(CallEnterNotify), NULL);
  g_signal_connect(G_OBJECT(window_contents_), "leave_notify_event",
                   G_CALLBACK(CallLeaveNotify), NULL);
  g_signal_connect(G_OBJECT(window_contents_), "motion_notify_event",
                   G_CALLBACK(CallMotionNotify), NULL);
  g_signal_connect(G_OBJECT(window_contents_), "button_press_event",
                   G_CALLBACK(CallButtonPress), NULL);
  g_signal_connect(G_OBJECT(window_contents_), "button_release_event",
                   G_CALLBACK(CallButtonRelease), NULL);
  g_signal_connect(G_OBJECT(window_contents_), "grab_broken_event",
                   G_CALLBACK(CallGrabBrokeEvent), NULL);
  g_signal_connect(G_OBJECT(window_contents_), "grab_notify",
                   G_CALLBACK(CallGrabNotify), NULL);
  g_signal_connect(G_OBJECT(window_contents_), "key_press_event",
                   G_CALLBACK(CallKeyPress), NULL);
  g_signal_connect(G_OBJECT(window_contents_), "key_release_event",
                   G_CALLBACK(CallKeyRelease), NULL);
  g_signal_connect(G_OBJECT(window_contents_), "scroll_event",
                   G_CALLBACK(CallScroll), NULL);
  g_signal_connect(G_OBJECT(window_contents_), "visibility_notify_event",
                   G_CALLBACK(CallVisibilityNotify), NULL);

  // In order to receive notification when the window is no longer the front
  // window, we need to install these on the widget.
  // NOTE: this doesn't work with focus follows mouse.
  g_signal_connect(G_OBJECT(widget_), "focus_in_event",
                   G_CALLBACK(CallFocusIn), NULL);
  g_signal_connect(G_OBJECT(widget_), "focus_out_event",
                   G_CALLBACK(CallFocusOut), NULL);
  g_signal_connect(G_OBJECT(widget_), "destroy",
                   G_CALLBACK(CallDestroy), NULL);
  if (transparent_) {
    g_signal_connect(G_OBJECT(widget_), "expose_event",
                     G_CALLBACK(CallWindowPaint), this);
  }

  // Drag and drop.
  gtk_drag_dest_set(window_contents_, static_cast<GtkDestDefaults>(0),
                    NULL, 0, GDK_ACTION_COPY);
  g_signal_connect(G_OBJECT(window_contents_), "drag_motion",
                   G_CALLBACK(CallDragMotion), this);
  g_signal_connect(G_OBJECT(window_contents_), "drag_data_received",
                   G_CALLBACK(CallDragDataReceived), this);
  g_signal_connect(G_OBJECT(window_contents_), "drag_drop",
                   G_CALLBACK(CallDragDrop), this);
  g_signal_connect(G_OBJECT(window_contents_), "drag_leave",
                   G_CALLBACK(CallDragLeave), this);
  g_signal_connect(G_OBJECT(window_contents_), "drag_data_get",
                   G_CALLBACK(CallDragDataGet), this);
  g_signal_connect(G_OBJECT(window_contents_), "drag_end",
                   G_CALLBACK(CallDragEnd), this);
  g_signal_connect(G_OBJECT(window_contents_), "drag_failed",
                   G_CALLBACK(CallDragFailed), this);

  tooltip_manager_.reset(new TooltipManagerGtk(this));

  // Register for tooltips.
  g_object_set(G_OBJECT(window_contents_), "has-tooltip", TRUE, NULL);
  g_signal_connect(G_OBJECT(window_contents_), "query_tooltip",
                   G_CALLBACK(CallQueryTooltip), this);

  if (type_ == TYPE_CHILD) {
    if (parent) {
      WidgetGtk* parent_widget = GetViewForNative(parent);
      parent_widget->PositionChild(widget_, bounds.x(), bounds.y(),
                                   bounds.width(), bounds.height());
    }
  } else {
    if (bounds.width() > 0 && bounds.height() > 0)
      gtk_window_resize(GTK_WINDOW(widget_), bounds.width(), bounds.height());
    gtk_window_move(GTK_WINDOW(widget_), bounds.x(), bounds.y());
  }
}

void WidgetGtk::SetContentsView(View* view) {
  root_view_->SetContentsView(view);
}

void WidgetGtk::GetBounds(gfx::Rect* out, bool including_frame) const {
  DCHECK(widget_);

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
    WidgetGtk* parent_widget = GetViewForNative(gtk_widget_get_parent(widget_));
    parent_widget->PositionChild(widget_, bounds.x(), bounds.y(),
                                 bounds.width(), bounds.height());
  } else if (GTK_WIDGET_MAPPED(widget_)) {
    // If the widget is mapped (on screen), we can move and resize with one
    // call, which avoids two separate window manager steps.
    gdk_window_move_resize(widget_->window, bounds.x(), bounds.y(),
                           bounds.width(), bounds.height());
  } else {
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
  NOTIMPLEMENTED();
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
  if (widget_)
    gtk_widget_destroy(widget_);
}

void WidgetGtk::Show() {
  if (widget_)
    gtk_widget_show(widget_);
}

void WidgetGtk::Hide() {
  if (widget_)
    gtk_widget_hide(widget_);
}

gfx::NativeView WidgetGtk::GetNativeView() const {
  return widget_;
}

void WidgetGtk::PaintNow(const gfx::Rect& update_rect) {
  if (widget_ && GTK_WIDGET_DRAWABLE(widget_)) {
    gtk_widget_queue_draw_area(widget_, update_rect.x(), update_rect.y(),
                               update_rect.width(), update_rect.height());
    // Force the paint to occur now.
    AutoReset auto_reset_in_paint_now(&in_paint_now_, true);
    gdk_window_process_updates(widget_->window, true);
  }
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
  NOTIMPLEMENTED();
}

RootView* WidgetGtk::GetRootView() {
  if (!root_view_.get()) {
    // First time the root view is being asked for, create it now.
    root_view_.reset(CreateRootView());
  }
  return root_view_.get();
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

void WidgetGtk::GenerateMousePressedForView(View* view,
                                            const gfx::Point& point) {
  NOTIMPLEMENTED();
}

TooltipManager* WidgetGtk::GetTooltipManager() {
  return tooltip_manager_.get();
}

bool WidgetGtk::GetAccelerator(int cmd_id, Accelerator* accelerator) {
  NOTIMPLEMENTED();
  return false;
}

Window* WidgetGtk::GetWindow() {
  return GetWindowImpl(widget_);
}

const Window* WidgetGtk::GetWindow() const {
  return GetWindowImpl(widget_);
}

void WidgetGtk::SetNativeWindowProperty(const std::wstring& name,
                                        void* value) {
  g_object_set_data(G_OBJECT(widget_), WideToUTF8(name).c_str(), value);
}

void* WidgetGtk::GetNativeWindowProperty(const std::wstring& name) {
  return g_object_get_data(G_OBJECT(widget_), WideToUTF8(name).c_str());
}

ThemeProvider* WidgetGtk::GetThemeProvider() const {
  return default_theme_provider_.get();
}

ThemeProvider* WidgetGtk::GetDefaultThemeProvider() const {
  return default_theme_provider_.get();
}

FocusManager* WidgetGtk::GetFocusManager() {
  if (focus_manager_.get())
    return focus_manager_.get();

  Widget* root = GetRootWidget();
  if (root && root != this) {
    // Widget subclasses may override GetFocusManager(), for example for
    // dealing with cases where the widget has been unparented.
    return root->GetFocusManager();
  }
  return NULL;
}

void WidgetGtk::ViewHierarchyChanged(bool is_add, View *parent,
                                     View *child) {
  if (drop_target_.get())
    drop_target_->ResetTargetViewIfEquals(child);
}

////////////////////////////////////////////////////////////////////////////////
// WidgetGtk, MessageLoopForUI::Observer implementation:

void WidgetGtk::WillProcessEvent(GdkEvent* event) {
}

void WidgetGtk::DidProcessEvent(GdkEvent* event) {
  if (root_view_->NeedsPainting(true))
    PaintNow(root_view_->GetScheduledPaintRect());
}

////////////////////////////////////////////////////////////////////////////////
// WidgetGtk, FocusTraversable implementation:

View* WidgetGtk::FindNextFocusableView(
    View* starting_view, bool reverse, Direction direction,
    bool check_starting_view, FocusTraversable** focus_traversable,
    View** focus_traversable_view) {
  return root_view_->FindNextFocusableView(starting_view,
                                           reverse,
                                           direction,
                                           check_starting_view,
                                           focus_traversable,
                                           focus_traversable_view);
}

FocusTraversable* WidgetGtk::GetFocusTraversableParent() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

View* WidgetGtk::GetFocusTraversableParentView() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// WidgetGtk, protected:

// static
int WidgetGtk::GetFlagsForEventButton(const GdkEventButton& event) {
  int flags = Event::GetFlagsFromGdkState(event.state);
  switch (event.button) {
    case 1:
      flags |= Event::EF_LEFT_BUTTON_DOWN;
      break;
    case 2:
      flags |= Event::EF_MIDDLE_BUTTON_DOWN;
      break;
    case 3:
      flags |= Event::EF_RIGHT_BUTTON_DOWN;
      break;
    default:
      // We only deal with 1-3.
      break;
  }
  if (event.type == GDK_2BUTTON_PRESS)
    flags |= MouseEvent::EF_IS_DOUBLE_CLICK;
  return flags;
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
  root_view_->SetBounds(0, 0, allocation->width, allocation->height);
  root_view_->SchedulePaint();
}

void WidgetGtk::OnPaint(GtkWidget* widget, GdkEventExpose* event) {
  root_view_->OnPaint(event);
}

void WidgetGtk::OnDragDataGet(GdkDragContext* context,
                              GtkSelectionData* data,
                              guint info,
                              guint time) {
  if (!drag_data_) {
    NOTREACHED();
    return;
  }
  drag_data_->WriteFormatToSelection(info, data);
}

void WidgetGtk::OnDragDataReceived(GdkDragContext* context,
                                   gint x,
                                   gint y,
                                   GtkSelectionData* data,
                                   guint info,
                                   guint time) {
  if (drop_target_.get())
    drop_target_->OnDragDataReceived(context, x, y, data, info, time);
}

gboolean WidgetGtk::OnDragDrop(GdkDragContext* context,
                               gint x,
                               gint y,
                               guint time) {
  if (drop_target_.get()) {
    return drop_target_->OnDragDrop(context, x, y, time);
  }
  return FALSE;
}

void WidgetGtk::OnDragEnd(GdkDragContext* context) {
  if (!drag_data_) {
    // This indicates we didn't start a drag operation, and should never
    // happen.
    NOTREACHED();
    return;
  }
  // Quit the nested message loop we spawned in DoDrag.
  MessageLoop::current()->Quit();
}

gboolean WidgetGtk::OnDragFailed(GdkDragContext* context,
                                 GtkDragResult result) {
  return FALSE;
}

void WidgetGtk::OnDragLeave(GdkDragContext* context,
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

gboolean WidgetGtk::OnDragMotion(GdkDragContext* context,
                                 gint x,
                                 gint y,
                                 guint time) {
  if (!drop_target_.get())
    drop_target_.reset(new DropTargetGtk(GetRootView(), context));
  return drop_target_->OnDragMotion(context, x, y, time);
}

gboolean WidgetGtk::OnEnterNotify(GtkWidget* widget, GdkEventCrossing* event) {
  return false;
}

gboolean WidgetGtk::OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event) {
  last_mouse_event_was_move_ = false;
  if (!has_capture_ && !is_mouse_down_)
    root_view_->ProcessOnMouseExited();
  return false;
}

gboolean WidgetGtk::OnMotionNotify(GtkWidget* widget, GdkEventMotion* event) {
  if (has_capture_ && is_mouse_down_) {
    last_mouse_event_was_move_ = false;
    int flags = Event::GetFlagsFromGdkState(event->state);
    if (event->state & GDK_BUTTON1_MASK)
      flags |= Event::EF_LEFT_BUTTON_DOWN;
    if (event->state & GDK_BUTTON2_MASK)
      flags |= Event::EF_MIDDLE_BUTTON_DOWN;
    if (event->state & GDK_BUTTON3_MASK)
      flags |= Event::EF_RIGHT_BUTTON_DOWN;
    MouseEvent mouse_drag(Event::ET_MOUSE_DRAGGED, event->x, event->y, flags);
    root_view_->OnMouseDragged(mouse_drag);
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
  if (event->state & GDK_BUTTON1_MASK)
    flags |= Event::EF_LEFT_BUTTON_DOWN;
  if (event->state & GDK_BUTTON2_MASK)
    flags |= Event::EF_MIDDLE_BUTTON_DOWN;
  if (event->state & GDK_BUTTON3_MASK)
    flags |= Event::EF_RIGHT_BUTTON_DOWN;
  MouseEvent mouse_move(Event::ET_MOUSE_MOVED, event->x, event->y, flags);
  root_view_->OnMouseMoved(mouse_move);
  return true;
}

gboolean WidgetGtk::OnButtonPress(GtkWidget* widget, GdkEventButton* event) {
  ProcessMousePressed(event);
  return true;
}

gboolean WidgetGtk::OnButtonRelease(GtkWidget* widget, GdkEventButton* event) {
  ProcessMouseReleased(event);
  return true;
}

gboolean WidgetGtk::OnFocusIn(GtkWidget* widget, GdkEventFocus* event) {
  if (type_ == TYPE_CHILD)
    return false;

  // The top-level window got focus, restore the last focused view.
  focus_manager_->RestoreFocusedView();
  return false;
}

gboolean WidgetGtk::OnFocusOut(GtkWidget* widget, GdkEventFocus* event) {
  if (type_ == TYPE_CHILD)
    return false;

  // The top-level window lost focus, store the focused view.
  focus_manager_->StoreFocusedView();
  return false;
}

gboolean WidgetGtk::OnKeyPress(GtkWidget* widget, GdkEventKey* event) {
  KeyEvent key_event(event);
  return root_view_->ProcessKeyEvent(key_event);
}

gboolean WidgetGtk::OnKeyRelease(GtkWidget* widget, GdkEventKey* event) {
  KeyEvent key_event(event);
  return root_view_->ProcessKeyEvent(key_event);
}

gboolean WidgetGtk::OnQueryTooltip(gint x,
                                   gint y,
                                   gboolean keyboard_mode,
                                   GtkTooltip* tooltip) {
  return tooltip_manager_->ShowTooltip(x, y, keyboard_mode, tooltip);
}

gboolean WidgetGtk::OnGrabBrokeEvent(GtkWidget* widget, GdkEvent* event) {
  HandleGrabBroke();
  return false;  // To let other widgets get the event.
}

void WidgetGtk::OnGrabNotify(GtkWidget* widget, gboolean was_grabbed) {
  gtk_grab_remove(window_contents_);
  HandleGrabBroke();
}

void WidgetGtk::OnDestroy() {
  widget_ = window_contents_ = NULL;
  if (delete_on_destroy_) {
    // Delays the deletion of this WidgetGtk as we want its children to have
    // access to it when destroyed.
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }
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

// static
void WidgetGtk::SetWindowForNative(GtkWidget* widget, WindowGtk* window) {
  g_object_set_data(G_OBJECT(widget), "chrome-window", window);
}

////////////////////////////////////////////////////////////////////////////////
// WidgetGtk, private:

RootView* WidgetGtk::CreateRootView() {
  return new RootView(this);
}

void WidgetGtk::OnWindowPaint(GtkWidget* widget, GdkEventExpose* event) {
  // NOTE: for reasons I don't understand this code is never hit. It should
  // be hit when transparent_, but we never get the expose-event for the
  // window in this case, even though a stand alone test case triggers it. I'm
  // leaving it in just in case.

  // Fill the background totally transparent. We don't need to paint the root
  // view here as that is done by OnPaint.
  DCHECK(transparent_);
  int width, height;
  gtk_window_get_size(GTK_WINDOW (widget), &width, &height);
  cairo_t* cr = gdk_cairo_create(widget->window);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba(cr, 0, 0, 0, 0);
  cairo_rectangle(cr, 0, 0, width, height);
  cairo_fill(cr);
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

  last_mouse_event_was_move_ = false;
  MouseEvent mouse_pressed(Event::ET_MOUSE_PRESSED, event->x, event->y,
                           GetFlagsForEventButton(*event));
  if (root_view_->OnMousePressed(mouse_pressed)) {
    is_mouse_down_ = true;
    if (!has_capture_)
      DoGrab();
    return true;
  }

  return false;
}

void WidgetGtk::ProcessMouseReleased(GdkEventButton* event) {
  last_mouse_event_was_move_ = false;
  MouseEvent mouse_up(Event::ET_MOUSE_RELEASED, event->x, event->y,
                      GetFlagsForEventButton(*event));
  // Release the capture first, that way we don't get confused if
  // OnMouseReleased blocks.

  if (has_capture_ && ReleaseCaptureOnMouseReleased())
    ReleaseGrab();
  is_mouse_down_ = false;
  // GTK generates a mouse release at the end of dnd. We need to ignore it.
  if (!drag_data_)
    root_view_->OnMouseReleased(mouse_up, false);
}

// static
void WidgetGtk::SetRootViewForWidget(GtkWidget* widget, RootView* root_view) {
  g_object_set_data(G_OBJECT(widget), "root-view", root_view);
}

// static
gboolean WidgetGtk::CallButtonPress(GtkWidget* widget, GdkEventButton* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnButtonPress(widget, event);
}

// static
gboolean WidgetGtk::CallButtonRelease(GtkWidget* widget, GdkEventButton* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnButtonRelease(widget, event);
}

// static
gboolean WidgetGtk::CallDragDrop(GtkWidget* widget,
                                 GdkDragContext* context,
                                 gint x,
                                 gint y,
                                 guint time,
                                 WidgetGtk* host) {
  return host->OnDragDrop(context, x, y, time);
}

// static
gboolean WidgetGtk::CallDragFailed(GtkWidget* widget,
                                   GdkDragContext* context,
                                   GtkDragResult result,
                                   WidgetGtk* host) {
  return host->OnDragFailed(context, result);
}

// static
gboolean WidgetGtk::CallDragMotion(GtkWidget* widget,
                                   GdkDragContext* context,
                                   gint x,
                                   gint y,
                                   guint time,
                                   WidgetGtk* host) {
  return host->OnDragMotion(context, x, y, time);
}

// static
gboolean WidgetGtk::CallEnterNotify(GtkWidget* widget, GdkEventCrossing* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnEnterNotify(widget, event);
}

// static
gboolean WidgetGtk::CallFocusIn(GtkWidget* widget, GdkEventFocus* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnFocusIn(widget, event);
}

// static
gboolean WidgetGtk::CallFocusOut(GtkWidget* widget, GdkEventFocus* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnFocusOut(widget, event);
}

// static
gboolean WidgetGtk::CallGrabBrokeEvent(GtkWidget* widget, GdkEvent* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnGrabBrokeEvent(widget, event);
}

// static
gboolean WidgetGtk::CallKeyPress(GtkWidget* widget, GdkEventKey* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnKeyPress(widget, event);
}

// static
gboolean WidgetGtk::CallKeyRelease(GtkWidget* widget, GdkEventKey* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnKeyRelease(widget, event);
}

// static
gboolean WidgetGtk::CallLeaveNotify(GtkWidget* widget, GdkEventCrossing* event)
{
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnLeaveNotify(widget, event);
}

// static
gboolean WidgetGtk::CallMotionNotify(GtkWidget* widget, GdkEventMotion* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnMotionNotify(widget, event);
}

// static
gboolean WidgetGtk::CallPaint(GtkWidget* widget, GdkEventExpose* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (widget_gtk)
    widget_gtk->OnPaint(widget, event);
  return false;  // False indicates other widgets should get the event as well.
}

// static
gboolean WidgetGtk::CallQueryTooltip(GtkWidget* widget,
                                     gint x,
                                     gint y,
                                     gboolean keyboard_mode,
                                     GtkTooltip* tooltip,
                                     WidgetGtk* host) {
  return host->OnQueryTooltip(static_cast<int>(x), static_cast<int>(y),
                              keyboard_mode, tooltip);
}

// static
gboolean WidgetGtk::CallScroll(GtkWidget* widget, GdkEventScroll* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnScroll(widget, event);
}

// static
gboolean WidgetGtk::CallVisibilityNotify(GtkWidget* widget,
                                         GdkEventVisibility* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnVisibilityNotify(widget, event);
}

// static
gboolean WidgetGtk::CallWindowPaint(GtkWidget* widget,
                                    GdkEventExpose* event,
                                    WidgetGtk* widget_gtk) {
  widget_gtk->OnWindowPaint(widget, event);
  return false;  // False indicates other widgets should get the event as well.
}

// static
void WidgetGtk::CallDestroy(GtkObject* object) {
  WidgetGtk* widget_gtk = GetViewForNative(GTK_WIDGET(object));
  if (widget_gtk)
    widget_gtk->OnDestroy();
}

// static
void WidgetGtk::CallDragDataGet(GtkWidget* widget,
                                GdkDragContext* context,
                                GtkSelectionData* data,
                                guint info,
                                guint time,
                                WidgetGtk* host) {
  host->OnDragDataGet(context, data, info, time);
}

// static
void WidgetGtk::CallDragDataReceived(GtkWidget* widget,
                                     GdkDragContext* context,
                                     gint x,
                                     gint y,
                                     GtkSelectionData* data,
                                     guint info,
                                     guint time,
                                     WidgetGtk* host) {
  return host->OnDragDataReceived(context, x, y, data, info, time);
}

// static
void WidgetGtk::CallDragEnd(GtkWidget* widget,
                            GdkDragContext* context,
                            WidgetGtk* host) {
  host->OnDragEnd(context);
}

// static
void WidgetGtk::CallDragLeave(GtkWidget* widget,
                              GdkDragContext* context,
                              guint time,
                              WidgetGtk* host) {
  host->OnDragLeave(context, time);
}

// static
void WidgetGtk::CallGrabNotify(GtkWidget* widget, gboolean was_grabbed) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return;

  return widget_gtk->OnGrabNotify(widget, was_grabbed);
}

// static
void WidgetGtk::CallSizeAllocate(GtkWidget* widget, GtkAllocation* allocation) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return;

  widget_gtk->OnSizeAllocate(widget, allocation);
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
    window_contents_ = widget_ = gtk_fixed_new();
    gtk_widget_set_name(widget_, "views-gtkwidget-child-fixed");
    GTK_WIDGET_UNSET_FLAGS(widget_, GTK_DOUBLE_BUFFERED);
    gtk_fixed_set_has_window(GTK_FIXED(widget_), true);
    if (!parent && !null_parent_) {
      GtkWidget* popup = gtk_window_new(GTK_WINDOW_POPUP);
      null_parent_ = gtk_fixed_new();
      gtk_widget_set_name(widget_, "views-gtkwidget-null-parent");
      gtk_container_add(GTK_CONTAINER(popup), null_parent_);
      gtk_widget_realize(null_parent_);
    }
    gtk_container_add(GTK_CONTAINER(parent ? parent : null_parent_), widget_);
  } else {
    widget_ = gtk_window_new(
        (type_ == TYPE_WINDOW || type_ == TYPE_DECORATED_WINDOW) ?
        GTK_WINDOW_TOPLEVEL : GTK_WINDOW_POPUP);
    gtk_widget_set_name(widget_, "views-gtkwidget-window");
    if (transient_to_parent_)
      gtk_window_set_transient_for(GTK_WINDOW(widget_), GTK_WINDOW(parent));
    GTK_WIDGET_UNSET_FLAGS(widget_, GTK_DOUBLE_BUFFERED);

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
    SetWindowForNative(widget_, static_cast<WindowGtk*>(this));

    window_contents_ = gtk_fixed_new();
    gtk_widget_set_name(window_contents_, "views-gtkwidget-window-fixed");
    GTK_WIDGET_UNSET_FLAGS(window_contents_, GTK_DOUBLE_BUFFERED);
    gtk_fixed_set_has_window(GTK_FIXED(window_contents_), true);
    gtk_container_add(GTK_CONTAINER(widget_), window_contents_);
    gtk_widget_show(window_contents_);
    g_object_set_data(G_OBJECT(window_contents_), kWidgetKey, this);

    if (transparent_)
      ConfigureWidgetForTransparentBackground();
  }

  // The widget needs to be realized before handlers like size-allocate can
  // function properly.
  gtk_widget_realize(widget_);

  // Associate this object with the widget.
  SetNativeWindowProperty(kWidgetWideKey, this);
}

void WidgetGtk::ConfigureWidgetForTransparentBackground() {
  DCHECK(widget_ && window_contents_ && widget_ != window_contents_);

  GdkColormap* rgba_colormap =
      gdk_screen_get_rgba_colormap(gdk_screen_get_default());
  if (!rgba_colormap) {
    transparent_ = false;
    return;
  }
  // To make the background transparent we need to install the RGBA colormap
  // on both the window and fixed. In addition we need to make sure no
  // decorations are drawn. The last bit is to make sure the widget doesn't
  // attempt to draw a pixmap in it's background.
  gtk_widget_set_colormap(widget_, rgba_colormap);
  gtk_widget_set_app_paintable(widget_, true);
  gtk_widget_realize(widget_);
  gdk_window_set_decorations(widget_->window,
                             static_cast<GdkWMDecoration>(0));
  // Widget must be realized before setting pixmap.
  gdk_window_set_back_pixmap(widget_->window, NULL, FALSE);

  gtk_widget_set_colormap(window_contents_, rgba_colormap);
  gtk_widget_set_app_paintable(window_contents_, true);
  gtk_widget_realize(window_contents_);
  // Widget must be realized before setting pixmap.
  gdk_window_set_back_pixmap(window_contents_->window, NULL, FALSE);
}

void WidgetGtk::HandleGrabBroke() {
  if (has_capture_) {
    if (is_mouse_down_)
      root_view_->ProcessMouseDragCanceled();
    is_mouse_down_ = false;
    has_capture_ = false;
  }
}


////////////////////////////////////////////////////////////////////////////////
// Widget, public:

// static
Widget* Widget::CreatePopupWidget(TransparencyParam transparent,
                                  EventsParam /*accept_events*/,
                                  DeleteParam delete_on_destroy) {
  WidgetGtk* popup = new WidgetGtk(WidgetGtk::TYPE_POPUP);
  popup->set_delete_on_destroy(delete_on_destroy == DeleteOnDestroy);
  if (transparent == Transparent)
    popup->MakeTransparent();
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

}  // namespace views
