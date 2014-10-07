// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PluginPlaceholderElement_h
#define PluginPlaceholderElement_h

#include "core/dom/Element.h"
#include "core/html/HTMLDivElement.h"

namespace blink {

class PluginPlaceholderElement final : public HTMLDivElement {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<PluginPlaceholderElement> create(Document&);

    void setMessage(const String&);

private:
    explicit PluginPlaceholderElement(Document&);
};

} // namespace blink

#endif // PluginPlaceholderElement_h
