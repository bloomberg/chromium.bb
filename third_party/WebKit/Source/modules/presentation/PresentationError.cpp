// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/PresentationError.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "public/platform/modules/presentation/WebPresentationError.h"
#include "wtf/OwnPtr.h"

namespace blink {

// static
PassRefPtrWillBeRawPtr<DOMException> PresentationError::take(WebPresentationError* errorRaw)
{
    ASSERT(errorRaw);
    OwnPtr<WebPresentationError> error = adoptPtr(errorRaw);

    // TODO(avayvod): figure out the mapping between WebPresentationError and
    // DOMException error codes.
    return DOMException::create(InvalidAccessError, error->message);
}

// static
void PresentationError::dispose(WebPresentationError* error)
{
    delete error;
}

} // namespace blink
