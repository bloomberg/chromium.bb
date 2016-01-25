// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLIFrameElementSandbox_h
#define HTMLIFrameElementSandbox_h

#include "core/dom/DOMSettableTokenList.h"

namespace blink {

class HTMLIFrameElementSandbox final : public DOMSettableTokenList {
public:
    static PassRefPtrWillBeRawPtr<HTMLIFrameElementSandbox> create(DOMSettableTokenListObserver* observer = nullptr)
    {
        return adoptRefWillBeNoop(new HTMLIFrameElementSandbox(observer));
    }

    ~HTMLIFrameElementSandbox() override;

private:
    explicit HTMLIFrameElementSandbox(DOMSettableTokenListObserver*);
    bool validateTokenValue(const AtomicString&, ExceptionState&) const override;
};

} // namespace blink

#endif
