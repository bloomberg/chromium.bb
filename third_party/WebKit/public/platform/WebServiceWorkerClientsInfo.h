// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerClientsInfo_h
#define WebServiceWorkerClientsInfo_h

#include "WebVector.h"

namespace blink {

struct WebServiceWorkerClientsInfo {
    WebVector<int> clientIDs;
};

} // namespace blink

#endif // WebServiceWorkerClientsInfo_h
