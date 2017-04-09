// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ColorSpaceProfileData_h
#define ColorSpaceProfileData_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Vector.h"

namespace blink {

PLATFORM_EXPORT void Bt709ColorProfileData(Vector<char>& data);
PLATFORM_EXPORT void Bt601ColorProfileData(Vector<char>& data);

}  // namespace blink

#endif  // ColorSpaceProfileData_h
