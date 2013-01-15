// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tree/tree_view_views.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/events/event.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/tree/tree_view_controller.h"

using ui::TreeModel;
using ui::TreeModelNode;

namespace views {

// Insets around the view.
static const int kHorizontalInset = 2;
static const int kVerticalInset = 2;
// Padding before/after the image.
static const int kImagePadding = 4;
// Size of the arrow region.
static const int kArrowRegionSize = 12;
// Padding around the text (on each side).
static const int kTextVerticalPadding = 3;
static const int kTextHorizontalPadding = 2;
// How much children are indented from their parent.
static const int kIndent = 20;

// TODO: these should come from native theme or something.
static const SkColor kArrowColor = SkColorSetRGB(0x7A, 0x7A, 0x7A);
static const SkColor kSelectedBackgroundColor = SkColorSetRGB(0xEE, 0xEE, 0xEE);
static const SkColor kTextColor = SK_ColorBLACK;

TreeView::TreeView()
    : model_(NULL),
      selected_node_(NULL),
      editor_(NULL),
      focus_manager_(NULL),
      auto_expand_children_(false),
      editable_(true),
      controller_(NULL),
      root_shown_(true),
      has_custom_icons_(false),
      row_height_(font_.GetHeight() + kTextVerticalPadding * 2) {
  set_focusable(true);
  set_background(Background::CreateSolidBackground(SK_ColorWHITE));
  closed_icon_ = *ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      (base::i18n::IsRTL() ? IDR_FOLDER_CLOSED_RTL
                           : IDR_FOLDER_CLOSED)).ToImageSkia();
  open_icon_ = *ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      (base::i18n::IsRTL() ? IDR_FOLDER_OPEN_RTL
                           : IDR_FOLDER_OPEN)).ToImageSkia();
  text_offset_ = closed_icon_.width() + kImagePadding + kImagePadding +
      kArrowRegionSize;
}

TreeView::~TreeView() {
  if (model_)
    model_->RemoveObserver(this);
  if (focus_manager_) {
    focus_manager_->RemoveFocusChangeListener(this);
    focus_manager_ = NULL;
  }
}

View* TreeView::CreateParentIfNecessary() {
  ScrollView* scroll_view = ScrollView::CreateScrollViewWithBorder();
  scroll_view->SetContents(this);
  return scroll_view;
}

void TreeView::SetModel(TreeModel* model) {
  if (model == model_)
    return;
  if (model_)
    model_->RemoveObserver(this);

  CancelEdit();

  model_ = model;
  selected_node_ = NULL;
  icons_.clear();
  if (model_) {
    model_->AddObserver(this);
    model_->GetIcons(&icons_);

    root_.RemoveAll();
    ConfigureInternalNode(model_->GetRoot(), &root_);
    LoadChildren(&root_);
    root_.set_is_expanded(true);
    if (root_shown_)
      selected_node_ = &root_;
    else if (root_.child_count())
      selected_node_ = root_.GetChild(0);
  }
  DrawnNodesChanged();
}

void TreeView::SetEditable(bool editable) {
  if (editable == editable_)
    return;
  editable_ = editable;
  CancelEdit();
}

void TreeView::StartEditing(TreeModelNode* node) {
  DCHECK(node);
  // Cancel the current edit.
  CancelEdit();
  // Make sure all ancestors are expanded.
  if (model_->GetParent(node))
    Expand(model_->GetParent(node));
  // Select the node, else if the user commits the edit the selection reverts.
  SetSelectedNode(node);
  if (GetSelectedNode() != node)
    return;  // Selection failed for some reason, don't start editing.
  DCHECK(!editor_);
  editor_ = new Textfield;
  // Add the editor immediately as GetPreferredSize returns the wrong thing if
  // not parented.
  AddChildView(editor_);
  editor_->SetFont(font_);
  empty_editor_size_ = editor_->GetPreferredSize();
  editor_->SetText(selected_node_->model_node()->GetTitle());
  editor_->SetController(this);
  LayoutEditor();
  SchedulePaintForNode(selected_node_);
  editor_->RequestFocus();
  editor_->SelectAll(false);

  // Listen for focus changes so that we can cancel editing.
  focus_manager_ = GetFocusManager();
  if (focus_manager_)
    focus_manager_->AddFocusChangeListener(this);
}

void TreeView::CancelEdit() {
  if (!editor_)
    return;

  if (focus_manager_) {
    focus_manager_->RemoveFocusChangeListener(this);
    focus_manager_ = NULL;
  }
  // WARNING: don't touch selected_node_, it may be bogus.
  RemoveChildView(editor_);
  // Don't delete immediately as we may be servicing a callback from the editor.
  MessageLoop::current()->DeleteSoon(FROM_HERE, editor_);
  editor_ = NULL;
  SchedulePaint();
}

void TreeView::CommitEdit() {
  if (!editor_)
    return;

  DCHECK(selected_node_);
  model_->SetTitle(GetSelectedNode(), editor_->text());
  CancelEdit();
  RequestFocus();
}

TreeModelNode* TreeView::GetEditingNode() {
  return editor_ ? selected_node_->model_node() : NULL;
}

void TreeView::SetSelectedNode(TreeModelNode* model_node) {
  if (editor_ || model_node != selected_node_)
    CancelEdit();
  if (model_node && model_->GetParent(model_node))
    Expand(model_->GetParent(model_node));
  if (model_node && model_node == root_.model_node() && !root_shown_)
    return;  // Ignore requests to select the root when not shown.
  InternalNode* node = model_node ? GetInternalNodeForModelNode(
      model_node, CREATE_IF_NOT_LOADED) : NULL;
  bool was_empty_selection = (selected_node_ == NULL);
  bool changed = (selected_node_ != node);
  if (changed) {
    SchedulePaintForNode(selected_node_);
    selected_node_ = node;
    if (selected_node_ == &root_ && !root_shown_)
      selected_node_ = NULL;
    if (selected_node_ && selected_node_ != &root_)
      Expand(model_->GetParent(selected_node_->model_node()));
    SchedulePaintForNode(selected_node_);
  }

  if (selected_node_)
    ScrollRectToVisible(GetBoundsForNode(selected_node_));

  // Notify controller if the old selection was empty to handle the case of
  // remove explicitly resetting selected_node_ before invoking this.
  if (controller_ && (changed || was_empty_selection))
    controller_->OnTreeViewSelectionChanged(this);
}

TreeModelNode* TreeView::GetSelectedNode() {
  return selected_node_ ? selected_node_->model_node() : NULL;
}

void TreeView::Collapse(ui::TreeModelNode* model_node) {
  // Don't collapse the root if the root isn't shown, otherwise nothing is
  // displayed.
  if (model_node == root_.model_node() && !root_shown_)
    return;
  InternalNode* node =
      GetInternalNodeForModelNode(model_node, DONT_CREATE_IF_NOT_LOADED);
  if (!node)
    return;
  bool was_expanded = IsExpanded(model_node);
  if (node->is_expanded()) {
    if (selected_node_ && selected_node_->HasAncestor(node))
      SetSelectedNode(model_node);
    node->set_is_expanded(false);
  }
  if (was_expanded)
    DrawnNodesChanged();
}

void TreeView::Expand(TreeModelNode* node) {
  if (ExpandImpl(node))
    DrawnNodesChanged();
  // TODO: need to support auto_expand_children_.
}

void TreeView::ExpandAll(TreeModelNode* node) {
  DCHECK(node);
  // Expand the node.
  bool expanded_at_least_one = ExpandImpl(node);
  // And recursively expand all the children.
  for (int i = model_->GetChildCount(node) - 1; i >= 0; --i) {
    TreeModelNode* child = model_->GetChild(node, i);
    if (ExpandImpl(child))
      expanded_at_least_one = true;
  }
  if (expanded_at_least_one)
    DrawnNodesChanged();
}

bool TreeView::IsExpanded(TreeModelNode* model_node) {
  if (!model_node) {
    // NULL check primarily for convenience for uses in this class so don't have
    // to add NULL checks every where we look up the parent.
    return true;
  }
  InternalNode* node = GetInternalNodeForModelNode(
      model_node, DONT_CREATE_IF_NOT_LOADED);
  if (!node)
    return false;

  while (node) {
    if (!node->is_expanded())
      return false;
    node = node->parent();
  }
  return true;
}

void TreeView::SetRootShown(bool root_shown) {
  if (root_shown_ == root_shown)
    return;
  root_shown_ = root_shown;
  if (!root_shown_ && selected_node_ == &root_) {
    if (model_->GetChildCount(root_.model_node()))
      SetSelectedNode(model_->GetChild(root_.model_node(), 0));
    else
      SetSelectedNode(NULL);
  }
  DrawnNodesChanged();
}

void TreeView::Layout() {
  int width = preferred_size_.width();
  int height = preferred_size_.height();
  if (parent()) {
    width = std::max(parent()->width(), width);
    height = std::max(parent()->height(), height);
  }
  SetBounds(x(), y(), width, height);
  LayoutEditor();
}

gfx::Size TreeView::GetPreferredSize() {
  return preferred_size_;
}

bool TreeView::OnMousePressed(const ui::MouseEvent& event) {
  return OnClickOrTap(event);
}

void TreeView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_DOUBLE_TAP) {
    if (OnClickOrTap(*event))
      event->SetHandled();
  }
}

void TreeView::ShowContextMenu(const gfx::Point& p, bool is_mouse_gesture) {
  if (!model_)
    return;
  if (is_mouse_gesture) {
    // Only invoke View's implementation (which notifies the
    // ContextMenuController) if over a node.
    gfx::Point local_point(p);
    ConvertPointToTarget(NULL, this, &local_point);
    int row = (local_point.y() - kVerticalInset) / row_height_;
    int depth;
    InternalNode* node = GetNodeByRow(row, &depth);
    if (!node)
      return;
    gfx::Rect bounds(GetBoundsForNodeImpl(node, row, depth));
    if (!bounds.Contains(local_point))
      return;
  }
  View::ShowContextMenu(p, is_mouse_gesture);
}

void TreeView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_OUTLINE;
  state->state = ui::AccessibilityTypes::STATE_READONLY;
}

void TreeView::TreeNodesAdded(TreeModel* model,
                              TreeModelNode* parent,
                              int start,
                              int count) {
  InternalNode* parent_node =
      GetInternalNodeForModelNode(parent, DONT_CREATE_IF_NOT_LOADED);
  if (!parent_node || !parent_node->loaded_children())
    return;
  for (int i = 0; i < count; ++i) {
    InternalNode* child = new InternalNode;
    ConfigureInternalNode(model_->GetChild(parent, start + i), child);
    parent_node->Add(child, start + i);
  }
  if (IsExpanded(parent))
    DrawnNodesChanged();
}

void TreeView::TreeNodesRemoved(TreeModel* model,
                                TreeModelNode* parent,
                                int start,
                                int count) {
  InternalNode* parent_node =
      GetInternalNodeForModelNode(parent, DONT_CREATE_IF_NOT_LOADED);
  if (!parent_node || !parent_node->loaded_children())
    return;
  bool reset_selection = false;
  for (int i = 0; i < count; ++i) {
    InternalNode* child_removing = parent_node->GetChild(start);
    if (selected_node_ && selected_node_->HasAncestor(child_removing))
      reset_selection = true;
    delete parent_node->Remove(child_removing);
  }
  if (reset_selection) {
    // selected_node_ is no longer valid (at the time we enter this function
    // its model_node() is likely deleted). Explicitly NULL out the field
    // rather than invoking SetSelectedNode() otherwise, we'll try and use a
    // deleted value.
    selected_node_ = NULL;
    TreeModelNode* to_select = parent;
    if (parent == root_.model_node() && !root_shown_) {
      to_select = model_->GetChildCount(parent) > 0 ?
          model_->GetChild(parent, 0) : NULL;
    }
    SetSelectedNode(to_select);
  }
  if (IsExpanded(parent))
    DrawnNodesChanged();
}

void TreeView::TreeNodeChanged(TreeModel* model, TreeModelNode* model_node) {
  InternalNode* node =
      GetInternalNodeForModelNode(model_node, DONT_CREATE_IF_NOT_LOADED);
  if (!node)
    return;
  int old_width = node->text_width();
  UpdateNodeTextWidth(node);
  if (old_width != node->text_width() &&
      ((node == &root_ && root_shown_) ||
       (node != &root_ && IsExpanded(node->parent()->model_node())))) {
    DrawnNodesChanged();
  }
}

void TreeView::ContentsChanged(Textfield* sender,
                               const string16& new_contents) {
}

bool TreeView::HandleKeyEvent(Textfield* sender,
                              const ui::KeyEvent& key_event) {
  switch (key_event.key_code()) {
    case ui::VKEY_RETURN:
      CommitEdit();
      return true;

    case ui::VKEY_ESCAPE:
      CancelEdit();
      RequestFocus();
      return true;

    default:
      return false;
  }
}

void TreeView::OnWillChangeFocus(View* focused_before,
                                 View* focused_now) {
}

void TreeView::OnDidChangeFocus(View* focused_before,
                                View* focused_now) {
  CancelEdit();
}

gfx::Point TreeView::GetKeyboardContextMenuLocation() {
  int y = height() / 2;
  if (selected_node_) {
    gfx::Rect node_bounds(GetBoundsForNode(selected_node_));
    gfx::Rect vis_bounds(GetVisibleBounds());
    if (node_bounds.y() >= vis_bounds.y() &&
        node_bounds.y() < vis_bounds.bottom()) {
      y = node_bounds.y();
    }
  }
  gfx::Point screen_loc(0, y);
  if (base::i18n::IsRTL())
    screen_loc.set_x(width());
  ConvertPointToScreen(this, &screen_loc);
  return screen_loc;
}

bool TreeView::OnKeyPressed(const ui::KeyEvent& event) {
  if (!HasFocus())
    return false;

  switch (event.key_code()) {
    case ui::VKEY_F2:
      if (!editor_) {
        TreeModelNode* selected_node = GetSelectedNode();
        if (selected_node && (!controller_ ||
                              controller_->CanEdit(this, selected_node))) {
          StartEditing(selected_node);
        }
      }
      return true;

    case ui::VKEY_UP:
    case ui::VKEY_DOWN:
      IncrementSelection(event.key_code() == ui::VKEY_UP ?
                         INCREMENT_PREVIOUS : INCREMENT_NEXT);
      return true;

    case ui::VKEY_LEFT:
      if (base::i18n::IsRTL())
        ExpandOrSelectChild();
      else
        CollapseOrSelectParent();
      return true;

    case ui::VKEY_RIGHT:
      if (base::i18n::IsRTL())
        CollapseOrSelectParent();
      else
        ExpandOrSelectChild();
      return true;

    default:
      break;
  }
  return false;
}

void TreeView::OnPaint(gfx::Canvas* canvas) {
  // Don't invoke View::OnPaint so that we can render our own focus border.
  OnPaintBackground(canvas);

  int min_y, max_y;
  {
    SkRect sk_clip_rect;
    if (canvas->sk_canvas()->getClipBounds(&sk_clip_rect)) {
      // Pixels partially inside the clip rect should be included.
      gfx::Rect clip_rect = gfx::ToEnclosingRect(
          gfx::SkRectToRectF(sk_clip_rect));
      min_y = clip_rect.y();
      max_y = clip_rect.bottom();
    } else {
      gfx::Rect vis_bounds = GetVisibleBounds();
      min_y = vis_bounds.y();
      max_y = vis_bounds.bottom();
    }
  }

  int min_row = std::max(0, (min_y - kVerticalInset) / row_height_);
  int max_row = (max_y - kVerticalInset) / row_height_;
  if ((max_y - kVerticalInset) % row_height_ != 0)
    max_row++;
  int current_row = root_row();
  PaintRows(canvas, min_row, max_row, &root_, root_depth(), &current_row);
}

void TreeView::OnFocus() {
  View::OnFocus();
  SchedulePaintForNode(selected_node_);
}

void TreeView::OnBlur() {
  SchedulePaintForNode(selected_node_);
}

bool TreeView::OnClickOrTap(const ui::LocatedEvent& event) {
  int row = (event.y() - kVerticalInset) / row_height_;
  int depth;
  InternalNode* node = GetNodeByRow(row, &depth);
  if (node) {
    RequestFocus();
    gfx::Rect bounds(GetBoundsForNodeImpl(node, row, depth));
    if (bounds.Contains(event.location())) {
      int relative_x = event.x() - bounds.x();
      if (base::i18n::IsRTL())
        relative_x = bounds.width() - relative_x;
      if (relative_x < kArrowRegionSize &&
          model_->GetChildCount(node->model_node())) {
        if (node->is_expanded())
          Collapse(node->model_node());
        else
          Expand(node->model_node());
      } else if (relative_x > kArrowRegionSize) {
        SetSelectedNode(node->model_node());
        if (event.flags() & ui::EF_IS_DOUBLE_CLICK ||
            event.type() == ui::ET_GESTURE_DOUBLE_TAP) {
          if (node->is_expanded())
            Collapse(node->model_node());
          else
            Expand(node->model_node());
        }
      }
    }
  }
  return true;
}

void TreeView::LoadChildren(InternalNode* node) {
  DCHECK_EQ(0, node->child_count());
  DCHECK(!node->loaded_children());
  node->set_loaded_children(true);
  for (int i = 0, child_count = model_->GetChildCount(node->model_node());
       i < child_count; ++i) {
    InternalNode* child = new InternalNode;
    ConfigureInternalNode(model_->GetChild(node->model_node(), i), child);
    node->Add(child, node->child_count());
  }
}

void TreeView::ConfigureInternalNode(TreeModelNode* model_node,
                                     InternalNode* node) {
  node->Reset(model_node);
  UpdateNodeTextWidth(node);
}

void TreeView::UpdateNodeTextWidth(InternalNode* node) {
  int width = 0, height = 0;
  gfx::Canvas::SizeStringInt(node->model_node()->GetTitle(),
      font_, &width, &height, gfx::Canvas::NO_ELLIPSIS);
  node->set_text_width(width);
}

void TreeView::DrawnNodesChanged() {
  UpdatePreferredSize();
  PreferredSizeChanged();
  SchedulePaint();
}

void TreeView::UpdatePreferredSize() {
  preferred_size_ = gfx::Size();
  if (!model_)
    return;

  preferred_size_.SetSize(
      root_.GetMaxWidth(text_offset_, root_shown_ ? 1 : 0) +
      kTextHorizontalPadding * 2,
      row_height_ * GetRowCount() + kVerticalInset * 2);
}

void TreeView::LayoutEditor() {
  if (!editor_)
    return;

  DCHECK(selected_node_);
  // Position the editor so that its text aligns with the text we drew.
  gfx::Rect row_bounds = GetBoundsForNode(selected_node_);
  row_bounds.set_x(
      GetMirroredXWithWidthInView(row_bounds.x(), row_bounds.width()));
  row_bounds.set_x(row_bounds.x() + text_offset_);
  row_bounds.set_width(row_bounds.width() - text_offset_);
  row_bounds.Inset(kTextHorizontalPadding, kTextVerticalPadding);
  row_bounds.Inset(-empty_editor_size_.width() / 2,
                   -(empty_editor_size_.height() - font_.GetHeight()) / 2);
  // Give a little extra space for editing.
  row_bounds.set_width(row_bounds.width() + 50);
  editor_->SetBoundsRect(row_bounds);
  editor_->Layout();
}

void TreeView::SchedulePaintForNode(InternalNode* node) {
  if (!node)
    return;  // Explicitly allow NULL to be passed in.
  SchedulePaintInRect(GetBoundsForNode(node));
}

void TreeView::PaintRows(gfx::Canvas* canvas,
                         int min_row,
                         int max_row,
                         InternalNode* node,
                         int depth,
                         int* row) {
  if (*row >= max_row)
    return;

  if (*row >= min_row && *row < max_row)
    PaintRow(canvas, node, *row, depth);
  (*row)++;
  if (!node->is_expanded())
    return;
  depth++;
  for (int i = 0; i < node->child_count() && *row < max_row; ++i)
    PaintRows(canvas, min_row, max_row, node->GetChild(i), depth, row);
}

void TreeView::PaintRow(gfx::Canvas* canvas,
                        InternalNode* node,
                        int row,
                        int depth) {
  gfx::Rect bounds(GetBoundsForNodeImpl(node, row, depth));

  if (model_->GetChildCount(node->model_node()))
    PaintExpandControl(canvas, bounds, node->is_expanded());

  // Paint the icon.
  gfx::ImageSkia icon;
  int icon_index = model_->GetIconIndex(node->model_node());
  if (icon_index != -1)
    icon = icons_[icon_index];
  else if (node == selected_node_)
    icon = open_icon_;
  else
    icon = closed_icon_;
  int icon_x = kArrowRegionSize + kImagePadding +
               (open_icon_.width() - icon.width()) / 2;
  if (base::i18n::IsRTL())
    icon_x = bounds.right() - icon_x - open_icon_.width();
  else
    icon_x += bounds.x();
  canvas->DrawImageInt(
      icon, icon_x,
      bounds.y() + (bounds.height() - icon.height()) / 2);

  if (!editor_ || node != selected_node_) {
    gfx::Rect text_bounds(bounds.x() + text_offset_, bounds.y(),
                          bounds.width() - text_offset_, bounds.height());
    if (base::i18n::IsRTL())
      text_bounds.set_x(bounds.x());
    if (node == selected_node_) {
      canvas->FillRect(text_bounds, kSelectedBackgroundColor);
      if (HasFocus())
        canvas->DrawFocusRect(text_bounds);
    }
    canvas->DrawStringInt(node->model_node()->GetTitle(), font_, kTextColor,
                          text_bounds.x() + kTextHorizontalPadding,
                          text_bounds.y() + kTextVerticalPadding,
                          text_bounds.width() - kTextHorizontalPadding * 2,
                          text_bounds.height() - kTextVerticalPadding * 2);
  }
}

void TreeView::PaintExpandControl(gfx::Canvas* canvas,
                                  const gfx::Rect& node_bounds,
                                  bool expanded) {
  int center_x;
  if (base::i18n::IsRTL()) {
    center_x = node_bounds.right() - kArrowRegionSize +
               (kArrowRegionSize - 4) / 2;
  } else {
    center_x = node_bounds.x() + (kArrowRegionSize - 4) / 2;
  }
  int center_y = node_bounds.y() + node_bounds.height() / 2;
  // TODO: this should come from an image.
  if (!expanded) {
    int delta = base::i18n::IsRTL() ? 1 : -1;
    for (int i = 0; i < 4; ++i) {
      canvas->FillRect(gfx::Rect(center_x + delta * (2 - i),
                                 center_y - (3 - i), 1, (3 - i) * 2 + 1),
                       kArrowColor);
    }
  } else {
    center_y -= 2;
    for (int i = 0; i < 4; ++i) {
      canvas->FillRect(gfx::Rect(center_x - (3 - i), center_y + i,
                                 (3 - i) * 2 + 1, 1), kArrowColor);
    }
  }
}

TreeView::InternalNode* TreeView::GetInternalNodeForModelNode(
    ui::TreeModelNode* model_node,
    GetInternalNodeCreateType create_type) {
  if (model_node == root_.model_node())
    return &root_;
  InternalNode* parent_internal_node =
      GetInternalNodeForModelNode(model_->GetParent(model_node), create_type);
  if (!parent_internal_node)
    return NULL;
  if (!parent_internal_node->loaded_children()) {
    if (create_type == DONT_CREATE_IF_NOT_LOADED)
      return NULL;
    LoadChildren(parent_internal_node);
  }
  return parent_internal_node->GetChild(
      model_->GetIndexOf(parent_internal_node->model_node(), model_node));
}

gfx::Rect TreeView::GetBoundsForNode(InternalNode* node) {
  int row, depth;
  row = GetRowForNode(node, &depth);
  return GetBoundsForNodeImpl(node, row, depth);
}

gfx::Rect TreeView::GetBoundsForNodeImpl(InternalNode* node,
                                         int row,
                                         int depth) {
  gfx::Rect rect(depth * kIndent + kHorizontalInset,
                 row * row_height_ + kVerticalInset,
                 text_offset_ + node->text_width() +
                 kTextHorizontalPadding * 2,
                 row_height_);
  rect.set_x(GetMirroredXWithWidthInView(rect.x(), rect.width()));
  return rect;
}

int TreeView::GetRowCount() {
  int row_count = root_.NumExpandedNodes();
  if (!root_shown_)
    row_count--;
  return row_count;
}

int TreeView::GetRowForNode(InternalNode* node, int* depth) {
  DCHECK(!node->parent() || IsExpanded(node->parent()->model_node()));
  *depth = -1;
  int row = -1;
  InternalNode* tmp_node = node;
  while (tmp_node->parent()) {
    int index_in_parent = tmp_node->parent()->GetIndexOf(tmp_node);
    (*depth)++;
    row++;  // For node.
    for (int i = 0; i < index_in_parent; ++i)
      row += tmp_node->parent()->GetChild(i)->NumExpandedNodes();
    tmp_node = tmp_node->parent();
  }
  if (root_shown_) {
    (*depth)++;
    row++;
  }
  return row;
}

TreeView::InternalNode* TreeView::GetNodeByRow(int row, int* depth) {
  int current_row = root_row();
  *depth = 0;
  return GetNodeByRowImpl(&root_, row, root_depth(), &current_row, depth);
}

TreeView::InternalNode* TreeView::GetNodeByRowImpl(InternalNode* node,
                                                   int target_row,
                                                   int current_depth,
                                                   int* current_row,
                                                   int* node_depth) {
  if (*current_row == target_row) {
    *node_depth = current_depth;
    return node;
  }
  (*current_row)++;
  if (node->is_expanded()) {
    current_depth++;
    for (int i = 0; i < node->child_count(); ++i) {
      InternalNode* result = GetNodeByRowImpl(
          node->GetChild(i), target_row, current_depth, current_row,
          node_depth);
      if (result)
        return result;
    }
  }
  return NULL;
}

void TreeView::IncrementSelection(IncrementType type) {
  if (!model_)
    return;

  if (!GetSelectedNode()) {
    // If nothing is selected select the first or last node.
    if (!root_.child_count())
      return;
    if (type == INCREMENT_PREVIOUS) {
      int row_count = GetRowCount();
      int depth;
      DCHECK(row_count);
      InternalNode* node = GetNodeByRow(row_count - 1, &depth);
      SetSelectedNode(node->model_node());
    } else if (root_shown_) {
      SetSelectedNode(root_.model_node());
    } else {
      SetSelectedNode(root_.GetChild(0)->model_node());
    }
    return;
  }

  int row, depth;
  int delta = type == INCREMENT_PREVIOUS ? -1 : 1;
  row = GetRowForNode(selected_node_, &depth);
  int new_row = std::min(GetRowCount() - 1, std::max(0, row + delta));
  if (new_row == row)
    return;  // At the end/beginning.
  SetSelectedNode(GetNodeByRow(new_row, &depth)->model_node());
}

void TreeView::CollapseOrSelectParent() {
  if (selected_node_) {
    if (selected_node_->is_expanded())
      Collapse(selected_node_->model_node());
    else if (selected_node_->parent())
      SetSelectedNode(selected_node_->parent()->model_node());
  }
}

void TreeView::ExpandOrSelectChild() {
  if (selected_node_) {
    if (!selected_node_->is_expanded())
      Expand(selected_node_->model_node());
    else if (selected_node_->child_count())
      SetSelectedNode(selected_node_->GetChild(0)->model_node());
  }
}

bool TreeView::ExpandImpl(TreeModelNode* model_node) {
  TreeModelNode* parent = model_->GetParent(model_node);
  if (!parent) {
    // Node should be the root.
    DCHECK_EQ(root_.model_node(), model_node);
    bool was_expanded = root_.is_expanded();
    root_.set_is_expanded(true);
    return !was_expanded;
  }

  // Expand all the parents.
  bool return_value = ExpandImpl(parent);
  InternalNode* internal_node =
      GetInternalNodeForModelNode(model_node, CREATE_IF_NOT_LOADED);
  DCHECK(internal_node);
  if (!internal_node->is_expanded()) {
    if (!internal_node->loaded_children())
      LoadChildren(internal_node);
    internal_node->set_is_expanded(true);
    return_value = true;
  }
  return return_value;
}

// InternalNode ----------------------------------------------------------------

TreeView::InternalNode::InternalNode()
    : model_node_(NULL),
      loaded_children_(false),
      is_expanded_(false),
      text_width_(0) {
}

TreeView::InternalNode::~InternalNode() {
}

void TreeView::InternalNode::Reset(ui::TreeModelNode* node) {
  model_node_ = node;
  loaded_children_ = false;
  is_expanded_ = false;
  text_width_ = 0;
}

int TreeView::InternalNode::NumExpandedNodes() {
  int result = 1;  // For this.
  if (!is_expanded_)
    return result;
  for (int i = 0; i < child_count(); ++i)
    result += GetChild(i)->NumExpandedNodes();
  return result;
}

int TreeView::InternalNode::GetMaxWidth(int indent, int depth) {
  int max_width = text_width_ + indent * depth;
  if (!is_expanded_)
    return max_width;
  for (int i = 0; i < child_count(); ++i) {
    max_width = std::max(max_width,
                         GetChild(i)->GetMaxWidth(indent, depth + 1));
  }
  return max_width;
}

}  // namespace views
