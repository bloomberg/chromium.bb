// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_IMAGE_PNG_REP_H_
#define UI_GFX_IMAGE_IMAGE_PNG_REP_H_

#include "base/memory/ref_counted_memory.h"
#include "ui/base/layout.h"
#include "ui/base/ui_export.h"

namespace gfx {

// An ImagePNGRep represents a bitmap's png encoded data and the scale factor it
// was intended for.
struct UI_EXPORT ImagePNGRep {
  ImagePNGRep();
  ImagePNGRep(const scoped_refptr<base::RefCountedMemory>& data,
              ui::ScaleFactor data_scale_factor);
  ~ImagePNGRep();

  scoped_refptr<base::RefCountedMemory> raw_data;
  ui::ScaleFactor scale_factor;
};

}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_PNG_REP_H_
