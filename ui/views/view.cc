// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view.h"

#include <algorithm>
#include <functional>

#include "gfx/canvas.h"
#include "gfx/point.h"
#include "gfx/size.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/views/events/context_menu_controller.h"
#include "ui/views/events/drag_controller.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/rendering/border.h"
#include "ui/views/widget/widget.h"

namespace ui {

namespace {

// Saves gfx::Canvas state upon construction and automatically restores it when
// it goes out of scope.
class ScopedCanvasState {
 public:
  explicit ScopedCanvasState(gfx::Canvas* canvas) : canvas_(canvas) {
    canvas_->Save();
  }
  ~ScopedCanvasState() {
    canvas_->Restore();
  }

 private:
  gfx::Canvas* canvas_;
  DISALLOW_COPY_AND_ASSIGN(ScopedCanvasState);
};

bool ExceededDragThreshold(const gfx::Point& press_point,
                           const gfx::Point& event_point) {
  // TODO(beng): implement
  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// View, public:

View::View()
    : parent_owned_(true),
      visible_(true),
      enabled_(true),
      id_(-1),
      group_(-1),
      parent_(NULL),
      focusable_(false),
      next_focusable_view_(NULL),
      prev_focusable_view_(NULL),
      context_menu_controller_(NULL),
      drag_controller_(NULL) {
}

View::~View() {
  if (parent_)
    parent_->RemoveChildView(this);

  ViewVector::const_iterator it = children_.begin();
  for (; it != children_.end(); ++it) {
    (*it)->parent_ = NULL;
    if ((*it)->parent_owned())
      delete *it;
  }
}

// Size and disposition --------------------------------------------------------

void View::SetBounds(int x, int y, int width, int height) {
  SetBoundsRect(gfx::Rect(x, y, std::max(0, width), std::max(0, height)));
}

void View::SetBoundsRect(const gfx::Rect& bounds) {
  gfx::Rect old_bounds = bounds_;
  bounds_ = bounds;
  // TODO(beng): investigate usage of needs_layout_ in old View code.
  if (old_bounds != bounds_) {
    OnBoundsChanged();
    Layout();
  }
}

gfx::Rect View::GetVisibleBounds() const {
  // TODO(beng):
  return bounds();
}

void View::SetSize(const gfx::Size& size) {
  SetBounds(x(), y(), size.width(), size.height());
}

void View::SetPosition(const gfx::Point& position) {
  SetBounds(position.x(), position.y(), width(), height());
}

void View::SetBorder(Border* border) {
  border_.reset(border);
}

gfx::Rect View::GetContentsBounds() const {
  if (border_.get()) {
    return gfx::Rect(
        border_->insets().left(), border_->insets().top(),
        width() - border_->insets().right() - border_->insets().left(),
        height() - border_->insets().bottom() - border_->insets().top());
  }
  return gfx::Rect(0, 0, width(), height());
}

void View::OnBoundsChanged() {
}

gfx::Size View::GetPreferredSize() const {
  return gfx::Size();
}

gfx::Size View::GetMinimumSize() const {
  return GetPreferredSize();
}

void View::SetLayoutManager(LayoutManager* layout_manager) {
  layout_manager_.reset(layout_manager);
}

void View::Layout() {
  if (layout_manager_.get()) {
    // Layout Manager handles laying out children.
    layout_manager_->Layout(this);
  } else {
    // We handle laying out our own children.
    ViewVector::iterator it = children_.begin();
    for (; it != children_.end(); ++it)
      (*it)->Layout();
  }
  // TODO(beng): needs_layout_? SchedulePaint()?
}

void View::SetVisible(bool visible) {
  if (visible != visible_) {
    visible_ = visible;

    // InvaldateRect() checks for view visibility before proceeding, so we need
    // to ask the parent to invalidate our bounds.
    if (parent_)
      parent_->InvalidateRect(bounds_);
  }
}

void View::SetEnabled(bool enabled) {
  if (enabled != enabled_) {
    enabled_ = enabled;
    Invalidate();
  }
}

// Attributes ------------------------------------------------------------------

View* View::GetViewById(int id) const {
  if (id_ == id)
    return const_cast<View*>(this);
  ViewVector::const_iterator it = children_.begin();
  for (; it != children_.end(); ++it) {
    View* view = (*it)->GetViewById(id);
    if (view)
      return view;
  }
  return NULL;
}

void View::GetViewsWithGroup(int group, ViewVector* vec) const {
  if (group_ == group)
    vec->push_back(const_cast<View*>(this));
  ViewVector::const_iterator it = children_.begin();
  for (; it != children_.end(); ++it)
    (*it)->GetViewsWithGroup(group, vec);
}

View* View::GetSelectedViewForGroup(int group_id) {
  // TODO(beng): implementme
  return NULL;
}

// Coordinate conversion -------------------------------------------------------

// static
void View::ConvertPointToView(View* source, View* target, gfx::Point* point) {
  View* inner = NULL;
  View* outer = NULL;
  if (source->Contains(target)) {
    inner = target;
    outer = source;
  } else if (target->Contains(source)) {
    inner = source;
    outer = target;
  } // Note that we cannot do a plain "else" here since |source| and |target|
    // may be in different hierarchies with no relation.

  if (inner && outer) {
    gfx::Point offset;
    View* temp = inner;
    while (temp != outer) {
      offset.Offset(temp->x(), temp->y());
      temp = temp->parent();
    }
    // When target is contained by source, we need to subtract the offset.
    // When source is contained by target, we need to add the fofset.
    int multiplier = inner == target ? -1 : 1;
    point->Offset(multiplier * offset.x(), multiplier * offset.y());
  }
}

// static
void View::ConvertPointToScreen(View* source, gfx::Point* point) {
  Widget* widget = source->GetWidget();
  if (widget) {
    ConvertPointToWidget(source, point);
    gfx::Rect r = widget->GetClientAreaScreenBounds();
    point->Offset(r.x(), r.y());
  }
}

// static
void View::ConvertPointToWidget(View* source, gfx::Point* point) {
  for (View* v = source; v; v = v->parent())
    point->Offset(v->x(), v->y());
}

// Tree operations -------------------------------------------------------------

Widget* View::GetWidget() const {
  return parent_ ? parent_->GetWidget() : NULL;
}

void View::AddChildView(View* view) {
  AddChildViewAt(view, children_.size());
}

void View::AddChildViewAt(View* view, size_t index) {
  CHECK(view != this) << "A view cannot be its own child.";

  // Remove the child from its current parent if any.
  if (view->parent())
    view->parent()->RemoveChildView(view);

  // TODO(beng): Move focus initialization to FocusManager.
  InitFocusSiblings(view, index);

  children_.insert(children_.begin() + index, view);
  view->parent_ = this;

  // Notify the hierarchy.
  NotifyHierarchyChanged(this, view, true);

  // TODO(beng): Notify other objects like tooltip, layout manager, etc.
  //             Figure out RegisterChildrenForVisibleBoundsNotification.
}

View* View::RemoveChildView(View* view) {
  ViewVector::iterator it = find(children_.begin(), children_.end(), view);
  if (it != children_.end()) {
    View* old_parent = view->parent_;
    view->parent_ = NULL;
    children_.erase(it);
    NotifyHierarchyChanged(old_parent, view, false);
  }

  // TODO(beng): Notify other objects like tooltip, layout manager, etc.
  return view;
}

void View::RemoveAllChildViews(bool delete_children) {
  // TODO(beng): use for_each.
  ViewVector::iterator it = children_.begin();
  while (it != children_.end()) {
    View* v = RemoveChildView(*it);
    if (delete_children)
      delete v;
    // TODO(beng): view deletion is actually more complicated in the old view.cc
    //             figure out why. (it uses a ScopedVector to accumulate a list
    //             of views to delete).
  }
}

View* View::GetChildViewAt(size_t index) {
  CHECK(index < child_count());
  return children_[index];
}

bool View::Contains(View* child) {
  while (child) {
    if (child == this)
      return true;
    child = child->parent();
  }
  return false;
}

// Painting --------------------------------------------------------------------

void View::Invalidate() {
  InvalidateRect(gfx::Rect(0, 0, width(), height()));
}

void View::InvalidateRect(const gfx::Rect& invalid_rect) {
  if (!visible_)
    return;

  if (parent_) {
    gfx::Rect r = invalid_rect;
    r.Offset(bounds_.origin());
    parent_->InvalidateRect(r);
  }
}

// Input -----------------------------------------------------------------------

bool View::HitTest(const gfx::Point& point) const {
  // TODO(beng): Hit test mask support.
  return gfx::Rect(0, 0, width(), height()).Contains(point);
}

// Accelerators ----------------------------------------------------------------

void View::AddAccelerator(const Accelerator& accelerator) {
}

void View::RemoveAccelerator(const Accelerator& accelerator) {
}

void View::RemoveAllAccelerators() {
}

// Focus -----------------------------------------------------------------------

FocusManager* View::GetFocusManager() const {
  Widget* widget = GetWidget();
  return widget ? widget->GetFocusManager() : NULL;
}

FocusTraversable* View::GetFocusTraversable() const {
  return NULL;
}

View* View::GetNextFocusableView() const {
  return NULL;
}

View* View::GetPreviousFocusableView() const {
  return NULL;
}

bool View::IsFocusable() const {
  return focusable_ && enabled_ && visible_;
}

bool View::HasFocus() const {
  FocusManager* focus_manager = GetFocusManager();
  return focus_manager ? focus_manager->focused_view() == this : false;
}

void View::RequestFocus() {
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager && focus_manager->focused_view() != this)
    focus_manager->SetFocusedView(this);
}

// Resources -------------------------------------------------------------------

ThemeProvider* View::GetThemeProvider() const {
  Widget* widget = GetWidget();
  return widget ? widget->GetThemeProvider() : NULL;
}

////////////////////////////////////////////////////////////////////////////////
// View, protected:

// Tree operations -------------------------------------------------------------

void View::OnViewAdded(View* parent, View* child) {
}

void View::OnViewRemoved(View* parent, View* child) {
}

void View::OnViewAddedToWidget() {
}

void View::OnViewRemovedFromWidget() {
}

// Painting --------------------------------------------------------------------

void View::PaintChildren(gfx::Canvas* canvas) {
  // TODO(beng): use for_each.
  // std::for_each(children_.begin(), children_.end(),
  //              std::bind2nd(std::mem_fun_ref(&View::Paint), canvas));
  ViewVector::iterator it = children_.begin();
  for (; it != children_.end(); ++it)
    (*it)->Paint(canvas);
}

void View::OnPaint(gfx::Canvas* canvas) {
  // TODO(beng): investigate moving these function calls to Paint().
  OnPaintBackground(canvas);
  OnPaintFocusBorder(canvas);
  OnPaintBorder(canvas);
}

void View::OnPaintBackground(gfx::Canvas* canvas) {
}

void View::OnPaintBorder(gfx::Canvas* canvas) {
  if (border_.get())
    border_->Paint(const_cast<const View*>(this), canvas);
}

void View::OnPaintFocusBorder(gfx::Canvas* canvas) {
}

// Input -----------------------------------------------------------------------

View* View::GetEventHandlerForPoint(const gfx::Point& point) const {
  ViewVector::const_reverse_iterator it = children_.rbegin();
  for (; it != children_.rend(); ++it) {
    View* child = *it;
    if (!child->visible())
      continue;

    gfx::Point point_in_child_coords(point);
    View::ConvertPointToView(const_cast<View*>(this), child,
                             &point_in_child_coords);
    if (child->HitTest(point_in_child_coords))
      return child->GetEventHandlerForPoint(point_in_child_coords);
  }
  return const_cast<View*>(this);
}

gfx::NativeCursor View::GetCursorForPoint(const gfx::Point& point) {
  return NULL;
}

bool View::OnKeyPressed(const KeyEvent& event) {
  return true;
}

bool View::OnKeyReleased(const KeyEvent& event) {
  return true;
}

bool View::OnMouseWheel(const MouseWheelEvent& event) {
  return true;
}

bool View::OnMousePressed(const MouseEvent& event) {
  return true;
}

bool View::OnMouseDragged(const MouseEvent& event) {
  return true;
}

void View::OnMouseReleased(const MouseEvent& event) {

}

void View::OnMouseCaptureLost() {

}

void View::OnMouseMoved(const MouseEvent& event) {

}

void View::OnMouseEntered(const MouseEvent& event) {

}

void View::OnMouseExited(const MouseEvent& event) {

}

// Accelerators ----------------------------------------------------------------

bool View::OnAcceleratorPressed(const Accelerator& accelerator) {
  return false;
}

// Focus -----------------------------------------------------------------------

bool View::SkipDefaultKeyEventProcessing(const KeyEvent& event) const {
  return false;
}

bool View::IsGroupFocusTraversable() const {
  return true;
}

bool View::IsFocusableInRootView() const {
  // TODO(beng): kill this, replace with direct check in focus manager.
  return IsFocusable();
}

bool View::IsAccessibilityFocusableInRootView() const {
  // TODO(beng): kill this, replace with direct check in focus manager.
  return false;
}

FocusTraversable* View::GetPaneFocusTraversable() const {
  // TODO(beng): figure out what to do about this.
  return NULL;
}

void View::OnFocus(/* const FocusEvent& event */) {
}

void View::OnBlur() {
}

////////////////////////////////////////////////////////////////////////////////
// View, private:

void View::DragInfo::Reset() {
  possible_drag = false;
  press_point = gfx::Point();
}

void View::DragInfo::PossibleDrag(const gfx::Point& point) {
  possible_drag = true;
  press_point = point;
}

// Tree operations -------------------------------------------------------------

void View::NotifyHierarchyChanged(View* parent, View* child, bool is_add) {
  // Notify the child. Note that we call GetWidget() on the parent, not the
  // child, since this method is called after the child is already removed from
  // the hierarchy when |is_add| is false and so child->GetWidget() will always
  // return NULL.
  bool has_widget = parent->GetWidget() != NULL;
  CallViewNotification(child, parent, child, is_add, has_widget);

  // Notify the hierarchy up.
  NotifyHierarchyChangedUp(parent, child, is_add);

  // Notify the hierarchy down.
  if (!is_add) {
    // Because |child| has already been removed from |parent|'s child list, we
    // need to notify its hierarchy manually.
    child->NotifyHierarchyChangedDown(parent, child, is_add, has_widget);
  }
  NotifyHierarchyChangedDown(parent, child, is_add, has_widget);
}

void View::NotifyHierarchyChangedUp(View* parent, View* child, bool is_add) {
  for (View* v = parent; v; v = v->parent()) {
    if (is_add)
      v->OnViewAdded(parent, child);
    else
      v->OnViewRemoved(parent, child);
  }
}

void View::NotifyHierarchyChangedDown(View* parent, View* child, bool is_add,
                                       bool has_widget) {
  ViewVector::iterator it = children_.begin();
  for (; it != children_.end(); ++it) {
    CallViewNotification(*it, parent, child, is_add, has_widget);
    (*it)->NotifyHierarchyChangedDown(parent, child, is_add, has_widget);
  }
}

void View::CallViewNotification(View* target,
                                View* parent,
                                View* child,
                                bool is_add,
                                bool has_widget) {
  if (is_add) {
    target->OnViewAdded(parent, child);
    if (has_widget)
      target->OnViewAddedToWidget();
  } else {
    target->OnViewRemoved(parent, child);
    if (has_widget)
      target->OnViewRemovedFromWidget();
  }
}

// Painting --------------------------------------------------------------------

void View::Paint(gfx::Canvas* canvas) {
  // Invisible views are not painted.
  if (!visible_)
    return;

  ScopedCanvasState canvas_state(canvas);
  if (canvas->ClipRectInt(x(), y(), width(), height())) {
    canvas->TranslateInt(x(), y());
    // TODO(beng): RTL
    ScopedCanvasState canvas_state(canvas);
    OnPaint(canvas);
    PaintChildren(canvas);
  }
}

// Input -----------------------------------------------------------------------

bool View::MousePressed(const MouseEvent& event, DragInfo* drag_info) {
  bool handled = OnMousePressed(event);
  // TODO(beng): deal with view deletion, see ProcessMousePressed() in old code.
  if (!enabled_)
    return handled;

  int drag_operations =
      enabled_ && event.IsOnlyLeftMouseButton() && HitTest(event.location()) ?
      GetDragOperations(event.location()) : DragDropTypes::DRAG_NONE;
  if (drag_operations != DragDropTypes::DRAG_NONE) {
    drag_info->PossibleDrag(event.location());
    return true;
  }
  bool has_context_menu = event.IsRightMouseButton() ?
      !!context_menu_controller_ : NULL;
  return has_context_menu || handled;
}

bool View::MouseDragged(const MouseEvent& event, DragInfo* drag_info) {
  if (drag_info->possible_drag &&
      ExceededDragThreshold(drag_info->press_point, event.location())) {
    if (!drag_controller_ ||
        drag_controller_->CanStartDrag(this, drag_info->press_point,
                                       event.location())) {
      StartShellDrag(event, drag_info->press_point);
    }
  } else {
    if (OnMouseDragged(event))
      return true;
  }
  // TODO(beng): Handle view deletion from OnMouseDragged().
  return !!context_menu_controller_ || drag_info->possible_drag;
}

void View::MouseReleased(const MouseEvent& event) {
  OnMouseReleased(event);
  // TODO(beng): Handle view deletion from OnMouseReleased().
  if (context_menu_controller_ && event.IsOnlyRightMouseButton()) {
    gfx::Point location(event.location());
    if (HitTest(location)) {
      ConvertPointToScreen(this, &location);
      context_menu_controller_->ShowContextMenu(this, location, true);
    }
  }
}

// Focus -----------------------------------------------------------------------

// TODO(beng): Move to FocusManager.
void View::InitFocusSiblings(View* view, size_t index) {
  if (child_count() == 0) {
    view->next_focusable_view_ = NULL;
    view->prev_focusable_view_ = NULL;
  } else {
    if (index == child_count()) {
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
        view->prev_focusable_view_ = prev;
        view->next_focusable_view_ = prev->next_focusable_view_;
        prev->next_focusable_view_->prev_focusable_view_ = view;
        prev->next_focusable_view_ = view;
      } else {
        last_focusable_view->next_focusable_view_ = view;
        view->next_focusable_view_ = NULL;
        view->prev_focusable_view_ = last_focusable_view;
      }
    } else {
      View* prev = children_[index]->GetPreviousFocusableView();
      view->prev_focusable_view_ = prev;
      view->next_focusable_view_ = children_[index];
      if (prev)
        prev->next_focusable_view_ = view;
      children_[index]->prev_focusable_view_ = view;
    }
  }
}

// Drag & Drop -----------------------------------------------------------------

int View::GetDragOperations(const gfx::Point& point) {
  return drag_controller_ ?
      drag_controller_->GetDragOperations(const_cast<View*>(this), point) :
      DragDropTypes::DRAG_NONE;
}

void View::WriteDragData(const gfx::Point& point, OSExchangeData* data) {
  drag_controller_->WriteDragData(this, point, data);
}

void View::StartShellDrag(const MouseEvent& event,
                          const gfx::Point& press_point) {
  // TODO(beng): system stuff.
}

}  // namespace ui
