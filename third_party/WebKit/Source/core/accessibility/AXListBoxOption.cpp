/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "config.h"
#include "core/accessibility/AXListBoxOption.h"

#include "core/accessibility/AXObjectCache.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/rendering/RenderListBox.h"


namespace blink {

using namespace HTMLNames;

AXListBoxOption::AXListBoxOption(RenderObject* renderer)
    : AXRenderObject(renderer)
{
}

AXListBoxOption::~AXListBoxOption()
{
}

PassRefPtr<AXListBoxOption> AXListBoxOption::create(RenderObject* renderer)
{
    return adoptRef(new AXListBoxOption(renderer));
}

bool AXListBoxOption::isEnabled() const
{
    if (!node())
        return false;

    if (equalIgnoringCase(getAttribute(aria_disabledAttr), "true"))
        return false;

    if (toElement(node())->hasAttribute(disabledAttr))
        return false;

    return true;
}

bool AXListBoxOption::isSelected() const
{
    return isHTMLOptionElement(node()) && toHTMLOptionElement(node())->selected();
}

bool AXListBoxOption::isSelectedOptionActive() const
{
    HTMLSelectElement* listBoxParentNode = listBoxOptionParentNode();
    if (!listBoxParentNode)
        return false;

    return listBoxParentNode->activeSelectionEndListIndex() == listBoxOptionIndex();
}

bool AXListBoxOption::computeAccessibilityIsIgnored() const
{
    if (!node())
        return true;

    if (accessibilityIsIgnoredByDefault())
        return true;

    return false;
}

bool AXListBoxOption::canSetSelectedAttribute() const
{
    if (!isHTMLOptionElement(node()))
        return false;

    if (toHTMLOptionElement(node())->isDisabledFormControl())
        return false;

    HTMLSelectElement* selectElement = listBoxOptionParentNode();
    if (selectElement && selectElement->isDisabledFormControl())
        return false;

    return true;
}

String AXListBoxOption::stringValue() const
{
    if (!node())
        return String();

    const AtomicString& ariaLabel = getAttribute(aria_labelAttr);
    if (!ariaLabel.isNull())
        return ariaLabel;

    if (isHTMLOptionElement(node()))
        return toHTMLOptionElement(node())->text();

    return String();
}

void AXListBoxOption::setSelected(bool selected)
{
    HTMLSelectElement* selectElement = listBoxOptionParentNode();
    if (!selectElement)
        return;

    if (!canSetSelectedAttribute())
        return;

    bool isOptionSelected = isSelected();
    if ((isOptionSelected && selected) || (!isOptionSelected && !selected))
        return;

    // Convert from the entire list index to the option index.
    int optionIndex = selectElement->listToOptionIndex(listBoxOptionIndex());
    selectElement->accessKeySetSelectedIndex(optionIndex);
}

HTMLSelectElement* AXListBoxOption::listBoxOptionParentNode() const
{
    if (!node())
        return 0;

    if (isHTMLOptionElement(node()))
        return toHTMLOptionElement(node())->ownerSelectElement();

    return 0;
}

int AXListBoxOption::listBoxOptionIndex() const
{
    HTMLSelectElement* selectElement = listBoxOptionParentNode();
    if (!selectElement)
        return -1;

    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = selectElement->listItems();
    unsigned length = listItems.size();
    for (unsigned i = 0; i < length; i++) {
        if (listItems[i] == node())
            return i;
    }

    return -1;
}

} // namespace blink
