// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/sample_window.h"

#include "ui/gfx/canvas.h"
#include "views/widget/widget.h"

namespace aura_shell {
namespace internal {

// static
void SampleWindow::CreateSampleWindow() {
  views::Widget::CreateWindowWithBounds(new SampleWindow,
                                        gfx::Rect(120, 150, 400, 300))->Show();
}

SampleWindow::SampleWindow() {
}

SampleWindow::~SampleWindow() {
}

void SampleWindow::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRectInt(SK_ColorDKGRAY, 0, 0, width(), height());
}

std::wstring SampleWindow::GetWindowTitle() const {
  return L"Sample Window";
}

views::View* SampleWindow::GetContentsView() {
  return this;
}

}  // namespace internal
}  // namespace aura_shell
