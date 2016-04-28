// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentOrShadowRoot_h
#define DocumentOrShadowRoot_h

#include "core/dom/Document.h"
#include "core/dom/shadow/ShadowRoot.h"

namespace blink {

class DocumentOrShadowRoot {
public:
    static Element* activeElement(Document& document)
    {
        return document.activeElement();
    }

    static Element* activeElement(ShadowRoot& shadowRoot)
    {
        return shadowRoot.activeElement();
    }

    static StyleSheetList* styleSheets(Document& document)
    {
        return &document.styleSheets();
    }

    static StyleSheetList* styleSheets(ShadowRoot& shadowRoot)
    {
        return &shadowRoot.styleSheets();
    }

    static DOMSelection* getSelection(TreeScope& treeScope)
    {
        return treeScope.getSelection();
    }

    static Element* elementFromPoint(TreeScope& treeScope, int x, int y)
    {
        return treeScope.elementFromPoint(x, y);
    }

    static HeapVector<Member<Element>> elementsFromPoint(TreeScope& treeScope, int x, int y)
    {
        return treeScope.elementsFromPoint(x, y);
    }
};

} // namespace blink

#endif // DocumentOrShadowRoot_h
