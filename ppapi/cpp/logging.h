// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_LOGGING_H_
#define PPAPI_CPP_LOGGING_H_

#include <cassert>

#define PP_DCHECK(a) assert(a)

#define PP_NOTREACHED() assert(false)

#endif  // PPAPI_CPP_LOGGING_H_
