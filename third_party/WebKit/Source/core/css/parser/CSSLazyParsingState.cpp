// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSLazyParsingState.h"
#include "core/css/parser/CSSLazyPropertyParserImpl.h"
#include "core/css/parser/CSSParserTokenStream.h"
#include "core/dom/Document.h"
#include "core/frame/UseCounter.h"
#include "platform/Histogram.h"

namespace blink {

CSSLazyParsingState::CSSLazyParsingState(const CSSParserContext* context,
                                         const String& sheet_text,
                                         StyleSheetContents* contents)
    : context_(context),
      sheet_text_(sheet_text),
      owning_contents_(contents),
      parsed_style_rules_(0),
      total_style_rules_(0),
      style_rules_needed_for_next_milestone_(0),
      usage_(kUsageGe0),
      should_use_count_(context_->IsUseCounterRecordingEnabled()) {}

void CSSLazyParsingState::FinishInitialParsing() {
  RecordUsageMetrics();
}

CSSLazyPropertyParserImpl* CSSLazyParsingState::CreateLazyParser(
    const size_t offset) {
  ++total_style_rules_;
  return new CSSLazyPropertyParserImpl(offset, this);
}

const CSSParserContext* CSSLazyParsingState::Context() {
  DCHECK(owning_contents_);
  if (!should_use_count_) {
    DCHECK(!context_->IsUseCounterRecordingEnabled());
    return context_;
  }

  // Try as best as possible to grab a valid Document if the old Document has
  // gone away so we can still use UseCounter.
  if (!document_)
    document_ = owning_contents_->AnyOwnerDocument();

  if (!context_->IsDocumentHandleEqual(document_))
    context_ = CSSParserContext::Create(context_, document_);
  return context_;
}

StyleEngine& CSSLazyParsingState::GetStyleEngine() {
  DCHECK(owning_contents_);
  // Try as best as possible to grab a valid Document if the old Document has
  // gone away so we can still use UseCounter.
  if (!document_)
    document_ = owning_contents_->AnyOwnerDocument();
  return document_->GetStyleEngine();
}

void CSSLazyParsingState::CountRuleParsed() {
  ++parsed_style_rules_;
  while (parsed_style_rules_ > style_rules_needed_for_next_milestone_) {
    DCHECK_NE(kUsageAll, usage_);
    ++usage_;
    RecordUsageMetrics();
  }
}

void CSSLazyParsingState::RecordUsageMetrics() {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, usage_histogram,
                      ("Style.LazyUsage.Percent", kUsageLastValue));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, total_rules_histogram,
                      ("Style.TotalLazyRules", 0, 100000, 50));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, total_rules_full_usage_histogram,
                      ("Style.TotalLazyRules.FullUsage", 0, 100000, 50));
  switch (usage_) {
    case kUsageGe0:
      total_rules_histogram.Count(total_style_rules_);
      style_rules_needed_for_next_milestone_ = total_style_rules_ * .1;
      break;
    case kUsageGt10:
      style_rules_needed_for_next_milestone_ = total_style_rules_ * .25;
      break;
    case kUsageGt25:
      style_rules_needed_for_next_milestone_ = total_style_rules_ * .5;
      break;
    case kUsageGt50:
      style_rules_needed_for_next_milestone_ = total_style_rules_ * .75;
      break;
    case kUsageGt75:
      style_rules_needed_for_next_milestone_ = total_style_rules_ * .9;
      break;
    case kUsageGt90:
      style_rules_needed_for_next_milestone_ = total_style_rules_ - 1;
      break;
    case kUsageAll:
      total_rules_full_usage_histogram.Count(total_style_rules_);
      style_rules_needed_for_next_milestone_ = total_style_rules_;
      break;
  }

  usage_histogram.Count(usage_);
}

void CSSLazyParsingState::Trace(blink::Visitor* visitor) {
  visitor->Trace(owning_contents_);
  visitor->Trace(document_);
  visitor->Trace(context_);
}

}  // namespace blink
