// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/NavigatorLanguage.h"

#include "platform/Language.h"

namespace WebCore {

AtomicString NavigatorLanguage::language(bool& isNull)
{
    isNull = false;
    return defaultLanguage();
}

} // namespace WebCore
