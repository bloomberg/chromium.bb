/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef CSSDefaultStyleSheets_h
#define CSSDefaultStyleSheets_h

#include "heap/Handle.h"

namespace WebCore {

class Element;
class RuleSet;
class StyleSheetContents;

class CSSDefaultStyleSheets : public NoBaseWillBeGarbageCollected<CSSDefaultStyleSheets> {
public:
    static CSSDefaultStyleSheets& instance();

    void ensureDefaultStyleSheetsForElement(Element*, bool& changedDefaultStyle);

    RuleSet* defaultStyle() { return m_defaultStyle;}
    RuleSet* defaultViewportStyle() { return m_defaultViewportStyle;}
    RuleSet* defaultQuirksStyle() { return m_defaultQuirksStyle;}
    RuleSet* defaultPrintStyle() { return m_defaultPrintStyle;}
    RuleSet* defaultViewSourceStyle();

    // FIXME: Remove WAP support.
    RuleSet* defaultXHTMLMobileProfileStyle();

    StyleSheetContents* defaultStyleSheet() { return m_defaultStyleSheet.get(); }
    StyleSheetContents* viewportStyleSheet() { return m_viewportStyleSheet.get(); }
    StyleSheetContents* quirksStyleSheet() { return m_quirksStyleSheet.get(); }
    StyleSheetContents* svgStyleSheet() { return m_svgStyleSheet.get(); }
    StyleSheetContents* mediaControlsStyleSheet() { return m_mediaControlsStyleSheet.get(); }
    StyleSheetContents* fullscreenStyleSheet() { return m_fullscreenStyleSheet.get(); }

    void trace(Visitor*);

private:
    CSSDefaultStyleSheets();

    RuleSet* m_defaultStyle;
    RuleSet* m_defaultViewportStyle;
    RuleSet* m_defaultQuirksStyle;
    RuleSet* m_defaultPrintStyle;
    RuleSet* m_defaultViewSourceStyle;
    RuleSet* m_defaultXHTMLMobileProfileStyle;

    RefPtrWillBeMember<StyleSheetContents> m_defaultStyleSheet;
    RefPtrWillBeMember<StyleSheetContents> m_viewportStyleSheet;
    RefPtrWillBeMember<StyleSheetContents> m_quirksStyleSheet;
    RefPtrWillBeMember<StyleSheetContents> m_svgStyleSheet;
    RefPtrWillBeMember<StyleSheetContents> m_mediaControlsStyleSheet;
    RefPtrWillBeMember<StyleSheetContents> m_fullscreenStyleSheet;
};

} // namespace WebCore

#endif // CSSDefaultStyleSheets_h
