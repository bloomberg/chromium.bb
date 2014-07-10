// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_TEST_TEST_CURSOR_FACTORY_H_
#define UI_OZONE_PLATFORM_TEST_TEST_CURSOR_FACTORY_H_

#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"

namespace ui {

class TestCursorFactory : public BitmapCursorFactoryOzone {
 public:
  TestCursorFactory();
  virtual ~TestCursorFactory();

  // BitmapCursorFactoryOzone:
  virtual gfx::AcceleratedWidget GetCursorWindow() OVERRIDE;
  virtual void SetBitmapCursor(
      gfx::AcceleratedWidget widget,
      scoped_refptr<BitmapCursorOzone> cursor) OVERRIDE;

 private:
  gfx::AcceleratedWidget cursor_window_;
  scoped_refptr<BitmapCursorOzone> cursor_;

  DISALLOW_COPY_AND_ASSIGN(TestCursorFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_TEST_TEST_CURSOR_FACTORY_H_
