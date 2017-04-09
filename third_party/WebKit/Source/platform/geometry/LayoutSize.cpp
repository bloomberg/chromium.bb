// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/LayoutSize.h"

#include "platform/wtf/text/WTFString.h"

namespace blink {

String LayoutSize::ToString() const {
  return String::Format("%sx%s", Width().ToString().Ascii().Data(),
                        Height().ToString().Ascii().Data());
}

}  // namespace blink
