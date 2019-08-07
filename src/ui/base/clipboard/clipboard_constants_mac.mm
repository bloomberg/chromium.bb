// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_constants.h"

#import <Foundation/Foundation.h>

namespace ui {

// TODO(dcheng): This name is temporary. See crbug.com/106449.
#if !defined(USE_AURA)
NSString* const kWebCustomDataPboardType = @"org.chromium.web-custom-data";
#endif

}  // namespace ui
