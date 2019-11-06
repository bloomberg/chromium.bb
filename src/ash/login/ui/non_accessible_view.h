// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_NON_ACCESSIBLE_VIEW_H_
#define ASH_LOGIN_UI_NON_ACCESSIBLE_VIEW_H_

#include <string>

#include "ui/views/view.h"

namespace ash {

// A views::View instance that is not shown in the accessibility hierarchy.
class NonAccessibleView : public views::View {
 public:
  NonAccessibleView();
  explicit NonAccessibleView(const std::string& name);
  ~NonAccessibleView() override;

  // views::View:
  const char* GetClassName() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  const std::string name_;

  DISALLOW_COPY_AND_ASSIGN(NonAccessibleView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_NON_ACCESSIBLE_VIEW_H_
