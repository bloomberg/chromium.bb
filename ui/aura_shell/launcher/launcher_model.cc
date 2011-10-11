// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/launcher/launcher_model.h"

#include "ui/aura_shell/launcher/launcher_model_observer.h"
#include "views/view.h"

namespace aura_shell {

LauncherModel::LauncherModel() : selected_index_(-1) {
}

LauncherModel::~LauncherModel() {
  for (Items::iterator i = items_.begin(); i != items_.end(); ++i) {
    delete i->view;
    i->view = NULL;
  }
}

void LauncherModel::AddItem(views::View* view, int index, bool draggable) {
  DCHECK(index >= 0 && index <= item_count());
  Item item;
  item.view = view;
  item.draggable = draggable;
  items_.insert(items_.begin() + index, item);
  FOR_EACH_OBSERVER(LauncherModelObserver, observers_,
                    LauncherItemAdded(index));
}

void LauncherModel::RemoveItem(views::View* view) {
  int index = IndexOfItemByView(view);
  if (index != -1)
    RemoveItemAt(index);
}

void LauncherModel::RemoveItemAt(int index) {
  DCHECK(index >= 0 && index < item_count());
  items_.erase(items_.begin() + index);
  FOR_EACH_OBSERVER(LauncherModelObserver, observers_,
                    LauncherItemRemoved(index));
}

void LauncherModel::SetSelectedIndex(int index) {
  if (index == selected_index_)
    return;
  DCHECK(selected_index_ == -1 ||
         (selected_index_ >= 0 && selected_index_ < item_count()));
  selected_index_ = index;
  FOR_EACH_OBSERVER(LauncherModelObserver, observers_,
                    LauncherSelectionChanged());
}

int LauncherModel::IndexOfItemByView(views::View* view) {
  for (Items::const_iterator i = items_.begin(); i != items_.end(); ++i) {
    if (i->view == view)
      return i - items_.begin();
  }
  return -1;
}

void LauncherModel::AddObserver(LauncherModelObserver* observer) {
  observers_.AddObserver(observer);
}

void LauncherModel::RemoveObserver(LauncherModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace aura_shell
