// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationError_h
#define PresentationError_h

#include "platform/heap/Handle.h"
#include "public/platform/modules/presentation/WebPresentationError.h"

namespace blink {

class DOMException;
class ScriptPromiseResolver;

// A container of methods taking care of WebPresentationError in WebCallbacks subclasses.
class PresentationError final {
    WTF_MAKE_NONCOPYABLE(PresentationError);
public:
    // For CallbackPromiseAdapter.
    using WebType = const WebPresentationError&;

    static DOMException* take(ScriptPromiseResolver*, const WebPresentationError&);

private:
    PresentationError() = delete;
    ~PresentationError() = delete;
};

} // namespace blink

#endif // PresentationError_h
