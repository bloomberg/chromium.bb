// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerRegistration_h
#define WebServiceWorkerRegistration_h

#include "public/platform/WebURL.h"

namespace blink {

class WebServiceWorkerRegistration {
public:
    virtual ~WebServiceWorkerRegistration() { }

    virtual WebURL scope() const { return WebURL(); }
};

} // namespace blink

#endif // WebServiceWorkerRegistration_h
