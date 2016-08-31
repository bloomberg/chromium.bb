// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGDirection_h
#define NGDirection_h

#include "core/CoreExport.h"
#include "platform/text/TextDirection.h"

namespace blink {

enum NGDirection { LeftToRight = 0, RightToLeft = 1 };

CORE_EXPORT NGDirection FromPlatformDirection(TextDirection);

}  // namespace blink

#endif  // NGDirection_h
