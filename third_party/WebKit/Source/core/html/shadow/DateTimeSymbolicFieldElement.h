/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef DateTimeSymbolicFieldElement_h
#define DateTimeSymbolicFieldElement_h

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "core/html/forms/TypeAhead.h"
#include "core/html/shadow/DateTimeFieldElement.h"

namespace blink {

// DateTimeSymbolicFieldElement represents non-numeric field of data time
// format, such as: AM/PM, and month.
class DateTimeSymbolicFieldElement : public DateTimeFieldElement, public TypeAheadDataSource {
    WTF_MAKE_NONCOPYABLE(DateTimeSymbolicFieldElement);

protected:
    DateTimeSymbolicFieldElement(Document&, FieldOwner&, const Vector<String>&, int minimum, int maximum);
    size_t symbolsSize() const { return m_symbols.size(); }
    virtual bool hasValue() const override final;
    void initialize(const AtomicString& pseudo, const String& axHelpText);
    virtual void setEmptyValue(EventBehavior = DispatchNoEvent) override final;
    virtual void setValueAsInteger(int, EventBehavior = DispatchNoEvent) override final;
    virtual int valueAsInteger() const override final;

private:
    static const int invalidIndex = -1;

    String visibleEmptyValue() const;
    bool indexIsInRange(int index) const { return index >= m_minimumIndex && index <= m_maximumIndex; }

    // DateTimeFieldElement functions.
    virtual void handleKeyboardEvent(KeyboardEvent*) override final;
    virtual float maximumWidth(const Font&) override;
    virtual void stepDown() override final;
    virtual void stepUp() override final;
    virtual String value() const override final;
    virtual int valueForARIAValueNow() const override final;
    virtual String visibleValue() const override final;

    // TypeAheadDataSource functions.
    virtual int indexOfSelectedOption() const override;
    virtual int optionCount() const override;
    virtual String optionAtIndex(int index) const override;

    const Vector<String> m_symbols;

    // We use AtomicString to share visible empty value among multiple
    // DateTimeEditElements in the page.
    const AtomicString m_visibleEmptyValue;
    int m_selectedIndex;
    TypeAhead m_typeAhead;
    const int m_minimumIndex;
    const int m_maximumIndex;
};

} // namespace blink

#endif
#endif
