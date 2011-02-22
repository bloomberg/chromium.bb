// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view.h"

#include <algorithm>
#include <functional>

#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"
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
    parent_->RemoveChildView(this, false);

  for (Views::const_iterator i(children_begin()); i != children_.end(); ++i) {
    (*i)->parent_ = NULL;
    if ((*i)->parent_owned())
      delete *i;
  }
}

// Size and disposition --------------------------------------------------------

gfx::Rect View::GetVisibleBounds() const {
  // TODO(beng):
  return bounds();
}

gfx::Rect View::GetContentsBounds() const {
  gfx::Rect rect(gfx::Point(), size());
  if (border_.get())
    rect.Inset(border_->insets());
  return rect;
}

void View::SetBounds(const gfx::Rect& bounds) {
  gfx::Rect old_bounds = bounds_;
  bounds_ = bounds;
  // TODO(beng): investigate usage of needs_layout_ in old View code.
  if (old_bounds != bounds_) {
    OnBoundsChanged();
    Layout();
  }
}

void View::SetOrigin(const gfx::Point& origin) {
  SetBounds(gfx::Rect(origin, size()));
}

void View::SetSize(const gfx::Size& size) {
  SetBounds(gfx::Rect(origin(), size));
}

void View::SetBorder(Border* border) {
  border_.reset(border);
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
  // The layout manager handles all child layout if present.
  if (layout_manager_.get()) {
    layout_manager_->Layout(this);
  } else {
    std::for_each(children_begin(), children_end(),
                  std::mem_fun(&View::Layout));
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

View* View::GetViewByID(int id) {
  if (id_ == id)
    return this;
  for (Views::const_iterator i(children_begin()); i != children_end(); ++i) {
    View* view = (*i)->GetViewByID(id);
    if (view)
      return view;
  }
  return NULL;
}

void View::GetViewsInGroup(int group, Views* vec) {
  if (group_ == group)
    vec->push_back(const_cast<View*>(this));
  for (Views::const_iterator i(children_begin()); i != children_end(); ++i)
    (*i)->GetViewsInGroup(group, vec);
}

View* View::GetSelectedViewForGroup(int group_id) {
  // TODO(beng): implementme
  return NULL;
}

// Coordinate conversion -------------------------------------------------------

// static
void View::ConvertPointToView(const View& source,
                              const View& target,
                              gfx::Point* point) {
  const View* inner = NULL;
  const View* outer = NULL;
  if (source.Contains(target)) {
    inner = &target;
    outer = &source;
  } else if (target.Contains(source)) {
    inner = &source;
    outer = &target;
  }  // Note that we cannot do a plain "else" here since |source| and |target|
     // may be in different hierarchies with no relation.
  if (!inner)
    return;

  gfx::Point offset;
  for (const View* v = inner; v != outer; v = v->parent())
    offset.Offset(v->x(), v->y());
  // When target is contained by source, we need to subtract the offset.
  // When source is contained by target, we need to add the offset.
  int multiplier = (inner == &target) ? -1 : 1;
  point->Offset(multiplier * offset.x(), multiplier * offset.y());
}

// static
void View::ConvertPointToScreen(const View& source, gfx::Point* point) {
  const Widget* widget = source.GetWidget();
  if (widget) {
    ConvertPointToWidget(source, point);
    gfx::Point client_origin(widget->GetClientAreaScreenBounds().origin());
    point->Offset(client_origin.x(), client_origin.y());
  }
}

// static
void View::ConvertPointToWidget(const View& source, gfx::Point* point) {
  for (const View* v = &source; v; v = v->parent())
    point->Offset(v->x(), v->y());
}

// Tree operations -------------------------------------------------------------

const Widget* View::GetWidget() const {
  return parent() ? parent()->GetWidget() : NULL;
}

void View::AddChildView(View* view) {
  AddChildViewAt(view, children_size());
}

void View::AddChildViewAt(View* view, size_t index) {
  CHECK_NE(this, view) << "A view cannot be its own child.";

  // Remove the child from its current parent if any.
  if (view->parent())
    view->parent()->RemoveChildView(view, false);

  // TODO(beng): Move focus initialization to FocusManager.
  InitFocusSiblings(view, index);

  children_.insert(children_.begin() + index, view);
  view->parent_ = this;

  // Notify the hierarchy.
  NotifyHierarchyChanged(view, true);

  // TODO(beng): Notify other objects like tooltip, layout manager, etc.
  //             Figure out RegisterChildrenForVisibleBoundsNotification.
}

void View::RemoveChildView(View* view, bool delete_child) {
  Views::const_iterator i(std::find(children_begin(), children_end(), view));
  DCHECK(i != children_end());
  view->parent_ = NULL;
  children_.erase(i);
  NotifyHierarchyChanged(view, false);
  // TODO(beng): Notify other objects like tooltip, layout manager, etc.
  if (delete_child)
    delete view;
}

void View::RemoveAllChildViews(bool delete_children) {
  while (!children_.empty()) {
    RemoveChildView(children_.front(), delete_children);
    // TODO(beng): view deletion is actually more complicated in the old view.cc
    //             figure out why. (it uses a ScopedVector to accumulate a list
    //             of views to delete).
  }
}

bool View::Contains(const View& child) const {
  for (const View* v = &child; v; v = v->parent()) {
    if (v == this)
      return true;
  }
  return false;
}

// Painting --------------------------------------------------------------------

void View::Invalidate() {
  InvalidateRect(gfx::Rect(gfx::Point(), size()));
}

void View::InvalidateRect(const gfx::Rect& invalid_rect) {
  if (visible_ && parent_) {
    gfx::Rect r(invalid_rect);
    r.Offset(bounds_.origin());
    parent_->InvalidateRect(r);
  }
}

// Input -----------------------------------------------------------------------

bool View::HitTest(const gfx::Point& point) const {
  // TODO(beng): Hit test mask support.
  return gfx::Rect(gfx::Point(), size()).Contains(point);
}

// Accelerators ----------------------------------------------------------------

void View::AddAccelerator(const Accelerator& accelerator) {
}

void View::RemoveAccelerator(const Accelerator& accelerator) {
}

void View::RemoveAllAccelerators() {
}

// Focus -----------------------------------------------------------------------

FocusManager* View::GetFocusManager() {
  return const_cast<FocusManager*>(static_cast<const View*>(this)->
      GetFocusManager());
}

const FocusManager* View::GetFocusManager() const {
  const Widget* widget = GetWidget();
  return widget ? widget->GetFocusManager() : NULL;
}

FocusTraversable* View::GetFocusTraversable() {
  return NULL;
}

View* View::GetNextFocusableView() {
  return NULL;
}

View* View::GetPreviousFocusableView() {
  return NULL;
}

bool View::IsFocusable() const {
  return focusable_ && enabled_ && visible_;
}

bool View::HasFocus() const {
  const FocusManager* focus_manager = GetFocusManager();
  return focus_manager ? (focus_manager->focused_view() == this) : false;
}

void View::RequestFocus() {
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager && (focus_manager->focused_view() != this))
    focus_manager->SetFocusedView(this);
}

// Resources -------------------------------------------------------------------

ThemeProvider* View::GetThemeProvider() {
  Widget* widget = GetWidget();
  return widget ? widget->GetThemeProvider() : NULL;
}

////////////////////////////////////////////////////////////////////////////////
// View, protected:

// Tree operations -------------------------------------------------------------

void View::OnViewAdded(const View& parent, const View& child) {
}

void View::OnViewRemoved(const View& parent, const View& child) {
}

void View::OnViewAddedToWidget() {
}

void View::OnViewRemovedFromWidget() {
}

// Painting --------------------------------------------------------------------

void View::PaintChildren(gfx::Canvas* canvas) {
  std::for_each(children_begin(), children_end(),
                std::bind2nd(std::mem_fun(&View::Paint), canvas));
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
    border_->Paint(this, canvas);
}

void View::OnPaintFocusBorder(gfx::Canvas* canvas) {
}

// Input -----------------------------------------------------------------------

View* View::GetEventHandlerForPoint(const gfx::Point& point) {
  for (Views::const_reverse_iterator i(children_rbegin()); i != children_rend();
       ++i) {
    View* child = *i;
    if (child->visible()) {
      gfx::Point point_in_child_coords(point);
      View::ConvertPointToView(*this, *child, &point_in_child_coords);
      if (child->HitTest(point_in_child_coords))
        return child->GetEventHandlerForPoint(point_in_child_coords);
    }
  }
  return this;
}

gfx::NativeCursor View::GetCursorForPoint(const gfx::Point& point) const {
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

FocusTraversable* View::GetPaneFocusTraversable() {
  // TODO(beng): figure out what to do about this.
  return NULL;
}

void View::OnFocus(const FocusEvent& event) {
}

void View::OnBlur(const FocusEvent& event) {
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

void View::NotifyHierarchyChanged(View* child, bool is_add) {
  // Notify the hierarchy up.
  for (View* v = parent(); v; v = v->parent())
    CallViewNotification(v, *child, is_add, false);

  // Notify the hierarchy down.
  bool has_widget = GetWidget() != NULL;
  if (!is_add) {
    // Because |child| has already been removed from |parent|'s child list, we
    // need to notify its hierarchy manually.
    child->NotifyHierarchyChangedDown(*child, is_add, has_widget);
  }
  NotifyHierarchyChangedDown(*child, is_add, has_widget);
}

void View::NotifyHierarchyChangedDown(const View& child,
                                      bool is_add,
                                      bool has_widget) {
  CallViewNotification(this, child, is_add, has_widget);
  for (Views::const_iterator i(children_begin()); i != children_end(); ++i)
    (*i)->NotifyHierarchyChangedDown(child, is_add, has_widget);
}

void View::CallViewNotification(View* target,
                                const View& child,
                                bool is_add,
                                bool has_widget) {
  if (is_add) {
    target->OnViewAdded(*this, child);
    if (has_widget)
      target->OnViewAddedToWidget();
  } else {
    target->OnViewRemoved(*this, child);
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

  if (!event.IsOnlyLeftMouseButton() || !HitTest(event.location()) ||
      (GetDragOperations(event.location()) == DragDropTypes::DRAG_NONE))
    return handled || (event.IsRightMouseButton() && context_menu_controller_);

  drag_info->PossibleDrag(event.location());
  return true;
}

bool View::MouseDragged(const MouseEvent& event, DragInfo* drag_info) {
  if (drag_info->possible_drag &&
      ExceededDragThreshold(drag_info->press_point, event.location())) {
    if (!drag_controller_ ||
        drag_controller_->CanStartDrag(this, drag_info->press_point,
                                       event.location()))
      StartShellDrag(event, drag_info->press_point);
  } else if (OnMouseDragged(event)) {
    return true;
  }
  // TODO(beng): Handle view deletion from OnMouseDragged().
  return context_menu_controller_ || drag_info->possible_drag;
}

void View::MouseReleased(const MouseEvent& event) {
  OnMouseReleased(event);
  // TODO(beng): Handle view deletion from OnMouseReleased().
  if (context_menu_controller_ && event.IsOnlyRightMouseButton()) {
    gfx::Point location(event.location());
    if (HitTest(location)) {
      ConvertPointToScreen(*this, &location);
      context_menu_controller_->ShowContextMenu(this, location, true);
    }
  }
}

// Focus -----------------------------------------------------------------------

// TODO(beng): Move to FocusManager.
void View::InitFocusSiblings(View* view, size_t index) {
  if (children_empty()) {
    view->next_focusable_view_ = NULL;
    view->prev_focusable_view_ = NULL;
    return;
  }

  if (index != children_size()) {
    View* prev = children_[index]->GetPreviousFocusableView();
    view->prev_focusable_view_ = prev;
    view->next_focusable_view_ = children_[index];
    if (prev)
      prev->next_focusable_view_ = view;
    children_[index]->prev_focusable_view_ = view;
    return;
  }

  // We are inserting at the end, but the end of the child list may not be
  // the last focusable element. Let's try to find an element with no next
  // focusable element to link to.
  for (Views::const_iterator i(children_begin()); i != children_end(); ++i) {
    if (!(*i)->next_focusable_view_) {
      (*i)->next_focusable_view_ = view;
      view->next_focusable_view_ = NULL;
      view->prev_focusable_view_ = *i;
      return;
    }
  }

  // Hum... there is a cycle in the focus list. Let's just insert ourself
  // after the last child.
  View* prev = children_[index - 1];
  view->prev_focusable_view_ = prev;
  view->next_focusable_view_ = prev->next_focusable_view_;
  prev->next_focusable_view_->prev_focusable_view_ = view;
  prev->next_focusable_view_ = view;
}

// Drag & Drop -----------------------------------------------------------------

int View::GetDragOperations(const gfx::Point& point) {
  return drag_controller_ ?
      drag_controller_->GetDragOperations(this, point) :
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
