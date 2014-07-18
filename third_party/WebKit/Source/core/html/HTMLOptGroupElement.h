/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLOptGroupElement_h
#define HTMLOptGroupElement_h

#include "core/html/HTMLElement.h"

namespace blink {

class HTMLSelectElement;
class HTMLDivElement;

class HTMLOptGroupElement FINAL : public HTMLElement {
public:
    static PassRefPtrWillBeRawPtr<HTMLOptGroupElement> create(Document&);

    virtual bool isDisabledFormControl() const OVERRIDE;
    HTMLSelectElement* ownerSelectElement() const;

    String groupLabelText() const;

private:
    explicit HTMLOptGroupElement(Document&);

    virtual bool rendererIsFocusable() const OVERRIDE { return true; }
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void childrenChanged(const ChildrenChange&) OVERRIDE;
    virtual void accessKeyAction(bool sendMouseEvents) OVERRIDE;
    virtual void didAddUserAgentShadowRoot(ShadowRoot&) OVERRIDE;

    void updateGroupLabel();
    void recalcSelectOptions();
    HTMLDivElement& optGroupLabelElement() const;
};

} //namespace

#endif
