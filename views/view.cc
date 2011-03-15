// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/view.h"

#include <algorithm>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/base/accessibility/accessibility_types.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/compositor.h"
#include "ui/gfx/path.h"
#include "ui/gfx/transform.h"
#include "views/background.h"
#include "views/layout/layout_manager.h"
#include "views/views_delegate.h"
#include "views/widget/native_widget.h"
#include "views/widget/root_view.h"
#include "views/widget/tooltip_manager.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

#if defined(OS_WIN)
#include "base/win/scoped_gdi_object.h"
#include "views/accessibility/native_view_accessibility_win.h"
#endif
#if defined(OS_LINUX)
#include "ui/base/gtk/scoped_handle_gtk.h"
#endif

namespace {
// Whether to use accelerated compositing when necessary (e.g. when a view has a
// transformation).
bool use_acceleration_when_possible = true;
}

namespace views {

// static
ViewsDelegate* ViewsDelegate::views_delegate = NULL;

// static
char View::kViewClassName[] = "views/View";

////////////////////////////////////////////////////////////////////////////////
// View, public:

// TO BE MOVED -----------------------------------------------------------------

void View::SetHotTracked(bool flag) {
}

bool View::IsHotTracked() const {
  return false;
}

// FATE TBD --------------------------------------------------------------------

Widget* View::child_widget() {
  return NULL;
}

// Creation and lifetime -------------------------------------------------------

View::View()
    : enabled_(true),
      id_(0),
      group_(-1),
      focusable_(false),
      accessibility_focusable_(false),
      is_parent_owned_(true),
      parent_(NULL),
      is_visible_(true),
      registered_for_visible_bounds_notification_(false),
      clip_x_(0.0),
      clip_y_(0.0),
      needs_layout_(true),
      flip_canvas_on_paint_for_rtl_ui_(false),
      texture_id_(0),  // TODO(sadrul): 0 can be a valid texture id.
      accelerator_registration_delayed_(false),
      accelerator_focus_manager_(NULL),
      registered_accelerator_count_(0),
      next_focusable_view_(NULL),
      previous_focusable_view_(NULL),
      context_menu_controller_(NULL),
      drag_controller_(NULL) {
}

View::~View() {
  if (parent_)
    parent_->RemoveChildView(this);

  int c = static_cast<int>(children_.size());
  while (--c >= 0) {
    children_[c]->SetParent(NULL);
    if (children_[c]->IsParentOwned())
      delete children_[c];
  }

#if defined(OS_WIN)
  if (native_view_accessibility_win_.get())
    native_view_accessibility_win_->set_view(NULL);
#endif
}

// Tree operations -------------------------------------------------------------

const Widget* View::GetWidget() const {
  // The root view holds a reference to this view hierarchy's Widget.
  return parent_ ? parent_->GetWidget() : NULL;
}

Widget* View::GetWidget() {
  return const_cast<Widget*>(const_cast<const View*>(this)->GetWidget());
}

void View::AddChildView(View* view) {
  AddChildViewAt(view, child_count());
}

void View::AddChildViewAt(View* view, int index) {
  CHECK(view != this) << "You cannot add a view as its own child";

  // Remove the view from its current parent if any.
  if (view->parent())
    view->parent()->RemoveChildView(view);

  // Sets the prev/next focus views.
  InitFocusSiblings(view, index);

  // Let's insert the view.
  children_.insert(children_.begin() + index, view);
  view->SetParent(this);

  for (View* p = this; p; p = p->parent())
    p->ViewHierarchyChangedImpl(false, true, this, view);

  view->PropagateAddNotifications(this, view);
  UpdateTooltip();
  if (GetWidget())
    RegisterChildrenForVisibleBoundsNotification(view);

  if (layout_manager_.get())
    layout_manager_->ViewAdded(this, view);
}

void View::RemoveChildView(View* view) {
  DoRemoveChildView(view, true, true, false);
}

void View::RemoveAllChildViews(bool delete_views) {
  ViewVector::iterator iter;
  while ((iter = children_.begin()) != children_.end())
    DoRemoveChildView(*iter, false, false, delete_views);
  UpdateTooltip();
}

const View* View::GetChildViewAt(int index) const {
  return index < child_count() ? children_[index] : NULL;
}

View* View::GetChildViewAt(int index) {
  return
      const_cast<View*>(const_cast<const View*>(this)->GetChildViewAt(index));
}

bool View::Contains(const View* view) const {
  const View* child = view;
  while (child) {
    if (child == this)
      return true;
    child = child->parent();
  }
  return false;
}

int View::GetIndexOf(const View* view) const {
  ViewVector::const_iterator it = std::find(children_.begin(), children_.end(),
                                            view);
  return it != children_.end() ? it - children_.begin() : -1;
}

// TODO(beng): remove
const Window* View::GetWindow() const {
  const Widget* widget = GetWidget();
  return widget ? widget->GetWindow() : NULL;
}

// TODO(beng): remove
Window* View::GetWindow() {
  return const_cast<Window*>(const_cast<const View*>(this)->GetWindow());
}

// TODO(beng): remove
bool View::ContainsNativeView(gfx::NativeView native_view) const {
  for (int i = 0, count = child_count(); i < count; ++i) {
    if (GetChildViewAt(i)->ContainsNativeView(native_view))
      return true;
  }
  return false;
}

// TODO(beng): remove
RootView* View::GetRootView() {
  Widget* widget = GetWidget();
  return widget ? widget->GetRootView() : NULL;
}

// Size and disposition --------------------------------------------------------

void View::SetBounds(int x, int y, int width, int height) {
  SetBoundsRect(gfx::Rect(x, y, std::max(0, width), std::max(0, height)));
}

void View::SetBoundsRect(const gfx::Rect& bounds) {
  if (bounds == bounds_) {
    if (needs_layout_) {
      needs_layout_ = false;
      Layout();
      SchedulePaint();
    }
    return;
  }

  gfx::Rect prev = bounds_;
  bounds_ = bounds;
  BoundsChanged(prev);
}

void View::SetSize(const gfx::Size& size) {
  SetBounds(x(), y(), size.width(), size.height());
}

void View::SetPosition(const gfx::Point& position) {
  SetBounds(position.x(), position.y(), width(), height());
}

void View::SetX(int x) {
  SetBounds(x, y(), width(), height());
}

void View::SetY(int y) {
  SetBounds(x(), y, width(), height());
}

gfx::Rect View::GetContentsBounds() const {
  gfx::Rect contents_bounds(GetLocalBounds());
  if (border_.get()) {
    gfx::Insets insets;
    border_->GetInsets(&insets);
    contents_bounds.Inset(insets);
  }
  return contents_bounds;
}

gfx::Rect View::GetLocalBounds() const {
  return gfx::Rect(0, 0, width(), height());
}

gfx::Insets View::GetInsets() const {
  gfx::Insets insets;
  if (border_.get())
    border_->GetInsets(&insets);
  return insets;
}

gfx::Rect View::GetVisibleBounds() {
  if (!IsVisibleInRootView())
    return gfx::Rect();
  gfx::Rect vis_bounds(0, 0, width(), height());
  gfx::Rect ancestor_bounds;
  View* view = this;
  int root_x = 0;
  int root_y = 0;
  while (view != NULL && !vis_bounds.IsEmpty()) {
    root_x += view->GetMirroredX();
    root_y += view->y();
    vis_bounds.Offset(view->GetMirroredX(), view->y());
    View* ancestor = view->parent();
    if (ancestor != NULL) {
      ancestor_bounds.SetRect(0, 0, ancestor->width(),
        ancestor->height());
      vis_bounds = vis_bounds.Intersect(ancestor_bounds);
    } else if (!view->GetWidget()) {
      // If the view has no Widget, we're not visible. Return an empty rect.
      return gfx::Rect();
    }
    view = ancestor;
  }
  if (vis_bounds.IsEmpty())
    return vis_bounds;
  // Convert back to this views coordinate system.
  vis_bounds.Offset(-root_x, -root_y);
  return vis_bounds;
}

gfx::Rect View::GetScreenBounds() const {
  gfx::Point origin;
  View::ConvertPointToScreen(this, &origin);
  return gfx::Rect(origin, size());
}

gfx::Size View::GetPreferredSize() {
  if (layout_manager_.get())
    return layout_manager_->GetPreferredSize(this);
  return gfx::Size();
}

int View::GetBaseline() {
  return -1;
}

void View::SizeToPreferredSize() {
  gfx::Size prefsize = GetPreferredSize();
  if ((prefsize.width() != width()) || (prefsize.height() != height()))
    SetBounds(x(), y(), prefsize.width(), prefsize.height());
}

gfx::Size View::GetMinimumSize() {
  return GetPreferredSize();
}

int View::GetHeightForWidth(int w) {
  if (layout_manager_.get())
    return layout_manager_->GetPreferredHeightForWidth(this, w);
  return GetPreferredSize().height();
}

void View::SetVisible(bool flag) {
  if (flag != is_visible_) {
    // If the tab is currently visible, schedule paint to
    // refresh parent
    if (IsVisible())
      SchedulePaint();

    is_visible_ = flag;

    // This notifies all sub-views recursively.
    PropagateVisibilityNotifications(this, flag);

    // If we are newly visible, schedule paint.
    if (IsVisible())
      SchedulePaint();
  }
}

bool View::IsVisible() const {
  return is_visible_;
}

bool View::IsVisibleInRootView() const {
  return IsVisible() && parent() ? parent()->IsVisibleInRootView() : false;
}

void View::SetEnabled(bool state) {
  if (enabled_ != state) {
    enabled_ = state;
    SchedulePaint();
  }
}

bool View::IsEnabled() const {
  return enabled_;
}

// Transformations -------------------------------------------------------------

const ui::Transform& View::GetTransform() const {
  static const ui::Transform* no_op = ui::Transform::Create();
  if (transform_.get())
    return *transform_.get();
  return *no_op;
}

void View::SetRotation(float degree) {
  InitTransform();
  transform_->SetRotate(degree);
}

void View::SetScaleX(float x) {
  InitTransform();
  transform_->SetScaleX(x);
}

void View::SetScaleY(float y) {
  InitTransform();
  transform_->SetScaleY(y);
}

void View::SetScale(float x, float y) {
  InitTransform();
  transform_->SetScale(x, y);
}

void View::SetTranslateX(float x) {
  InitTransform();
  transform_->SetTranslateX(x);
}

void View::SetTranslateY(float y) {
  InitTransform();
  transform_->SetTranslateY(y);
}

void View::SetTranslate(float x, float y) {
  InitTransform();
  transform_->SetTranslate(x, y);
}

void View::ConcatRotation(float degree) {
  InitTransform();
  transform_->ConcatRotate(degree);
}

void View::ConcatScale(float x, float y) {
  InitTransform();
  transform_->ConcatScale(x, y);
}

void View::ConcatTranslate(float x, float y) {
  InitTransform();
  transform_->ConcatTranslate(x, y);
}

void View::ResetTransform() {
  transform_.reset(NULL);
  clip_x_ = clip_y_ = 0.0;
  canvas_.reset();
}


// RTL positioning -------------------------------------------------------------

gfx::Rect View::GetMirroredBounds() const {
  gfx::Rect bounds(bounds_);
  bounds.set_x(GetMirroredX());
  return bounds;
}

gfx::Point View::GetMirroredPosition() const {
  return gfx::Point(GetMirroredX(), y());
}

int View::GetMirroredX() const {
  return parent() ? parent()->GetMirroredXForRect(bounds_) : x();
}

int View::GetMirroredXForRect(const gfx::Rect& bounds) const {
  return base::i18n::IsRTL() ?
      (width() - bounds.x() - bounds.width()) : bounds.x();
}

int View::GetMirroredXInView(int x) const {
  return base::i18n::IsRTL() ? width() - x : x;
}

int View::GetMirroredXWithWidthInView(int x, int w) const {
  return base::i18n::IsRTL() ? width() - x - w : x;
}

// Layout ----------------------------------------------------------------------

void View::Layout() {
  needs_layout_ = false;

  // If we have a layout manager, let it handle the layout for us.
  if (layout_manager_.get())
    layout_manager_->Layout(this);

  // Make sure to propagate the Layout() call to any children that haven't
  // received it yet through the layout manager and need to be laid out. This
  // is needed for the case when the child requires a layout but its bounds
  // weren't changed by the layout manager. If there is no layout manager, we
  // just propagate the Layout() call down the hierarchy, so whoever receives
  // the call can take appropriate action.
  for (int i = 0, count = child_count(); i < count; ++i) {
    View* child = GetChildViewAt(i);
    if (child->needs_layout_ || !layout_manager_.get()) {
      child->needs_layout_ = false;
      child->Layout();
    }
  }
}

void View::InvalidateLayout() {
  // Always invalidate up. This is needed to handle the case of us already being
  // valid, but not our parent.
  needs_layout_ = true;
  if (parent_)
    parent_->InvalidateLayout();
}

LayoutManager* View::GetLayoutManager() const {
  return layout_manager_.get();
}

void View::SetLayoutManager(LayoutManager* layout_manager) {
  if (layout_manager_.get())
    layout_manager_->Uninstalled(this);

  layout_manager_.reset(layout_manager);
  if (layout_manager_.get())
    layout_manager_->Installed(this);
}

// Attributes ------------------------------------------------------------------

std::string View::GetClassName() const {
  return kViewClassName;
}

View* View::GetAncestorWithClassName(const std::string& name) {
  for (View* view = this; view; view = view->parent()) {
    if (view->GetClassName() == name)
      return view;
  }
  return NULL;
}

const View* View::GetViewByID(int id) const {
  if (id == id_)
    return const_cast<View*>(this);

  for (int i = 0, count = child_count(); i < count; ++i) {
    const View* view = GetChildViewAt(i)->GetViewByID(id);
    if (view)
      return view;
  }
  return NULL;
}

View* View::GetViewByID(int id) {
  return const_cast<View*>(const_cast<const View*>(this)->GetViewByID(id));
}

void View::SetID(int id) {
  id_ = id;
}

int View::GetID() const {
  return id_;
}

void View::SetGroup(int gid) {
  // Don't change the group id once it's set.
  DCHECK(group_ == -1 || group_ == gid);
  group_ = gid;
}

int View::GetGroup() const {
  return group_;
}

bool View::IsGroupFocusTraversable() const {
  return true;
}

void View::GetViewsWithGroup(int group_id, std::vector<View*>* out) {
  if (group_ == group_id)
    out->push_back(this);

  for (int i = 0, count = child_count(); i < count; ++i)
    GetChildViewAt(i)->GetViewsWithGroup(group_id, out);
}

View* View::GetSelectedViewForGroup(int group_id) {
  std::vector<View*> views;
  GetWidget()->GetRootView()->GetViewsWithGroup(group_id, &views);
  return views.empty() ? NULL : views[0];
}

// Coordinate conversion -------------------------------------------------------

// static
void View::ConvertPointToView(const View* src,
                              const View* dst,
                              gfx::Point* point) {
  ConvertPointToView(src, dst, point, true);
}

// static
void View::ConvertPointToWidget(const View* src, gfx::Point* p) {
  DCHECK(src);
  DCHECK(p);

  src->ConvertPointForAncestor(NULL, p);
}

// static
void View::ConvertPointFromWidget(const View* dest, gfx::Point* p) {
  DCHECK(dest);
  DCHECK(p);

  dest->ConvertPointFromAncestor(NULL, p);
}

// static
void View::ConvertPointToScreen(const View* src, gfx::Point* p) {
  DCHECK(src);
  DCHECK(p);

  // If the view is not connected to a tree, there's nothing we can do.
  const Widget* widget = src->GetWidget();
  if (widget) {
    ConvertPointToWidget(src, p);
    gfx::Rect r = widget->GetClientAreaScreenBounds();
    p->SetPoint(p->x() + r.x(), p->y() + r.y());
  }
}

gfx::Rect View::ConvertRectToParent(const gfx::Rect& rect) const {
  if (!transform_.get() || !transform_->HasChange())
    return rect;
  gfx::Rect x_rect = rect;
  transform_->TransformRect(&x_rect);
  return x_rect;
}

// Painting --------------------------------------------------------------------

void View::SchedulePaint() {
  SchedulePaintInRect(GetLocalBounds());
}

void View::SchedulePaintInRect(const gfx::Rect& rect) {
  if (!IsVisible())
    return;

  if (parent_) {
    // Translate the requested paint rect to the parent's coordinate system
    // then pass this notification up to the parent.
    gfx::Rect paint_rect = ConvertRectToParent(rect);
    paint_rect.Offset(GetMirroredPosition());
    parent_->SchedulePaintInRect(paint_rect);
  }
}

void View::Paint(gfx::Canvas* canvas) {
  if (!IsVisible())
    return;

  if (use_acceleration_when_possible &&
      transform_.get() && transform_->HasChange()) {
    // This view has a transformation. So this maintains its own canvas.
    if (!canvas_.get())
      canvas_.reset(gfx::Canvas::CreateCanvas(width(), height(), false));

    canvas = canvas_.get();
  } else {
    // We're going to modify the canvas, save its state first.
    canvas->Save();

    // Paint this View and its children, setting the clip rect to the bounds
    // of this View and translating the origin to the local bounds' top left
    // point.
    //
    // Note that the X (or left) position we pass to ClipRectInt takes into
    // consideration whether or not the view uses a right-to-left layout so that
    // we paint our view in its mirrored position if need be.
    if (!canvas->ClipRectInt(GetMirroredX(), y(),
                            width() - static_cast<int>(clip_x_),
                            height() - static_cast<int>(clip_y_))) {
      canvas->Restore();
      return;
    }
    // Non-empty clip, translate the graphics such that 0,0 corresponds to
    // where this view is located (related to its parent).
    canvas->TranslateInt(GetMirroredX(), y());

    if (transform_.get() && transform_->HasChange())
      canvas->Transform(*transform_.get());
  }

  // If the View we are about to paint requested the canvas to be flipped, we
  // should change the transform appropriately.
  canvas->Save();
  if (FlipCanvasOnPaintForRTLUI()) {
    canvas->TranslateInt(width(), 0);
    canvas->ScaleInt(-1, 1);
  }

  OnPaint(canvas);

  // We must undo the canvas mirroring once the View is done painting so that
  // we don't pass the canvas with the mirrored transform to Views that
  // didn't request the canvas to be flipped.
  canvas->Restore();

  PaintChildren(canvas);

  if (canvas == canvas_.get()) {
    texture_id_ = canvas->GetTextureID();

    // TODO(sadrul): Make sure the Widget's compositor tree updates itself?
  } else {
    // Restore the canvas's original transform.
    canvas->Restore();
  }
}

void View::PaintNow() {
  if (!IsVisible())
    return;

  if (parent())
    parent()->PaintNow();
}



ThemeProvider* View::GetThemeProvider() const {
  const Widget* widget = GetWidget();
  return widget ? widget->GetThemeProvider() : NULL;
}

// Accelerated Painting --------------------------------------------------------

// static
void View::set_use_acceleration_when_possible(bool use) {
  use_acceleration_when_possible = use;
}

// Input -----------------------------------------------------------------------

View* View::GetEventHandlerForPoint(const gfx::Point& point) {
  // Walk the child Views recursively looking for the View that most
  // tightly encloses the specified point.
  for (int i = child_count() - 1; i >= 0; --i) {
    View* child = GetChildViewAt(i);
    if (!child->IsVisible())
      continue;

    gfx::Point point_in_child_coords(point);
    View::ConvertPointToView(this, child, &point_in_child_coords);
    if (child->HitTest(point_in_child_coords))
      return child->GetEventHandlerForPoint(point_in_child_coords);
  }
  return this;
}

gfx::NativeCursor View::GetCursorForPoint(ui::EventType event_type,
                                          const gfx::Point& p) {
  return NULL;
}

bool View::HitTest(const gfx::Point& l) const {
  if (l.x() >= 0 && l.x() < width() && l.y() >= 0 && l.y() < height()) {
    if (HasHitTestMask()) {
      gfx::Path mask;
      GetHitTestMask(&mask);
      // TODO: can this use SkRegion's contains instead?
#if defined(OS_WIN)
      base::win::ScopedRegion rgn(mask.CreateNativeRegion());
      return !!PtInRegion(rgn, l.x(), l.y());
#elif defined(TOOLKIT_USES_GTK)
      ui::ScopedRegion rgn(mask.CreateNativeRegion());
      return gdk_region_point_in(rgn.Get(), l.x(), l.y());
#endif
    }
    // No mask, but inside our bounds.
    return true;
  }
  // Outside our bounds.
  return false;
}

bool View::OnMousePressed(const MouseEvent& event) {
  return false;
}

bool View::OnMouseDragged(const MouseEvent& event) {
  return false;
}

void View::OnMouseReleased(const MouseEvent& event, bool canceled) {
}

void View::OnMouseMoved(const MouseEvent& event) {
}

void View::OnMouseEntered(const MouseEvent& event) {
}

void View::OnMouseExited(const MouseEvent& event) {
}

#if defined(TOUCH_UI)
View::TouchStatus View::OnTouchEvent(const TouchEvent& event) {
  DVLOG(1) << "visited the OnTouchEvent";
  return TOUCH_STATUS_UNKNOWN;
}
#endif

void View::SetMouseHandler(View *new_mouse_handler) {
  // It is valid for new_mouse_handler to be NULL
  if (parent_)
    parent_->SetMouseHandler(new_mouse_handler);
}

bool View::OnKeyPressed(const KeyEvent& event) {
  return false;
}

bool View::OnKeyReleased(const KeyEvent& event) {
  return false;
}

bool View::OnMouseWheel(const MouseWheelEvent& event) {
  return false;
}

// Accelerators ----------------------------------------------------------------

void View::AddAccelerator(const Accelerator& accelerator) {
  if (!accelerators_.get())
    accelerators_.reset(new std::vector<Accelerator>());

  std::vector<Accelerator>::iterator iter =
      std::find(accelerators_->begin(), accelerators_->end(), accelerator);
  DCHECK(iter == accelerators_->end())
      << "Registering the same accelerator multiple times";

  accelerators_->push_back(accelerator);
  RegisterPendingAccelerators();
}

void View::RemoveAccelerator(const Accelerator& accelerator) {
  std::vector<Accelerator>::iterator iter;
  if (!accelerators_.get() ||
      ((iter = std::find(accelerators_->begin(), accelerators_->end(),
          accelerator)) == accelerators_->end())) {
    NOTREACHED() << "Removing non-existing accelerator";
    return;
  }

  size_t index = iter - accelerators_->begin();
  accelerators_->erase(iter);
  if (index >= registered_accelerator_count_) {
    // The accelerator is not registered to FocusManager.
    return;
  }
  --registered_accelerator_count_;

  // Providing we are attached to a Widget and registered with a focus manager,
  // we should de-register from that focus manager now.
  if (GetWidget() && accelerator_focus_manager_)
    accelerator_focus_manager_->UnregisterAccelerator(accelerator, this);
}

void View::ResetAccelerators() {
  if (accelerators_.get())
    UnregisterAccelerators(false);
}

bool View::AcceleratorPressed(const Accelerator& accelerator) {
  return false;
}

// Focus -----------------------------------------------------------------------

bool View::HasFocus() {
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager)
    return focus_manager->GetFocusedView() == this;
  return false;
}

View* View::GetNextFocusableView() {
  return next_focusable_view_;
}

const View* View::GetNextFocusableView() const {
  return next_focusable_view_;
}

View* View::GetPreviousFocusableView() {
  return previous_focusable_view_;
}

void View::SetNextFocusableView(View* view) {
  view->previous_focusable_view_ = this;
  next_focusable_view_ = view;
}

void View::SetFocusable(bool focusable) {
  focusable_ = focusable;
}

bool View::IsFocusableInRootView() const {
  return IsFocusable() && IsVisibleInRootView();
}

bool View::IsAccessibilityFocusableInRootView() const {
  return (focusable_ || accessibility_focusable_) && IsEnabled() &&
    IsVisibleInRootView();
}

void View::set_accessibility_focusable(bool accessibility_focusable) {
  accessibility_focusable_ = accessibility_focusable;
}

FocusManager* View::GetFocusManager() {
  Widget* widget = GetWidget();
  return widget ? widget->GetFocusManager() : NULL;
}

void View::RequestFocus() {
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager && IsFocusableInRootView())
    focus_manager->SetFocusedView(this);
}

bool View::SkipDefaultKeyEventProcessing(const KeyEvent& event) {
  return false;
}

FocusTraversable* View::GetFocusTraversable() {
  return NULL;
}

FocusTraversable* View::GetPaneFocusTraversable() {
  return NULL;
}

// Tooltips --------------------------------------------------------------------

bool View::GetTooltipText(const gfx::Point& p, std::wstring* tooltip) {
  return false;
}

bool View::GetTooltipTextOrigin(const gfx::Point& p, gfx::Point* loc) {
  return false;
}

// Context menus ---------------------------------------------------------------

void View::SetContextMenuController(ContextMenuController* menu_controller) {
  context_menu_controller_ = menu_controller;
}

void View::ShowContextMenu(const gfx::Point& p, bool is_mouse_gesture) {
  if (!context_menu_controller_)
    return;

  context_menu_controller_->ShowContextMenuForView(this, p, is_mouse_gesture);
}

// Drag and drop ---------------------------------------------------------------

void View::SetDragController(DragController* drag_controller) {
  drag_controller_ = drag_controller;
}

DragController* View::GetDragController() {
  return drag_controller_;
}

bool View::GetDropFormats(
      int* formats,
      std::set<OSExchangeData::CustomFormat>* custom_formats) {
  return false;
}

bool View::AreDropTypesRequired() {
  return false;
}

bool View::CanDrop(const OSExchangeData& data) {
  // TODO(sky): when I finish up migration, this should default to true.
  return false;
}

void View::OnDragEntered(const DropTargetEvent& event) {
}

int View::OnDragUpdated(const DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_NONE;
}

void View::OnDragExited() {
}

int View::OnPerformDrop(const DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_NONE;
}

void View::OnDragDone() {
}

// static
bool View::ExceededDragThreshold(int delta_x, int delta_y) {
  return (abs(delta_x) > GetHorizontalDragThreshold() ||
          abs(delta_y) > GetVerticalDragThreshold());
}

// Scrolling -------------------------------------------------------------------

void View::ScrollRectToVisible(const gfx::Rect& rect) {
  // We must take RTL UI mirroring into account when adjusting the position of
  // the region.
  if (parent()) {
    gfx::Rect scroll_rect(rect);
    scroll_rect.Offset(GetMirroredX(), y());
    parent()->ScrollRectToVisible(scroll_rect);
  }
}

int View::GetPageScrollIncrement(ScrollView* scroll_view,
                                 bool is_horizontal, bool is_positive) {
  return 0;
}

int View::GetLineScrollIncrement(ScrollView* scroll_view,
                                 bool is_horizontal, bool is_positive) {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// View, protected:

// Size and disposition --------------------------------------------------------

void View::OnBoundsChanged(const gfx::Rect& previous_bounds) {
}

void View::PreferredSizeChanged() {
  InvalidateLayout();
  if (parent_)
    parent_->ChildPreferredSizeChanged(this);
}

bool View::NeedsNotificationWhenVisibleBoundsChange() const {
  return false;
}

void View::OnVisibleBoundsChanged() {
}

// Tree operations -------------------------------------------------------------

void View::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
}

void View::VisibilityChanged(View* starting_from, bool is_visible) {
}

void View::NativeViewHierarchyChanged(bool attached,
                                      gfx::NativeView native_view,
                                      RootView* root_view) {
  FocusManager* focus_manager = GetFocusManager();
  if (!accelerator_registration_delayed_ &&
      accelerator_focus_manager_ &&
      accelerator_focus_manager_ != focus_manager) {
    UnregisterAccelerators(true);
    accelerator_registration_delayed_ = true;
  }
  if (accelerator_registration_delayed_ && attached) {
    if (focus_manager) {
      RegisterPendingAccelerators();
      accelerator_registration_delayed_ = false;
    }
  }
}

// Painting --------------------------------------------------------------------

void View::PaintChildren(gfx::Canvas* canvas) {
  for (int i = 0, count = child_count(); i < count; ++i) {
    View* child = GetChildViewAt(i);
    if (!child) {
      NOTREACHED() << "Should not have a NULL child View for index in bounds";
      continue;
    }
    child->Paint(canvas);
  }
}

void View::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
  OnPaintFocusBorder(canvas);
  OnPaintBorder(canvas);
}

void View::OnPaintBackground(gfx::Canvas* canvas) {
  if (background_.get())
    background_->Paint(canvas, this);
}

void View::OnPaintBorder(gfx::Canvas* canvas) {
  if (border_.get())
    border_->Paint(*this, canvas);
}

void View::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (IsFocusable() || IsAccessibilityFocusableInRootView()))
    canvas->DrawFocusRect(0, 0, width(), height());
}

// Accelerated Painting --------------------------------------------------------

void View::PaintComposite(ui::Compositor* compositor) {
  compositor->SaveTransform();

  // TODO(sad): Push a transform matrix for the offset and bounds of the view?
  if (texture_id_)
    compositor->DrawTextureWithTransform(texture_id_, GetTransform());

  for (int i = 0, count = child_count(); i < count; ++i) {
    View* child = GetChildViewAt(i);
    child->PaintComposite(compositor);
  }

  compositor->RestoreTransform();
}

// Input -----------------------------------------------------------------------

bool View::HasHitTestMask() const {
  return false;
}

void View::GetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);
}

// Focus -----------------------------------------------------------------------

bool View::IsFocusable() const {
  return focusable_ && IsEnabled() && IsVisible();
}

void View::OnFocus() {
  // TODO(beng): Investigate whether it's possible for us to move this to
  //             Focus().
  // By default, we clear the native focus. This ensures that no visible native
  // view as the focus and that we still receive keyboard inputs.
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->ClearNativeFocus();

  // TODO(beng): Investigate whether it's possible for us to move this to
  //             Focus().
  // Notify assistive technologies of the focus change.
  GetWidget()->NotifyAccessibilityEvent(
      this, ui::AccessibilityTypes::EVENT_FOCUS, true);
}

void View::OnBlur() {
}

void View::Focus() {
  SchedulePaint();
  OnFocus();
}

void View::Blur() {
  SchedulePaint();
  OnBlur();
}

// Tooltips --------------------------------------------------------------------

void View::TooltipTextChanged() {
  Widget* widget = GetWidget();
  if (widget)
    widget->native_widget()->GetTooltipManager()->TooltipTextChanged(this);
}

// Context menus ---------------------------------------------------------------

gfx::Point View::GetKeyboardContextMenuLocation() {
  gfx::Rect vis_bounds = GetVisibleBounds();
  gfx::Point screen_point(vis_bounds.x() + vis_bounds.width() / 2,
                          vis_bounds.y() + vis_bounds.height() / 2);
  ConvertPointToScreen(this, &screen_point);
  return screen_point;
}

// Drag and drop ---------------------------------------------------------------

int View::GetDragOperations(const gfx::Point& press_pt) {
  return drag_controller_ ?
      drag_controller_->GetDragOperationsForView(this, press_pt) :
      ui::DragDropTypes::DRAG_NONE;
}

void View::WriteDragData(const gfx::Point& press_pt, OSExchangeData* data) {
  DCHECK(drag_controller_);
  drag_controller_->WriteDragDataForView(this, press_pt, data);
}

bool View::InDrag() {
  Widget* widget = GetWidget();
  return widget ? widget->dragged_view() == this : false;
}

////////////////////////////////////////////////////////////////////////////////
// View, private:

// DropInfo --------------------------------------------------------------------

void View::DragInfo::Reset() {
  possible_drag = false;
  start_pt = gfx::Point();
}

void View::DragInfo::PossibleDrag(const gfx::Point& p) {
  possible_drag = true;
  start_pt = p;
}

// Tree operations -------------------------------------------------------------

void View::DoRemoveChildView(View* view,
                             bool update_focus_cycle,
                             bool update_tool_tip,
                             bool delete_removed_view) {
  DCHECK(view);
  const ViewVector::iterator i = find(children_.begin(), children_.end(), view);
  scoped_ptr<View> view_to_be_deleted;
  if (i != children_.end()) {
    if (update_focus_cycle) {
      // Let's remove the view from the focus traversal.
      View* next_focusable = view->next_focusable_view_;
      View* prev_focusable = view->previous_focusable_view_;
      if (prev_focusable)
        prev_focusable->next_focusable_view_ = next_focusable;
      if (next_focusable)
        next_focusable->previous_focusable_view_ = prev_focusable;
    }

    if (GetWidget())
      UnregisterChildrenForVisibleBoundsNotification(view);
    view->PropagateRemoveNotifications(this);
    view->SetParent(NULL);

    if (delete_removed_view && view->IsParentOwned())
      view_to_be_deleted.reset(view);

    children_.erase(i);
  }

  if (update_tool_tip)
    UpdateTooltip();

  if (layout_manager_.get())
    layout_manager_->ViewRemoved(this, view);
}

void View::SetParent(View* parent) {
  if (parent != parent_)
    parent_ = parent;
}

void View::PropagateRemoveNotifications(View* parent) {
  for (int i = 0, count = child_count(); i < count; ++i)
    GetChildViewAt(i)->PropagateRemoveNotifications(parent);

  for (View* v = this; v; v = v->parent())
    v->ViewHierarchyChangedImpl(true, false, parent, this);
}

void View::PropagateAddNotifications(View* parent, View* child) {
  for (int i = 0, count = child_count(); i < count; ++i)
    GetChildViewAt(i)->PropagateAddNotifications(parent, child);
  ViewHierarchyChangedImpl(true, true, parent, child);
}

void View::PropagateNativeViewHierarchyChanged(bool attached,
                                               gfx::NativeView native_view,
                                               RootView* root_view) {
  for (int i = 0, count = child_count(); i < count; ++i)
    GetChildViewAt(i)->PropagateNativeViewHierarchyChanged(attached,
                                                           native_view,
                                                           root_view);
  NativeViewHierarchyChanged(attached, native_view, root_view);
}

void View::ViewHierarchyChangedImpl(bool register_accelerators,
                                    bool is_add,
                                    View* parent,
                                    View* child) {
  if (register_accelerators) {
    if (is_add) {
      // If you get this registration, you are part of a subtree that has been
      // added to the view hierarchy.
      if (GetFocusManager()) {
        RegisterPendingAccelerators();
      } else {
        // Delay accelerator registration until visible as we do not have
        // focus manager until then.
        accelerator_registration_delayed_ = true;
      }
    } else {
      if (child == this)
        UnregisterAccelerators(true);
    }
  }

  ViewHierarchyChanged(is_add, parent, child);
  parent->needs_layout_ = true;
}

// Size and disposition --------------------------------------------------------

void View::PropagateVisibilityNotifications(View* start, bool is_visible) {
  for (int i = 0, count = child_count(); i < count; ++i)
    GetChildViewAt(i)->PropagateVisibilityNotifications(start, is_visible);
  VisibilityChangedImpl(start, is_visible);
}

void View::VisibilityChangedImpl(View* starting_from, bool is_visible) {
  if (is_visible)
    RegisterPendingAccelerators();
  else
    UnregisterAccelerators(true);
  VisibilityChanged(starting_from, is_visible);
}

void View::BoundsChanged(const gfx::Rect& previous_bounds) {
  if (canvas_.get())
    canvas_.reset(gfx::Canvas::CreateCanvas(width(), height(), false));

  if (parent_) {
    parent_->SchedulePaintInRect(previous_bounds);
    parent_->SchedulePaintInRect(bounds_);
  } else {
    // Previous bounds has no meaning to an orphan. This should only happen
    // when the View is a RootView.
    SchedulePaintInRect(gfx::Rect(0, 0, bounds_.width(), bounds_.height()));
  }

  OnBoundsChanged(previous_bounds);

  if (previous_bounds.size() != size()) {
    needs_layout_ = false;
    Layout();
  }

  // Notify interested Views that visible bounds within the root view may have
  // changed.
  if (descendants_to_notify_.get()) {
    for (std::vector<View*>::iterator i = descendants_to_notify_->begin();
         i != descendants_to_notify_->end(); ++i) {
      (*i)->OnVisibleBoundsChanged();
    }
  }
}

// static
void View::RegisterChildrenForVisibleBoundsNotification(View* view) {
  if (view->NeedsNotificationWhenVisibleBoundsChange())
    view->RegisterForVisibleBoundsNotification();
  for (int i = 0; i < view->child_count(); ++i)
    RegisterChildrenForVisibleBoundsNotification(view->GetChildViewAt(i));
}

// static
void View::UnregisterChildrenForVisibleBoundsNotification(View* view) {
  if (view->NeedsNotificationWhenVisibleBoundsChange())
    view->UnregisterForVisibleBoundsNotification();
  for (int i = 0; i < view->child_count(); ++i)
    UnregisterChildrenForVisibleBoundsNotification(view->GetChildViewAt(i));
}

void View::RegisterForVisibleBoundsNotification() {
  if (registered_for_visible_bounds_notification_)
    return;
  registered_for_visible_bounds_notification_ = true;
  View* ancestor = parent();
  while (ancestor) {
    ancestor->AddDescendantToNotify(this);
    ancestor = ancestor->parent();
  }
}

void View::UnregisterForVisibleBoundsNotification() {
  if (!registered_for_visible_bounds_notification_)
    return;
  registered_for_visible_bounds_notification_ = false;
  View* ancestor = parent();
  while (ancestor) {
    ancestor->RemoveDescendantToNotify(this);
    ancestor = ancestor->parent();
  }
}

void View::AddDescendantToNotify(View* view) {
  DCHECK(view);
  if (!descendants_to_notify_.get())
    descendants_to_notify_.reset(new ViewVector);
  descendants_to_notify_->push_back(view);
}

void View::RemoveDescendantToNotify(View* view) {
  DCHECK(view && descendants_to_notify_.get());
  ViewVector::iterator i = find(descendants_to_notify_->begin(),
                                descendants_to_notify_->end(),
                                view);
  DCHECK(i != descendants_to_notify_->end());
  descendants_to_notify_->erase(i);
  if (descendants_to_notify_->empty())
    descendants_to_notify_.reset();
}

// Transformations -------------------------------------------------------------

void View::InitTransform() {
  if (!transform_.get())
    transform_.reset(ui::Transform::Create());
}

// Coordinate conversion -------------------------------------------------------

// static
void View::ConvertPointToView(const View* src,
                              const View* dst,
                              gfx::Point* point,
                              bool try_other_direction) {
  // src can be NULL
  DCHECK(dst);
  DCHECK(point);

  if (src == NULL || src->Contains(dst)) {
    dst->ConvertPointFromAncestor(src, point);
    if (!src) {
      const Widget* widget = dst->GetWidget();
      if (widget) {
        gfx::Rect b = widget->GetClientAreaScreenBounds();
        point->SetPoint(point->x() - b.x(), point->y() - b.y());
      }
    }
  } else if (src && try_other_direction) {
    if (!src->ConvertPointForAncestor(dst, point)) {
      // |src| is not an ancestor of |dst|, and |dst| is not an ancestor of
      // |src| either. At this stage, |point| is in the widget's coordinate
      // system. So convert from the widget's to |dst|'s coordinate system now.
      ConvertPointFromWidget(dst, point);
    }
  }
}

bool View::ConvertPointForAncestor(const View* ancestor,
                                   gfx::Point* point) const {
  scoped_ptr<ui::Transform> trans(ui::Transform::Create());

  // TODO(sad): Have some way of caching the transformation results.

  const View* v = this;
  for (; v && v != ancestor; v = v->parent()) {
    if (v->GetTransform().HasChange()) {
      if (!trans->ConcatTransform(v->GetTransform()))
        return false;
    }
    trans->ConcatTranslate(static_cast<float>(v->GetMirroredX()),
                           static_cast<float>(v->y()));
  }

  if (trans->HasChange()) {
    trans->TransformPoint(point);
  }

  return v == ancestor;
}

bool View::ConvertPointFromAncestor(const View* ancestor,
                                    gfx::Point* point) const {
  scoped_ptr<ui::Transform> trans(ui::Transform::Create());

  const View* v = this;
  for (; v && v != ancestor; v = v->parent()) {
    if (v->GetTransform().HasChange()) {
      if (!trans->ConcatTransform(v->GetTransform()))
        return false;
    }
    trans->ConcatTranslate(static_cast<float>(v->GetMirroredX()),
                           static_cast<float>(v->y()));
  }

  if (trans->HasChange()) {
    trans->TransformPointReverse(point);
  }

  return v == ancestor;
}

// Input -----------------------------------------------------------------------

bool View::ProcessMousePressed(const MouseEvent& event, DragInfo* drag_info) {
  const bool enabled = IsEnabled();
  int drag_operations =
      (enabled && event.IsOnlyLeftMouseButton() && HitTest(event.location())) ?
      GetDragOperations(event.location()) : 0;
  ContextMenuController* context_menu_controller = event.IsRightMouseButton() ?
      context_menu_controller_ : 0;

  const bool result = OnMousePressed(event);
  // WARNING: we may have been deleted, don't use any View variables.

  if (!enabled)
    return result;

  if (drag_operations != ui::DragDropTypes::DRAG_NONE) {
    drag_info->PossibleDrag(event.location());
    return true;
  }
  return !!context_menu_controller || result;
}

bool View::ProcessMouseDragged(const MouseEvent& event, DragInfo* drag_info) {
  // Copy the field, that way if we're deleted after drag and drop no harm is
  // done.
  ContextMenuController* context_menu_controller = context_menu_controller_;
  const bool possible_drag = drag_info->possible_drag;
  if (possible_drag && ExceededDragThreshold(
      drag_info->start_pt.x() - event.x(),
      drag_info->start_pt.y() - event.y())) {
    if (!drag_controller_ ||
        drag_controller_->CanStartDragForView(
            this, drag_info->start_pt, event.location()))
      DoDrag(event, drag_info->start_pt);
  } else {
    if (OnMouseDragged(event))
      return true;
    // Fall through to return value based on context menu controller.
  }
  // WARNING: we may have been deleted.
  return (context_menu_controller != NULL) || possible_drag;
}

void View::ProcessMouseReleased(const MouseEvent& event, bool canceled) {
  if (!canceled && context_menu_controller_ && event.IsOnlyRightMouseButton()) {
    // Assume that if there is a context menu controller we won't be deleted
    // from mouse released.
    gfx::Point location(event.location());
    OnMouseReleased(event, canceled);
    if (HitTest(location)) {
      ConvertPointToScreen(this, &location);
      ShowContextMenu(location, true);
    }
  } else {
    OnMouseReleased(event, canceled);
  }
  // WARNING: we may have been deleted.
}

#if defined(TOUCH_UI)
View::TouchStatus View::ProcessTouchEvent(const TouchEvent& event) {
  // TODO(rjkroege): Implement a grab scheme similar to as
  // as is found in MousePressed.
  return OnTouchEvent(event);
}
#endif

// Accelerators ----------------------------------------------------------------

void View::RegisterPendingAccelerators() {
  if (!accelerators_.get() ||
      registered_accelerator_count_ == accelerators_->size()) {
    // No accelerators are waiting for registration.
    return;
  }

  if (!GetWidget()) {
    // The view is not yet attached to a widget, defer registration until then.
    return;
  }

  accelerator_focus_manager_ = GetFocusManager();
  if (!accelerator_focus_manager_) {
    // Some crash reports seem to show that we may get cases where we have no
    // focus manager (see bug #1291225).  This should never be the case, just
    // making sure we don't crash.

    // TODO(jcampan): This fails for a view under WidgetGtk with TYPE_CHILD.
    // (see http://crbug.com/21335) reenable NOTREACHED assertion and
    // verify accelerators works as expected.
#if defined(OS_WIN)
    NOTREACHED();
#endif
    return;
  }
  // Only register accelerators if we are visible.
  if (!IsVisibleInRootView())
    return;
  std::vector<Accelerator>::const_iterator iter;
  for (iter = accelerators_->begin() + registered_accelerator_count_;
       iter != accelerators_->end(); ++iter) {
    accelerator_focus_manager_->RegisterAccelerator(*iter, this);
  }
  registered_accelerator_count_ = accelerators_->size();
}

void View::UnregisterAccelerators(bool leave_data_intact) {
  if (!accelerators_.get())
    return;

  if (GetWidget()) {
    if (accelerator_focus_manager_) {
      // We may not have a FocusManager if the window containing us is being
      // closed, in which case the FocusManager is being deleted so there is
      // nothing to unregister.
      accelerator_focus_manager_->UnregisterAccelerators(this);
      accelerator_focus_manager_ = NULL;
    }
    if (!leave_data_intact) {
      accelerators_->clear();
      accelerators_.reset();
    }
    registered_accelerator_count_ = 0;
  }
}

// Focus -----------------------------------------------------------------------

void View::InitFocusSiblings(View* v, int index) {
  int child_count = static_cast<int>(children_.size());

  if (child_count == 0) {
    v->next_focusable_view_ = NULL;
    v->previous_focusable_view_ = NULL;
  } else {
    if (index == child_count) {
      // We are inserting at the end, but the end of the child list may not be
      // the last focusable element. Let's try to find an element with no next
      // focusable element to link to.
      View* last_focusable_view = NULL;
      for (std::vector<View*>::iterator iter = children_.begin();
           iter != children_.end(); ++iter) {
          if (!(*iter)->next_focusable_view_) {
            last_focusable_view = *iter;
            break;
          }
      }
      if (last_focusable_view == NULL) {
        // Hum... there is a cycle in the focus list. Let's just insert ourself
        // after the last child.
        View* prev = children_[index - 1];
        v->previous_focusable_view_ = prev;
        v->next_focusable_view_ = prev->next_focusable_view_;
        prev->next_focusable_view_->previous_focusable_view_ = v;
        prev->next_focusable_view_ = v;
      } else {
        last_focusable_view->next_focusable_view_ = v;
        v->next_focusable_view_ = NULL;
        v->previous_focusable_view_ = last_focusable_view;
      }
    } else {
      View* prev = children_[index]->GetPreviousFocusableView();
      v->previous_focusable_view_ = prev;
      v->next_focusable_view_ = children_[index];
      if (prev)
        prev->next_focusable_view_ = v;
      children_[index]->previous_focusable_view_ = v;
    }
  }
}

// System events ---------------------------------------------------------------

void View::PropagateThemeChanged() {
  for (int i = child_count() - 1; i >= 0; --i)
    GetChildViewAt(i)->PropagateThemeChanged();
  OnThemeChanged();
}

void View::PropagateLocaleChanged() {
  for (int i = child_count() - 1; i >= 0; --i)
    GetChildViewAt(i)->PropagateLocaleChanged();
  OnLocaleChanged();
}

// Tooltips --------------------------------------------------------------------

void View::UpdateTooltip() {
  Widget* widget = GetWidget();
  // TODO(beng): The TooltipManager NULL check can be removed when we
  //             consolidate Init() methods and make views_unittests Init() all
  //             Widgets that it uses.
  if (widget && widget->native_widget()->GetTooltipManager())
    widget->native_widget()->GetTooltipManager()->UpdateTooltip();
}

// Drag and drop ---------------------------------------------------------------

void View::DoDrag(const MouseEvent& event, const gfx::Point& press_pt) {
  int drag_operations = GetDragOperations(press_pt);
  if (drag_operations == ui::DragDropTypes::DRAG_NONE)
    return;

  OSExchangeData data;
  WriteDragData(press_pt, &data);

  // Message the RootView to do the drag and drop. That way if we're removed
  // the RootView can detect it and avoid calling us back.
  GetWidget()->RunShellDrag(this, data, drag_operations);
}

}  // namespace views
