// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElement.h"

#include "platform/text/Character.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

bool CustomElement::isValidName(const AtomicString& name)
{
    if (!name.length() || name[0] < 'a' || name[0] > 'z')
        return false;

    bool hasHyphens = false;
    for (size_t i = 1; i < name.length(); ) {
        UChar32 ch;
        if (name.is8Bit())
            ch = name[i++];
        else
            U16_NEXT(name.characters16(), i, name.length(), ch);
        if (ch == '-')
            hasHyphens = true;
        else if (!Character::isPotentialCustomElementNameChar(ch))
            return false;
    }
    if (!hasHyphens)
        return false;

    // https://html.spec.whatwg.org/multipage/scripting.html#valid-custom-element-name
    DEFINE_STATIC_LOCAL(HashSet<AtomicString>, hyphenContainingElementNames, ());
    if (hyphenContainingElementNames.isEmpty()) {
        hyphenContainingElementNames.add("annotation-xml");
        hyphenContainingElementNames.add("color-profile");
        hyphenContainingElementNames.add("font-face");
        hyphenContainingElementNames.add("font-face-src");
        hyphenContainingElementNames.add("font-face-uri");
        hyphenContainingElementNames.add("font-face-format");
        hyphenContainingElementNames.add("font-face-name");
        hyphenContainingElementNames.add("missing-glyph");
    }

    return !hyphenContainingElementNames.contains(name);
}

} // namespace blink
