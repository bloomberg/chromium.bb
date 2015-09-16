// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_HOST_H_
#define UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_HOST_H_

#include "base/macros.h"
#include "ui/views/animation/ink_drop_host.h"

namespace views {

// A non-functional, dummy implementation of an InkDropHost that can be used as
// a placeholder during tests.
class TestInkDropHost : public InkDropHost {
 public:
  TestInkDropHost();
  ~TestInkDropHost() override;

  // TestInkDropHost:
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestInkDropHost);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_HOST_H_
