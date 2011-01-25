// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/listbox/listbox.h"

#include "views/controls/listbox/native_listbox_wrapper.h"
#include "views/controls/native/native_view_host.h"
#include "views/layout/fill_layout.h"

namespace views {

// Listbox ------------------------------------------------------------------

Listbox::Listbox(
    const std::vector<string16>& strings, Listbox::Listener* listener)
    : strings_(strings),
      listener_(listener),
      native_wrapper_(NULL) {
  SetLayoutManager(new FillLayout());
}

Listbox::~Listbox() {
}

int Listbox::GetRowCount() const {
  return static_cast<int>(strings_.size());
}

int Listbox::SelectedRow() const {
  if (!native_wrapper_)
    return -1;
  return native_wrapper_->SelectedRow();
}

void Listbox::SelectRow(int model_row) {
  if (!native_wrapper_)
    return;
  native_wrapper_->SelectRow(model_row);
}

void Listbox::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (is_add && !native_wrapper_ && GetWidget()) {
    // The native wrapper's lifetime will be managed by the view hierarchy after
    // we call AddChildView.
    native_wrapper_ = CreateWrapper();
    AddChildView(native_wrapper_->GetView());
  }
}

////////////////////////////////////////////////////////////////////////////////
// Listbox, protected:

NativeListboxWrapper* Listbox::CreateWrapper() {
  return NativeListboxWrapper::CreateNativeWrapper(this, strings_, listener_);
}

}  // namespace views
