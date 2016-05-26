// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElement_h
#define CustomElement_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class Document;
class HTMLElement;
class QualifiedName;

class CORE_EXPORT CustomElement {
    STATIC_ONLY(CustomElement);
public:
    static bool isValidName(const AtomicString& name);

    static bool shouldCreateCustomElement(Document&, const AtomicString& localName);
    static bool shouldCreateCustomElement(Document&, const QualifiedName&);

    static HTMLElement* createCustomElement(Document&, const AtomicString& localName);
    static HTMLElement* createCustomElement(Document&, const QualifiedName&);
};

} // namespace blink

#endif // CustomElement_h
