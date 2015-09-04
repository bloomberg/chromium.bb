// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Alias_h
#define Alias_h

#include "wtf/WTFExport.h"

namespace WTF {

// Make the optimizer think that var is aliased. This is to prevent it from
// optimizing out variables that would not otherwise be live at the point
// of a potential crash.
void WTF_EXPORT alias(const void* var);

} // namespace WTF

#endif // Alias_h
