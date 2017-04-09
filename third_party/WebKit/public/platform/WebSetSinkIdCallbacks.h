// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSetSinkIdCallbacks_h
#define WebSetSinkIdCallbacks_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebString.h"

namespace blink {

enum class WebSetSinkIdError {
  kNotFound = 1,
  kNotAuthorized,
  kAborted,
  kNotSupported,
  kLast = kNotSupported
};

using WebSetSinkIdCallbacks = WebCallbacks<void, WebSetSinkIdError>;

}  // namespace blink

#endif  // WebSetSinkIdCallbacks_h
