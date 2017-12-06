// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scroll/ScrollSnapData.h"

namespace blink {

std::ostream& operator<<(std::ostream& ostream, const SnapAreaData& area_data) {
  return ostream << area_data.snap_offset.Width() << ", "
                 << area_data.snap_offset.Height();
}

std::ostream& operator<<(std::ostream& ostream,
                         const SnapContainerData& container_data) {
  for (SnapAreaData area_data : container_data.snap_area_list) {
    ostream << area_data << "\n";
  }
  return ostream;
}

}  // namespace blink
