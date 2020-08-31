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

#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"

#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/leak_annotations.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSDefaultStyleSheets& CSSDefaultStyleSheets::Instance() {
  DEFINE_STATIC_LOCAL(Persistent<CSSDefaultStyleSheets>,
                      css_default_style_sheets,
                      (MakeGarbageCollected<CSSDefaultStyleSheets>()));
  return *css_default_style_sheets;
}

static const MediaQueryEvaluator& ScreenEval() {
  DEFINE_STATIC_LOCAL(const Persistent<MediaQueryEvaluator>, static_screen_eval,
                      (MakeGarbageCollected<MediaQueryEvaluator>("screen")));
  return *static_screen_eval;
}

static const MediaQueryEvaluator& PrintEval() {
  DEFINE_STATIC_LOCAL(const Persistent<MediaQueryEvaluator>, static_print_eval,
                      (MakeGarbageCollected<MediaQueryEvaluator>("print")));
  return *static_print_eval;
}

static const MediaQueryEvaluator& ForcedColorsEval() {
  DEFINE_STATIC_LOCAL(
      Persistent<MediaQueryEvaluator>, forced_colors_eval,
      (MakeGarbageCollected<MediaQueryEvaluator>("forced-colors")));
  return *forced_colors_eval;
}

static StyleSheetContents* ParseUASheet(const String& str) {
  // UA stylesheets always parse in the insecure context mode.
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(
          kUASheetMode, SecureContextMode::kInsecureContext));
  sheet->ParseString(str);
  // User Agent stylesheets are parsed once for the lifetime of the renderer
  // process and are intentionally leaked.
  LEAK_SANITIZER_IGNORE_OBJECT(sheet);
  return sheet;
}

CSSDefaultStyleSheets::CSSDefaultStyleSheets()
    : media_controls_style_sheet_loader_(nullptr) {
  // Strict-mode rules.
  String forced_colors_style_sheet =
      RuntimeEnabledFeatures::ForcedColorsEnabled()
          ? UncompressResourceAsASCIIString(IDR_UASTYLE_THEME_FORCED_COLORS_CSS)
          : String();
  String default_rules = UncompressResourceAsASCIIString(IDR_UASTYLE_HTML_CSS) +
                         LayoutTheme::GetTheme().ExtraDefaultStyleSheet() +
                         forced_colors_style_sheet;

  default_style_sheet_ = ParseUASheet(default_rules);

  // Quirks-mode rules.
  String quirks_rules =
      UncompressResourceAsASCIIString(IDR_UASTYLE_QUIRKS_CSS) +
      LayoutTheme::GetTheme().ExtraQuirksStyleSheet();
  quirks_style_sheet_ = ParseUASheet(quirks_rules);

  InitializeDefaultStyles();

#if DCHECK_IS_ON()
  default_style_->CompactRulesIfNeeded();
  default_mathml_style_->CompactRulesIfNeeded();
  default_svg_style_->CompactRulesIfNeeded();
  default_quirks_style_->CompactRulesIfNeeded();
  default_print_style_->CompactRulesIfNeeded();
  default_forced_color_style_->CompactRulesIfNeeded();
  DCHECK(default_style_->UniversalRules()->IsEmpty());
  DCHECK(default_mathml_style_->UniversalRules()->IsEmpty());
  DCHECK(default_svg_style_->UniversalRules()->IsEmpty());
  DCHECK(default_quirks_style_->UniversalRules()->IsEmpty());
  DCHECK(default_print_style_->UniversalRules()->IsEmpty());
  DCHECK(default_forced_color_style_->UniversalRules()->IsEmpty());
#endif
}

void CSSDefaultStyleSheets::PrepareForLeakDetection() {
  // Clear the optional style sheets.
  mobile_viewport_style_sheet_.Clear();
  television_viewport_style_sheet_.Clear();
  xhtml_mobile_profile_style_sheet_.Clear();
  svg_style_sheet_.Clear();
  mathml_style_sheet_.Clear();
  media_controls_style_sheet_.Clear();
  text_track_style_sheet_.Clear();
  fullscreen_style_sheet_.Clear();
  webxr_overlay_style_sheet_.Clear();
  // Recreate the default style sheet to clean up possible SVG resources.
  String default_rules = UncompressResourceAsASCIIString(IDR_UASTYLE_HTML_CSS) +
                         LayoutTheme::GetTheme().ExtraDefaultStyleSheet();
  default_style_sheet_ = ParseUASheet(default_rules);

  // Initialize the styles that have the lazily loaded style sheets.
  InitializeDefaultStyles();
  default_view_source_style_.Clear();
}

void CSSDefaultStyleSheets::InitializeDefaultStyles() {
  // This must be called only from constructor / PrepareForLeakDetection.
  default_style_ = MakeGarbageCollected<RuleSet>();
  default_mathml_style_ = MakeGarbageCollected<RuleSet>();
  default_svg_style_ = MakeGarbageCollected<RuleSet>();
  default_quirks_style_ = MakeGarbageCollected<RuleSet>();
  default_print_style_ = MakeGarbageCollected<RuleSet>();
  default_forced_color_style_ = MakeGarbageCollected<RuleSet>();

  default_style_->AddRulesFromSheet(DefaultStyleSheet(), ScreenEval());
  default_quirks_style_->AddRulesFromSheet(QuirksStyleSheet(), ScreenEval());
  default_print_style_->AddRulesFromSheet(DefaultStyleSheet(), PrintEval());
  default_forced_color_style_->AddRulesFromSheet(DefaultStyleSheet(),
                                                 ForcedColorsEval());
}

RuleSet* CSSDefaultStyleSheets::DefaultViewSourceStyle() {
  if (!default_view_source_style_) {
    default_view_source_style_ = MakeGarbageCollected<RuleSet>();
    // Loaded stylesheet is leaked on purpose.
    StyleSheetContents* stylesheet = ParseUASheet(
        UncompressResourceAsASCIIString(IDR_UASTYLE_VIEW_SOURCE_CSS));
    default_view_source_style_->AddRulesFromSheet(stylesheet, ScreenEval());
  }
  return default_view_source_style_;
}

StyleSheetContents*
CSSDefaultStyleSheets::EnsureXHTMLMobileProfileStyleSheet() {
  if (!xhtml_mobile_profile_style_sheet_) {
    xhtml_mobile_profile_style_sheet_ =
        ParseUASheet(UncompressResourceAsASCIIString(IDR_UASTYLE_XHTMLMP_CSS));
  }
  return xhtml_mobile_profile_style_sheet_;
}

StyleSheetContents* CSSDefaultStyleSheets::EnsureMobileViewportStyleSheet() {
  if (!mobile_viewport_style_sheet_) {
    mobile_viewport_style_sheet_ = ParseUASheet(
        UncompressResourceAsASCIIString(IDR_UASTYLE_VIEWPORT_ANDROID_CSS));
  }
  return mobile_viewport_style_sheet_;
}

StyleSheetContents*
CSSDefaultStyleSheets::EnsureTelevisionViewportStyleSheet() {
  if (!television_viewport_style_sheet_) {
    television_viewport_style_sheet_ = ParseUASheet(
        UncompressResourceAsASCIIString(IDR_UASTYLE_VIEWPORT_TELEVISION_CSS));
  }
  return television_viewport_style_sheet_;
}

static void AddTextTrackCSSProperties(StringBuilder* builder,
                                      CSSPropertyID propertyId,
                                      String value) {
  builder->Append(CSSProperty::Get(propertyId).GetPropertyNameString());
  builder->Append(": ");
  builder->Append(value);
  builder->Append("; ");
}

bool CSSDefaultStyleSheets::EnsureDefaultStyleSheetsForElement(
    const Element& element) {
  bool changed_default_style = false;
  // FIXME: We should assert that the sheet only styles SVG elements.
  if (element.IsSVGElement() && !svg_style_sheet_) {
    svg_style_sheet_ =
        ParseUASheet(UncompressResourceAsASCIIString(IDR_UASTYLE_SVG_CSS));
    default_svg_style_->AddRulesFromSheet(SvgStyleSheet(), ScreenEval());
    default_print_style_->AddRulesFromSheet(SvgStyleSheet(), PrintEval());
    default_forced_color_style_->AddRulesFromSheet(SvgStyleSheet(),
                                                   ForcedColorsEval());
    changed_default_style = true;
  }

  // FIXME: We should assert that the sheet only styles MathML elements.
  if (element.namespaceURI() == mathml_names::kNamespaceURI &&
      !mathml_style_sheet_) {
    mathml_style_sheet_ = ParseUASheet(
        RuntimeEnabledFeatures::MathMLCoreEnabled()
            ? UncompressResourceAsASCIIString(IDR_UASTYLE_MATHML_CSS)
            : UncompressResourceAsASCIIString(IDR_UASTYLE_MATHML_FALLBACK_CSS));
    default_mathml_style_->AddRulesFromSheet(MathmlStyleSheet(), ScreenEval());
    default_print_style_->AddRulesFromSheet(MathmlStyleSheet(), PrintEval());
    changed_default_style = true;
  }

  if (!media_controls_style_sheet_ && HasMediaControlsStyleSheetLoader() &&
      (IsA<HTMLVideoElement>(element) || IsA<HTMLAudioElement>(element))) {
    // FIXME: We should assert that this sheet only contains rules for <video>
    // and <audio>.
    media_controls_style_sheet_ =
        ParseUASheet(media_controls_style_sheet_loader_->GetUAStyleSheet());
    default_style_->AddRulesFromSheet(MediaControlsStyleSheet(), ScreenEval());
    default_print_style_->AddRulesFromSheet(MediaControlsStyleSheet(),
                                            PrintEval());
    default_forced_color_style_->AddRulesFromSheet(MediaControlsStyleSheet(),
                                                   ForcedColorsEval());
    changed_default_style = true;
  }

  if (!text_track_style_sheet_ && IsA<HTMLVideoElement>(element)) {
    Settings* settings = element.GetDocument().GetSettings();
    if (settings) {
      StringBuilder builder;
      builder.Append("video::-webkit-media-text-track-display { ");
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kBackgroundColor,
                                settings->GetTextTrackWindowColor());
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kPadding,
                                settings->GetTextTrackWindowPadding());
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kBorderRadius,
                                settings->GetTextTrackWindowRadius());
      builder.Append(" } video::cue { ");
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kBackgroundColor,
                                settings->GetTextTrackBackgroundColor());
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kFontFamily,
                                settings->GetTextTrackFontFamily());
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kFontStyle,
                                settings->GetTextTrackFontStyle());
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kFontVariant,
                                settings->GetTextTrackFontVariant());
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kColor,
                                settings->GetTextTrackTextColor());
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kTextShadow,
                                settings->GetTextTrackTextShadow());
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kFontSize,
                                settings->GetTextTrackTextSize());
      builder.Append(" } ");
      text_track_style_sheet_ = ParseUASheet(builder.ToString());
      default_style_->AddRulesFromSheet(text_track_style_sheet_, ScreenEval());
      default_print_style_->AddRulesFromSheet(text_track_style_sheet_,
                                              PrintEval());
      changed_default_style = true;
    }
  }

  DCHECK(!default_style_->Features().HasIdsInSelectors());
  return changed_default_style;
}

void CSSDefaultStyleSheets::SetMediaControlsStyleSheetLoader(
    std::unique_ptr<UAStyleSheetLoader> loader) {
  media_controls_style_sheet_loader_.swap(loader);
}

bool CSSDefaultStyleSheets::EnsureDefaultStyleSheetForXrOverlay() {
  if (webxr_overlay_style_sheet_)
    return false;

  webxr_overlay_style_sheet_ = ParseUASheet(
      UncompressResourceAsASCIIString(IDR_UASTYLE_WEBXR_OVERLAY_CSS));
  default_style_->AddRulesFromSheet(webxr_overlay_style_sheet_, ScreenEval());
  default_print_style_->AddRulesFromSheet(webxr_overlay_style_sheet_,
                                          PrintEval());
  default_forced_color_style_->AddRulesFromSheet(webxr_overlay_style_sheet_,
                                                 ForcedColorsEval());
  return true;
}

void CSSDefaultStyleSheets::EnsureDefaultStyleSheetForFullscreen() {
  if (fullscreen_style_sheet_)
    return;

  String fullscreen_rules =
      UncompressResourceAsASCIIString(IDR_UASTYLE_FULLSCREEN_CSS) +
      LayoutTheme::GetTheme().ExtraFullscreenStyleSheet();
  fullscreen_style_sheet_ = ParseUASheet(fullscreen_rules);
  default_style_->AddRulesFromSheet(FullscreenStyleSheet(), ScreenEval());
  default_quirks_style_->AddRulesFromSheet(FullscreenStyleSheet(),
                                           ScreenEval());
}

void CSSDefaultStyleSheets::Trace(Visitor* visitor) {
  visitor->Trace(default_style_);
  visitor->Trace(default_mathml_style_);
  visitor->Trace(default_svg_style_);
  visitor->Trace(default_quirks_style_);
  visitor->Trace(default_print_style_);
  visitor->Trace(default_view_source_style_);
  visitor->Trace(default_forced_color_style_);
  visitor->Trace(default_style_sheet_);
  visitor->Trace(mobile_viewport_style_sheet_);
  visitor->Trace(television_viewport_style_sheet_);
  visitor->Trace(xhtml_mobile_profile_style_sheet_);
  visitor->Trace(quirks_style_sheet_);
  visitor->Trace(svg_style_sheet_);
  visitor->Trace(mathml_style_sheet_);
  visitor->Trace(media_controls_style_sheet_);
  visitor->Trace(text_track_style_sheet_);
  visitor->Trace(fullscreen_style_sheet_);
  visitor->Trace(webxr_overlay_style_sheet_);
}

}  // namespace blink
