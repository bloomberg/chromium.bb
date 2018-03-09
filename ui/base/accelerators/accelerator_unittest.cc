// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"

namespace ui {

TEST(AcceleratorTest, Repeat) {
  const Accelerator accelerator_a(VKEY_A, EF_NONE);
  EXPECT_FALSE(accelerator_a.IsRepeat());

  const Accelerator accelerator_b(VKEY_B, EF_IS_REPEAT);
  EXPECT_TRUE(accelerator_b.IsRepeat());

  const Accelerator accelerator_b_copy(accelerator_b);
  EXPECT_TRUE(accelerator_b_copy.IsRepeat());
}

TEST(AcceleratorTest, TimeStamp) {
  const Accelerator accelerator_a(VKEY_A, EF_NONE);
  EXPECT_EQ(base::TimeTicks(), accelerator_a.time_stamp());

  const base::TimeTicks event_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(1);
  KeyEvent keyevent(ET_KEY_PRESSED, VKEY_SPACE, EF_NONE, event_time);

  const Accelerator accelerator_b(keyevent);
  EXPECT_EQ(event_time, accelerator_b.time_stamp());
}

}  // namespace ui
