// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerClientFocusCallback_h
#define WebServiceWorkerClientFocusCallback_h

#include "WebCallbacks.h"

namespace blink {

struct WebServiceWorkerError;

typedef WebCallbacks<bool, WebServiceWorkerError> WebServiceWorkerClientFocusCallback;

} // namespace blink

#endif // WebServiceWorkerClientFocusCallback_h
