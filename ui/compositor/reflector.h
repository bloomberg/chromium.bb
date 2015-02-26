// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_REFLECTOR_H_
#define UI_COMPOSITOR_REFLECTOR_H_

#include "ui/compositor/compositor_export.h"

namespace ui {

class COMPOSITOR_EXPORT Reflector {
 public:
  virtual ~Reflector();
  virtual void OnMirroringCompositorResized() = 0;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_REFLECTOR_H_
