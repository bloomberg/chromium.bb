/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "core/css/CSSDefaultStyleSheets.h"

#include "UserAgentStyleSheets.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/RuleSet.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/rendering/RenderTheme.h"

namespace WebCore {

using namespace HTMLNames;

RuleSet* CSSDefaultStyleSheets::defaultStyle;
RuleSet* CSSDefaultStyleSheets::defaultViewportStyle;
RuleSet* CSSDefaultStyleSheets::defaultQuirksStyle;
RuleSet* CSSDefaultStyleSheets::defaultPrintStyle;
RuleSet* CSSDefaultStyleSheets::defaultViewSourceStyle;
RuleSet* CSSDefaultStyleSheets::defaultXHTMLMobileProfileStyle;

StyleSheetContents* CSSDefaultStyleSheets::defaultStyleSheet;
StyleSheetContents* CSSDefaultStyleSheets::viewportStyleSheet;
StyleSheetContents* CSSDefaultStyleSheets::quirksStyleSheet;
StyleSheetContents* CSSDefaultStyleSheets::svgStyleSheet;
StyleSheetContents* CSSDefaultStyleSheets::mediaControlsStyleSheet;
StyleSheetContents* CSSDefaultStyleSheets::fullscreenStyleSheet;

static const MediaQueryEvaluator& screenEval()
{
    DEFINE_STATIC_LOCAL(const MediaQueryEvaluator, staticScreenEval, ("screen"));
    return staticScreenEval;
}

static const MediaQueryEvaluator& printEval()
{
    DEFINE_STATIC_LOCAL(const MediaQueryEvaluator, staticPrintEval, ("print"));
    return staticPrintEval;
}

static StyleSheetContents* parseUASheet(const String& str)
{
    StyleSheetContents* sheet = StyleSheetContents::create(CSSParserContext(UASheetMode)).leakRef(); // leak the sheet on purpose
    sheet->parseString(str);
    return sheet;
}

static StyleSheetContents* parseUASheet(const char* characters, unsigned size)
{
    return parseUASheet(String(characters, size));
}

void CSSDefaultStyleSheets::loadDefaultStylesheetIfNecessary()
{
    if (!defaultStyle)
        loadDefaultStyle();
}

void CSSDefaultStyleSheets::loadDefaultStyle()
{
    ASSERT(!defaultStyle);
    defaultStyle = RuleSet::create().leakPtr();
    defaultViewportStyle = RuleSet::create().leakPtr();
    defaultPrintStyle = RuleSet::create().leakPtr();
    defaultQuirksStyle = RuleSet::create().leakPtr();

    // Strict-mode rules.
    String defaultRules = String(htmlUserAgentStyleSheet, sizeof(htmlUserAgentStyleSheet)) + RenderTheme::theme().extraDefaultStyleSheet();
    defaultStyleSheet = parseUASheet(defaultRules);
    defaultStyle->addRulesFromSheet(defaultStyleSheet, screenEval());
#if OS(ANDROID)
    String viewportRules(viewportAndroidUserAgentStyleSheet, sizeof(viewportAndroidUserAgentStyleSheet));
#else
    String viewportRules;
#endif
    viewportStyleSheet = parseUASheet(viewportRules);
    defaultViewportStyle->addRulesFromSheet(viewportStyleSheet, screenEval());
    defaultPrintStyle->addRulesFromSheet(defaultStyleSheet, printEval());

    // Quirks-mode rules.
    String quirksRules = String(quirksUserAgentStyleSheet, sizeof(quirksUserAgentStyleSheet)) + RenderTheme::theme().extraQuirksStyleSheet();
    quirksStyleSheet = parseUASheet(quirksRules);
    defaultQuirksStyle->addRulesFromSheet(quirksStyleSheet, screenEval());
}

RuleSet* CSSDefaultStyleSheets::viewSourceStyle()
{
    if (!defaultViewSourceStyle) {
        defaultViewSourceStyle = RuleSet::create().leakPtr();
        defaultViewSourceStyle->addRulesFromSheet(parseUASheet(sourceUserAgentStyleSheet, sizeof(sourceUserAgentStyleSheet)), screenEval());
    }
    return defaultViewSourceStyle;
}

RuleSet* CSSDefaultStyleSheets::xhtmlMobileProfileStyle()
{
    if (!defaultXHTMLMobileProfileStyle) {
        defaultXHTMLMobileProfileStyle = RuleSet::create().leakPtr();
        defaultXHTMLMobileProfileStyle->addRulesFromSheet(parseUASheet(xhtmlmpUserAgentStyleSheet, sizeof(xhtmlmpUserAgentStyleSheet)), screenEval());
    }
    return defaultXHTMLMobileProfileStyle;
}

void CSSDefaultStyleSheets::ensureDefaultStyleSheetsForElement(Element* element, bool& changedDefaultStyle)
{
    // FIXME: We should assert that the sheet only styles SVG elements.
    if (element->isSVGElement() && !svgStyleSheet) {
        svgStyleSheet = parseUASheet(svgUserAgentStyleSheet, sizeof(svgUserAgentStyleSheet));
        defaultStyle->addRulesFromSheet(svgStyleSheet, screenEval());
        defaultPrintStyle->addRulesFromSheet(svgStyleSheet, printEval());
        changedDefaultStyle = true;
    }

    // FIXME: We should assert that this sheet only contains rules for <video> and <audio>.
    if (!mediaControlsStyleSheet && (element->hasTagName(videoTag) || element->hasTagName(audioTag))) {
        String mediaRules = String(mediaControlsUserAgentStyleSheet, sizeof(mediaControlsUserAgentStyleSheet)) + RenderTheme::theme().extraMediaControlsStyleSheet();
        mediaControlsStyleSheet = parseUASheet(mediaRules);
        defaultStyle->addRulesFromSheet(mediaControlsStyleSheet, screenEval());
        defaultPrintStyle->addRulesFromSheet(mediaControlsStyleSheet, printEval());
        changedDefaultStyle = true;
    }

    // FIXME: This only works because we Force recalc the entire document so the new sheet
    // is loaded for <html> and the correct styles apply to everyone.
    if (!fullscreenStyleSheet && FullscreenElementStack::isFullScreen(&element->document())) {
        String fullscreenRules = String(fullscreenUserAgentStyleSheet, sizeof(fullscreenUserAgentStyleSheet)) + RenderTheme::theme().extraFullScreenStyleSheet();
        fullscreenStyleSheet = parseUASheet(fullscreenRules);
        defaultStyle->addRulesFromSheet(fullscreenStyleSheet, screenEval());
        defaultQuirksStyle->addRulesFromSheet(fullscreenStyleSheet, screenEval());
        changedDefaultStyle = true;
    }

    ASSERT(!defaultStyle->features().hasIdsInSelectors());
    ASSERT(defaultStyle->features().siblingRules.isEmpty());
}

} // namespace WebCore
