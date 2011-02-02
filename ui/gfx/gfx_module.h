// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MODULE_H_
#define UI_GFX_MODULE_H_
#pragma once

#include "base/basictypes.h"
#include "base/string_piece.h"

namespace gfx {

// Defines global initializers and associated methods for the gfx module.
// See net/base/net_module.h for more details.
class GfxModule {
 public:
  typedef base::StringPiece (*ResourceProvider)(int key);

  // Set the function to call when the gfx module needs resources
  static void SetResourceProvider(ResourceProvider func);

  // Call the resource provider (if one exists) to get the specified resource.
  // Returns an empty string if the resource does not exist or if there is no
  // resource provider.
  static base::StringPiece GetResource(int key);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(GfxModule);
};

}  // namespace gfx

#endif  // UI_GFX_MODULE_H_
