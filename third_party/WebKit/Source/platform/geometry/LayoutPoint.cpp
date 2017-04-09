// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/LayoutPoint.h"

#include "platform/wtf/text/WTFString.h"

namespace blink {

String LayoutPoint::ToString() const {
  return String::Format("%s,%s", X().ToString().Ascii().Data(),
                        Y().ToString().Ascii().Data());
}

}  // namespace blink
