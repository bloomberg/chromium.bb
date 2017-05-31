// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DataResourceHelper_h
#define DataResourceHelper_h

#include "platform/PlatformExport.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

PLATFORM_EXPORT String GetDataResourceAsASCIIString(const char* resource);

}  // namespace blink

#endif  // DataResourceHelper_h
