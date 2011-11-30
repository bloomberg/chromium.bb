// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/launcher/view_model.h"

#include "base/logging.h"
#include "ui/views/view.h"

namespace aura_shell {

ViewModel::ViewModel() {
}

ViewModel::~ViewModel() {
  // view are owned by their parent, no need to delete them.
}

void ViewModel::Add(views::View* view, int index) {
  Entry entry;
  entry.view = view;
  entries_.insert(entries_.begin() + index, entry);
}

void ViewModel::Remove(int index) {
  if (index == -1)
    return;

  entries_.erase(entries_.begin() + index);
}

void ViewModel::Move(int index, int target_index) {
  if (index == target_index)
    return;
  Entry entry(entries_[index]);
  entries_.erase(entries_.begin() + index);
  entries_.insert(entries_.begin() + target_index, entry);
}

void ViewModel::Clear() {
  Entries entries;
  entries.swap(entries_);
  for (size_t i = 0; i < entries.size(); ++i)
    delete entries[i].view;
}

int ViewModel::GetIndexOfView(views::View* view) const {
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].view == view)
      return static_cast<int>(i);
  }
  return -1;
}

}  // namespace aura_shell
