// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerClientsClaimCallbacks_h
#define WebServiceWorkerClientsClaimCallbacks_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebPassOwnPtr.h"

namespace blink {

struct WebServiceWorkerError;

using WebServiceWorkerClientsClaimCallbacks = WebCallbacks<void, WebPassOwnPtr<WebServiceWorkerError>>;

} // namespace blink

#endif // WebServiceWorkerClientsClaimCallbacks_h
