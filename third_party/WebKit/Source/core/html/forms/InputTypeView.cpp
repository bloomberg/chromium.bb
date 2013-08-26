/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2009, 2010, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#include "config.h"
#include "core/html/forms/InputTypeView.h"

#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/rendering/RenderObject.h"

namespace WebCore {

InputTypeUI::~InputTypeUI()
{
}

bool InputTypeUI::sizeShouldIncludeDecoration(int, int& preferredSize) const
{
    preferredSize = element()->size();
    return false;
}

void InputTypeUI::handleClickEvent(MouseEvent*)
{
}

void InputTypeUI::handleMouseDownEvent(MouseEvent*)
{
}

void InputTypeUI::handleDOMActivateEvent(Event*)
{
}

void InputTypeUI::handleKeydownEvent(KeyboardEvent*)
{
}

void InputTypeUI::handleKeypressEvent(KeyboardEvent*)
{
}

void InputTypeUI::handleKeyupEvent(KeyboardEvent*)
{
}

void InputTypeUI::handleBeforeTextInsertedEvent(BeforeTextInsertedEvent*)
{
}

void InputTypeUI::handleTouchEvent(TouchEvent*)
{
}

void InputTypeUI::forwardEvent(Event*)
{
}

bool InputTypeUI::shouldSubmitImplicitly(Event* event)
{
    return false;
}

PassRefPtr<HTMLFormElement> InputTypeUI::formForSubmission() const
{
    return element()->form();
}

RenderObject* InputTypeUI::createRenderer(RenderStyle* style) const
{
    return RenderObject::createObject(element(), style);
}

PassRefPtr<RenderStyle> InputTypeUI::customStyleForRenderer(PassRefPtr<RenderStyle> originalStyle)
{
    return originalStyle;
}

void InputTypeUI::blur()
{
    element()->defaultBlur();
}

bool InputTypeUI::hasCustomFocusLogic() const
{
    return false;
}

void InputTypeUI::handleFocusEvent(Element*, FocusDirection)
{
}

void InputTypeUI::handleBlurEvent()
{
}

void InputTypeUI::attach()
{
}

void InputTypeUI::altAttributeChanged()
{
}

void InputTypeUI::srcAttributeChanged()
{
}

void InputTypeUI::minOrMaxAttributeChanged()
{
}

void InputTypeUI::stepAttributeChanged()
{
}

PassOwnPtr<ClickHandlingState> InputTypeUI::willDispatchClick()
{
    return nullptr;
}

void InputTypeUI::didDispatchClick(Event*, const ClickHandlingState&)
{
}

void InputTypeUI::updateInnerTextValue()
{
}

void InputTypeUI::attributeChanged()
{
}

void InputTypeUI::multipleAttributeChanged()
{
}

void InputTypeUI::disabledAttributeChanged()
{
}

void InputTypeUI::readonlyAttributeChanged()
{
}

void InputTypeUI::requiredAttributeChanged()
{
}

void InputTypeUI::valueAttributeChanged()
{
}

void InputTypeUI::subtreeHasChanged()
{
    ASSERT_NOT_REACHED();
}

bool InputTypeUI::hasTouchEventHandler() const
{
    return false;
}

void InputTypeUI::listAttributeTargetChanged()
{
}

void InputTypeUI::updateClearButtonVisibility()
{
}

} // namespace WebCore
