// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/StyleWillChangeData.h"

namespace blink {

StyleWillChangeData::StyleWillChangeData()
    : contents_(false), scroll_position_(false) {}

StyleWillChangeData::StyleWillChangeData(const StyleWillChangeData& o)
    : RefCounted<StyleWillChangeData>(),
      properties_(o.properties_),
      contents_(o.contents_),
      scroll_position_(o.scroll_position_) {}

}  // namespace blink
