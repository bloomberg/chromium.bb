// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator_cocoa.h"

namespace ui {

AcceleratorCocoa::AcceleratorCocoa() : Accelerator() {}

AcceleratorCocoa::AcceleratorCocoa(NSString* key_code, NSUInteger mask)
    : Accelerator(ui::VKEY_UNKNOWN, mask),
      characters_([key_code copy]) {
}

AcceleratorCocoa::AcceleratorCocoa(const AcceleratorCocoa& accelerator)
    : Accelerator(accelerator) {
  characters_.reset([accelerator.characters_ copy]);
}

AcceleratorCocoa::~AcceleratorCocoa() {}

AcceleratorCocoa& AcceleratorCocoa::operator=(
    const AcceleratorCocoa& accelerator) {
  if (this != &accelerator) {
    *static_cast<Accelerator*>(this) = accelerator;
    characters_.reset([accelerator.characters_ copy]);
  }
  return *this;
}

bool AcceleratorCocoa::operator==(const AcceleratorCocoa& rhs) const {
  return [characters_ isEqualToString:rhs.characters_.get()] &&
  (modifiers_ == rhs.modifiers_);
}

bool AcceleratorCocoa::operator!=(const AcceleratorCocoa& rhs) const {
  return !(*this == rhs);
}

}  // namespace ui
