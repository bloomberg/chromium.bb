// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_COMBOBOX_MODEL_H_
#define UI_BASE_MODELS_COMBOBOX_MODEL_H_
#pragma once

#include "base/string16.h"

namespace ui {

// A data model for a combo box.
class ComboboxModel {
 public:
  // Returns the number of items in the combo box.
  virtual int GetItemCount() const = 0;

  // Returns the string at the specified index.
  virtual string16 GetItemAt(int index) = 0;

 protected:
  virtual ~ComboboxModel() {}
};

}  // namespace ui

#endif  // UI_BASE_MODELS_COMBOBOX_MODEL_H_
