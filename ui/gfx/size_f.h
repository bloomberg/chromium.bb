// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SIZE_F_H_
#define UI_GFX_SIZE_F_H_
#pragma once

#include <string>

#include "ui/base/ui_export.h"
#include "ui/gfx/size.h"
#include "ui/gfx/size_base.h"

namespace gfx {

// A floating version of gfx::Size.
class UI_EXPORT SizeF : public SizeBase<SizeF, float> {
 public:
  SizeF();
  SizeF(float width, float height);
  ~SizeF();

  Size ToSize() const;

  std::string ToString() const;
};

}  // namespace gfx

#endif  // UI_GFX_SIZE_F_H_
