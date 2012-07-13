// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/textfield_controller.h"

namespace views {

bool TextfieldController::IsCommandIdEnabled(int command_id) const {
  return false;
}

bool TextfieldController::IsItemForCommandIdDynamic(int command_id) const {
  return false;
}

string16 TextfieldController::GetLabelForCommandId(int command_id) const {
  return string16();
}

}  // namespace views
