// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_png_rep.h"

namespace gfx {

ImagePNGRep::ImagePNGRep()
    : raw_data(NULL),
      scale_factor(ui::SCALE_FACTOR_NONE) {
}

ImagePNGRep::ImagePNGRep(const scoped_refptr<base::RefCountedMemory>& data,
                         ui::ScaleFactor data_scale_factor)
    : raw_data(data),
      scale_factor(data_scale_factor) {
}

ImagePNGRep::~ImagePNGRep() {
}

}  // namespace gfx
