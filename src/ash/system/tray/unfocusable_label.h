// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_UNFOCUSABLE_LABEL_H_
#define ASH_SYSTEM_TRAY_UNFOCUSABLE_LABEL_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/controls/label.h"

namespace ash {

// A label which is not focusable with ChromeVox.
class ASH_EXPORT UnfocusableLabel : public views::Label {
 public:
  UnfocusableLabel() = default;
  ~UnfocusableLabel() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnfocusableLabel);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_UNFOCUSABLE_LABEL_H_
