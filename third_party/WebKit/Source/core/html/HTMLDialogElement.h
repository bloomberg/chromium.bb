/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLDialogElement_h
#define HTMLDialogElement_h

#include "core/html/HTMLElement.h"

namespace WebCore {

class Document;
class ExceptionState;
class QualifiedName;

class HTMLDialogElement FINAL : public HTMLElement {
public:
    static PassRefPtr<HTMLDialogElement> create(const QualifiedName&, Document&);

    void close(const String& returnValue, ExceptionState&);
    void show();
    void showModal(ExceptionState&);

    String returnValue() const { return m_returnValue; }
    void setReturnValue(const String& returnValue) { m_returnValue = returnValue; }

private:
    HTMLDialogElement(const QualifiedName&, Document&);

    virtual PassRefPtr<RenderStyle> customStyleForRenderer() OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void defaultEventHandler(Event*) OVERRIDE;
    virtual bool shouldBeReparentedUnderRenderView(const RenderStyle*) const OVERRIDE;

    void closeDialog(const String& returnValue = String());
    void reposition();

    bool m_topIsValid;
    LayoutUnit m_top;
    String m_returnValue;
};

inline HTMLDialogElement* toHTMLDialogElement(Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->hasTagName(HTMLNames::dialogTag));
    return static_cast<HTMLDialogElement*>(node);
}

} // namespace WebCore

#endif
