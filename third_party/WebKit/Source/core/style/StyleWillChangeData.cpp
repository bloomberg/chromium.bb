// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/StyleWillChangeData.h"

namespace blink {

StyleWillChangeData::StyleWillChangeData()
    : will_change_contents_(false), will_change_scroll_position_(false) {}

StyleWillChangeData::StyleWillChangeData(const StyleWillChangeData& o)
    : RefCounted<StyleWillChangeData>(),
      will_change_properties_(o.will_change_properties_),
      will_change_contents_(o.will_change_contents_),
      will_change_scroll_position_(o.will_change_scroll_position_) {}

}  // namespace blink
