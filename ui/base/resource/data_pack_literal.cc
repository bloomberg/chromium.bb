// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"

namespace ui {

extern const char kSamplePakContents[] = {
    0x03, 0x00, 0x00, 0x00,               // header(version
    0x04, 0x00, 0x00, 0x00,               //        no. entries)
    0x01, 0x00, 0x26, 0x00, 0x00, 0x00,   // index entry 1
    0x04, 0x00, 0x26, 0x00, 0x00, 0x00,   // index entry 4
    0x06, 0x00, 0x32, 0x00, 0x00, 0x00,   // index entry 6
    0x0a, 0x00, 0x3e, 0x00, 0x00, 0x00,   // index entry 10
    0x00, 0x00, 0x3e, 0x00, 0x00, 0x00,   // extra entry for the size of last
    't', 'h', 'i', 's', ' ', 'i', 's', ' ', 'i', 'd', ' ', '4',
    't', 'h', 'i', 's', ' ', 'i', 's', ' ', 'i', 'd', ' ', '6'
};

extern const size_t kSamplePakSize = sizeof(kSamplePakContents);

}  // namespace ui
