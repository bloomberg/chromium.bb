// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeofencingError_h
#define GeofencingError_h

#include "core/dom/DOMException.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebGeofencingError.h"

namespace blink {

class ScriptPromiseResolver;

class GeofencingError {
    WTF_MAKE_NONCOPYABLE(GeofencingError);
public:
    // For CallbackPromiseAdapter.
    typedef blink::WebGeofencingError WebType;
    static PassRefPtrWillBeRawPtr<DOMException> take(ScriptPromiseResolver*, WebType* webErrorRaw);
    static void dispose(WebType* webErrorRaw);

private:
    GeofencingError() WTF_DELETED_FUNCTION;
};

} // namespace blink

#endif // GeofencingError_h
