/*
 * Copyright (C) 2012-2013 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "core/css/resolver/ViewportStyleResolver.h"

#include "core/CSSValueKeywords.h"
#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/css/DocumentStyleSheetCollection.h"
#include "core/css/MediaValuesInitialViewport.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleRuleImport.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/ViewportDescription.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/layout/api/LayoutViewItem.h"

namespace blink {

ViewportStyleResolver::ViewportStyleResolver(Document& document)
    : document_(document) {
  DCHECK(document.GetFrame());
  initial_viewport_medium_ = new MediaQueryEvaluator(
      MediaValuesInitialViewport::Create(*document.GetFrame()));
}

void ViewportStyleResolver::Reset() {
  viewport_dependent_media_query_results_.clear();
  device_dependent_media_query_results_.clear();
  property_set_ = nullptr;
  has_author_style_ = false;
  has_viewport_units_ = false;
  needs_update_ = kNoUpdate;
}

void ViewportStyleResolver::CollectViewportRulesFromUASheets() {
  CSSDefaultStyleSheets& default_style_sheets =
      CSSDefaultStyleSheets::Instance();
  WebViewportStyle viewport_style =
      document_->GetSettings() ? document_->GetSettings()->GetViewportStyle()
                               : WebViewportStyle::kDefault;
  StyleSheetContents* viewport_contents = nullptr;
  switch (viewport_style) {
    case WebViewportStyle::kDefault:
      break;
    case WebViewportStyle::kMobile:
      viewport_contents = default_style_sheets.EnsureMobileViewportStyleSheet();
      break;
    case WebViewportStyle::kTelevision:
      viewport_contents =
          default_style_sheets.EnsureTelevisionViewportStyleSheet();
      break;
  }
  if (viewport_contents)
    CollectViewportChildRules(viewport_contents->ChildRules(),
                              kUserAgentOrigin);

  if (document_->IsMobileDocument()) {
    CollectViewportChildRules(
        default_style_sheets.EnsureXHTMLMobileProfileStyleSheet()->ChildRules(),
        kUserAgentOrigin);
  }
  DCHECK(!default_style_sheets.DefaultStyleSheet()->HasViewportRule());
}

void ViewportStyleResolver::CollectViewportChildRules(
    const HeapVector<Member<StyleRuleBase>>& rules,
    Origin origin) {
  for (auto& rule : rules) {
    if (rule->IsViewportRule()) {
      AddViewportRule(*ToStyleRuleViewport(rule), origin);
    } else if (rule->IsMediaRule()) {
      StyleRuleMedia* media_rule = ToStyleRuleMedia(rule);
      if (!media_rule->MediaQueries() ||
          initial_viewport_medium_->Eval(
              *media_rule->MediaQueries(),
              &viewport_dependent_media_query_results_,
              &device_dependent_media_query_results_))
        CollectViewportChildRules(media_rule->ChildRules(), origin);
    } else if (rule->IsSupportsRule()) {
      StyleRuleSupports* supports_rule = ToStyleRuleSupports(rule);
      if (supports_rule->ConditionIsSupported())
        CollectViewportChildRules(supports_rule->ChildRules(), origin);
    }
  }
}

void ViewportStyleResolver::CollectViewportRulesFromImports(
    StyleSheetContents& contents) {
  for (const auto& import_rule : contents.ImportRules()) {
    if (!import_rule->GetStyleSheet())
      continue;
    if (!import_rule->GetStyleSheet()->HasViewportRule())
      continue;
    if (import_rule->MediaQueries() &&
        initial_viewport_medium_->Eval(*import_rule->MediaQueries(),
                                       &viewport_dependent_media_query_results_,
                                       &device_dependent_media_query_results_))
      CollectViewportRulesFromAuthorSheetContents(
          *import_rule->GetStyleSheet());
  }
}

void ViewportStyleResolver::CollectViewportRulesFromAuthorSheetContents(
    StyleSheetContents& contents) {
  CollectViewportRulesFromImports(contents);
  if (contents.HasViewportRule())
    CollectViewportChildRules(contents.ChildRules(), kAuthorOrigin);
}

void ViewportStyleResolver::CollectViewportRulesFromAuthorSheet(
    const CSSStyleSheet& sheet) {
  DCHECK(sheet.Contents());
  StyleSheetContents& contents = *sheet.Contents();
  if (!contents.HasViewportRule() && contents.ImportRules().IsEmpty())
    return;
  if (sheet.MediaQueries() &&
      !initial_viewport_medium_->Eval(*sheet.MediaQueries(),
                                      &viewport_dependent_media_query_results_,
                                      &device_dependent_media_query_results_))
    return;
  CollectViewportRulesFromAuthorSheetContents(contents);
}

void ViewportStyleResolver::AddViewportRule(StyleRuleViewport& viewport_rule,
                                            Origin origin) {
  StylePropertySet& property_set = viewport_rule.MutableProperties();

  unsigned property_count = property_set.PropertyCount();
  if (!property_count)
    return;

  if (origin == kAuthorOrigin)
    has_author_style_ = true;

  if (!property_set_) {
    property_set_ = property_set.MutableCopy();
    return;
  }

  // We cannot use mergeAndOverrideOnConflict() here because it doesn't
  // respect the !important declaration (but addRespectingCascade() does).
  for (unsigned i = 0; i < property_count; ++i)
    property_set_->AddRespectingCascade(
        property_set.PropertyAt(i).ToCSSProperty());
}

void ViewportStyleResolver::Resolve() {
  if (!property_set_) {
    document_->SetViewportDescription(
        ViewportDescription(ViewportDescription::kUserAgentStyleSheet));
    return;
  }

  ViewportDescription description(
      has_author_style_ ? ViewportDescription::kAuthorStyleSheet
                        : ViewportDescription::kUserAgentStyleSheet);

  description.user_zoom = ViewportArgumentValue(CSSPropertyUserZoom);
  description.zoom = ViewportArgumentValue(CSSPropertyZoom);
  description.min_zoom = ViewportArgumentValue(CSSPropertyMinZoom);
  description.max_zoom = ViewportArgumentValue(CSSPropertyMaxZoom);
  description.min_width = ViewportLengthValue(CSSPropertyMinWidth);
  description.max_width = ViewportLengthValue(CSSPropertyMaxWidth);
  description.min_height = ViewportLengthValue(CSSPropertyMinHeight);
  description.max_height = ViewportLengthValue(CSSPropertyMaxHeight);
  description.orientation = ViewportArgumentValue(CSSPropertyOrientation);

  document_->SetViewportDescription(description);
}

float ViewportStyleResolver::ViewportArgumentValue(CSSPropertyID id) const {
  float default_value = ViewportDescription::kValueAuto;

  // UserZoom default value is CSSValueZoom, which maps to true, meaning that
  // yes, it is user scalable. When the value is set to CSSValueFixed, we
  // return false.
  if (id == CSSPropertyUserZoom)
    default_value = 1;

  const CSSValue* value = property_set_->GetPropertyCSSValue(id);
  if (!value || !(value->IsPrimitiveValue() || value->IsIdentifierValue()))
    return default_value;

  if (value->IsIdentifierValue()) {
    switch (ToCSSIdentifierValue(value)->GetValueID()) {
      case CSSValueAuto:
        return default_value;
      case CSSValueLandscape:
        return ViewportDescription::kValueLandscape;
      case CSSValuePortrait:
        return ViewportDescription::kValuePortrait;
      case CSSValueZoom:
        return default_value;
      case CSSValueInternalExtendToZoom:
        return ViewportDescription::kValueExtendToZoom;
      case CSSValueFixed:
        return 0;
      default:
        return default_value;
    }
  }

  const CSSPrimitiveValue* primitive_value = ToCSSPrimitiveValue(value);

  if (primitive_value->IsNumber() || primitive_value->IsPx())
    return primitive_value->GetFloatValue();

  if (primitive_value->IsFontRelativeLength())
    return primitive_value->GetFloatValue() *
           document_->GetComputedStyle()->GetFontDescription().ComputedSize();

  if (primitive_value->IsPercentage()) {
    float percent_value = primitive_value->GetFloatValue() / 100.0f;
    switch (id) {
      case CSSPropertyMaxZoom:
      case CSSPropertyMinZoom:
      case CSSPropertyZoom:
        return percent_value;
      default:
        NOTREACHED();
        break;
    }
  }

  NOTREACHED();
  return default_value;
}

Length ViewportStyleResolver::ViewportLengthValue(CSSPropertyID id) {
  DCHECK(id == CSSPropertyMaxHeight || id == CSSPropertyMinHeight ||
         id == CSSPropertyMaxWidth || id == CSSPropertyMinWidth);

  const CSSValue* value = property_set_->GetPropertyCSSValue(id);
  if (!value || !(value->IsPrimitiveValue() || value->IsIdentifierValue()))
    return Length();  // auto

  if (value->IsIdentifierValue()) {
    CSSValueID value_id = ToCSSIdentifierValue(value)->GetValueID();
    if (value_id == CSSValueInternalExtendToZoom)
      return Length(kExtendToZoom);
    if (value_id == CSSValueAuto)
      return Length(kAuto);
  }

  const CSSPrimitiveValue* primitive_value = ToCSSPrimitiveValue(value);
  ComputedStyle* document_style = document_->MutableComputedStyle();

  // If we have viewport units the conversion will mark the document style as
  // having viewport units.
  bool document_style_has_viewport_units = document_style->HasViewportUnits();
  document_style->SetHasViewportUnits(false);

  LocalFrameView* view = document_->GetFrame()->View();
  DCHECK(view);

  CSSToLengthConversionData::FontSizes font_sizes(document_style,
                                                  document_style);
  CSSToLengthConversionData::ViewportSize viewport_size(
      view->InitialViewportWidth(), view->InitialViewportHeight());

  Length result = primitive_value->ConvertToLength(CSSToLengthConversionData(
      document_style, font_sizes, viewport_size, 1.0f));
  if (document_style->HasViewportUnits())
    has_viewport_units_ = true;
  document_style->SetHasViewportUnits(document_style_has_viewport_units);

  return result;
}

void ViewportStyleResolver::InitialViewportChanged() {
  if (needs_update_ == kCollectRules)
    return;
  if (has_viewport_units_)
    needs_update_ = kResolve;

  auto& results = viewport_dependent_media_query_results_;
  for (unsigned i = 0; i < results.size(); i++) {
    if (initial_viewport_medium_->Eval(results[i].Expression()) !=
        results[i].Result()) {
      needs_update_ = kCollectRules;
      break;
    }
  }
  if (needs_update_ == kNoUpdate)
    return;
  document_->ScheduleLayoutTreeUpdateIfNeeded();
}

void ViewportStyleResolver::SetNeedsCollectRules() {
  needs_update_ = kCollectRules;
  document_->ScheduleLayoutTreeUpdateIfNeeded();
}

void ViewportStyleResolver::UpdateViewport(
    DocumentStyleSheetCollection& collection) {
  if (needs_update_ == kNoUpdate)
    return;
  if (needs_update_ == kCollectRules) {
    Reset();
    CollectViewportRulesFromUASheets();
    if (RuntimeEnabledFeatures::CSSViewportEnabled())
      collection.CollectViewportRules(*this);
  }
  Resolve();
  needs_update_ = kNoUpdate;
}

DEFINE_TRACE(ViewportStyleResolver) {
  visitor->Trace(document_);
  visitor->Trace(property_set_);
  visitor->Trace(initial_viewport_medium_);
}

}  // namespace blink
