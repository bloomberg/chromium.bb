// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_gtk.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <set>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_gtk.h"
#include "ui/base/gtk/g_object_destructor_filo.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/base/gtk/gtk_windowing.h"
#include "ui/base/gtk/scoped_handle_gtk.h"
#include "ui/base/hit_test.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/path.h"
#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/ime/input_method_gtk.h"
#include "views/controls/textfield/native_textfield_views.h"
#include "views/views_delegate.h"
#include "ui/views/widget/drop_target_gtk.h"
#include "ui/views/widget/gtk_views_fixed.h"
#include "ui/views/widget/gtk_views_window.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(TOUCH_UI)
#include "ui/base/touch/touch_factory.h"
#include "ui/views/widget/tooltip_manager_views.h"
#else
#include "ui/views/widget/tooltip_manager_gtk.h"
#endif

#if defined(HAVE_IBUS)
#include "ui/views/ime/input_method_ibus.h"
#endif

using ui::OSExchangeData;
using ui::OSExchangeDataProviderGtk;
using ui::ActiveWindowWatcherX;

namespace views {

namespace {

// Links the GtkWidget to its NativeWidget.
const char* const kNativeWidgetKey = "__VIEWS_NATIVE_WIDGET__";

// A g_object data key to associate a CompositePainter object to a GtkWidget.
const char* kCompositePainterKey = "__VIEWS_COMPOSITE_PAINTER__";

// A g_object data key to associate the flag whether or not the widget
// is composited to a GtkWidget. gtk_widget_is_composited simply tells
// if x11 supports composition and cannot be used to tell if given widget
// is composited.
const char* kCompositeEnabledKey = "__VIEWS_COMPOSITE_ENABLED__";

// A g_object data key to associate the expose handler id that is
// used to remove FREEZE_UPDATE property on the window.
const char* kExposeHandlerIdKey = "__VIEWS_EXPOSE_HANDLER_ID__";

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

void EnumerateChildWidgetsForNativeWidgets(GtkWidget* child_widget,
                                           gpointer param) {
  // Walk child widgets, if necessary.
  if (GTK_IS_CONTAINER(child_widget)) {
    gtk_container_foreach(GTK_CONTAINER(child_widget),
                          EnumerateChildWidgetsForNativeWidgets,
                          param);
  }

  Widget* widget = Widget::GetWidgetForNativeView(child_widget);
  if (widget) {
    Widget::Widgets* widgets = reinterpret_cast<Widget::Widgets*>(param);
    widgets->insert(widget);
  }
}

void RemoveExposeHandlerIfExists(GtkWidget* widget) {
  gulong id = reinterpret_cast<gulong>(g_object_get_data(G_OBJECT(widget),
                                                         kExposeHandlerIdKey));
  if (id) {
    g_signal_handler_disconnect(G_OBJECT(widget), id);
    g_object_set_data(G_OBJECT(widget), kExposeHandlerIdKey, 0);
  }
}

GtkWindowType WindowTypeToGtkWindowType(Widget::InitParams::Type type) {
  switch (type) {
    case Widget::InitParams::TYPE_BUBBLE:
    case Widget::InitParams::TYPE_WINDOW:
    case Widget::InitParams::TYPE_WINDOW_FRAMELESS:
      return GTK_WINDOW_TOPLEVEL;
    default:
      return GTK_WINDOW_POPUP;
  }
  NOTREACHED();
  return GTK_WINDOW_TOPLEVEL;
}

// Converts a Windows-style hit test result code into a GDK window edge.
GdkWindowEdge HitTestCodeToGDKWindowEdge(int hittest_code) {
  switch (hittest_code) {
    case HTBOTTOM:
      return GDK_WINDOW_EDGE_SOUTH;
    case HTBOTTOMLEFT:
      return GDK_WINDOW_EDGE_SOUTH_WEST;
    case HTBOTTOMRIGHT:
    case HTGROWBOX:
      return GDK_WINDOW_EDGE_SOUTH_EAST;
    case HTLEFT:
      return GDK_WINDOW_EDGE_WEST;
    case HTRIGHT:
      return GDK_WINDOW_EDGE_EAST;
    case HTTOP:
      return GDK_WINDOW_EDGE_NORTH;
    case HTTOPLEFT:
      return GDK_WINDOW_EDGE_NORTH_WEST;
    case HTTOPRIGHT:
      return GDK_WINDOW_EDGE_NORTH_EAST;
    default:
      NOTREACHED();
      break;
  }
  // Default to something defaultish.
  return HitTestCodeToGDKWindowEdge(HTGROWBOX);
}

// Converts a Windows-style hit test result code into a GDK cursor type.
GdkCursorType HitTestCodeToGdkCursorType(int hittest_code) {
  switch (hittest_code) {
    case HTBOTTOM:
      return GDK_BOTTOM_SIDE;
    case HTBOTTOMLEFT:
      return GDK_BOTTOM_LEFT_CORNER;
    case HTBOTTOMRIGHT:
    case HTGROWBOX:
      return GDK_BOTTOM_RIGHT_CORNER;
    case HTLEFT:
      return GDK_LEFT_SIDE;
    case HTRIGHT:
      return GDK_RIGHT_SIDE;
    case HTTOP:
      return GDK_TOP_SIDE;
    case HTTOPLEFT:
      return GDK_TOP_LEFT_CORNER;
    case HTTOPRIGHT:
      return GDK_TOP_RIGHT_CORNER;
    default:
      break;
  }
  // Default to something defaultish.
  return GDK_LEFT_PTR;
}

}  // namespace

// During drag and drop GTK sends a drag-leave during a drop. This means we
// have no way to tell the difference between a normal drag leave and a drop.
// To work around that we listen for DROP_START, then ignore the subsequent
// drag-leave that GTK generates.
class NativeWidgetGtk::DropObserver : public MessageLoopForUI::Observer {
 public:
  DropObserver() {}

  static DropObserver* GetInstance() {
    return Singleton<DropObserver>::get();
  }
#if defined(TOUCH_UI)
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE {
    return base::EVENT_CONTINUE;
  }

  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE {
  }
#else
  virtual void WillProcessEvent(GdkEvent* event) {
    if (event->type == GDK_DROP_START) {
      NativeWidgetGtk* widget = GetNativeWidgetGtkForEvent(event);
      if (widget)
        widget->ignore_drag_leave_ = true;
    }
  }

  virtual void DidProcessEvent(GdkEvent* event) {
  }
#endif

 private:
  NativeWidgetGtk* GetNativeWidgetGtkForEvent(GdkEvent* event) {
    GtkWidget* gtk_widget = gtk_get_event_widget(event);
    if (!gtk_widget)
      return NULL;

    return static_cast<NativeWidgetGtk*>(
        internal::NativeWidgetPrivate::GetNativeWidgetForNativeView(
            gtk_widget));
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
GtkWidget* NativeWidgetGtk::null_parent_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetGtk, public:

NativeWidgetGtk::NativeWidgetGtk(internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate),
      widget_(NULL),
      window_contents_(NULL),
      child_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_widget_factory_(this)),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      transparent_(false),
      ignore_events_(false),
      ignore_drag_leave_(false),
      opacity_(255),
      drag_data_(NULL),
      window_state_(GDK_WINDOW_STATE_WITHDRAWN),
      is_active_(false),
      transient_to_parent_(false),
      got_initial_focus_in_(false),
      has_focus_(false),
      always_on_top_(false),
      is_double_buffered_(false),
      should_handle_menu_key_release_(false),
      dragged_view_(NULL),
      painted_(false),
      has_pointer_grab_(false),
      has_keyboard_grab_(false),
      grab_notify_signal_id_(0),
      is_menu_(false),
      signal_registrar_(new ui::GtkSignalRegistrar) {
#if defined(TOUCH_UI)
  // Make sure the touch factory is initialized so that it can setup XInput2 for
  // the widget.
  ui::TouchFactory::GetInstance();
#endif
  static bool installed_message_loop_observer = false;
  if (!installed_message_loop_observer) {
    installed_message_loop_observer = true;
    MessageLoopForUI* loop = MessageLoopForUI::current();
    if (loop)
      loop->AddObserver(DropObserver::GetInstance());
  }
}

NativeWidgetGtk::~NativeWidgetGtk() {
  // We need to delete the input method before calling DestroyRootView(),
  // because it'll set focus_manager_ to NULL.
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET) {
    DCHECK(widget_ == NULL);
    delete delegate_;
  } else {
    // Disconnect from GObjectDestructorFILO because we're
    // deleting the NativeWidgetGtk.
    bool has_widget = !!widget_;
    if (has_widget)
      ui::GObjectDestructorFILO::GetInstance()->Disconnect(
          G_OBJECT(widget_), &OnDestroyedThunk, this);
    CloseNow();
    // Call OnNativeWidgetDestroyed because we're not calling
    // OnDestroyedThunk
    if (has_widget)
      delegate_->OnNativeWidgetDestroyed();
  }
}

GtkWindow* NativeWidgetGtk::GetTransientParent() const {
  return (!child_ && widget_) ?
      gtk_window_get_transient_for(GTK_WINDOW(widget_)) : NULL;
}

bool NativeWidgetGtk::MakeTransparent() {
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

void NativeWidgetGtk::EnableDoubleBuffer(bool enabled) {
  is_double_buffered_ = enabled;
  if (window_contents_) {
    if (is_double_buffered_)
      GTK_WIDGET_SET_FLAGS(window_contents_, GTK_DOUBLE_BUFFERED);
    else
      GTK_WIDGET_UNSET_FLAGS(window_contents_, GTK_DOUBLE_BUFFERED);
  }
}

void NativeWidgetGtk::AddChild(GtkWidget* child) {
  gtk_container_add(GTK_CONTAINER(window_contents_), child);
}

void NativeWidgetGtk::RemoveChild(GtkWidget* child) {
  // We can be called after the contents widget has been destroyed, e.g. any
  // NativeViewHost not removed from the view hierarchy before the window is
  // closed.
  if (GTK_IS_CONTAINER(window_contents_)) {
    gtk_container_remove(GTK_CONTAINER(window_contents_), child);
    gtk_views_fixed_set_widget_size(child, 0, 0);
  }
}

void NativeWidgetGtk::ReparentChild(GtkWidget* child) {
  gtk_widget_reparent(child, window_contents_);
}

void NativeWidgetGtk::PositionChild(GtkWidget* child, int x, int y, int w,
                                    int h) {
  gtk_views_fixed_set_widget_size(child, w, h);
  gtk_fixed_move(GTK_FIXED(window_contents_), child, x, y);
}

void NativeWidgetGtk::DoDrag(const OSExchangeData& data, int operation) {
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
  MessageLoopForUI::current()->RunWithDispatcher(NULL);

  drag_data_ = NULL;

  if (drag_icon_widget) {
    gtk_widget_destroy(drag_icon_widget);
    g_object_unref(provider.drag_image());
  }
}

void NativeWidgetGtk::OnActiveChanged() {
  delegate_->OnNativeWidgetActivationChanged(IsActive());
}

void NativeWidgetGtk::ResetDropTarget() {
  ignore_drag_leave_ = false;
  drop_target_.reset(NULL);
}

void NativeWidgetGtk::GetRequestedSize(gfx::Size* out) const {
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
// NativeWidgetGtk, ActiveWindowWatcherX::Observer implementation:

void NativeWidgetGtk::ActiveWindowChanged(GdkWindow* active_window) {
  if (!GetNativeView())
    return;

  bool was_active = IsActive();
  is_active_ = (active_window == GTK_WIDGET(GetNativeView())->window);
  if (!is_active_ && active_window && !child_) {
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
  if (was_active != IsActive()) {
    OnActiveChanged();
    GetWidget()->GetRootView()->SchedulePaint();
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetGtk implementation:

bool NativeWidgetGtk::HandleKeyboardEvent(const KeyEvent& key) {
  if (!GetWidget()->GetFocusManager())
    return false;

  const int key_code = key.key_code();
  bool handled = false;

  // Always reset |should_handle_menu_key_release_| unless we are handling a
  // VKEY_MENU key release event. It ensures that VKEY_MENU accelerator can only
  // be activated when handling a VKEY_MENU key release event which is preceded
  // by an un-handled VKEY_MENU key press event.
  if (key_code != ui::VKEY_MENU || key.type() != ui::ET_KEY_RELEASED)
    should_handle_menu_key_release_ = false;

  if (key.type() == ui::ET_KEY_PRESSED) {
    // VKEY_MENU is triggered by key release event.
    // FocusManager::OnKeyEvent() returns false when the key has been consumed.
    if (key_code != ui::VKEY_MENU)
      handled = !GetWidget()->GetFocusManager()->OnKeyEvent(key);
    else
      should_handle_menu_key_release_ = true;
  } else if (key_code == ui::VKEY_MENU && should_handle_menu_key_release_ &&
             (key.flags() & ~ui::EF_ALT_DOWN) == 0) {
    // Trigger VKEY_MENU when only this key is pressed and released, and both
    // press and release events are not handled by others.
    ui::Accelerator accelerator(ui::VKEY_MENU, false, false, false);
    handled = GetWidget()->GetFocusManager()->ProcessAccelerator(accelerator);
  }

  return handled;
}

bool NativeWidgetGtk::SuppressFreezeUpdates() {
  if (!painted_) {
    painted_ = true;
    return true;
  }
  return false;
}

// static
void NativeWidgetGtk::UpdateFreezeUpdatesProperty(GtkWindow* window,
                                                  bool enable) {
  if (!GTK_WIDGET_REALIZED(GTK_WIDGET(window)))
    gtk_widget_realize(GTK_WIDGET(window));
  GdkWindow* gdk_window = GTK_WIDGET(window)->window;

  static GdkAtom freeze_atom_ =
      gdk_atom_intern("_CHROME_FREEZE_UPDATES", FALSE);
  if (enable) {
    VLOG(1) << "setting FREEZE UPDATES property. xid=" <<
        GDK_WINDOW_XID(gdk_window);
    int32 val = 1;
    gdk_property_change(gdk_window,
                        freeze_atom_,
                        freeze_atom_,
                        32,
                        GDK_PROP_MODE_REPLACE,
                        reinterpret_cast<const guchar*>(&val),
                        1);
  } else {
    VLOG(1) << "deleting FREEZE UPDATES property. xid=" <<
        GDK_WINDOW_XID(gdk_window);
    gdk_property_delete(gdk_window, freeze_atom_);
  }
}

// static
void NativeWidgetGtk::RegisterChildExposeHandler(GtkWidget* child) {
  RemoveExposeHandlerIfExists(child);
  gulong id = g_signal_connect_after(child, "expose-event",
                                     G_CALLBACK(&ChildExposeHandler), NULL);
  g_object_set_data(G_OBJECT(child), kExposeHandlerIdKey,
                    reinterpret_cast<void*>(id));
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetGtk, NativeWidget implementation:

void NativeWidgetGtk::InitNativeWidget(const Widget::InitParams& params) {
  SetInitParams(params);

  Widget::InitParams modified_params = params;
  if (params.parent_widget) {
    NativeWidgetGtk* parent_gtk =
        static_cast<NativeWidgetGtk*>(params.parent_widget->native_widget());
    modified_params.parent = child_ ? parent_gtk->window_contents()
                                    : params.parent_widget->GetNativeView();
  }

  if (!child_)
    ActiveWindowWatcherX::AddObserver(this);

  // Make container here.
  CreateGtkWidget(modified_params);

  if (params.type == Widget::InitParams::TYPE_MENU) {
    gtk_window_set_destroy_with_parent(GTK_WINDOW(GetNativeView()), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(GetNativeView()),
                             GDK_WINDOW_TYPE_HINT_MENU);
  }

  if (View::get_use_acceleration_when_possible()) {
    if (ui::Compositor::compositor_factory()) {
      compositor_ = (*ui::Compositor::compositor_factory())(this);
    } else {
      gint width, height;
      gdk_drawable_get_size(window_contents_->window, &width, &height);
      compositor_ = ui::Compositor::Create(this,
          GDK_WINDOW_XID(window_contents_->window),
          gfx::Size(width, height));
    }
    if (compositor_.get()) {
      View* root_view = delegate_->AsWidget()->GetRootView();
      root_view->SetPaintToLayer(true);
      compositor_->SetRootLayer(root_view->layer());
      root_view->SetFillsBoundsOpaquely(!transparent_);
    }
  }

  delegate_->OnNativeWidgetCreated();

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

  signal_registrar_->ConnectAfter(G_OBJECT(window_contents_), "size_request",
                                  G_CALLBACK(&OnSizeRequestThunk), this);
  signal_registrar_->ConnectAfter(G_OBJECT(window_contents_), "size_allocate",
                                  G_CALLBACK(&OnSizeAllocateThunk), this);
  gtk_widget_set_app_paintable(window_contents_, true);
  signal_registrar_->Connect(window_contents_, "expose_event",
                             G_CALLBACK(&OnPaintThunk), this);
  signal_registrar_->Connect(window_contents_, "enter_notify_event",
                             G_CALLBACK(&OnEnterNotifyThunk), this);
  signal_registrar_->Connect(window_contents_, "leave_notify_event",
                             G_CALLBACK(&OnLeaveNotifyThunk), this);
  signal_registrar_->Connect(window_contents_, "motion_notify_event",
                             G_CALLBACK(&OnMotionNotifyThunk), this);
  signal_registrar_->Connect(window_contents_, "button_press_event",
                             G_CALLBACK(&OnButtonPressThunk), this);
  signal_registrar_->Connect(window_contents_, "button_release_event",
                             G_CALLBACK(&OnButtonReleaseThunk), this);
  signal_registrar_->Connect(window_contents_, "grab_broken_event",
                             G_CALLBACK(&OnGrabBrokeEventThunk), this);
  signal_registrar_->Connect(window_contents_, "scroll_event",
                             G_CALLBACK(&OnScrollThunk), this);
  signal_registrar_->Connect(window_contents_, "visibility_notify_event",
                             G_CALLBACK(&OnVisibilityNotifyThunk), this);

  // In order to receive notification when the window is no longer the front
  // window, we need to install these on the widget.
  // NOTE: this doesn't work with focus follows mouse.
  signal_registrar_->Connect(widget_, "focus_in_event",
                             G_CALLBACK(&OnFocusInThunk), this);
  signal_registrar_->Connect(widget_, "focus_out_event",
                             G_CALLBACK(&OnFocusOutThunk), this);
  signal_registrar_->Connect(widget_, "destroy",
                             G_CALLBACK(&OnDestroyThunk), this);
  signal_registrar_->Connect(widget_, "show",
                             G_CALLBACK(&OnShowThunk), this);
  signal_registrar_->Connect(widget_, "map",
                             G_CALLBACK(&OnMapThunk), this);
  signal_registrar_->Connect(widget_, "hide",
                             G_CALLBACK(&OnHideThunk), this);
  signal_registrar_->Connect(widget_, "configure-event",
                             G_CALLBACK(&OnConfigureEventThunk), this);

  // Views/FocusManager (re)sets the focus to the root window,
  // so we need to connect signal handlers to the gtk window.
  // See views::Views::Focus and views::FocusManager::ClearNativeFocus
  // for more details.
  signal_registrar_->Connect(widget_, "key_press_event",
                             G_CALLBACK(&OnEventKeyThunk), this);
  signal_registrar_->Connect(widget_, "key_release_event",
                             G_CALLBACK(&OnEventKeyThunk), this);

  // Drag and drop.
  gtk_drag_dest_set(window_contents_, static_cast<GtkDestDefaults>(0),
                    NULL, 0, GDK_ACTION_COPY);
  signal_registrar_->Connect(window_contents_, "drag_motion",
                             G_CALLBACK(&OnDragMotionThunk), this);
  signal_registrar_->Connect(window_contents_, "drag_data_received",
                             G_CALLBACK(&OnDragDataReceivedThunk), this);
  signal_registrar_->Connect(window_contents_, "drag_drop",
                             G_CALLBACK(&OnDragDropThunk), this);
  signal_registrar_->Connect(window_contents_, "drag_leave",
                             G_CALLBACK(&OnDragLeaveThunk), this);
  signal_registrar_->Connect(window_contents_, "drag_data_get",
                             G_CALLBACK(&OnDragDataGetThunk), this);
  signal_registrar_->Connect(window_contents_, "drag_end",
                             G_CALLBACK(&OnDragEndThunk), this);
  signal_registrar_->Connect(window_contents_, "drag_failed",
                             G_CALLBACK(&OnDragFailedThunk), this);
  signal_registrar_->Connect(G_OBJECT(widget_), "window-state-event",
                             G_CALLBACK(&OnWindowStateEventThunk), this);

  ui::GObjectDestructorFILO::GetInstance()->Connect(
      G_OBJECT(widget_), &OnDestroyedThunk, this);

#if defined(TOUCH_UI)
  if (params.type != Widget::InitParams::TYPE_TOOLTIP) {
    views::TooltipManagerViews* manager = new views::TooltipManagerViews(
        static_cast<internal::RootView*>(GetWidget()->GetRootView()));
    tooltip_manager_.reset(manager);
  }
#else
  tooltip_manager_.reset(new TooltipManagerGtk(this));
#endif

  // Register for tooltips.
  g_object_set(G_OBJECT(window_contents_), "has-tooltip", TRUE, NULL);
  signal_registrar_->Connect(window_contents_, "query_tooltip",
                             G_CALLBACK(&OnQueryTooltipThunk), this);

  if (child_) {
    if (modified_params.parent)
      SetBounds(params.bounds);
  } else {
    gtk_widget_add_events(widget_,
                          GDK_STRUCTURE_MASK);
    if (params.bounds.width() > 0 && params.bounds.height() > 0)
      gtk_window_resize(GTK_WINDOW(widget_), params.bounds.width(),
                        params.bounds.height());
    gtk_window_move(GTK_WINDOW(widget_), params.bounds.x(), params.bounds.y());
  }
}

NonClientFrameView* NativeWidgetGtk::CreateNonClientFrameView() {
  return NULL;
}

void NativeWidgetGtk::UpdateFrameAfterFrameChange() {
  // We currently don't support different frame types on Gtk, so we don't
  // need to implement this.
  NOTIMPLEMENTED();
}

bool NativeWidgetGtk::ShouldUseNativeFrame() const {
  return false;
}

void NativeWidgetGtk::FrameTypeChanged() {
  // This is called when the Theme has changed, so forward the event to the root
  // widget.
  GetWidget()->ThemeChanged();
  GetWidget()->GetRootView()->SchedulePaint();
}

Widget* NativeWidgetGtk::GetWidget() {
  return delegate_->AsWidget();
}

const Widget* NativeWidgetGtk::GetWidget() const {
  return delegate_->AsWidget();
}

gfx::NativeView NativeWidgetGtk::GetNativeView() const {
  return widget_;
}

gfx::NativeWindow NativeWidgetGtk::GetNativeWindow() const {
  return child_ ? NULL : GTK_WINDOW(widget_);
}

Widget* NativeWidgetGtk::GetTopLevelWidget() {
  NativeWidgetPrivate* native_widget = GetTopLevelNativeWidget(GetNativeView());
  return native_widget ? native_widget->GetWidget() : NULL;
}

const ui::Compositor* NativeWidgetGtk::GetCompositor() const {
  return compositor_.get();
}

ui::Compositor* NativeWidgetGtk::GetCompositor() {
  return compositor_.get();
}

void NativeWidgetGtk::CalculateOffsetToAncestorWithLayer(
    gfx::Point* offset,
    ui::Layer** layer_parent) {
}

void NativeWidgetGtk::ReorderLayers() {
}

void NativeWidgetGtk::ViewRemoved(View* view) {
  if (drop_target_.get())
    drop_target_->ResetTargetViewIfEquals(view);
}

void NativeWidgetGtk::SetNativeWindowProperty(const char* name, void* value) {
  g_object_set_data(G_OBJECT(widget_), name, value);
}

void* NativeWidgetGtk::GetNativeWindowProperty(const char* name) const {
  return g_object_get_data(G_OBJECT(widget_), name);
}

TooltipManager* NativeWidgetGtk::GetTooltipManager() const {
  return tooltip_manager_.get();
}

bool NativeWidgetGtk::IsScreenReaderActive() const {
  return false;
}

void NativeWidgetGtk::SendNativeAccessibilityEvent(
    View* view,
    ui::AccessibilityTypes::Event event_type) {
  // In the future if we add native GTK accessibility support, the
  // notification should be sent here.
}

void NativeWidgetGtk::SetMouseCapture() {
  DCHECK(!HasMouseCapture());

  // Release the current grab.
  GtkWidget* current_grab_window = gtk_grab_get_current();
  if (current_grab_window)
    gtk_grab_remove(current_grab_window);

  if (is_menu_ && gdk_pointer_is_grabbed())
    gdk_pointer_ungrab(GDK_CURRENT_TIME);

  // Make sure all app mouse/keyboard events are targeted at us only.
  gtk_grab_add(window_contents_);
  if (gtk_grab_get_current() == window_contents_ && !grab_notify_signal_id_) {
    // "grab_notify" is sent any time the grab changes. We only care about grab
    // changes when we have done a grab.
    grab_notify_signal_id_ = g_signal_connect(
        window_contents_, "grab_notify", G_CALLBACK(&OnGrabNotifyThunk), this);
  }

  if (is_menu_) {
    // For menus we do a pointer grab too. This ensures we get mouse events from
    // other apps. In theory we should do this for all widget types, but doing
    // so leads to gdk_pointer_grab randomly returning GDK_GRAB_ALREADY_GRABBED.
    GdkGrabStatus pointer_grab_status =
        gdk_pointer_grab(window_contents()->window, FALSE,
                         static_cast<GdkEventMask>(
                             GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                             GDK_POINTER_MOTION_MASK),
                         NULL, NULL, GDK_CURRENT_TIME);
    // NOTE: technically grab may fail. We may want to try and continue on in
    // that case.
    DCHECK_EQ(GDK_GRAB_SUCCESS, pointer_grab_status);
    has_pointer_grab_ = pointer_grab_status == GDK_GRAB_SUCCESS;

#if defined(TOUCH_UI)
    ::Window window = GDK_WINDOW_XID(window_contents()->window);
    Display* display = GDK_WINDOW_XDISPLAY(window_contents()->window);
    bool xi2grab =
        ui::TouchFactory::GetInstance()->GrabTouchDevices(display, window);
    // xi2grab should always succeed if has_pointer_grab_ succeeded.
    DCHECK(xi2grab);
    has_pointer_grab_ = has_pointer_grab_ && xi2grab;
#endif
  }
}

void NativeWidgetGtk::ReleaseMouseCapture() {
  bool delegate_lost_capture = HasMouseCapture();
  if (GTK_WIDGET_HAS_GRAB(window_contents_))
    gtk_grab_remove(window_contents_);
  if (grab_notify_signal_id_) {
    g_signal_handler_disconnect(window_contents_, grab_notify_signal_id_);
    grab_notify_signal_id_ = 0;
  }
  if (has_pointer_grab_) {
    has_pointer_grab_ = false;
    gdk_pointer_ungrab(GDK_CURRENT_TIME);
#if defined(TOUCH_UI)
    ui::TouchFactory::GetInstance()->UngrabTouchDevices(
        GDK_WINDOW_XDISPLAY(window_contents()->window));
#endif
  }
  if (delegate_lost_capture)
    delegate_->OnMouseCaptureLost();
}

bool NativeWidgetGtk::HasMouseCapture() const {
  return GTK_WIDGET_HAS_GRAB(window_contents_) || has_pointer_grab_;
}

InputMethod* NativeWidgetGtk::CreateInputMethod() {
  // Create input method when pure views is enabled but not on views desktop.
  // TODO(suzhe): Always enable input method when we start to use
  // RenderWidgetHostViewViews in normal ChromeOS.
  if (views::Widget::IsPureViews()) {
#if defined(HAVE_IBUS)
    InputMethod* input_method =
        InputMethodIBus::IsInputMethodIBusEnabled() ?
        static_cast<InputMethod*>(new InputMethodIBus(this)) :
        static_cast<InputMethod*>(new InputMethodGtk(this));
#else
    InputMethod* input_method = new InputMethodGtk(this);
#endif
    input_method->Init(GetWidget());
    return input_method;
  }
  // GTK's textfield will handle IME.
  return NULL;
}

void NativeWidgetGtk::CenterWindow(const gfx::Size& size) {
  gfx::Rect center_rect;

  GtkWindow* parent = gtk_window_get_transient_for(GetNativeWindow());
  if (parent) {
    // We have a parent window, center over it.
    gint parent_x = 0;
    gint parent_y = 0;
    gtk_window_get_position(parent, &parent_x, &parent_y);
    gint parent_w = 0;
    gint parent_h = 0;
    gtk_window_get_size(parent, &parent_w, &parent_h);
    center_rect = gfx::Rect(parent_x, parent_y, parent_w, parent_h);
  } else {
    // We have no parent window, center over the screen.
    center_rect = gfx::Screen::GetMonitorWorkAreaNearestWindow(GetNativeView());
  }
  gfx::Rect bounds(center_rect.x() + (center_rect.width() - size.width()) / 2,
                   center_rect.y() + (center_rect.height() - size.height()) / 2,
                   size.width(), size.height());
  GetWidget()->SetBoundsConstrained(bounds);
}

void NativeWidgetGtk::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  // Do nothing for now. ChromeOS isn't yet saving window placement.
}

void NativeWidgetGtk::SetWindowTitle(const string16& title) {
  // We don't have a window title on ChromeOS (right now).
}

void NativeWidgetGtk::SetWindowIcons(const SkBitmap& window_icon,
                                     const SkBitmap& app_icon) {
  // We don't have window icons on ChromeOS.
}

void NativeWidgetGtk::SetAccessibleName(const string16& name) {
}

void NativeWidgetGtk::SetAccessibleRole(ui::AccessibilityTypes::Role role) {
}

void NativeWidgetGtk::SetAccessibleState(ui::AccessibilityTypes::State state) {
}

void NativeWidgetGtk::BecomeModal() {
  gtk_window_set_modal(GetNativeWindow(), true);
}

gfx::Rect NativeWidgetGtk::GetWindowScreenBounds() const {
  // Client == Window bounds on Gtk.
  return GetClientAreaScreenBounds();
}

gfx::Rect NativeWidgetGtk::GetClientAreaScreenBounds() const {
  // Due to timing we can get a request for bounds after Close().
  // TODO(beng): Figure out if this is bogus.
  if (!widget_)
    return gfx::Rect(size_);

  int x = 0, y = 0, w = 0, h = 0;
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
  return gfx::Rect(x, y, w, h);
}

gfx::Rect NativeWidgetGtk::GetRestoredBounds() const {
  // We currently don't support tiling, so this doesn't matter.
  return GetWindowScreenBounds();
}

void NativeWidgetGtk::SetBounds(const gfx::Rect& bounds) {
  if (child_) {
    GtkWidget* parent = gtk_widget_get_parent(widget_);
    if (GTK_IS_VIEWS_FIXED(parent)) {
      NativeWidgetGtk* parent_widget = static_cast<NativeWidgetGtk*>(
          internal::NativeWidgetPrivate::GetNativeWidgetForNativeView(parent));
      parent_widget->PositionChild(widget_, bounds.x(), bounds.y(),
                                   bounds.width(), bounds.height());
    } else {
      DCHECK(GTK_IS_FIXED(parent))
          << "Parent of NativeWidgetGtk has to be Fixed or ViewsFixed";
      // Just request the size if the parent is not NativeWidgetGtk but plain
      // GtkFixed. NativeWidgetGtk does not know the minimum size so we assume
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

void NativeWidgetGtk::SetSize(const gfx::Size& size) {
  if (child_) {
    GtkWidget* parent = gtk_widget_get_parent(widget_);
    if (GTK_IS_VIEWS_FIXED(parent)) {
      gtk_views_fixed_set_widget_size(widget_, size.width(), size.height());
    } else {
      gtk_widget_set_size_request(widget_, size.width(), size.height());
    }
  } else {
    if (GTK_WIDGET_MAPPED(widget_))
      gdk_window_resize(widget_->window, size.width(), size.height());
    GtkWindow* gtk_window = GTK_WINDOW(widget_);
    if (!size.IsEmpty())
      gtk_window_resize(gtk_window, size.width(), size.height());
  }
}

void NativeWidgetGtk::StackAbove(gfx::NativeView native_view) {
  ui::StackPopupWindow(GetNativeView(), native_view);
}

void NativeWidgetGtk::StackAtTop() {
  DCHECK(GTK_IS_WINDOW(GetNativeView()));
  gtk_window_present(GTK_WINDOW(GetNativeView()));
}

void NativeWidgetGtk::SetShape(gfx::NativeRegion region) {
  if (widget_ && widget_->window) {
    gdk_window_shape_combine_region(widget_->window, region, 0, 0);
    gdk_region_destroy(region);
  }
}

void NativeWidgetGtk::Close() {
  if (!widget_)
    return;  // No need to do anything.

  // Hide first.
  Hide();
  if (!close_widget_factory_.HasWeakPtrs()) {
    // And we delay the close just in case we're on the stack.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&NativeWidgetGtk::CloseNow,
                   close_widget_factory_.GetWeakPtr()));
  }
}

void NativeWidgetGtk::CloseNow() {
  if (widget_) {
    gtk_widget_destroy(widget_);  // Triggers OnDestroy().
  }
}

void NativeWidgetGtk::Show() {
  if (widget_) {
    gtk_widget_show(widget_);
    if (widget_->window)
      gdk_window_raise(widget_->window);
  }
}

void NativeWidgetGtk::Hide() {
  if (widget_) {
    gtk_widget_hide(widget_);
    if (widget_->window)
      gdk_window_lower(widget_->window);
  }
}

void NativeWidgetGtk::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  // TODO: when we add maximization support update this.
  Show();
}

void NativeWidgetGtk::ShowWithWindowState(ui::WindowShowState show_state) {
  // No concept of maximization (yet) on ChromeOS.
  if (show_state == ui::SHOW_STATE_INACTIVE)
    gtk_window_set_focus_on_map(GetNativeWindow(), false);
  gtk_widget_show(GetNativeView());
}

bool NativeWidgetGtk::IsVisible() const {
  return GTK_WIDGET_VISIBLE(GetNativeView()) && (GetWidget()->is_top_level() ||
      GetWidget()->GetTopLevelWidget()->IsVisible());
}

void NativeWidgetGtk::Activate() {
  gtk_window_present(GetNativeWindow());
}

void NativeWidgetGtk::Deactivate() {
  gdk_window_lower(GTK_WIDGET(GetNativeView())->window);
}

bool NativeWidgetGtk::IsActive() const {
  DCHECK(!child_);
  return is_active_;
}

void NativeWidgetGtk::SetAlwaysOnTop(bool on_top) {
  DCHECK(!child_);
  always_on_top_ = on_top;
  if (widget_)
    gtk_window_set_keep_above(GTK_WINDOW(widget_), on_top);
}

void NativeWidgetGtk::Maximize() {
#if defined(TOUCH_UI)
  // There may not be a window manager. So maximize ourselves: move to the
  // top-left corner and resize to the entire bounds of the screen.
  gfx::Rect screen = gfx::Screen::GetMonitorAreaNearestWindow(GetNativeView());
  gtk_window_move(GTK_WINDOW(GetNativeWindow()), screen.x(), screen.y());
  // TODO(backer): Remove this driver bug workaround once it is fixed.
  gtk_window_resize(GTK_WINDOW(GetNativeWindow()),
                    screen.width() - 1, screen.height());
#else
  gtk_window_maximize(GetNativeWindow());
#endif
}

void NativeWidgetGtk::Minimize() {
  gtk_window_iconify(GetNativeWindow());
}

bool NativeWidgetGtk::IsMaximized() const {
  return window_state_ & GDK_WINDOW_STATE_MAXIMIZED;
}

bool NativeWidgetGtk::IsMinimized() const {
  return window_state_ & GDK_WINDOW_STATE_ICONIFIED;
}

void NativeWidgetGtk::Restore() {
  if (IsFullscreen()) {
    SetFullscreen(false);
  } else {
    if (IsMaximized())
      gtk_window_unmaximize(GetNativeWindow());
    else if (IsMinimized())
      gtk_window_deiconify(GetNativeWindow());
  }
}

void NativeWidgetGtk::SetFullscreen(bool fullscreen) {
  if (fullscreen)
    gtk_window_fullscreen(GetNativeWindow());
  else
    gtk_window_unfullscreen(GetNativeWindow());
}

bool NativeWidgetGtk::IsFullscreen() const {
  return window_state_ & GDK_WINDOW_STATE_FULLSCREEN;
}

void NativeWidgetGtk::SetOpacity(unsigned char opacity) {
  opacity_ = opacity;
  if (widget_) {
    // We can only set the opacity when the widget has been realized.
    gdk_window_set_opacity(widget_->window, static_cast<gdouble>(opacity) /
        static_cast<gdouble>(255));
  }
}

void NativeWidgetGtk::SetUseDragFrame(bool use_drag_frame) {
  NOTIMPLEMENTED();
}

bool NativeWidgetGtk::IsAccessibleWidget() const {
  return false;
}

void NativeWidgetGtk::RunShellDrag(View* view,
                                   const ui::OSExchangeData& data,
                                   int operation) {
  DoDrag(data, operation);
}

void NativeWidgetGtk::SchedulePaintInRect(const gfx::Rect& rect) {
  // No need to schedule paint if
  // 1) widget_ is NULL. This may happen because this instance may
  // be deleted after the gtk widget has been destroyed (See OnDestroy()).
  // 2) widget_ is not drawable (mapped and visible)
  // 3) If it's never painted before. The first expose event will
  // paint the area that has to be painted.
  if (widget_ && GTK_WIDGET_DRAWABLE(widget_) && painted_) {
    gtk_widget_queue_draw_area(widget_, rect.x(), rect.y(), rect.width(),
                               rect.height());
  }
}

void NativeWidgetGtk::SetCursor(gfx::NativeCursor cursor) {
#if defined(TOUCH_UI)
  if (!ui::TouchFactory::GetInstance()->is_cursor_visible())
    cursor = gfx::GetCursor(GDK_BLANK_CURSOR);
#endif
  // |window_contents_| is placed on top of |widget_|. So the cursor needs to be
  // set on |window_contents_| instead of |widget_|.
  if (window_contents_)
    gdk_window_set_cursor(window_contents_->window, cursor);
}

void NativeWidgetGtk::ClearNativeFocus() {
  DCHECK(!child_);
  if (!GetNativeView()) {
    NOTREACHED();
    return;
  }
  gtk_window_set_focus(GTK_WINDOW(GetNativeView()), NULL);
}

void NativeWidgetGtk::FocusNativeView(gfx::NativeView native_view) {
  if (native_view && !gtk_widget_is_focus(native_view))
    gtk_widget_grab_focus(native_view);
}

gfx::Rect NativeWidgetGtk::GetWorkAreaBoundsInScreen() const {
  return gfx::Screen::GetMonitorWorkAreaNearestWindow(GetNativeView());
}

void NativeWidgetGtk::SetInactiveRenderingDisabled(bool value) {
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetGtk, protected:

void NativeWidgetGtk::OnSizeRequest(GtkWidget* widget,
                                    GtkRequisition* requisition) {
  // Do only return the preferred size for child windows. GtkWindow interprets
  // the requisition as a minimum size for top level windows, returning a
  // preferred size for these would prevents us from setting smaller window
  // sizes.
  if (child_) {
    gfx::Size size(GetWidget()->GetRootView()->GetPreferredSize());
    requisition->width = size.width();
    requisition->height = size.height();
  }
}

void NativeWidgetGtk::OnSizeAllocate(GtkWidget* widget,
                                     GtkAllocation* allocation) {
  // See comment next to size_ as to why we do this. Also note, it's tempting
  // to put this in the static method so subclasses don't need to worry about
  // it, but if a subclasses needs to set a shape then they need to always
  // reset the shape in this method regardless of whether the size changed.
  gfx::Size new_size(allocation->width, allocation->height);
  if (new_size == size_)
    return;
  size_ = new_size;
  if (compositor_.get())
    compositor_->WidgetSizeChanged(size_);
  delegate_->OnNativeWidgetSizeChanged(size_);

  if (GetWidget()->non_client_view()) {
    // The Window's NonClientView may provide a custom shape for the Window.
    gfx::Path window_mask;
    GetWidget()->non_client_view()->GetWindowMask(gfx::Size(allocation->width,
                                                            allocation->height),
                                                  &window_mask);
    GdkRegion* mask_region = window_mask.CreateNativeRegion();
    gdk_window_shape_combine_region(GetNativeView()->window, mask_region, 0, 0);
    if (mask_region)
      gdk_region_destroy(mask_region);

    SaveWindowPosition();
  }
}

gboolean NativeWidgetGtk::OnPaint(GtkWidget* widget, GdkEventExpose* event) {
  gdk_window_set_debug_updates(Widget::IsDebugPaintEnabled());

  if (transparent_ && child_) {
    // Clear the background before drawing any view and native components.
    DrawTransparentBackground(widget, event);
    if (!CompositePainter::IsComposited(widget_) &&
        gdk_screen_is_composited(gdk_screen_get_default())) {
      // Let the parent draw the content only after something is drawn on
      // the widget.
      CompositePainter::SetComposited(widget_);
    }
  }

  ui::ScopedRegion region(gdk_region_copy(event->region));
  if (!gdk_region_empty(region.Get())) {
    GdkRectangle clip_bounds;
    gdk_region_get_clipbox(region.Get(), &clip_bounds);
    if (!delegate_->OnNativeWidgetPaintAccelerated(gfx::Rect(clip_bounds))) {
      gfx::CanvasSkiaPaint canvas(event);
      if (!canvas.is_empty()) {
        canvas.set_composite_alpha(is_transparent());
        delegate_->OnNativeWidgetPaint(&canvas);
      }
    }
  }

  if (!painted_) {
    painted_ = true;
    if (!child_)
      UpdateFreezeUpdatesProperty(GTK_WINDOW(widget_), false /* remove */);
  }
  return false;  // False indicates other widgets should get the event as well.
}

void NativeWidgetGtk::OnDragDataGet(GtkWidget* widget,
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

void NativeWidgetGtk::OnDragDataReceived(GtkWidget* widget,
                                         GdkDragContext* context,
                                         gint x,
                                         gint y,
                                         GtkSelectionData* data,
                                         guint info,
                                         guint time) {
  if (drop_target_.get())
    drop_target_->OnDragDataReceived(context, x, y, data, info, time);
}

gboolean NativeWidgetGtk::OnDragDrop(GtkWidget* widget,
                                     GdkDragContext* context,
                                     gint x,
                                     gint y,
                                     guint time) {
  if (drop_target_.get()) {
    return drop_target_->OnDragDrop(context, x, y, time);
  }
  return FALSE;
}

void NativeWidgetGtk::OnDragEnd(GtkWidget* widget, GdkDragContext* context) {
  if (!drag_data_) {
    // This indicates we didn't start a drag operation, and should never
    // happen.
    NOTREACHED();
    return;
  }
  // Quit the nested message loop we spawned in DoDrag.
  MessageLoop::current()->Quit();
}

gboolean NativeWidgetGtk::OnDragFailed(GtkWidget* widget,
                                       GdkDragContext* context,
                                       GtkDragResult result) {
  return FALSE;
}

void NativeWidgetGtk::OnDragLeave(GtkWidget* widget,
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

gboolean NativeWidgetGtk::OnDragMotion(GtkWidget* widget,
                                       GdkDragContext* context,
                                       gint x,
                                       gint y,
                                       guint time) {
  if (!drop_target_.get()) {
    drop_target_.reset(new DropTargetGtk(
        reinterpret_cast<internal::RootView*>(GetWidget()->GetRootView()),
        context));
  }
  return drop_target_->OnDragMotion(context, x, y, time);
}

gboolean NativeWidgetGtk::OnEnterNotify(GtkWidget* widget,
                                        GdkEventCrossing* event) {
  if (HasMouseCapture() && event->mode == GDK_CROSSING_GRAB) {
    // Doing a grab results an async enter event, regardless of where the mouse
    // is. We don't want to generate a mouse move in this case.
    return false;
  }

  if (!GetWidget()->last_mouse_event_was_move_ &&
      !GetWidget()->is_mouse_button_pressed_) {
    // When a mouse button is pressed gtk generates a leave, enter, press.
    // RootView expects to get a mouse move before a press, otherwise enter is
    // not set. So we generate a move here.
    GdkEventMotion motion = { GDK_MOTION_NOTIFY, event->window,
        event->send_event, event->time, event->x, event->y, NULL, event->state,
        0, NULL, event->x_root, event->y_root };

    // If this event is the result of pressing a button then one of the button
    // modifiers is set. Unset it as we're compensating for the leave generated
    // when you press a button.
    motion.state &= ~(GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK);

    MouseEvent mouse_event(TransformEvent(&motion));
    delegate_->OnMouseEvent(mouse_event);
  }
  return false;
}

gboolean NativeWidgetGtk::OnLeaveNotify(GtkWidget* widget,
                                        GdkEventCrossing* event) {
  gdk_window_set_cursor(widget->window, gfx::GetCursor(GDK_LEFT_PTR));

  GetWidget()->ResetLastMouseMoveFlag();

  if (!HasMouseCapture() && !GetWidget()->is_mouse_button_pressed_) {
    // Don't convert if the event is synthetic and has 0x0 coordinates.
    if (event->x_root || event->y_root || event->x || event->y ||
        !event->send_event) {
      TransformEvent(event);
    }
    MouseEvent mouse_event(reinterpret_cast<GdkEvent*>(event));
    delegate_->OnMouseEvent(mouse_event);
  }
  return false;
}

gboolean NativeWidgetGtk::OnMotionNotify(GtkWidget* widget,
                                         GdkEventMotion* event) {
  if (GetWidget()->non_client_view()) {
    GdkEventMotion transformed_event = *event;
    TransformEvent(&transformed_event);
    gfx::Point translated_location(transformed_event.x, transformed_event.y);

    // Update the cursor for the screen edge.
    int hittest_code =
        GetWidget()->non_client_view()->NonClientHitTest(translated_location);
    if (hittest_code != HTCLIENT) {
      GdkCursorType cursor_type = HitTestCodeToGdkCursorType(hittest_code);
      gdk_window_set_cursor(widget->window, gfx::GetCursor(cursor_type));
    }
  }

  MouseEvent mouse_event(TransformEvent(event));
  delegate_->OnMouseEvent(mouse_event);
  return true;
}

gboolean NativeWidgetGtk::OnButtonPress(GtkWidget* widget,
                                        GdkEventButton* event) {
  if (GetWidget()->non_client_view()) {
    GdkEventButton transformed_event = *event;
    MouseEvent mouse_event(TransformEvent(&transformed_event));

    int hittest_code = GetWidget()->non_client_view()->NonClientHitTest(
        mouse_event.location());
    switch (hittest_code) {
      case HTCAPTION: {
        // Start dragging if the mouse event is a single click and *not* a right
        // click. If it is a right click, then pass it through to
        // NativeWidgetGtk::OnButtonPress so that View class can show
        // ContextMenu upon a mouse release event. We only start drag on single
        // clicks as we get a crash in Gtk on double/triple clicks.
        if (event->type == GDK_BUTTON_PRESS &&
            !mouse_event.IsOnlyRightMouseButton()) {
          gfx::Point screen_point(event->x, event->y);
          View::ConvertPointToScreen(GetWidget()->GetRootView(), &screen_point);
          gtk_window_begin_move_drag(GetNativeWindow(), event->button,
                                     screen_point.x(), screen_point.y(),
                                     event->time);
          return TRUE;
        }
        break;
      }
      case HTBOTTOM:
      case HTBOTTOMLEFT:
      case HTBOTTOMRIGHT:
      case HTGROWBOX:
      case HTLEFT:
      case HTRIGHT:
      case HTTOP:
      case HTTOPLEFT:
      case HTTOPRIGHT: {
        gfx::Point screen_point(event->x, event->y);
        View::ConvertPointToScreen(GetWidget()->GetRootView(), &screen_point);
        // TODO(beng): figure out how to get a good minimum size.
        gtk_widget_set_size_request(GetNativeView(), 100, 100);
        gtk_window_begin_resize_drag(GetNativeWindow(),
                                     HitTestCodeToGDKWindowEdge(hittest_code),
                                     event->button, screen_point.x(),
                                     screen_point.y(), event->time);
        return TRUE;
      }
      default:
        // Everything else falls into standard client event handling...
        break;
    }
  }

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

  MouseEvent mouse_event(TransformEvent(event));
  // Returns true to consume the event when widget is not transparent.
  return delegate_->OnMouseEvent(mouse_event) || !transparent_;
}

gboolean NativeWidgetGtk::OnButtonRelease(GtkWidget* widget,
                                          GdkEventButton* event) {
  // GTK generates a mouse release at the end of dnd. We need to ignore it.
  if (!drag_data_) {
    MouseEvent mouse_event(TransformEvent(event));
    delegate_->OnMouseEvent(mouse_event);
  }
  return true;
}

gboolean NativeWidgetGtk::OnScroll(GtkWidget* widget, GdkEventScroll* event) {
  MouseWheelEvent mouse_event(TransformEvent(event));
  return delegate_->OnMouseEvent(mouse_event);
}

gboolean NativeWidgetGtk::OnFocusIn(GtkWidget* widget, GdkEventFocus* event) {
  if (has_focus_)
    return false;  // This is the second focus-in event in a row, ignore it.
  has_focus_ = true;

  should_handle_menu_key_release_ = false;

  if (!GetWidget()->is_top_level())
    return false;

  // Only top-level Widget should have an InputMethod instance.
  InputMethod* input_method = GetWidget()->GetInputMethod();
  if (input_method)
    input_method->OnFocus();

  // See description of got_initial_focus_in_ for details on this.
  if (!got_initial_focus_in_) {
    got_initial_focus_in_ = true;
    // Sets initial focus here. On X11/Gtk, window creation
    // is asynchronous and a focus request has to be made after a window
    // gets created.
    GetWidget()->SetInitialFocus();
  }
  return false;
}

gboolean NativeWidgetGtk::OnFocusOut(GtkWidget* widget, GdkEventFocus* event) {
  if (!has_focus_)
    return false;  // This is the second focus-out event in a row, ignore it.
  has_focus_ = false;

  if (!GetWidget()->is_top_level())
    return false;

  // Only top-level Widget should have an InputMethod instance.
  InputMethod* input_method = GetWidget()->GetInputMethod();
  if (input_method)
    input_method->OnBlur();
  return false;
}

gboolean NativeWidgetGtk::OnEventKey(GtkWidget* widget, GdkEventKey* event) {
  KeyEvent key(reinterpret_cast<GdkEvent*>(event));
  InputMethod* input_method = GetWidget()->GetInputMethod();
  if (input_method)
    input_method->DispatchKeyEvent(key);
  else
    DispatchKeyEventPostIME(key);

  // Returns true to prevent GtkWindow's default key event handler.
  return true;
}

gboolean NativeWidgetGtk::OnQueryTooltip(GtkWidget* widget,
                                         gint x,
                                         gint y,
                                         gboolean keyboard_mode,
                                         GtkTooltip* tooltip) {
#if defined(TOUCH_UI)
  return false; // Tell GTK not to draw tooltips as we draw tooltips in views
#else
  return static_cast<TooltipManagerGtk*>(tooltip_manager_.get())->
      ShowTooltip(x, y, keyboard_mode, tooltip);
#endif
}

gboolean NativeWidgetGtk::OnVisibilityNotify(GtkWidget* widget,
                                             GdkEventVisibility* event) {
  return false;
}

gboolean NativeWidgetGtk::OnGrabBrokeEvent(GtkWidget* widget, GdkEvent* event) {
  if (!has_pointer_grab_ && !has_keyboard_grab_) {
    // We don't have any grabs; don't attempt to do anything.
    return false;
  }

  // Sent when either the keyboard or pointer grab is broke. We drop both grabs
  // in this case.
  if (event->grab_broken.keyboard) {
    // Keyboard grab was broke.
    has_keyboard_grab_ = false;
    if (has_pointer_grab_) {
      has_pointer_grab_ = false;
      gdk_pointer_ungrab(GDK_CURRENT_TIME);
      delegate_->OnMouseCaptureLost();
    }
  } else {
    // Mouse grab was broke.
    has_pointer_grab_ = false;
    if (has_keyboard_grab_) {
      has_keyboard_grab_ = false;
      gdk_keyboard_ungrab(GDK_CURRENT_TIME);
    }
    delegate_->OnMouseCaptureLost();
  }
  ReleaseMouseCapture();

#if defined(TOUCH_UI)
  ui::TouchFactory::GetInstance()->UngrabTouchDevices(
      GDK_WINDOW_XDISPLAY(window_contents()->window));
#endif
  return false;  // To let other widgets get the event.
}

void NativeWidgetGtk::OnGrabNotify(GtkWidget* widget, gboolean was_grabbed) {
  // Sent when gtk_grab_add changes.
  if (!window_contents_)
    return;  // Grab broke after window destroyed, don't try processing it.
  if (!was_grabbed)  // Indicates we've been shadowed (lost grab).
    HandleGtkGrabBroke();
}

void NativeWidgetGtk::OnDestroy(GtkWidget* object) {
  signal_registrar_.reset();
  if (grab_notify_signal_id_) {
    g_signal_handler_disconnect(window_contents_, grab_notify_signal_id_);
    grab_notify_signal_id_ = 0;
  }
  delegate_->OnNativeWidgetDestroying();
  if (!child_)
    ActiveWindowWatcherX::RemoveObserver(this);
  // Note that this handler is hooked to GtkObject::destroy.
  // NULL out pointers here since we might still be in an observer list
  // until deletion happens.
  widget_ = window_contents_ = NULL;
}

void NativeWidgetGtk::OnDestroyed(GObject *where_the_object_was) {
  delegate_->OnNativeWidgetDestroyed();
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete this;
}

void NativeWidgetGtk::OnShow(GtkWidget* widget) {
  delegate_->OnNativeWidgetVisibilityChanged(true);
}

void NativeWidgetGtk::OnMap(GtkWidget* widget) {
#if defined(TOUCH_UI)
  // Force an expose event to trigger OnPaint for touch. This is
  // a workaround for a bug that X Expose event does not trigger
  // Gdk's expose signal. This happens when you try to open views menu
  // while a virtual keyboard gets kicked in or out. This seems to be
  // a bug in message_pump_x.cc as we do get X Expose event but
  // it doesn't trigger gtk's expose signal. We're not going to fix this
  // as we're removing gtk and migrating to new compositor.
  gdk_window_process_all_updates();
#endif
}

void NativeWidgetGtk::OnHide(GtkWidget* widget) {
  delegate_->OnNativeWidgetVisibilityChanged(false);
}

gboolean NativeWidgetGtk::OnWindowStateEvent(GtkWidget* widget,
                                             GdkEventWindowState* event) {
  if (GetWidget()->non_client_view() &&
      !(event->new_window_state & GDK_WINDOW_STATE_WITHDRAWN)) {
    SaveWindowPosition();
  }
  window_state_ = event->new_window_state;
  return FALSE;
}

gboolean NativeWidgetGtk::OnConfigureEvent(GtkWidget* widget,
                                           GdkEventConfigure* event) {
  SaveWindowPosition();
  return FALSE;
}

void NativeWidgetGtk::HandleGtkGrabBroke() {
  ReleaseMouseCapture();
  delegate_->OnMouseCaptureLost();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetGtk, private:

void NativeWidgetGtk::ScheduleDraw() {
  SchedulePaintInRect(gfx::Rect(gfx::Point(), size_));
}

void NativeWidgetGtk::DispatchKeyEventPostIME(const KeyEvent& key) {
  // Always reset |should_handle_menu_key_release_| unless we are handling a
  // VKEY_MENU key release event. It ensures that VKEY_MENU accelerator can only
  // be activated when handling a VKEY_MENU key release event which is preceded
  // by an unhandled VKEY_MENU key press event. See also HandleKeyboardEvent().
  if (key.key_code() != ui::VKEY_MENU || key.type() != ui::ET_KEY_RELEASED)
    should_handle_menu_key_release_ = false;

  // Send the key event to View hierarchy first.
  bool handled = delegate_->OnKeyEvent(key);

  if (key.key_code() == ui::VKEY_PROCESSKEY || handled)
    return;

  // Dispatch the key event to native GtkWidget hierarchy.
  // To prevent GtkWindow from handling the key event as a keybinding, we need
  // to bypass GtkWindow's default key event handler and dispatch the event
  // here.
  GdkEventKey* event = reinterpret_cast<GdkEventKey*>(key.gdk_event());
  if (!handled && event && GTK_IS_WINDOW(widget_))
    handled = gtk_window_propagate_key_event(GTK_WINDOW(widget_), event);

  // On Linux, in order to handle VKEY_MENU (Alt) accelerator key correctly and
  // avoid issues like: http://crbug.com/40966 and http://crbug.com/49701, we
  // should only send the key event to the focus manager if it's not handled by
  // any View or native GtkWidget.
  // The flow is different when the focus is in a RenderWidgetHostViewGtk, which
  // always consumes the key event and send it back to us later by calling
  // HandleKeyboardEvent() directly, if it's not handled by webkit.
  if (!handled)
    handled = HandleKeyboardEvent(key);

  // Dispatch the key event for bindings processing.
  if (!handled && event && GTK_IS_WINDOW(widget_))
    gtk_bindings_activate_event(GTK_OBJECT(widget_), event);
}

void NativeWidgetGtk::SetInitParams(const Widget::InitParams& params) {
  DCHECK(!GetNativeView());

  ownership_ = params.ownership;
  child_ = params.child;
  is_menu_ = params.type == Widget::InitParams::TYPE_MENU;

  // TODO(beng): The secondary checks here actually obviate the need for
  //             params.transient but that's only because NativeWidgetGtk
  //             considers any top-level widget to be a transient widget. We
  //             will probably want to ammend this assumption at some point.
  if (params.transient || params.parent || params.parent_widget)
    transient_to_parent_  = true;
  if (params.transparent)
    MakeTransparent();
  if (!params.accept_events && !child_)
    ignore_events_ = true;
  if (params.double_buffer)
    EnableDoubleBuffer(true);
}

gboolean NativeWidgetGtk::OnWindowPaint(GtkWidget* widget,
                                        GdkEventExpose* event) {
  // Clear the background to be totally transparent. We don't need to
  // paint the root view here as that is done by OnPaint.
  DCHECK(transparent_);
  DrawTransparentBackground(widget, event);
  // The Keyboard layout view has a renderer that covers the entire
  // window, which prevents OnPaint from being called on window_contents_,
  // so we need to remove the FREEZE_UPDATES property here.
  if (!painted_) {
    painted_ = true;
    UpdateFreezeUpdatesProperty(GTK_WINDOW(widget_), false /* remove */);
  }
  return false;
}

void NativeWidgetGtk::OnChildExpose(GtkWidget* child) {
  DCHECK(!child_);
  if (!painted_) {
    painted_ = true;
    UpdateFreezeUpdatesProperty(GTK_WINDOW(widget_), false /* remove */);
  }
  RemoveExposeHandlerIfExists(child);
}

// static
gboolean NativeWidgetGtk::ChildExposeHandler(GtkWidget* widget,
                                             GdkEventExpose* event) {
  GtkWidget* toplevel = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);
  CHECK(toplevel);
  Widget* views_widget = Widget::GetWidgetForNativeView(toplevel);
  CHECK(views_widget);
  NativeWidgetGtk* widget_gtk =
      static_cast<NativeWidgetGtk*>(views_widget->native_widget());
  widget_gtk->OnChildExpose(widget);
  return false;
}

void NativeWidgetGtk::CreateGtkWidget(const Widget::InitParams& params) {
  // We turn off double buffering for two reasons:
  // 1. We draw to a canvas then composite to the screen, which means we're
  //    doing our own double buffering already.
  // 2. GTKs double buffering clips to the dirty region. RootView occasionally
  //    needs to expand the paint region (see RootView::OnPaint). This means
  //    that if we use GTK's double buffering and we tried to expand the dirty
  //    region, it wouldn't get painted.
  if (child_) {
    window_contents_ = widget_ = gtk_views_fixed_new();
    gtk_widget_set_name(widget_, "views-gtkwidget-child-fixed");
    if (!is_double_buffered_)
      GTK_WIDGET_UNSET_FLAGS(widget_, GTK_DOUBLE_BUFFERED);
    gtk_fixed_set_has_window(GTK_FIXED(widget_), true);
    if (!params.parent && !null_parent_) {
      GtkWidget* popup = gtk_window_new(GTK_WINDOW_POPUP);
      null_parent_ = gtk_fixed_new();
      gtk_widget_set_name(null_parent_, "views-gtkwidget-null-parent");
      gtk_container_add(GTK_CONTAINER(popup), null_parent_);
      gtk_widget_realize(null_parent_);
    }
    if (transparent_) {
      // transparency has to be configured before widget is realized.
      DCHECK(params.parent) <<
          "Transparent widget must have parent when initialized";
      ConfigureWidgetForTransparentBackground(params.parent);
    }
    gtk_container_add(
        GTK_CONTAINER(params.parent ? params.parent : null_parent_), widget_);
    gtk_widget_realize(widget_);
    if (transparent_) {
      // The widget has to be realized to set composited flag.
      // I tried "realize" signal to set this flag, but it did not work
      // when the top level is popup.
      DCHECK(GTK_WIDGET_REALIZED(widget_));
      gdk_window_set_composited(widget_->window, true);
    }
    if (params.parent && !params.bounds.size().IsEmpty()) {
      // Make sure that an widget is given it's initial size before
      // we're done initializing, to take care of some potential
      // corner cases when programmatically arranging hierarchies as
      // seen in
      // http://code.google.com/p/chromium-os/issues/detail?id=5987

      // This can't be done without a parent present, or stale data
      // might show up on the screen as seen in
      // http://code.google.com/p/chromium/issues/detail?id=53870
      GtkAllocation alloc =
          { 0, 0, params.bounds.width(), params.bounds.height() };
      gtk_widget_size_allocate(widget_, &alloc);
    }
    if (params.type == Widget::InitParams::TYPE_CONTROL) {
      // Controls are initially visible.
      gtk_widget_show(widget_);
    }
  } else {
    Widget::InitParams::Type type = params.type;
    if (type == Widget::InitParams::TYPE_BUBBLE &&
        params.delegate->AsBubbleDelegate() &&
        params.delegate->AsBubbleDelegate()->use_focusless()) {
      // Handles focusless bubble type, which are bubbles that should
      // act like popups rather than gtk windows. They do not get focus
      // and are not controlled by window manager placement.
      type = Widget::InitParams::TYPE_POPUP;
    }

    // Use our own window class to override GtkWindow's move_focus method.
    widget_ = gtk_views_window_new(WindowTypeToGtkWindowType(type));
    gtk_widget_set_name(widget_, "views-gtkwidget-window");
    if (transient_to_parent_) {
      gtk_window_set_transient_for(GTK_WINDOW(widget_),
                                   GTK_WINDOW(params.parent));
    }
    GTK_WIDGET_UNSET_FLAGS(widget_, GTK_DOUBLE_BUFFERED);

    // Gtk determines the size for windows based on the requested size of the
    // child. For NativeWidgetGtk the child is a fixed. If the fixed ends up
    // with a child widget it's possible the child widget will drive the
    // requested size of the widget, which we don't want. We explicitly set a
    // value of 1x1 here so that gtk doesn't attempt to resize the window if we
    // end up with a situation where the requested size of a child of the fixed
    // is greater than the size of the window. By setting the size in this
    // manner we're also allowing users of WidgetGtk to change the requested
    // size at any time.
    gtk_widget_set_size_request(widget_, 1, 1);

    if (!params.bounds.size().IsEmpty()) {
      // When we realize the window, the window manager is given a size. If we
      // don't specify a size before then GTK defaults to 200x200. Specify
      // a size now so that the window manager sees the requested size.
      GtkAllocation alloc =
          { 0, 0, params.bounds.width(), params.bounds.height() };
      gtk_widget_size_allocate(widget_, &alloc);
    }
    gtk_window_set_decorated(GTK_WINDOW(widget_), false);
    // We'll take care of positioning our window.
    gtk_window_set_position(GTK_WINDOW(widget_), GTK_WIN_POS_NONE);

    window_contents_ = gtk_views_fixed_new();
    gtk_widget_set_name(window_contents_, "views-gtkwidget-window-fixed");
    if (!is_double_buffered_)
      GTK_WIDGET_UNSET_FLAGS(window_contents_, GTK_DOUBLE_BUFFERED);
    gtk_fixed_set_has_window(GTK_FIXED(window_contents_), true);
    gtk_container_add(GTK_CONTAINER(widget_), window_contents_);
    gtk_widget_show(window_contents_);
    g_object_set_data(G_OBJECT(window_contents_), kNativeWidgetKey,
                       static_cast<NativeWidgetGtk*>(this));
    if (transparent_)
      ConfigureWidgetForTransparentBackground(NULL);

    if (ignore_events_)
      ConfigureWidgetForIgnoreEvents();

    // Realize the window_contents_ so that we can always get a handle for
    // acceleration. Without this we need to check every time paint is
    // invoked.
    gtk_widget_realize(window_contents_);

    SetAlwaysOnTop(always_on_top_);
    // UpdateFreezeUpdatesProperty will realize the widget and handlers like
    // size-allocate will function properly.
    UpdateFreezeUpdatesProperty(GTK_WINDOW(widget_), true /* add */);
  }
  SetNativeWindowProperty(kNativeWidgetKey, this);
}

void NativeWidgetGtk::ConfigureWidgetForTransparentBackground(
    GtkWidget* parent) {
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
  if (!child_) {
    DCHECK(parent == NULL);
    gtk_widget_set_colormap(widget_, rgba_colormap);
    gtk_widget_set_app_paintable(widget_, true);
    signal_registrar_->Connect(widget_, "expose_event",
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

void NativeWidgetGtk::ConfigureWidgetForIgnoreEvents() {
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

void NativeWidgetGtk::DrawTransparentBackground(GtkWidget* widget,
                                                GdkEventExpose* event) {
  cairo_t* cr = gdk_cairo_create(widget->window);
  cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
  gdk_cairo_region(cr, event->region);
  cairo_fill(cr);
  cairo_destroy(cr);
}

void NativeWidgetGtk::SaveWindowPosition() {
  // The delegate may have gone away on us.
  if (!GetWidget()->widget_delegate())
    return;

  ui::WindowShowState show_state = ui::SHOW_STATE_NORMAL;
  if (IsMaximized())
    show_state = ui::SHOW_STATE_MAXIMIZED;
  else if (IsMinimized())
    show_state = ui::SHOW_STATE_MINIMIZED;
  GetWidget()->widget_delegate()->SaveWindowPlacement(
      GetWidget()->GetWindowScreenBounds(),
      show_state);
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

// static
void Widget::NotifyLocaleChanged() {
  GList *window_list = gtk_window_list_toplevels();
  for (GList* element = window_list; element; element = g_list_next(element)) {
    Widget* widget =
        Widget::GetWidgetForNativeWindow(GTK_WINDOW(element->data));
    if (widget)
      widget->LocaleChanged();
  }
  g_list_free(window_list);
}

// static
void Widget::CloseAllSecondaryWidgets() {
  GList* windows = gtk_window_list_toplevels();
  for (GList* window = windows; window;
       window = g_list_next(window)) {
    Widget* widget = Widget::GetWidgetForNativeView(GTK_WIDGET(window->data));
    if (widget && widget->is_secondary_widget())
      widget->Close();
  }
  g_list_free(windows);
}

// static
bool Widget::ConvertRect(const Widget* source,
                         const Widget* target,
                         gfx::Rect* rect) {
  DCHECK(source);
  DCHECK(target);
  DCHECK(rect);

  // TODO(oshima): Add check if source and target belongs to the same
  // screen.

  if (source == target)
    return true;
  if (!source || !target)
    return false;

  gfx::Point source_point = source->GetWindowScreenBounds().origin();
  gfx::Point target_point = target->GetWindowScreenBounds().origin();

  rect->set_origin(
      source_point.Subtract(target_point).Add(rect->origin()));
  return true;
}

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetPrivate, public:

// static
NativeWidgetPrivate* NativeWidgetPrivate::CreateNativeWidget(
    NativeWidgetDelegate* delegate) {
  return new NativeWidgetGtk(delegate);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeView(
    gfx::NativeView native_view) {
  if (!native_view)
    return NULL;
  return reinterpret_cast<NativeWidgetGtk*>(
      g_object_get_data(G_OBJECT(native_view), kNativeWidgetKey));
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeWindow(
    gfx::NativeWindow native_window) {
  if (!native_window)
    return NULL;
  return reinterpret_cast<NativeWidgetGtk*>(
      g_object_get_data(G_OBJECT(native_window), kNativeWidgetKey));
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetTopLevelNativeWidget(
    gfx::NativeView native_view) {
  if (!native_view)
    return NULL;

  NativeWidgetPrivate* widget = NULL;

  GtkWidget* parent_gtkwidget = native_view;
  NativeWidgetPrivate* parent_widget;
  do {
    parent_widget = GetNativeWidgetForNativeView(parent_gtkwidget);
    if (parent_widget)
      widget = parent_widget;
    parent_gtkwidget = gtk_widget_get_parent(parent_gtkwidget);
  } while (parent_gtkwidget);

  return widget && widget->GetWidget()->is_top_level() ? widget : NULL;
}

// static
void NativeWidgetPrivate::GetAllChildWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* children) {
  if (!native_view)
    return;

  Widget* widget = Widget::GetWidgetForNativeView(native_view);
  if (widget)
    children->insert(widget);
  gtk_container_foreach(GTK_CONTAINER(native_view),
                        EnumerateChildWidgetsForNativeWidgets,
                        reinterpret_cast<gpointer>(children));
}

// static
void NativeWidgetPrivate::ReparentNativeView(gfx::NativeView native_view,
                                             gfx::NativeView new_parent) {
  if (!native_view)
    return;

  gfx::NativeView previous_parent = gtk_widget_get_parent(native_view);
  if (previous_parent == new_parent)
    return;

  Widget::Widgets widgets;
  GetAllChildWidgets(native_view, &widgets);

  // First notify all the widgets that they are being disassociated
  // from their previous parent.
  for (Widget::Widgets::iterator it = widgets.begin();
       it != widgets.end(); ++it) {
    // TODO(beng): Rename this notification to NotifyNativeViewChanging()
    // and eliminate the bool parameter.
    (*it)->NotifyNativeViewHierarchyChanged(false, previous_parent);
  }

  if (gtk_widget_get_parent(native_view))
    gtk_widget_reparent(native_view, new_parent);
  else
    gtk_container_add(GTK_CONTAINER(new_parent), native_view);

  // And now, notify them that they have a brand new parent.
  for (Widget::Widgets::iterator it = widgets.begin();
       it != widgets.end(); ++it) {
    (*it)->NotifyNativeViewHierarchyChanged(true, new_parent);
  }
}

// static
bool NativeWidgetPrivate::IsMouseButtonDown() {
  bool button_pressed = false;
  GdkEvent* event = gtk_get_current_event();
  if (event) {
    button_pressed = event->type == GDK_BUTTON_PRESS ||
        event->type == GDK_2BUTTON_PRESS ||
        event->type == GDK_3BUTTON_PRESS;
    gdk_event_free(event);
  }
  return button_pressed;
}

}  // namespace internal
}  // namespace views
