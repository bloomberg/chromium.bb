// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/throbber_example.h"

#include "views/controls/throbber.h"
#include "views/layout/fill_layout.h"
#include "views/view.h"

namespace {

// Time in ms per throbber frame.
const int kThrobberFrameMs = 60;

class ThrobberView : public views::View {
 public:
  ThrobberView() {
    throbber_ = new views::Throbber(kThrobberFrameMs, false);
    AddChildView(throbber_);
    throbber_->SetVisible(true);
    throbber_->Start();
  }

  virtual gfx::Size GetPreferredSize() {
    return gfx::Size(width(), height());
  }

  virtual void Layout() {
    views::View* child = GetChildViewAt(0);
    gfx::Size ps = child->GetPreferredSize();
    child->SetBounds((width() - ps.width()) / 2,
                     (height() - ps.height()) / 2,
                     ps.width(), ps.height());
    SizeToPreferredSize();
  }

 private:
  views::Throbber* throbber_;

  DISALLOW_COPY_AND_ASSIGN(ThrobberView);
};

}  // namespace

namespace examples {

ThrobberExample::ThrobberExample(ExamplesMain* main)
    : ExampleBase(main) {
}

ThrobberExample::~ThrobberExample() {
}

std::wstring ThrobberExample::GetExampleTitle() {
  return L"Throbber";
}

void ThrobberExample::CreateExampleView(views::View* container) {
  container->SetLayoutManager(new views::FillLayout());
  container->AddChildView(new ThrobberView());
}

}  // namespace examples
