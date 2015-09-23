/*
 * Copyright (C) 2006 Apple Computer, Inc.
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
#include "core/events/UIEventWithKeyState.h"

namespace blink {

bool UIEventWithKeyState::s_newTabModifierSetFromIsolatedWorld = false;

void UIEventWithKeyState::didCreateEventInIsolatedWorld(bool ctrlKey, bool shiftKey, bool altKey, bool metaKey)
{
#if OS(MACOSX)
    const bool newTabModifierSet = metaKey;
#else
    const bool newTabModifierSet = ctrlKey;
#endif
    s_newTabModifierSetFromIsolatedWorld |= newTabModifierSet;
}

bool UIEventWithKeyState::getModifierState(const String& keyIdentifier) const
{
    // FIXME: The following keyIdentifiers are not supported yet (crbug.com/265458):
    // "AltGraph", "CapsLock", "Fn", "NumLock", "ScrollLock", "SymbolLock", "OS".
    if (keyIdentifier == "Control")
        return ctrlKey();
    if (keyIdentifier == "Shift")
        return shiftKey();
    if (keyIdentifier == "Alt")
        return altKey();
    if (keyIdentifier == "Meta")
        return metaKey();
    return false;
}

UIEventWithKeyState* findEventWithKeyState(Event* event)
{
    for (Event* e = event; e; e = e->underlyingEvent())
        if (e->isKeyboardEvent() || e->isMouseEvent())
            return static_cast<UIEventWithKeyState*>(e);
    return nullptr;
}

} // namespace blink
