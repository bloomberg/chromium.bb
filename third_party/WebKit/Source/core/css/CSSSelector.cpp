/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 *               2001 Andreas Schlapbach (schlpbch@iam.unibe.ch)
 *               2001-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 */

#include "config.h"
#include "core/css/CSSSelector.h"

#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "core/css/CSSOMUtils.h"
#include "core/css/CSSSelectorList.h"
#include "wtf/Assertions.h"
#include "wtf/HashMap.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringHash.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace WebCore {

using namespace HTMLNames;

struct SameSizeAsCSSSelector {
    unsigned bitfields;
    void *pointers[1];
};

COMPILE_ASSERT(sizeof(CSSSelector) == sizeof(SameSizeAsCSSSelector), CSSSelectorShouldStaySmall);

void CSSSelector::createRareData()
{
    ASSERT(m_match != Tag);
    if (m_hasRareData)
        return;
    AtomicString value(m_data.m_value);
    if (m_data.m_value)
        m_data.m_value->deref();
    m_data.m_rareData = RareData::create(value).leakRef();
    m_hasRareData = true;
}

unsigned CSSSelector::specificity() const
{
    // make sure the result doesn't overflow
    static const unsigned maxValueMask = 0xffffff;
    static const unsigned idMask = 0xff0000;
    static const unsigned classMask = 0xff00;
    static const unsigned elementMask = 0xff;

    if (isForPage())
        return specificityForPage() & maxValueMask;

    unsigned total = 0;
    unsigned temp = 0;

    for (const CSSSelector* selector = this; selector; selector = selector->tagHistory()) {
        temp = total + selector->specificityForOneSelector();
        // Clamp each component to its max in the case of overflow.
        if ((temp & idMask) < (total & idMask))
            total |= idMask;
        else if ((temp & classMask) < (total & classMask))
            total |= classMask;
        else if ((temp & elementMask) < (total & elementMask))
            total |= elementMask;
        else
            total = temp;
    }
    return total;
}

inline unsigned CSSSelector::specificityForOneSelector() const
{
    // FIXME: Pseudo-elements and pseudo-classes do not have the same specificity. This function
    // isn't quite correct.
    switch (m_match) {
    case Id:
        return 0x10000;
    case PseudoClass:
        if (pseudoType() == PseudoHost || pseudoType() == PseudoAncestor)
            return 0;
        // fall through.
    case Exact:
    case Class:
    case Set:
    case List:
    case Hyphen:
    case PseudoElement:
    case Contain:
    case Begin:
    case End:
        // FIXME: PseudoAny should base the specificity on the sub-selectors.
        // See http://lists.w3.org/Archives/Public/www-style/2010Sep/0530.html
        if (pseudoType() == PseudoNot) {
            ASSERT(selectorList());
            return selectorList()->first()->specificityForOneSelector();
        }
        return 0x100;
    case Tag:
        return (tagQName().localName() != starAtom) ? 1 : 0;
    case Unknown:
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

unsigned CSSSelector::specificityForPage() const
{
    // See http://dev.w3.org/csswg/css3-page/#cascading-and-page-context
    unsigned s = 0;

    for (const CSSSelector* component = this; component; component = component->tagHistory()) {
        switch (component->m_match) {
        case Tag:
            s += tagQName().localName() == starAtom ? 0 : 4;
            break;
        case PagePseudoClass:
            switch (component->pseudoType()) {
            case PseudoFirstPage:
                s += 2;
                break;
            case PseudoLeftPage:
            case PseudoRightPage:
                s += 1;
                break;
            case PseudoNotParsed:
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        default:
            break;
        }
    }
    return s;
}

PseudoId CSSSelector::pseudoId(PseudoType type)
{
    switch (type) {
    case PseudoFirstLine:
        return FIRST_LINE;
    case PseudoFirstLetter:
        return FIRST_LETTER;
    case PseudoSelection:
        return SELECTION;
    case PseudoBefore:
        return BEFORE;
    case PseudoAfter:
        return AFTER;
    case PseudoBackdrop:
        return BACKDROP;
    case PseudoScrollbar:
        return SCROLLBAR;
    case PseudoScrollbarButton:
        return SCROLLBAR_BUTTON;
    case PseudoScrollbarCorner:
        return SCROLLBAR_CORNER;
    case PseudoScrollbarThumb:
        return SCROLLBAR_THUMB;
    case PseudoScrollbarTrack:
        return SCROLLBAR_TRACK;
    case PseudoScrollbarTrackPiece:
        return SCROLLBAR_TRACK_PIECE;
    case PseudoResizer:
        return RESIZER;
    case PseudoUnknown:
    case PseudoEmpty:
    case PseudoFirstChild:
    case PseudoFirstOfType:
    case PseudoLastChild:
    case PseudoLastOfType:
    case PseudoOnlyChild:
    case PseudoOnlyOfType:
    case PseudoNthChild:
    case PseudoNthOfType:
    case PseudoNthLastChild:
    case PseudoNthLastOfType:
    case PseudoLink:
    case PseudoVisited:
    case PseudoAny:
    case PseudoAnyLink:
    case PseudoAutofill:
    case PseudoHover:
    case PseudoDrag:
    case PseudoFocus:
    case PseudoActive:
    case PseudoChecked:
    case PseudoEnabled:
    case PseudoFullPageMedia:
    case PseudoDefault:
    case PseudoDisabled:
    case PseudoOptional:
    case PseudoRequired:
    case PseudoReadOnly:
    case PseudoReadWrite:
    case PseudoValid:
    case PseudoInvalid:
    case PseudoIndeterminate:
    case PseudoTarget:
    case PseudoLang:
    case PseudoNot:
    case PseudoRoot:
    case PseudoScope:
    case PseudoScrollbarBack:
    case PseudoScrollbarForward:
    case PseudoWindowInactive:
    case PseudoCornerPresent:
    case PseudoDecrement:
    case PseudoIncrement:
    case PseudoHorizontal:
    case PseudoVertical:
    case PseudoStart:
    case PseudoEnd:
    case PseudoDoubleButton:
    case PseudoSingleButton:
    case PseudoNoButton:
    case PseudoFirstPage:
    case PseudoLeftPage:
    case PseudoRightPage:
    case PseudoInRange:
    case PseudoOutOfRange:
    case PseudoUserAgentCustomElement:
    case PseudoWebKitCustomElement:
    case PseudoCue:
    case PseudoFutureCue:
    case PseudoPastCue:
    case PseudoDistributed:
    case PseudoUnresolved:
    case PseudoHost:
    case PseudoAncestor:
    case PseudoFullScreen:
    case PseudoFullScreenDocument:
    case PseudoFullScreenAncestor:
        return NOPSEUDO;
    case PseudoNotParsed:
        ASSERT_NOT_REACHED();
        return NOPSEUDO;
    }

    ASSERT_NOT_REACHED();
    return NOPSEUDO;
}

// Could be made smaller and faster by replacing pointer with an
// offset into a string buffer and making the bit fields smaller but
// that could not be maintained by hand.
struct NameToPseudoStruct {
    const char* string;
    unsigned type:8;
    unsigned requirement:8;
};

const static NameToPseudoStruct pseudoTypeMap[] = {
{"active",                        CSSSelector::PseudoActive,              0},
{"after",                         CSSSelector::PseudoAfter,               0},
{"-webkit-any(",                  CSSSelector::PseudoAny,                 0},
{"-webkit-any-link",              CSSSelector::PseudoAnyLink,             0},
{"-webkit-autofill",              CSSSelector::PseudoAutofill,            0},
{"backdrop",                      CSSSelector::PseudoBackdrop,            0},
{"before",                        CSSSelector::PseudoBefore,              0},
{"checked",                       CSSSelector::PseudoChecked,             0},
{"default",                       CSSSelector::PseudoDefault,             0},
{"disabled",                      CSSSelector::PseudoDisabled,            0},
{"read-only",                     CSSSelector::PseudoReadOnly,            0},
{"read-write",                    CSSSelector::PseudoReadWrite,           0},
{"valid",                         CSSSelector::PseudoValid,               0},
{"invalid",                       CSSSelector::PseudoInvalid,             0},
{"-webkit-drag",                  CSSSelector::PseudoDrag,                0},
{"empty",                         CSSSelector::PseudoEmpty,               0},
{"enabled",                       CSSSelector::PseudoEnabled,             0},
{"first-child",                   CSSSelector::PseudoFirstChild,          0},
{"first-letter",                  CSSSelector::PseudoFirstLetter,         0},
{"first-line",                    CSSSelector::PseudoFirstLine,           0},
{"first-of-type",                 CSSSelector::PseudoFirstOfType,         0},
{"-webkit-full-page-media",       CSSSelector::PseudoFullPageMedia,       0},
{"nth-child(",                    CSSSelector::PseudoNthChild,            0},
{"nth-of-type(",                  CSSSelector::PseudoNthOfType,           0},
{"nth-last-child(",               CSSSelector::PseudoNthLastChild,        0},
{"nth-last-of-type(",             CSSSelector::PseudoNthLastOfType,       0},
{"focus",                         CSSSelector::PseudoFocus,               0},
{"hover",                         CSSSelector::PseudoHover,               0},
{"indeterminate",                 CSSSelector::PseudoIndeterminate,       0},
{"last-child",                    CSSSelector::PseudoLastChild,           0},
{"last-of-type",                  CSSSelector::PseudoLastOfType,          0},
{"link",                          CSSSelector::PseudoLink,                0},
{"lang(",                         CSSSelector::PseudoLang,                0},
{"not(",                          CSSSelector::PseudoNot,                 0},
{"only-child",                    CSSSelector::PseudoOnlyChild,           0},
{"only-of-type",                  CSSSelector::PseudoOnlyOfType,          0},
{"optional",                      CSSSelector::PseudoOptional,            0},
{"required",                      CSSSelector::PseudoRequired,            0},
{"-webkit-resizer",               CSSSelector::PseudoResizer,             0},
{"root",                          CSSSelector::PseudoRoot,                0},
{"-webkit-scrollbar",             CSSSelector::PseudoScrollbar,           0},
{"-webkit-scrollbar-button",      CSSSelector::PseudoScrollbarButton,     0},
{"-webkit-scrollbar-corner",      CSSSelector::PseudoScrollbarCorner,     0},
{"-webkit-scrollbar-thumb",       CSSSelector::PseudoScrollbarThumb,      0},
{"-webkit-scrollbar-track",       CSSSelector::PseudoScrollbarTrack,      0},
{"-webkit-scrollbar-track-piece", CSSSelector::PseudoScrollbarTrackPiece, 0},
{"selection",                     CSSSelector::PseudoSelection,           0},
{"target",                        CSSSelector::PseudoTarget,              0},
{"visited",                       CSSSelector::PseudoVisited,             0},
{"window-inactive",               CSSSelector::PseudoWindowInactive,      0},
{"decrement",                     CSSSelector::PseudoDecrement,           0},
{"increment",                     CSSSelector::PseudoIncrement,           0},
{"start",                         CSSSelector::PseudoStart,               0},
{"end",                           CSSSelector::PseudoEnd,                 0},
{"horizontal",                    CSSSelector::PseudoHorizontal,          0},
{"vertical",                      CSSSelector::PseudoVertical,            0},
{"double-button",                 CSSSelector::PseudoDoubleButton,        0},
{"single-button",                 CSSSelector::PseudoSingleButton,        0},
{"no-button",                     CSSSelector::PseudoNoButton,            0},
{"corner-present",                CSSSelector::PseudoCornerPresent,       0},
{"first",                         CSSSelector::PseudoFirstPage,           0},
{"left",                          CSSSelector::PseudoLeftPage,            0},
{"right",                         CSSSelector::PseudoRightPage,           0},
{"-webkit-full-screen",           CSSSelector::PseudoFullScreen,          0},
{"-webkit-full-screen-document",  CSSSelector::PseudoFullScreenDocument,  0},
{"-webkit-full-screen-ancestor",  CSSSelector::PseudoFullScreenAncestor,  0},
{"cue(",                          CSSSelector::PseudoCue,                 0},
{"cue",                           CSSSelector::PseudoWebKitCustomElement, 0},
{"future",                        CSSSelector::PseudoFutureCue,           0},
{"past",                          CSSSelector::PseudoPastCue,             0},
{"-webkit-distributed(",          CSSSelector::PseudoDistributed,         0},
{"in-range",                      CSSSelector::PseudoInRange,             0},
{"out-of-range",                  CSSSelector::PseudoOutOfRange,          0},
{"scope",                         CSSSelector::PseudoScope,               0},
{"unresolved",                    CSSSelector::PseudoUnresolved,          0},
{"host",                          CSSSelector::PseudoHost,                CSSSelector::RequiresShadowDOM},
{"host(",                         CSSSelector::PseudoHost,                CSSSelector::RequiresShadowDOM},
{"ancestor",                      CSSSelector::PseudoAncestor,            CSSSelector::RequiresShadowDOM},
{"ancestor(",                     CSSSelector::PseudoAncestor,            CSSSelector::RequiresShadowDOM},
};

static HashMap<StringImpl*, CSSSelector::PseudoType>* nameToPseudoTypeMap()
{
    static HashMap<StringImpl*, CSSSelector::PseudoType>* nameToPseudoType = 0;
    if (!nameToPseudoType) {
        nameToPseudoType = new HashMap<StringImpl*, CSSSelector::PseudoType>;

        size_t pseudoCount = WTF_ARRAY_LENGTH(pseudoTypeMap);
        for (size_t i = 0; i < pseudoCount; i++) {
            if (pseudoTypeMap[i].requirement == CSSSelector::RequiresShadowDOM) {
                if (!RuntimeEnabledFeatures::shadowDOMEnabled())
                    continue;
            }

            const char* str = pseudoTypeMap[i].string;
            CSSSelector::PseudoType type;
            type = static_cast<CSSSelector::PseudoType>(pseudoTypeMap[i].type);
            // This is a one-time leak.
            AtomicString* name = new AtomicString(str, strlen(str), AtomicString::ConstructFromLiteral);
            nameToPseudoType->set(name->impl(), type);
        }
    }

    return nameToPseudoType;
}

#ifndef NDEBUG
void CSSSelector::show(int indent) const
{
    printf("%*sselectorText(): %s\n", indent, "", selectorText().ascii().data());
    printf("%*sm_match: %d\n", indent, "", m_match);
    printf("%*sisCustomPseudoElement(): %d\n", indent, "", isCustomPseudoElement());
    if (m_match != Tag)
        printf("%*svalue(): %s\n", indent, "", value().ascii().data());
    printf("%*spseudoType(): %d\n", indent, "", pseudoType());
    if (m_match == Tag)
        printf("%*stagQName().localName: %s\n", indent, "", tagQName().localName().ascii().data());
    printf("%*sisAttributeSelector(): %d\n", indent, "", isAttributeSelector());
    if (isAttributeSelector())
        printf("%*sattribute(): %s\n", indent, "", attribute().localName().ascii().data());
    printf("%*sargument(): %s\n", indent, "", argument().ascii().data());
    printf("%*sspecificity(): %u\n", indent, "", specificity());
    if (tagHistory()) {
        printf("\n%*s--> (relation == %d)\n", indent, "", relation());
        tagHistory()->show(indent + 2);
    }
}

void CSSSelector::show() const
{
    printf("\n******* CSSSelector::show(\"%s\") *******\n", selectorText().ascii().data());
    show(2);
    printf("******* end *******\n");
}
#endif

CSSSelector::PseudoType CSSSelector::parsePseudoType(const AtomicString& name)
{
    if (name.isNull())
        return PseudoUnknown;
    HashMap<StringImpl*, CSSSelector::PseudoType>* nameToPseudoType = nameToPseudoTypeMap();
    HashMap<StringImpl*, CSSSelector::PseudoType>::iterator slot = nameToPseudoType->find(name.impl());

    if (slot != nameToPseudoType->end())
        return slot->value;

    if (name.startsWith("-webkit-"))
        return PseudoWebKitCustomElement;
    if (name.startsWith("x-") || name.startsWith("cue"))
        return PseudoUserAgentCustomElement;

    return PseudoUnknown;
}

void CSSSelector::extractPseudoType() const
{
    if (m_match != PseudoClass && m_match != PseudoElement && m_match != PagePseudoClass)
        return;

    m_pseudoType = parsePseudoType(value());

    bool element = false; // pseudo-element
    bool compat = false; // single colon compatbility mode
    bool isPagePseudoClass = false; // Page pseudo-class

    switch (m_pseudoType) {
    case PseudoAfter:
    case PseudoBefore:
    case PseudoCue:
    case PseudoFirstLetter:
    case PseudoFirstLine:
        compat = true;
    case PseudoBackdrop:
    case PseudoDistributed:
    case PseudoResizer:
    case PseudoScrollbar:
    case PseudoScrollbarCorner:
    case PseudoScrollbarButton:
    case PseudoScrollbarThumb:
    case PseudoScrollbarTrack:
    case PseudoScrollbarTrackPiece:
    case PseudoSelection:
    case PseudoUserAgentCustomElement:
    case PseudoWebKitCustomElement:
        element = true;
        break;
    case PseudoUnknown:
    case PseudoEmpty:
    case PseudoFirstChild:
    case PseudoFirstOfType:
    case PseudoLastChild:
    case PseudoLastOfType:
    case PseudoOnlyChild:
    case PseudoOnlyOfType:
    case PseudoNthChild:
    case PseudoNthOfType:
    case PseudoNthLastChild:
    case PseudoNthLastOfType:
    case PseudoLink:
    case PseudoVisited:
    case PseudoAny:
    case PseudoAnyLink:
    case PseudoAutofill:
    case PseudoHover:
    case PseudoDrag:
    case PseudoFocus:
    case PseudoActive:
    case PseudoChecked:
    case PseudoEnabled:
    case PseudoFullPageMedia:
    case PseudoDefault:
    case PseudoDisabled:
    case PseudoOptional:
    case PseudoRequired:
    case PseudoReadOnly:
    case PseudoReadWrite:
    case PseudoScope:
    case PseudoValid:
    case PseudoInvalid:
    case PseudoIndeterminate:
    case PseudoTarget:
    case PseudoLang:
    case PseudoNot:
    case PseudoRoot:
    case PseudoScrollbarBack:
    case PseudoScrollbarForward:
    case PseudoWindowInactive:
    case PseudoCornerPresent:
    case PseudoDecrement:
    case PseudoIncrement:
    case PseudoHorizontal:
    case PseudoVertical:
    case PseudoStart:
    case PseudoEnd:
    case PseudoDoubleButton:
    case PseudoSingleButton:
    case PseudoNoButton:
    case PseudoNotParsed:
    case PseudoFullScreen:
    case PseudoFullScreenDocument:
    case PseudoFullScreenAncestor:
    case PseudoInRange:
    case PseudoOutOfRange:
    case PseudoFutureCue:
    case PseudoPastCue:
    case PseudoHost:
    case PseudoAncestor:
    case PseudoUnresolved:
        break;
    case PseudoFirstPage:
    case PseudoLeftPage:
    case PseudoRightPage:
        isPagePseudoClass = true;
        break;
    }

    bool matchPagePseudoClass = (m_match == PagePseudoClass);
    if (matchPagePseudoClass != isPagePseudoClass)
        m_pseudoType = PseudoUnknown;
    else if (m_match == PseudoClass && element) {
        if (!compat)
            m_pseudoType = PseudoUnknown;
        else
            m_match = PseudoElement;
    } else if (m_match == PseudoElement && !element)
        m_pseudoType = PseudoUnknown;
}

bool CSSSelector::operator==(const CSSSelector& other) const
{
    const CSSSelector* sel1 = this;
    const CSSSelector* sel2 = &other;

    while (sel1 && sel2) {
        if (sel1->attribute() != sel2->attribute()
            || sel1->relation() != sel2->relation()
            || sel1->m_match != sel2->m_match
            || sel1->value() != sel2->value()
            || sel1->pseudoType() != sel2->pseudoType()
            || sel1->argument() != sel2->argument()) {
            return false;
        }
        if (sel1->m_match == Tag) {
            if (sel1->tagQName() != sel2->tagQName())
                return false;
        }
        sel1 = sel1->tagHistory();
        sel2 = sel2->tagHistory();
    }

    if (sel1 || sel2)
        return false;

    return true;
}

String CSSSelector::selectorText(const String& rightSide) const
{
    StringBuilder str;

    if (m_match == CSSSelector::Tag && !m_tagIsForNamespaceRule) {
        if (tagQName().prefix().isNull())
            str.append(tagQName().localName());
        else {
            str.append(tagQName().prefix().string());
            str.append('|');
            str.append(tagQName().localName());
        }
    }

    const CSSSelector* cs = this;
    while (true) {
        if (cs->m_match == CSSSelector::Id) {
            str.append('#');
            serializeIdentifier(cs->value(), str);
        } else if (cs->m_match == CSSSelector::Class) {
            str.append('.');
            serializeIdentifier(cs->value(), str);
        } else if (cs->m_match == CSSSelector::PseudoClass || cs->m_match == CSSSelector::PagePseudoClass) {
            str.append(':');
            str.append(cs->value());

            switch (cs->pseudoType()) {
            case PseudoNot:
                ASSERT(cs->selectorList());
                str.append(cs->selectorList()->first()->selectorText());
                str.append(')');
                break;
            case PseudoLang:
            case PseudoNthChild:
            case PseudoNthLastChild:
            case PseudoNthOfType:
            case PseudoNthLastOfType:
                str.append(cs->argument());
                str.append(')');
                break;
            case PseudoAny: {
                const CSSSelector* firstSubSelector = cs->selectorList()->first();
                for (const CSSSelector* subSelector = firstSubSelector; subSelector; subSelector = CSSSelectorList::next(*subSelector)) {
                    if (subSelector != firstSubSelector)
                        str.append(',');
                    str.append(subSelector->selectorText());
                }
                str.append(')');
                break;
            }
            case PseudoHost:
            case PseudoAncestor: {
                if (cs->selectorList()) {
                    const CSSSelector* firstSubSelector = cs->selectorList()->first();
                    for (const CSSSelector* subSelector = firstSubSelector; subSelector; subSelector = CSSSelectorList::next(*subSelector)) {
                        if (subSelector != firstSubSelector)
                            str.append(',');
                        str.append(subSelector->selectorText());
                    }
                    str.append(')');
                }
                break;
            }
            default:
                break;
            }
        } else if (cs->m_match == CSSSelector::PseudoElement) {
            str.appendLiteral("::");
            str.append(cs->value());
        } else if (cs->isAttributeSelector()) {
            str.append('[');
            const AtomicString& prefix = cs->attribute().prefix();
            if (!prefix.isNull()) {
                str.append(prefix);
                str.append("|");
            }
            str.append(cs->attribute().localName());
            switch (cs->m_match) {
                case CSSSelector::Exact:
                    str.append('=');
                    break;
                case CSSSelector::Set:
                    // set has no operator or value, just the attrName
                    str.append(']');
                    break;
                case CSSSelector::List:
                    str.appendLiteral("~=");
                    break;
                case CSSSelector::Hyphen:
                    str.appendLiteral("|=");
                    break;
                case CSSSelector::Begin:
                    str.appendLiteral("^=");
                    break;
                case CSSSelector::End:
                    str.appendLiteral("$=");
                    break;
                case CSSSelector::Contain:
                    str.appendLiteral("*=");
                    break;
                default:
                    break;
            }
            if (cs->m_match != CSSSelector::Set) {
                serializeString(cs->value(), str);
                str.append(']');
            }
        }
        if (cs->relation() != CSSSelector::SubSelector || !cs->tagHistory())
            break;
        cs = cs->tagHistory();
    }

    if (const CSSSelector* tagHistory = cs->tagHistory()) {
        switch (cs->relation()) {
        case CSSSelector::Descendant:
            if (cs->relationIsAffectedByPseudoContent())
                return tagHistory->selectorText("::-webkit-distributed(" + str.toString() + rightSide + ")");
            return tagHistory->selectorText(" " + str.toString() + rightSide);
        case CSSSelector::Child:
            if (cs->relationIsAffectedByPseudoContent())
                return tagHistory->selectorText("::-webkit-distributed(> " + str.toString() + rightSide + ")");
            return tagHistory->selectorText(" > " + str.toString() + rightSide);
        case CSSSelector::Shadow:
            return tagHistory->selectorText(" /shadow/ " + str.toString() + rightSide);
        case CSSSelector::ShadowDeep:
            return tagHistory->selectorText(" /shadow-deep/ " + str.toString() + rightSide);
        case CSSSelector::DirectAdjacent:
            return tagHistory->selectorText(" + " + str.toString() + rightSide);
        case CSSSelector::IndirectAdjacent:
            return tagHistory->selectorText(" ~ " + str.toString() + rightSide);
        case CSSSelector::SubSelector:
            ASSERT_NOT_REACHED();
        case CSSSelector::ShadowPseudo:
            return tagHistory->selectorText(str.toString() + rightSide);
        case CSSSelector::ShadowContent:
            return tagHistory->selectorText(" /content/ " + str.toString() + rightSide);
        }
    }
    return str.toString() + rightSide;
}

void CSSSelector::setAttribute(const QualifiedName& value)
{
    createRareData();
    m_data.m_rareData->m_attribute = value;
}

void CSSSelector::setArgument(const AtomicString& value)
{
    createRareData();
    m_data.m_rareData->m_argument = value;
}

void CSSSelector::setSelectorList(PassOwnPtr<CSSSelectorList> selectorList)
{
    createRareData();
    m_data.m_rareData->m_selectorList = selectorList;
}

static bool validateSubSelector(const CSSSelector* selector)
{
    switch (selector->m_match) {
    case CSSSelector::Tag:
    case CSSSelector::Id:
    case CSSSelector::Class:
    case CSSSelector::Exact:
    case CSSSelector::Set:
    case CSSSelector::List:
    case CSSSelector::Hyphen:
    case CSSSelector::Contain:
    case CSSSelector::Begin:
    case CSSSelector::End:
        return true;
    case CSSSelector::PseudoElement:
        return false;
    case CSSSelector::PagePseudoClass:
    case CSSSelector::PseudoClass:
        break;
    }

    switch (selector->pseudoType()) {
    case CSSSelector::PseudoEmpty:
    case CSSSelector::PseudoLink:
    case CSSSelector::PseudoVisited:
    case CSSSelector::PseudoTarget:
    case CSSSelector::PseudoEnabled:
    case CSSSelector::PseudoDisabled:
    case CSSSelector::PseudoChecked:
    case CSSSelector::PseudoIndeterminate:
    case CSSSelector::PseudoNthChild:
    case CSSSelector::PseudoNthLastChild:
    case CSSSelector::PseudoNthOfType:
    case CSSSelector::PseudoNthLastOfType:
    case CSSSelector::PseudoFirstChild:
    case CSSSelector::PseudoLastChild:
    case CSSSelector::PseudoFirstOfType:
    case CSSSelector::PseudoLastOfType:
    case CSSSelector::PseudoOnlyOfType:
    case CSSSelector::PseudoHost:
    case CSSSelector::PseudoAncestor:
        return true;
    default:
        return false;
    }
}

bool CSSSelector::isCompound() const
{
    if (!validateSubSelector(this))
        return false;

    const CSSSelector* prevSubSelector = this;
    const CSSSelector* subSelector = tagHistory();

    while (subSelector) {
        if (prevSubSelector->relation() != CSSSelector::SubSelector)
            return false;
        if (!validateSubSelector(subSelector))
            return false;

        prevSubSelector = subSelector;
        subSelector = subSelector->tagHistory();
    }

    return true;
}

bool CSSSelector::parseNth() const
{
    if (!m_hasRareData)
        return false;
    if (m_parsedNth)
        return true;
    m_parsedNth = m_data.m_rareData->parseNth();
    return m_parsedNth;
}

bool CSSSelector::matchNth(int count) const
{
    ASSERT(m_hasRareData);
    return m_data.m_rareData->matchNth(count);
}

CSSSelector::RareData::RareData(const AtomicString& value)
    : m_value(value)
    , m_a(0)
    , m_b(0)
    , m_attribute(anyQName())
    , m_argument(nullAtom)
{
}

CSSSelector::RareData::~RareData()
{
}

// a helper function for parsing nth-arguments
bool CSSSelector::RareData::parseNth()
{
    String argument = m_argument.lower();

    if (argument.isEmpty())
        return false;

    m_a = 0;
    m_b = 0;
    if (argument == "odd") {
        m_a = 2;
        m_b = 1;
    } else if (argument == "even") {
        m_a = 2;
        m_b = 0;
    } else {
        size_t n = argument.find('n');
        if (n != kNotFound) {
            if (argument[0] == '-') {
                if (n == 1)
                    m_a = -1; // -n == -1n
                else
                    m_a = argument.substring(0, n).toInt();
            } else if (!n)
                m_a = 1; // n == 1n
            else
                m_a = argument.substring(0, n).toInt();

            size_t p = argument.find('+', n);
            if (p != kNotFound)
                m_b = argument.substring(p + 1, argument.length() - p - 1).toInt();
            else {
                p = argument.find('-', n);
                if (p != kNotFound)
                    m_b = -argument.substring(p + 1, argument.length() - p - 1).toInt();
            }
        } else
            m_b = argument.toInt();
    }
    return true;
}

// a helper function for checking nth-arguments
bool CSSSelector::RareData::matchNth(int count)
{
    if (!m_a)
        return count == m_b;
    else if (m_a > 0) {
        if (count < m_b)
            return false;
        return (count - m_b) % m_a == 0;
    } else {
        if (count > m_b)
            return false;
        return (m_b - count) % (-m_a) == 0;
    }
}

} // namespace WebCore
