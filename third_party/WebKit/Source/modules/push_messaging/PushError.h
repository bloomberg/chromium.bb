// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushError_h
#define PushError_h

#include "core/dom/DOMError.h"
#include "heap/Handle.h"
#include "public/platform/WebPushError.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class PushError {
    WTF_MAKE_NONCOPYABLE(PushError);
public:
    // For CallbackPromiseAdapter.
    typedef blink::WebPushError WebType;
    static PassRefPtrWillBeRawPtr<DOMError> from(WebType* webErrorRaw)
    {
        OwnPtr<WebType> webError = adoptPtr(webErrorRaw);
        RefPtrWillBeRawPtr<DOMError> error = DOMError::create(errorString(webError->errorType), webError->message);
        return error.release();
    }

private:
    static String errorString(blink::WebPushError::ErrorType);

    PushError() WTF_DELETED_FUNCTION;
};

} // namespace WebCore

#endif // PushError_h
