// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_TEST_VIEWS_H_
#define UI_VIEWS_TEST_TEST_VIEWS_H_

#include "base/memory/scoped_ptr.h"
#include "ui/events/event_constants.h"
#include "ui/views/view.h"

namespace views {

// A view that requests a set amount of space.
class StaticSizedView : public View {
 public:
  explicit StaticSizedView(const gfx::Size& size);
  ~StaticSizedView() override;

  gfx::Size GetPreferredSize() const override;

 private:
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(StaticSizedView);
};

// A view that accomodates testing layouts that use GetHeightForWidth.
class ProportionallySizedView : public View {
 public:
  explicit ProportionallySizedView(int factor);
  ~ProportionallySizedView() override;

  void set_preferred_width(int width) { preferred_width_ = width; }

  int GetHeightForWidth(int w) const override;
  gfx::Size GetPreferredSize() const override;

 private:
  // The multiplicative factor between width and height, i.e.
  // height = width * factor_.
  int factor_;

  // The width used as the preferred size. -1 if not used.
  int preferred_width_;

  DISALLOW_COPY_AND_ASSIGN(ProportionallySizedView);
};

// Class that closes the widget (which ends up deleting it immediately) when the
// appropriate event is received.
class CloseWidgetView : public View {
 public:
  explicit CloseWidgetView(ui::EventType event_type);

  // ui::EventHandler override:
  void OnEvent(ui::Event* event) override;

 private:
  const ui::EventType event_type_;

  DISALLOW_COPY_AND_ASSIGN(CloseWidgetView);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_TEST_VIEWS_H_
