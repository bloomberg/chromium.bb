// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "wtf/Alias.h"

// TODO(tasak): This code is copied from //base/debug/alias.cc.
// If Blink is allowed to use //base/debug, remove this code and use //base/debug.
namespace WTF {

#if COMPILER(MSVC)
#pragma optimize("", off)
#endif

void alias(const void* var)
{
}

#if COMPILER(MSVC)
#pragma optimize("", on)
#endif

} // namespace WTF
