// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file overrides the inclusion of talk/base/scoped_ptr.h.  We use
// a version of scoped_ptr from Chromium base, to avoid multiple definitions.

#ifndef OVERRIDES_TALK_BASE_SCOPED_PTR_H__
#define OVERRIDES_TALK_BASE_SCOPED_PTR_H__

#include "base/scoped_ptr.h"

namespace talk_base {

using ::scoped_ptr;
using ::scoped_array;

}  // namespace talk_base

#endif // OVERRIDES_TALK_BASE_SCOPED_PTR_H__
