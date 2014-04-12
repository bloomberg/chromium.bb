// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_tree_source_views.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/views/widget/widget.h"

namespace views {

// A simple store associating each |View| with a unique id.
class AXViewStore {
 public:
  AXViewStore() : view_id_(0) {};

  int32 GetOrStoreView(View* view) {
    int32 current_id = RetrieveId(view);
    if (current_id != -1)
      return current_id;
    id_to_view_[++view_id_] = view;
    view_to_id_[view] = view_id_;
    return view_id_;
  }

  void RemoveView(View* view) {
    std::map<View*, int32>::iterator view_it = view_to_id_.find(view);
    if (view_it == view_to_id_.end())
      return;
    int32 view_id = view_it->second;
    view_to_id_.erase(view_it);
    std::map<int32, View*>::iterator id_it = id_to_view_.find(view_id);
    if (id_it == id_to_view_.end()) {
      NOTREACHED();
      return;
    }
    id_to_view_.erase(id_it);
  }

  View* RetrieveView(int32 id) {
    std::map<int32, View*>::iterator it = id_to_view_.find(id);
    if (it != id_to_view_.end())
      return it->second;
    return NULL;
  }

  int32 RetrieveId(View* view) {
    if (!view)
      return -1;
    std::map<View*, int32>::iterator it = view_to_id_.find(view);
    if (it != view_to_id_.end())
      return it->second;
    return -1;
  }

 private:
  // Unique id assigned to a view.
  int32 view_id_;

  // Maps from id to |View*| and |View*| to id.
  std::map<int32, View*> id_to_view_;
  std::map<View*, int32> view_to_id_;
};

AXTreeSourceViews::AXTreeSourceViews(Widget* root) :
    view_store_(new AXViewStore()),
    root_(root) {
  root->AddObserver(this);
}

AXTreeSourceViews::~AXTreeSourceViews() {
  if (root_)
    root_->RemoveObserver(this);
}

View* AXTreeSourceViews::GetRoot() const {
  if (!root_)
    return NULL;
  return root_->GetRootView();
}

View* AXTreeSourceViews::GetFromId(int32 id) const {
  return view_store_->RetrieveView(id);
}

int32 AXTreeSourceViews::GetId(View* node) const {
  return view_store_->GetOrStoreView(node);
}

void AXTreeSourceViews::GetChildren(View* node,
    std::vector<View*>* out_children) const {
  for (int i = 0; i < node->child_count(); ++i) {
    View* child_view = node->child_at(i);
    out_children->push_back(child_view);
  }
}

View* AXTreeSourceViews::GetParent(View* node) const {
  return node->parent();
}

bool AXTreeSourceViews::IsValid(View* node) const {
  if (root_ && root_->GetRootView())
    return root_->GetRootView()->Contains(node);
  return false;
}

bool AXTreeSourceViews::IsEqual(View* node1,
                                View* node2) const {
  return node1 == node2;
}

View* AXTreeSourceViews::GetNull() const {
  return NULL;
}

void AXTreeSourceViews::SerializeNode(
    View* node, ui::AXNodeData* out_data) const {
  ui::AXViewState view_data;
  const_cast<View*>(node)->GetAccessibleState(&view_data);
  out_data->id = view_store_->GetOrStoreView(node);
  out_data->role = view_data.role;

  out_data->state = 0;
  for (int32 state = ui::AX_STATE_NONE;
       state <= ui::AX_STATE_LAST;
       ++state) {
    ui::AXState state_flag = static_cast<ui::AXState>(state);
    if (view_data.HasStateFlag(state_flag))
      out_data->state |= 1 << state;
  }

  out_data->location = node->GetBoundsInScreen();
  out_data->AddStringAttribute(
      ui::AX_ATTR_NAME, base::UTF16ToUTF8(view_data.name));
  out_data->AddStringAttribute(
      ui::AX_ATTR_VALUE, base::UTF16ToUTF8(view_data.value));
}

void AXTreeSourceViews::OnWillRemoveView(Widget* widget, View* view) {
  if (widget == root_)
    view_store_->RemoveView(view);
}

void AXTreeSourceViews::OnWidgetDestroyed(Widget* widget) {
  if (widget == root_)
    root_ = NULL;
}

} // namespace views
