// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServicePortCallbacks_h
#define WebServicePortCallbacks_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/modules/navigator_services/WebServicePort.h"

namespace blink {

using WebServicePortConnectCallbacks = WebCallbacks<WebServicePortID, void>;
using WebServicePortConnectEventCallbacks = WebCallbacks<const WebServicePort&, void>;

} // namespace blink

#endif // WebServicePortCallbacks_h
