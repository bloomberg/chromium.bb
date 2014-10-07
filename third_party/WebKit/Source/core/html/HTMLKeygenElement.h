/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLKeygenElement_h
#define HTMLKeygenElement_h

#include "core/html/HTMLFormControlElementWithState.h"

namespace blink {

class HTMLSelectElement;

class HTMLKeygenElement final : public HTMLFormControlElementWithState {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<HTMLKeygenElement> create(Document&, HTMLFormElement*);

    virtual bool willValidate() const override { return false; }

private:
    HTMLKeygenElement(Document&, HTMLFormElement*);

    virtual bool areAuthorShadowsAllowed() const override { return false; }

    virtual bool canStartSelection() const override { return false; }

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;

    virtual bool appendFormData(FormDataList&, bool) override;
    virtual const AtomicString& formControlType() const override;
    virtual bool isOptionalFormControl() const override { return false; }

    virtual bool isEnumeratable() const override { return true; }
    virtual bool isInteractiveContent() const override;
    virtual bool supportsAutofocus() const override;
    virtual bool supportLabels() const override { return true; }

    virtual void resetImpl() override;
    virtual bool shouldSaveAndRestoreFormControlState() const override { return false; }

    virtual void didAddUserAgentShadowRoot(ShadowRoot&) override;

    HTMLSelectElement* shadowSelect() const;
};

} // namespace blink

#endif // HTMLKeygenElement_h
