// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/FloatBox.h"

#include "platform/wtf/text/WTFString.h"

namespace blink {

String FloatBox::ToString() const {
  return String::Format("%lg,%lg,%lg %lgx%lgx%lg", X(), Y(), Z(), Width(),
                        Height(), Depth());
}

}  // namespace blink
