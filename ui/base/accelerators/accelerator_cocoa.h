// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_ACCELERATOR_COCOA_H_
#define UI_BASE_ACCELERATORS_ACCELERATOR_COCOA_H_
#pragma once

#include <Foundation/Foundation.h>

#include "base/memory/scoped_nsobject.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ui_export.h"

namespace ui {

// This is a subclass of the cross-platform Accelerator, but with more direct
// support for Cocoa key equivalents. Note that the typical use case for this
// class is to initialize it with a string literal, which is why it sends
// |-copy| to the |key_code| paramater in the constructor.
class UI_EXPORT AcceleratorCocoa : public Accelerator {
 public:
  AcceleratorCocoa();
  AcceleratorCocoa(NSString* key_code, NSUInteger mask);
  AcceleratorCocoa(const AcceleratorCocoa& accelerator);
  virtual ~AcceleratorCocoa();

  AcceleratorCocoa& operator=(const AcceleratorCocoa& accelerator);

  bool operator==(const AcceleratorCocoa& rhs) const;
  bool operator!=(const AcceleratorCocoa& rhs) const;

  NSString* characters() const {
    return characters_.get();
  }

 private:
  // String of characters for the key equivalent.
  scoped_nsobject<NSString> characters_;
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_ACCELERATOR_COCOA_H_
