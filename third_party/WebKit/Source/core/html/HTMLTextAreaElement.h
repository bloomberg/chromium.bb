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

#ifndef HTMLTextAreaElement_h
#define HTMLTextAreaElement_h

#include "core/html/HTMLTextFormControlElement.h"

namespace blink {

class BeforeTextInsertedEvent;
class ExceptionState;

class HTMLTextAreaElement final : public HTMLTextFormControlElement {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<HTMLTextAreaElement> create(Document&, HTMLFormElement*);

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }

    bool shouldWrapText() const { return m_wrap != NoWrap; }

    virtual String value() const override;
    void setValue(const String&, TextFieldEventBehavior = DispatchNoEvent);
    String defaultValue() const;
    void setDefaultValue(const String&);
    int textLength() const { return value().length(); }
    int maxLength() const;
    void setMaxLength(int, ExceptionState&);

    String suggestedValue() const;
    void setSuggestedValue(const String&);

    // For ValidityState
    virtual String validationMessage() const override;
    virtual bool valueMissing() const override;
    virtual bool tooLong() const override;
    bool isValidValue(const String&) const;

    void setCols(int);
    void setRows(int);

private:
    HTMLTextAreaElement(Document&, HTMLFormElement*);

    enum WrapMethod { NoWrap, SoftWrap, HardWrap };

    virtual void didAddUserAgentShadowRoot(ShadowRoot&) override;
    // FIXME: Author shadows should be allowed
    // https://bugs.webkit.org/show_bug.cgi?id=92608
    virtual bool areAuthorShadowsAllowed() const override { return false; }

    void handleBeforeTextInsertedEvent(BeforeTextInsertedEvent*) const;
    static String sanitizeUserInputValue(const String&, unsigned maxLength);
    void updateValue() const;
    virtual void setInnerEditorValue(const String&) override;
    void setNonDirtyValue(const String&);
    void setValueCommon(const String&, TextFieldEventBehavior, SelectionOption = NotChangeSelection);

    virtual bool supportsPlaceholder() const override { return true; }
    virtual void updatePlaceholderText() override;
    virtual bool isEmptyValue() const override { return value().isEmpty(); }
    virtual bool isEmptySuggestedValue() const override final { return suggestedValue().isEmpty(); }

    virtual bool isOptionalFormControl() const override { return !isRequiredFormControl(); }
    virtual bool isRequiredFormControl() const override { return isRequired(); }

    virtual void defaultEventHandler(Event*) override;
    virtual void handleFocusEvent(Element* oldFocusedNode, FocusType) override;

    virtual void subtreeHasChanged() override;

    virtual bool isEnumeratable() const override { return true; }
    virtual bool isInteractiveContent() const override;
    virtual bool supportsAutofocus() const override;
    virtual bool supportLabels() const override { return true; }

    virtual const AtomicString& formControlType() const override;

    virtual FormControlState saveFormControlState() const override;
    virtual void restoreFormControlState(const FormControlState&) override;

    virtual bool isTextFormControl() const override { return true; }

    virtual void childrenChanged(const ChildrenChange&) override;
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual bool isPresentationAttribute(const QualifiedName&) const override;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) override;
    virtual RenderObject* createRenderer(RenderStyle*) override;
    virtual bool appendFormData(FormDataList&, bool) override;
    virtual void resetImpl() override;
    virtual bool hasCustomFocusLogic() const override;
    virtual bool shouldShowFocusRingOnMouseFocus() const override;
    virtual bool isKeyboardFocusable() const override;
    virtual void updateFocusAppearance(bool restorePreviousSelection) override;

    virtual void accessKeyAction(bool sendMouseEvents) override;

    virtual bool matchesReadOnlyPseudoClass() const override;
    virtual bool matchesReadWritePseudoClass() const override;

    // If the String* argument is 0, apply this->value().
    bool valueMissing(const String*) const;
    bool tooLong(const String*, NeedsToCheckDirtyFlag) const;

    int m_rows;
    int m_cols;
    WrapMethod m_wrap;
    mutable String m_value;
    mutable bool m_isDirty;
    bool m_valueIsUpToDate;
    String m_suggestedValue;
};

} // namespace blink

#endif // HTMLTextAreaElement_h
