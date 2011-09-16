// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_SAMPLE_WINDOW_H_
#define UI_AURA_SHELL_SAMPLE_WINDOW_H_
#pragma once

#include "views/widget/widget_delegate.h"

namespace aura_shell {
namespace internal {

class SampleWindow : public views::WidgetDelegateView {
 public:
  static void CreateSampleWindow();

 private:
  SampleWindow();
  virtual ~SampleWindow();

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual std::wstring GetWindowTitle() const OVERRIDE;
  virtual View* GetContentsView() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SampleWindow);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_SAMPLE_WINDOW_H_
