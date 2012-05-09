// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_DROP_SHADOW_LABEL_H_
#define UI_APP_LIST_DROP_SHADOW_LABEL_H_
#pragma once

#include <vector>

#include "ui/gfx/shadow_value.h"
#include "ui/views/controls/label.h"

namespace app_list {

/////////////////////////////////////////////////////////////////////////////
//
// DropShadowLabel class
//
// A drop shadow label is a view subclass that can display a string
// with a drop shadow.
//
/////////////////////////////////////////////////////////////////////////////
class DropShadowLabel : public views::Label  {
 public:
  DropShadowLabel();
  virtual ~DropShadowLabel();

  void SetTextShadows(int shadow_count, const gfx::ShadowValue* shadows);

 private:
  // Overridden from views::Label:
  virtual gfx::Insets GetInsets() const OVERRIDE;
  virtual void PaintText(gfx::Canvas* canvas,
                         const string16& text,
                         const gfx::Rect& text_bounds,
                         int flags) OVERRIDE;

  std::vector<gfx::ShadowValue> text_shadows_;

  DISALLOW_COPY_AND_ASSIGN(DropShadowLabel);
};

}  // namespace app_list

#endif  // UI_APP_LIST_DROP_SHADOW_LABEL_H_
