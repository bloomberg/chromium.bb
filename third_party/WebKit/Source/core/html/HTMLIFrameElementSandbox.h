// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLIFrameElementSandbox_h
#define HTMLIFrameElementSandbox_h

#include "core/dom/DOMTokenList.h"

namespace blink {

class HTMLIFrameElementSandbox final : public DOMTokenList {
public:
    static PassRefPtrWillBeRawPtr<HTMLIFrameElementSandbox> create(DOMTokenListObserver* observer = nullptr)
    {
        return adoptRefWillBeNoop(new HTMLIFrameElementSandbox(observer));
    }

    ~HTMLIFrameElementSandbox() override;

private:
    explicit HTMLIFrameElementSandbox(DOMTokenListObserver*);
    bool validateTokenValue(const AtomicString&, ExceptionState&) const override;
};

} // namespace blink

#endif
