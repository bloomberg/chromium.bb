/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
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

#include "core/css/CSSDefaultStyleSheets.h"

#include "core/css/MediaQueryEvaluator.h"
#include "core/css/RuleSet.h"
#include "core/css/StyleSheetContents.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/layout/LayoutTheme.h"
#include "core/mathml_names.h"
#include "platform/DataResourceHelper.h"
#include "platform/runtime_enabled_features.h"
#include "platform/wtf/LeakAnnotations.h"

namespace blink {

using namespace HTMLNames;

CSSDefaultStyleSheets& CSSDefaultStyleSheets::Instance() {
  DEFINE_STATIC_LOCAL(CSSDefaultStyleSheets, css_default_style_sheets,
                      (new CSSDefaultStyleSheets));
  return css_default_style_sheets;
}

static const MediaQueryEvaluator& ScreenEval() {
  DEFINE_STATIC_LOCAL(MediaQueryEvaluator, static_screen_eval,
                      (new MediaQueryEvaluator("screen")));
  return static_screen_eval;
}

static const MediaQueryEvaluator& PrintEval() {
  DEFINE_STATIC_LOCAL(MediaQueryEvaluator, static_print_eval,
                      (new MediaQueryEvaluator("print")));
  return static_print_eval;
}

static StyleSheetContents* ParseUASheet(const String& str) {
  StyleSheetContents* sheet =
      StyleSheetContents::Create(CSSParserContext::Create(kUASheetMode));
  sheet->ParseString(str);
  // User Agent stylesheets are parsed once for the lifetime of the renderer
  // process and are intentionally leaked.
  LEAK_SANITIZER_IGNORE_OBJECT(sheet);
  return sheet;
}

CSSDefaultStyleSheets::CSSDefaultStyleSheets()
    : media_controls_style_sheet_loader_(nullptr) {
  default_style_ = RuleSet::Create();
  default_print_style_ = RuleSet::Create();
  default_quirks_style_ = RuleSet::Create();

  // Strict-mode rules.
  String default_rules = GetDataResourceAsASCIIString("html.css") +
                         LayoutTheme::GetTheme().ExtraDefaultStyleSheet();
  default_style_sheet_ = ParseUASheet(default_rules);
  default_style_->AddRulesFromSheet(DefaultStyleSheet(), ScreenEval());
  default_print_style_->AddRulesFromSheet(DefaultStyleSheet(), PrintEval());

  // Quirks-mode rules.
  String quirks_rules = GetDataResourceAsASCIIString("quirks.css") +
                        LayoutTheme::GetTheme().ExtraQuirksStyleSheet();
  quirks_style_sheet_ = ParseUASheet(quirks_rules);
  default_quirks_style_->AddRulesFromSheet(QuirksStyleSheet(), ScreenEval());

#if DCHECK_IS_ON()
  default_style_->CompactRulesIfNeeded();
  default_print_style_->CompactRulesIfNeeded();
  default_quirks_style_->CompactRulesIfNeeded();
  DCHECK(default_style_->UniversalRules()->IsEmpty());
  DCHECK(default_print_style_->UniversalRules()->IsEmpty());
  DCHECK(default_quirks_style_->UniversalRules()->IsEmpty());
#endif
}

RuleSet* CSSDefaultStyleSheets::DefaultViewSourceStyle() {
  if (!default_view_source_style_) {
    default_view_source_style_ = RuleSet::Create();
    // Loaded stylesheet is leaked on purpose.
    StyleSheetContents* stylesheet =
        ParseUASheet(GetDataResourceAsASCIIString("view-source.css"));
    default_view_source_style_->AddRulesFromSheet(stylesheet, ScreenEval());
  }
  return default_view_source_style_;
}

StyleSheetContents*
CSSDefaultStyleSheets::EnsureXHTMLMobileProfileStyleSheet() {
  if (!xhtml_mobile_profile_style_sheet_) {
    xhtml_mobile_profile_style_sheet_ =
        ParseUASheet(GetDataResourceAsASCIIString("xhtmlmp.css"));
  }
  return xhtml_mobile_profile_style_sheet_;
}

StyleSheetContents* CSSDefaultStyleSheets::EnsureMobileViewportStyleSheet() {
  if (!mobile_viewport_style_sheet_) {
    mobile_viewport_style_sheet_ =
        ParseUASheet(GetDataResourceAsASCIIString("viewportAndroid.css"));
  }
  return mobile_viewport_style_sheet_;
}

StyleSheetContents*
CSSDefaultStyleSheets::EnsureTelevisionViewportStyleSheet() {
  if (!television_viewport_style_sheet_) {
    television_viewport_style_sheet_ =
        ParseUASheet(GetDataResourceAsASCIIString("viewportTelevision.css"));
  }
  return television_viewport_style_sheet_;
}

bool CSSDefaultStyleSheets::EnsureDefaultStyleSheetsForElement(
    const Element& element) {
  bool changed_default_style = false;
  // FIXME: We should assert that the sheet only styles SVG elements.
  if (element.IsSVGElement() && !svg_style_sheet_) {
    svg_style_sheet_ = ParseUASheet(GetDataResourceAsASCIIString("svg.css"));
    default_style_->AddRulesFromSheet(SvgStyleSheet(), ScreenEval());
    default_print_style_->AddRulesFromSheet(SvgStyleSheet(), PrintEval());
    changed_default_style = true;
  }

  // FIXME: We should assert that the sheet only styles MathML elements.
  if (element.namespaceURI() == MathMLNames::mathmlNamespaceURI &&
      !mathml_style_sheet_) {
    mathml_style_sheet_ =
        ParseUASheet(GetDataResourceAsASCIIString("mathml.css"));
    default_style_->AddRulesFromSheet(MathmlStyleSheet(), ScreenEval());
    default_print_style_->AddRulesFromSheet(MathmlStyleSheet(), PrintEval());
    changed_default_style = true;
  }

  if (!media_controls_style_sheet_ && HasMediaControlsStyleSheetLoader() &&
      (IsHTMLVideoElement(element) || IsHTMLAudioElement(element))) {
    // FIXME: We should assert that this sheet only contains rules for <video>
    // and <audio>.
    media_controls_style_sheet_ =
        ParseUASheet(media_controls_style_sheet_loader_->GetUAStyleSheet());
    default_style_->AddRulesFromSheet(MediaControlsStyleSheet(), ScreenEval());
    default_print_style_->AddRulesFromSheet(MediaControlsStyleSheet(),
                                            PrintEval());
    changed_default_style = true;
  }

  DCHECK(!default_style_->Features().HasIdsInSelectors());
  return changed_default_style;
}

void CSSDefaultStyleSheets::SetMediaControlsStyleSheetLoader(
    std::unique_ptr<UAStyleSheetLoader> loader) {
  media_controls_style_sheet_loader_.swap(loader);
}

void CSSDefaultStyleSheets::EnsureDefaultStyleSheetForFullscreen() {
  if (fullscreen_style_sheet_)
    return;

  String fullscreen_rules = GetDataResourceAsASCIIString("fullscreen.css") +
                            LayoutTheme::GetTheme().ExtraFullscreenStyleSheet();
  fullscreen_style_sheet_ = ParseUASheet(fullscreen_rules);
  default_style_->AddRulesFromSheet(FullscreenStyleSheet(), ScreenEval());
  default_quirks_style_->AddRulesFromSheet(FullscreenStyleSheet(),
                                           ScreenEval());
}

DEFINE_TRACE(CSSDefaultStyleSheets) {
  visitor->Trace(default_style_);
  visitor->Trace(default_quirks_style_);
  visitor->Trace(default_print_style_);
  visitor->Trace(default_view_source_style_);
  visitor->Trace(default_style_sheet_);
  visitor->Trace(mobile_viewport_style_sheet_);
  visitor->Trace(television_viewport_style_sheet_);
  visitor->Trace(xhtml_mobile_profile_style_sheet_);
  visitor->Trace(quirks_style_sheet_);
  visitor->Trace(svg_style_sheet_);
  visitor->Trace(mathml_style_sheet_);
  visitor->Trace(media_controls_style_sheet_);
  visitor->Trace(fullscreen_style_sheet_);
}

}  // namespace blink
