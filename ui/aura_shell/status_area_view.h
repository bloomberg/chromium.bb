// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_STATUS_AREA_VIEW_H_
#define UI_AURA_SHELL_STATUS_AREA_VIEW_H_
#pragma once

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/widget/widget_delegate.h"

namespace aura_shell {
namespace internal {

class StatusAreaView : public views::WidgetDelegateView {
 public:
  StatusAreaView();
  virtual ~StatusAreaView();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

 private:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  SkBitmap status_mock_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaView);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_STATUS_AREA_VIEW_H_
