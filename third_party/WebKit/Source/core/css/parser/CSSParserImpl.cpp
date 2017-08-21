// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserImpl.h"

#include <bitset>
#include <memory>
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/PropertyRegistry.h"
#include "core/css/StyleRuleImport.h"
#include "core/css/StyleRuleKeyframe.h"
#include "core/css/StyleRuleNamespace.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSAtRuleID.h"
#include "core/css/parser/CSSLazyParsingState.h"
#include "core/css/parser/CSSLazyPropertyParserImpl.h"
#include "core/css/parser/CSSParserObserver.h"
#include "core/css/parser/CSSParserObserverWrapper.h"
#include "core/css/parser/CSSParserSelector.h"
#include "core/css/parser/CSSParserTokenStream.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/parser/CSSSelectorParser.h"
#include "core/css/parser/CSSSupportsParser.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/css/parser/CSSVariableParser.h"
#include "core/css/parser/MediaQueryParser.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/Deprecation.h"
#include "core/frame/UseCounter.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

CSSParserImpl::CSSParserImpl(const CSSParserContext* context,
                             StyleSheetContents* style_sheet)
    : context_(context),
      style_sheet_(style_sheet),
      observer_wrapper_(nullptr) {}

MutableStylePropertySet::SetResult CSSParserImpl::ParseValue(
    MutableStylePropertySet* declaration,
    CSSPropertyID unresolved_property,
    const String& string,
    bool important,
    const CSSParserContext* context) {
  CSSParserImpl parser(context);
  StyleRule::RuleType rule_type = StyleRule::kStyle;
  if (declaration->CssParserMode() == kCSSViewportRuleMode)
    rule_type = StyleRule::kViewport;
  else if (declaration->CssParserMode() == kCSSFontFaceRuleMode)
    rule_type = StyleRule::kFontFace;
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  // TODO(shend): Use streams instead of ranges
  parser.ConsumeDeclarationValue(stream.MakeRangeToEOF(), unresolved_property,
                                 important, rule_type);
  bool did_parse = false;
  bool did_change = false;
  if (!parser.parsed_properties_.IsEmpty()) {
    did_parse = true;
    did_change = declaration->AddParsedProperties(parser.parsed_properties_);
  }
  return MutableStylePropertySet::SetResult{did_parse, did_change};
}

MutableStylePropertySet::SetResult CSSParserImpl::ParseVariableValue(
    MutableStylePropertySet* declaration,
    const AtomicString& property_name,
    const PropertyRegistry* registry,
    const String& value,
    bool important,
    const CSSParserContext* context,
    bool is_animation_tainted) {
  CSSParserImpl parser(context);
  CSSTokenizer tokenizer(value);
  CSSParserTokenStream stream(tokenizer);
  // TODO(shend): Use streams instead of ranges
  const CSSParserTokenRange range = stream.MakeRangeToEOF();
  parser.ConsumeVariableValue(range, property_name, important,
                              is_animation_tainted);
  bool did_parse = false;
  bool did_change = false;
  if (!parser.parsed_properties_.IsEmpty()) {
    const CSSCustomPropertyDeclaration* parsed_declaration =
        ToCSSCustomPropertyDeclaration(parser.parsed_properties_[0].Value());
    if (parsed_declaration->Value() && registry) {
      const PropertyRegistration* registration =
          registry->Registration(property_name);
      // TODO(timloh): This is a bit wasteful, we parse the registered property
      // to validate but throw away the result.
      if (registration &&
          !registration->Syntax().Parse(range, context, is_animation_tainted)) {
        return MutableStylePropertySet::SetResult{did_parse, did_change};
      }
    }
    did_parse = true;
    did_change = declaration->AddParsedProperties(parser.parsed_properties_);
  }
  return MutableStylePropertySet::SetResult{did_parse, did_change};
}

static inline void FilterProperties(
    bool important,
    const HeapVector<CSSProperty, 256>& input,
    HeapVector<CSSProperty, 256>& output,
    size_t& unused_entries,
    std::bitset<numCSSProperties>& seen_properties,
    HashSet<AtomicString>& seen_custom_properties) {
  // Add properties in reverse order so that highest priority definitions are
  // reached first. Duplicate definitions can then be ignored when found.
  for (size_t i = input.size(); i--;) {
    const CSSProperty& property = input[i];
    if (property.IsImportant() != important)
      continue;
    const unsigned property_id_index = property.Id() - firstCSSProperty;

    if (property.Id() == CSSPropertyVariable) {
      const AtomicString& name =
          ToCSSCustomPropertyDeclaration(property.Value())->GetName();
      if (seen_custom_properties.Contains(name))
        continue;
      seen_custom_properties.insert(name);
    } else if (property.Id() == CSSPropertyApplyAtRule) {
      // TODO(timloh): Do we need to do anything here?
    } else {
      if (seen_properties.test(property_id_index))
        continue;
      seen_properties.set(property_id_index);
    }
    output[--unused_entries] = property;
  }
}

static ImmutableStylePropertySet* CreateStylePropertySet(
    HeapVector<CSSProperty, 256>& parsed_properties,
    CSSParserMode mode) {
  std::bitset<numCSSProperties> seen_properties;
  size_t unused_entries = parsed_properties.size();
  HeapVector<CSSProperty, 256> results(unused_entries);
  HashSet<AtomicString> seen_custom_properties;

  FilterProperties(true, parsed_properties, results, unused_entries,
                   seen_properties, seen_custom_properties);
  FilterProperties(false, parsed_properties, results, unused_entries,
                   seen_properties, seen_custom_properties);

  ImmutableStylePropertySet* result = ImmutableStylePropertySet::Create(
      results.data() + unused_entries, results.size() - unused_entries, mode);
  parsed_properties.clear();
  return result;
}

ImmutableStylePropertySet* CSSParserImpl::ParseInlineStyleDeclaration(
    const String& string,
    Element* element) {
  Document& document = element->GetDocument();
  CSSParserContext* context = CSSParserContext::Create(
      document.ElementSheet().Contents()->ParserContext(), &document);
  CSSParserMode mode = element->IsHTMLElement() && !document.InQuirksMode()
                           ? kHTMLStandardMode
                           : kHTMLQuirksMode;
  context->SetMode(mode);
  CSSParserImpl parser(context, document.ElementSheet().Contents());
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle);
  return CreateStylePropertySet(parser.parsed_properties_, mode);
}

bool CSSParserImpl::ParseDeclarationList(MutableStylePropertySet* declaration,
                                         const String& string,
                                         const CSSParserContext* context) {
  CSSParserImpl parser(context);
  StyleRule::RuleType rule_type = StyleRule::kStyle;
  if (declaration->CssParserMode() == kCSSViewportRuleMode)
    rule_type = StyleRule::kViewport;
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  parser.ConsumeDeclarationList(stream, rule_type);
  if (parser.parsed_properties_.IsEmpty())
    return false;

  std::bitset<numCSSProperties> seen_properties;
  size_t unused_entries = parser.parsed_properties_.size();
  HeapVector<CSSProperty, 256> results(unused_entries);
  HashSet<AtomicString> seen_custom_properties;
  FilterProperties(true, parser.parsed_properties_, results, unused_entries,
                   seen_properties, seen_custom_properties);
  FilterProperties(false, parser.parsed_properties_, results, unused_entries,
                   seen_properties, seen_custom_properties);
  if (unused_entries)
    results.erase(0, unused_entries);
  return declaration->AddParsedProperties(results);
}

StyleRuleBase* CSSParserImpl::ParseRule(const String& string,
                                        const CSSParserContext* context,
                                        StyleSheetContents* style_sheet,
                                        AllowedRulesType allowed_rules) {
  CSSParserImpl parser(context, style_sheet);
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  // TODO(shend): Use streams instead of ranges
  CSSParserTokenRange range = stream.MakeRangeToEOF();
  range.ConsumeWhitespace();
  if (range.AtEnd())
    return nullptr;  // Parse error, empty rule
  StyleRuleBase* rule;
  if (range.Peek().GetType() == kAtKeywordToken)
    rule = parser.ConsumeAtRule(range, allowed_rules);
  else
    rule = parser.ConsumeQualifiedRule(range, allowed_rules);
  if (!rule)
    return nullptr;  // Parse error, failed to consume rule
  range.ConsumeWhitespace();
  if (!rule || !range.AtEnd())
    return nullptr;  // Parse error, trailing garbage
  return rule;
}

void CSSParserImpl::ParseStyleSheet(const String& string,
                                    const CSSParserContext* context,
                                    StyleSheetContents* style_sheet,
                                    bool defer_property_parsing) {
  TRACE_EVENT_BEGIN2("blink,blink_style", "CSSParserImpl::parseStyleSheet",
                     "baseUrl", context->BaseURL().GetString().Utf8(), "mode",
                     context->Mode());

  TRACE_EVENT_BEGIN0("blink,blink_style",
                     "CSSParserImpl::parseStyleSheet.tokenize");
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  // TODO(shend): Use streams instead of ranges. Streams will ruin
  // tokenize/parse metrics as we will be tokenizing on demand.
  const CSSParserTokenRange range = stream.MakeRangeToEOF();
  TRACE_EVENT_END0("blink,blink_style",
                   "CSSParserImpl::parseStyleSheet.tokenize");

  TRACE_EVENT_BEGIN0("blink,blink_style",
                     "CSSParserImpl::parseStyleSheet.parse");
  CSSParserImpl parser(context, style_sheet);
  if (defer_property_parsing) {
    parser.lazy_state_ = new CSSLazyParsingState(
        context, tokenizer.TakeEscapedStrings(), string, parser.style_sheet_);
  }
  bool first_rule_valid = parser.ConsumeRuleList(
      range, kTopLevelRuleList, [&style_sheet](StyleRuleBase* rule) {
        if (rule->IsCharsetRule())
          return;
        style_sheet->ParserAppendRule(rule);
      });
  style_sheet->SetHasSyntacticallyValidCSSHeader(first_rule_valid);
  if (parser.lazy_state_)
    parser.lazy_state_->FinishInitialParsing();
  TRACE_EVENT_END0("blink,blink_style", "CSSParserImpl::parseStyleSheet.parse");

  TRACE_EVENT_END2("blink,blink_style", "CSSParserImpl::parseStyleSheet",
                   "tokenCount", tokenizer.TokenCount(), "length",
                   string.length());
}

CSSSelectorList CSSParserImpl::ParsePageSelector(
    CSSParserTokenRange range,
    StyleSheetContents* style_sheet) {
  // We only support a small subset of the css-page spec.
  range.ConsumeWhitespace();
  AtomicString type_selector;
  if (range.Peek().GetType() == kIdentToken)
    type_selector = range.Consume().Value().ToAtomicString();

  AtomicString pseudo;
  if (range.Peek().GetType() == kColonToken) {
    range.Consume();
    if (range.Peek().GetType() != kIdentToken)
      return CSSSelectorList();
    pseudo = range.Consume().Value().ToAtomicString();
  }

  range.ConsumeWhitespace();
  if (!range.AtEnd())
    return CSSSelectorList();  // Parse error; extra tokens in @page selector

  std::unique_ptr<CSSParserSelector> selector;
  if (!type_selector.IsNull() && pseudo.IsNull()) {
    selector = CSSParserSelector::Create(QualifiedName(
        g_null_atom, type_selector, style_sheet->DefaultNamespace()));
  } else {
    selector = CSSParserSelector::Create();
    if (!pseudo.IsNull()) {
      selector->SetMatch(CSSSelector::kPagePseudoClass);
      selector->UpdatePseudoPage(pseudo.LowerASCII());
      if (selector->GetPseudoType() == CSSSelector::kPseudoUnknown)
        return CSSSelectorList();
    }
    if (!type_selector.IsNull()) {
      selector->PrependTagSelector(QualifiedName(
          g_null_atom, type_selector, style_sheet->DefaultNamespace()));
    }
  }

  selector->SetForPage();
  Vector<std::unique_ptr<CSSParserSelector>> selector_vector;
  selector_vector.push_back(std::move(selector));
  CSSSelectorList selector_list =
      CSSSelectorList::AdoptSelectorVector(selector_vector);
  return selector_list;
}

ImmutableStylePropertySet* CSSParserImpl::ParseCustomPropertySet(
    CSSParserTokenRange range) {
  range.ConsumeWhitespace();
  if (range.Peek().GetType() != kLeftBraceToken)
    return nullptr;
  CSSParserTokenRange block = range.ConsumeBlock();
  range.ConsumeWhitespace();
  if (!range.AtEnd())
    return nullptr;
  CSSParserImpl parser(StrictCSSParserContext());
  parser.ConsumeDeclarationList(block, StyleRule::kStyle);

  // Drop nested @apply rules. Seems nicer to do this here instead of making
  // a different StyleRule type
  for (size_t i = parser.parsed_properties_.size(); i--;) {
    if (parser.parsed_properties_[i].Id() == CSSPropertyApplyAtRule)
      parser.parsed_properties_.erase(i);
  }

  return CreateStylePropertySet(parser.parsed_properties_, kHTMLStandardMode);
}

std::unique_ptr<Vector<double>> CSSParserImpl::ParseKeyframeKeyList(
    const String& key_list) {
  CSSTokenizer tokenizer(key_list);
  // TODO(shend): Use streams instead of ranges
  return ConsumeKeyframeKeyList(
      CSSParserTokenStream(tokenizer).MakeRangeToEOF());
}

bool CSSParserImpl::SupportsDeclaration(CSSParserTokenRange& range) {
  DCHECK(parsed_properties_.IsEmpty());
  ConsumeDeclaration(range, StyleRule::kStyle);
  bool result = !parsed_properties_.IsEmpty();
  parsed_properties_.clear();
  return result;
}

void CSSParserImpl::ParseDeclarationListForInspector(
    const String& declaration,
    const CSSParserContext* context,
    CSSParserObserver& observer) {
  CSSParserImpl parser(context);
  CSSParserObserverWrapper wrapper(observer);
  parser.observer_wrapper_ = &wrapper;
  CSSTokenizer tokenizer(declaration, wrapper);
  observer.StartRuleHeader(StyleRule::kStyle, 0);
  observer.EndRuleHeader(1);
  CSSParserTokenStream stream(tokenizer);
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle);
}

void CSSParserImpl::ParseStyleSheetForInspector(const String& string,
                                                const CSSParserContext* context,
                                                StyleSheetContents* style_sheet,
                                                CSSParserObserver& observer) {
  CSSParserImpl parser(context, style_sheet);
  CSSParserObserverWrapper wrapper(observer);
  parser.observer_wrapper_ = &wrapper;
  CSSTokenizer tokenizer(string, wrapper);
  // TODO(shend): Use streams instead of ranges
  bool first_rule_valid =
      parser.ConsumeRuleList(tokenizer.TokenRange(), kTopLevelRuleList,
                             [&style_sheet](StyleRuleBase* rule) {
                               if (rule->IsCharsetRule())
                                 return;
                               style_sheet->ParserAppendRule(rule);
                             });
  style_sheet->SetHasSyntacticallyValidCSSHeader(first_rule_valid);
}

StylePropertySet* CSSParserImpl::ParseDeclarationListForLazyStyle(
    CSSParserTokenRange block,
    const CSSParserContext* context) {
  CSSParserImpl parser(context);
  parser.ConsumeDeclarationList(std::move(block), StyleRule::kStyle);
  return CreateStylePropertySet(parser.parsed_properties_, context->Mode());
}

static CSSParserImpl::AllowedRulesType ComputeNewAllowedRules(
    CSSParserImpl::AllowedRulesType allowed_rules,
    StyleRuleBase* rule) {
  if (!rule || allowed_rules == CSSParserImpl::kKeyframeRules ||
      allowed_rules == CSSParserImpl::kNoRules)
    return allowed_rules;
  DCHECK_LE(allowed_rules, CSSParserImpl::kRegularRules);
  if (rule->IsCharsetRule() || rule->IsImportRule())
    return CSSParserImpl::kAllowImportRules;
  if (rule->IsNamespaceRule())
    return CSSParserImpl::kAllowNamespaceRules;
  return CSSParserImpl::kRegularRules;
}

template <typename T>
bool CSSParserImpl::ConsumeRuleList(CSSParserTokenRange range,
                                    RuleListType rule_list_type,
                                    const T callback) {
  AllowedRulesType allowed_rules = kRegularRules;
  switch (rule_list_type) {
    case kTopLevelRuleList:
      allowed_rules = kAllowCharsetRules;
      break;
    case kRegularRuleList:
      allowed_rules = kRegularRules;
      break;
    case kKeyframesRuleList:
      allowed_rules = kKeyframeRules;
      break;
    default:
      NOTREACHED();
  }

  bool seen_rule = false;
  bool first_rule_valid = false;
  while (!range.AtEnd()) {
    StyleRuleBase* rule;
    switch (range.Peek().GetType()) {
      case kWhitespaceToken:
        range.ConsumeWhitespace();
        continue;
      case kAtKeywordToken:
        rule = ConsumeAtRule(range, allowed_rules);
        break;
      case kCDOToken:
      case kCDCToken:
        if (rule_list_type == kTopLevelRuleList) {
          range.Consume();
          continue;
        }
      // fallthrough
      default:
        rule = ConsumeQualifiedRule(range, allowed_rules);
        break;
    }
    if (!seen_rule) {
      seen_rule = true;
      first_rule_valid = rule;
    }
    if (rule) {
      allowed_rules = ComputeNewAllowedRules(allowed_rules, rule);
      callback(rule);
    }
  }

  return first_rule_valid;
}

StyleRuleBase* CSSParserImpl::ConsumeAtRule(CSSParserTokenStream& stream,
                                            AllowedRulesType allowed_rules) {
  DCHECK_EQ(stream.Peek().GetType(), kAtKeywordToken);
  const StringView name = stream.ConsumeIncludingWhitespace().Value();
  const CSSAtRuleID id = CssAtRuleID(name);

  const auto prelude_start = stream.Position();
  while (!stream.AtEnd() &&
         stream.UncheckedPeek().GetType() != kLeftBraceToken &&
         stream.UncheckedPeek().GetType() != kSemicolonToken)
    stream.UncheckedConsumeComponentValue();

  CSSParserTokenRange prelude =
      stream.MakeSubRange(prelude_start, stream.Position());
  if (id != kCSSAtRuleInvalid && context_->IsUseCounterRecordingEnabled())
    CountAtRule(context_, id);

  if (stream.AtEnd() || stream.Peek().GetType() == kSemicolonToken) {
    stream.Consume();
    if (allowed_rules == kAllowCharsetRules && id == kCSSAtRuleCharset)
      return ConsumeCharsetRule(prelude);
    if (allowed_rules <= kAllowImportRules && id == kCSSAtRuleImport)
      return ConsumeImportRule(prelude);
    if (allowed_rules <= kAllowNamespaceRules && id == kCSSAtRuleNamespace)
      return ConsumeNamespaceRule(prelude);
    if (allowed_rules == kApplyRules && id == kCSSAtRuleApply) {
      ConsumeApplyRule(prelude);
      return nullptr;  // ConsumeApplyRule just updates parsed_properties_
    }
    return nullptr;  // Parse error, unrecognised at-rule without block
  }

  // TODO(shend): Use streams instead of ranges
  CSSParserTokenRange range = stream.MakeRangeToEOF();
  CSSParserTokenRange block = range.ConsumeBlock();
  stream.UpdatePositionFromRange(range);

  if (allowed_rules == kKeyframeRules)
    return nullptr;  // Parse error, no at-rules supported inside @keyframes
  if (allowed_rules == kNoRules || allowed_rules == kApplyRules)
    return nullptr;  // Parse error, no at-rules with blocks supported inside
                     // declaration lists

  DCHECK_LE(allowed_rules, kRegularRules);

  switch (id) {
    case kCSSAtRuleMedia:
      return ConsumeMediaRule(prelude, block);
    case kCSSAtRuleSupports:
      return ConsumeSupportsRule(prelude, block);
    case kCSSAtRuleViewport:
      return ConsumeViewportRule(prelude, block);
    case kCSSAtRuleFontFace:
      return ConsumeFontFaceRule(prelude, block);
    case kCSSAtRuleWebkitKeyframes:
      return ConsumeKeyframesRule(true, prelude, block);
    case kCSSAtRuleKeyframes:
      return ConsumeKeyframesRule(false, prelude, block);
    case kCSSAtRulePage:
      return ConsumePageRule(prelude, block);
    default:
      return nullptr;  // Parse error, unrecognised at-rule with block
  }
}

StyleRuleBase* CSSParserImpl::ConsumeAtRule(CSSParserTokenRange& range,
                                            AllowedRulesType allowed_rules) {
  DCHECK_EQ(range.Peek().GetType(), kAtKeywordToken);
  const StringView name = range.ConsumeIncludingWhitespace().Value();
  const CSSParserToken* prelude_start = &range.Peek();
  while (!range.AtEnd() && range.Peek().GetType() != kLeftBraceToken &&
         range.Peek().GetType() != kSemicolonToken)
    range.ConsumeComponentValue();

  CSSParserTokenRange prelude =
      range.MakeSubRange(prelude_start, &range.Peek());
  CSSAtRuleID id = CssAtRuleID(name);
  if (id != kCSSAtRuleInvalid && context_->IsUseCounterRecordingEnabled())
    CountAtRule(context_, id);

  if (range.AtEnd() || range.Peek().GetType() == kSemicolonToken) {
    range.Consume();
    if (allowed_rules == kAllowCharsetRules && id == kCSSAtRuleCharset)
      return ConsumeCharsetRule(prelude);
    if (allowed_rules <= kAllowImportRules && id == kCSSAtRuleImport)
      return ConsumeImportRule(prelude);
    if (allowed_rules <= kAllowNamespaceRules && id == kCSSAtRuleNamespace)
      return ConsumeNamespaceRule(prelude);
    if (allowed_rules == kApplyRules && id == kCSSAtRuleApply) {
      ConsumeApplyRule(prelude);
      return nullptr;  // ConsumeApplyRule just updates parsed_properties_
    }
    return nullptr;  // Parse error, unrecognised at-rule without block
  }

  CSSParserTokenRange block = range.ConsumeBlock();
  if (allowed_rules == kKeyframeRules)
    return nullptr;  // Parse error, no at-rules supported inside @keyframes
  if (allowed_rules == kNoRules || allowed_rules == kApplyRules)
    return nullptr;  // Parse error, no at-rules with blocks supported inside
                     // declaration lists

  DCHECK_LE(allowed_rules, kRegularRules);

  switch (id) {
    case kCSSAtRuleMedia:
      return ConsumeMediaRule(prelude, block);
    case kCSSAtRuleSupports:
      return ConsumeSupportsRule(prelude, block);
    case kCSSAtRuleViewport:
      return ConsumeViewportRule(prelude, block);
    case kCSSAtRuleFontFace:
      return ConsumeFontFaceRule(prelude, block);
    case kCSSAtRuleWebkitKeyframes:
      return ConsumeKeyframesRule(true, prelude, block);
    case kCSSAtRuleKeyframes:
      return ConsumeKeyframesRule(false, prelude, block);
    case kCSSAtRulePage:
      return ConsumePageRule(prelude, block);
    default:
      return nullptr;  // Parse error, unrecognised at-rule with block
  }
}

StyleRuleBase* CSSParserImpl::ConsumeQualifiedRule(
    CSSParserTokenRange& range,
    AllowedRulesType allowed_rules) {
  const CSSParserToken* prelude_start = &range.Peek();
  while (!range.AtEnd() && range.Peek().GetType() != kLeftBraceToken)
    range.ConsumeComponentValue();

  if (range.AtEnd())
    return nullptr;  // Parse error, EOF instead of qualified rule block

  CSSParserTokenRange prelude =
      range.MakeSubRange(prelude_start, &range.Peek());
  CSSParserTokenRange block = range.ConsumeBlock();

  if (allowed_rules <= kRegularRules)
    return ConsumeStyleRule(prelude, block);
  if (allowed_rules == kKeyframeRules)
    return ConsumeKeyframeStyleRule(prelude, block);

  NOTREACHED();
  return nullptr;
}

// This may still consume tokens if it fails
static AtomicString ConsumeStringOrURI(CSSParserTokenRange& range) {
  const CSSParserToken& token = range.Peek();

  if (token.GetType() == kStringToken || token.GetType() == kUrlToken)
    return range.ConsumeIncludingWhitespace().Value().ToAtomicString();

  if (token.GetType() != kFunctionToken ||
      !EqualIgnoringASCIICase(token.Value(), "url"))
    return AtomicString();

  CSSParserTokenRange contents = range.ConsumeBlock();
  const CSSParserToken& uri = contents.ConsumeIncludingWhitespace();
  if (uri.GetType() == kBadStringToken || !contents.AtEnd())
    return AtomicString();
  DCHECK_EQ(uri.GetType(), kStringToken);
  return uri.Value().ToAtomicString();
}

StyleRuleCharset* CSSParserImpl::ConsumeCharsetRule(
    CSSParserTokenRange prelude) {
  const CSSParserToken& string = prelude.ConsumeIncludingWhitespace();
  if (string.GetType() != kStringToken || !prelude.AtEnd())
    return nullptr;  // Parse error, expected a single string
  return StyleRuleCharset::Create();
}

StyleRuleImport* CSSParserImpl::ConsumeImportRule(CSSParserTokenRange prelude) {
  AtomicString uri(ConsumeStringOrURI(prelude));
  if (uri.IsNull())
    return nullptr;  // Parse error, expected string or URI

  if (observer_wrapper_) {
    unsigned end_offset = observer_wrapper_->EndOffset(prelude);
    observer_wrapper_->Observer().StartRuleHeader(
        StyleRule::kImport, observer_wrapper_->StartOffset(prelude));
    observer_wrapper_->Observer().EndRuleHeader(end_offset);
    observer_wrapper_->Observer().StartRuleBody(end_offset);
    observer_wrapper_->Observer().EndRuleBody(end_offset);
  }

  return StyleRuleImport::Create(uri,
                                 MediaQueryParser::ParseMediaQuerySet(prelude));
}

StyleRuleNamespace* CSSParserImpl::ConsumeNamespaceRule(
    CSSParserTokenRange prelude) {
  AtomicString namespace_prefix;
  if (prelude.Peek().GetType() == kIdentToken)
    namespace_prefix =
        prelude.ConsumeIncludingWhitespace().Value().ToAtomicString();

  AtomicString uri(ConsumeStringOrURI(prelude));
  if (uri.IsNull() || !prelude.AtEnd())
    return nullptr;  // Parse error, expected string or URI

  return StyleRuleNamespace::Create(namespace_prefix, uri);
}

StyleRuleMedia* CSSParserImpl::ConsumeMediaRule(CSSParserTokenRange prelude,
                                                CSSParserTokenRange block) {
  HeapVector<Member<StyleRuleBase>> rules;

  if (observer_wrapper_) {
    observer_wrapper_->Observer().StartRuleHeader(
        StyleRule::kMedia, observer_wrapper_->StartOffset(prelude));
    observer_wrapper_->Observer().EndRuleHeader(
        observer_wrapper_->EndOffset(prelude));
    observer_wrapper_->Observer().StartRuleBody(
        observer_wrapper_->PreviousTokenStartOffset(block));
  }

  if (style_sheet_)
    style_sheet_->SetHasMediaQueries();

  ConsumeRuleList(block, kRegularRuleList,
                  [&rules](StyleRuleBase* rule) { rules.push_back(rule); });

  if (observer_wrapper_)
    observer_wrapper_->Observer().EndRuleBody(
        observer_wrapper_->EndOffset(block));

  return StyleRuleMedia::Create(MediaQueryParser::ParseMediaQuerySet(prelude),
                                rules);
}

StyleRuleSupports* CSSParserImpl::ConsumeSupportsRule(
    CSSParserTokenRange prelude,
    CSSParserTokenRange block) {
  CSSSupportsParser::SupportsResult supported =
      CSSSupportsParser::SupportsCondition(prelude, *this,
                                           CSSSupportsParser::kForAtRule);
  if (supported == CSSSupportsParser::kInvalid)
    return nullptr;  // Parse error, invalid @supports condition

  if (observer_wrapper_) {
    observer_wrapper_->Observer().StartRuleHeader(
        StyleRule::kSupports, observer_wrapper_->StartOffset(prelude));
    observer_wrapper_->Observer().EndRuleHeader(
        observer_wrapper_->EndOffset(prelude));
    observer_wrapper_->Observer().StartRuleBody(
        observer_wrapper_->PreviousTokenStartOffset(block));
  }

  HeapVector<Member<StyleRuleBase>> rules;
  ConsumeRuleList(block, kRegularRuleList,
                  [&rules](StyleRuleBase* rule) { rules.push_back(rule); });

  if (observer_wrapper_)
    observer_wrapper_->Observer().EndRuleBody(
        observer_wrapper_->EndOffset(block));

  return StyleRuleSupports::Create(prelude.Serialize().StripWhiteSpace(),
                                   supported, rules);
}

StyleRuleViewport* CSSParserImpl::ConsumeViewportRule(
    CSSParserTokenRange prelude,
    CSSParserTokenRange block) {
  // Allow @viewport rules from UA stylesheets even if the feature is disabled.
  if (!RuntimeEnabledFeatures::CSSViewportEnabled() &&
      !IsUASheetBehavior(context_->Mode()))
    return nullptr;

  if (!prelude.AtEnd())
    return nullptr;  // Parser error; @viewport prelude should be empty

  if (observer_wrapper_) {
    unsigned end_offset = observer_wrapper_->EndOffset(prelude);
    observer_wrapper_->Observer().StartRuleHeader(
        StyleRule::kViewport, observer_wrapper_->StartOffset(prelude));
    observer_wrapper_->Observer().EndRuleHeader(end_offset);
    observer_wrapper_->Observer().StartRuleBody(end_offset);
    observer_wrapper_->Observer().EndRuleBody(end_offset);
  }

  if (style_sheet_)
    style_sheet_->SetHasViewportRule();

  ConsumeDeclarationList(block, StyleRule::kViewport);
  return StyleRuleViewport::Create(
      CreateStylePropertySet(parsed_properties_, kCSSViewportRuleMode));
}

StyleRuleFontFace* CSSParserImpl::ConsumeFontFaceRule(
    CSSParserTokenRange prelude,
    CSSParserTokenRange block) {
  if (!prelude.AtEnd())
    return nullptr;  // Parse error; @font-face prelude should be empty

  if (observer_wrapper_) {
    unsigned end_offset = observer_wrapper_->EndOffset(prelude);
    observer_wrapper_->Observer().StartRuleHeader(
        StyleRule::kFontFace, observer_wrapper_->StartOffset(prelude));
    observer_wrapper_->Observer().EndRuleHeader(end_offset);
    observer_wrapper_->Observer().StartRuleBody(end_offset);
    observer_wrapper_->Observer().EndRuleBody(end_offset);
  }

  if (style_sheet_)
    style_sheet_->SetHasFontFaceRule();

  ConsumeDeclarationList(block, StyleRule::kFontFace);
  return StyleRuleFontFace::Create(
      CreateStylePropertySet(parsed_properties_, kCSSFontFaceRuleMode));
}

StyleRuleKeyframes* CSSParserImpl::ConsumeKeyframesRule(
    bool webkit_prefixed,
    CSSParserTokenRange prelude,
    CSSParserTokenRange block) {
  CSSParserTokenRange range_copy = prelude;  // For inspector callbacks
  const CSSParserToken& name_token = prelude.ConsumeIncludingWhitespace();
  if (!prelude.AtEnd())
    return nullptr;  // Parse error; expected single non-whitespace token in
                     // @keyframes header

  String name;
  if (name_token.GetType() == kIdentToken) {
    name = name_token.Value().ToString();
  } else if (name_token.GetType() == kStringToken && webkit_prefixed) {
    context_->Count(WebFeature::kQuotedKeyframesRule);
    name = name_token.Value().ToString();
  } else {
    return nullptr;  // Parse error; expected ident token in @keyframes header
  }

  if (observer_wrapper_) {
    observer_wrapper_->Observer().StartRuleHeader(
        StyleRule::kKeyframes, observer_wrapper_->StartOffset(range_copy));
    observer_wrapper_->Observer().EndRuleHeader(
        observer_wrapper_->EndOffset(prelude));
    observer_wrapper_->Observer().StartRuleBody(
        observer_wrapper_->PreviousTokenStartOffset(block));
    observer_wrapper_->Observer().EndRuleBody(
        observer_wrapper_->EndOffset(block));
  }

  StyleRuleKeyframes* keyframe_rule = StyleRuleKeyframes::Create();
  ConsumeRuleList(
      block, kKeyframesRuleList, [keyframe_rule](StyleRuleBase* keyframe) {
        keyframe_rule->ParserAppendKeyframe(ToStyleRuleKeyframe(keyframe));
      });
  keyframe_rule->SetName(name);
  keyframe_rule->SetVendorPrefixed(webkit_prefixed);
  return keyframe_rule;
}

StyleRulePage* CSSParserImpl::ConsumePageRule(CSSParserTokenRange prelude,
                                              CSSParserTokenRange block) {
  CSSSelectorList selector_list = ParsePageSelector(prelude, style_sheet_);
  if (!selector_list.IsValid())
    return nullptr;  // Parse error, invalid @page selector

  if (observer_wrapper_) {
    unsigned end_offset = observer_wrapper_->EndOffset(prelude);
    observer_wrapper_->Observer().StartRuleHeader(
        StyleRule::kPage, observer_wrapper_->StartOffset(prelude));
    observer_wrapper_->Observer().EndRuleHeader(end_offset);
  }

  ConsumeDeclarationList(block, StyleRule::kStyle);

  return StyleRulePage::Create(
      std::move(selector_list),
      CreateStylePropertySet(parsed_properties_, context_->Mode()));
}

void CSSParserImpl::ConsumeApplyRule(CSSParserTokenRange prelude) {
  DCHECK(RuntimeEnabledFeatures::CSSApplyAtRulesEnabled());

  const CSSParserToken& ident = prelude.ConsumeIncludingWhitespace();
  if (!prelude.AtEnd() || !CSSVariableParser::IsValidVariableName(ident))
    return;  // Parse error, expected a single custom property name
  parsed_properties_.push_back(CSSProperty(
      CSSPropertyApplyAtRule,
      *CSSCustomIdentValue::Create(ident.Value().ToAtomicString())));
}

StyleRuleKeyframe* CSSParserImpl::ConsumeKeyframeStyleRule(
    CSSParserTokenRange prelude,
    CSSParserTokenRange block) {
  std::unique_ptr<Vector<double>> key_list = ConsumeKeyframeKeyList(prelude);
  if (!key_list)
    return nullptr;

  if (observer_wrapper_) {
    observer_wrapper_->Observer().StartRuleHeader(
        StyleRule::kKeyframe, observer_wrapper_->StartOffset(prelude));
    observer_wrapper_->Observer().EndRuleHeader(
        observer_wrapper_->EndOffset(prelude));
  }

  ConsumeDeclarationList(block, StyleRule::kKeyframe);
  return StyleRuleKeyframe::Create(
      std::move(key_list),
      CreateStylePropertySet(parsed_properties_, context_->Mode()));
}

static void ObserveSelectors(CSSParserObserverWrapper& wrapper,
                             CSSParserTokenRange selectors) {
  // This is easier than hooking into the CSSSelectorParser
  selectors.ConsumeWhitespace();
  CSSParserTokenRange original_range = selectors;
  wrapper.Observer().StartRuleHeader(StyleRule::kStyle,
                                     wrapper.StartOffset(original_range));

  while (!selectors.AtEnd()) {
    const CSSParserToken* selector_start = &selectors.Peek();
    while (!selectors.AtEnd() && selectors.Peek().GetType() != kCommaToken)
      selectors.ConsumeComponentValue();
    CSSParserTokenRange selector =
        selectors.MakeSubRange(selector_start, &selectors.Peek());
    selectors.ConsumeIncludingWhitespace();

    wrapper.Observer().ObserveSelector(wrapper.StartOffset(selector),
                                       wrapper.EndOffset(selector));
  }

  wrapper.Observer().EndRuleHeader(wrapper.EndOffset(original_range));
}

StyleRule* CSSParserImpl::ConsumeStyleRule(CSSParserTokenRange prelude,
                                           CSSParserTokenRange block) {
  CSSSelectorList selector_list =
      CSSSelectorParser::ParseSelector(prelude, context_, style_sheet_);
  if (!selector_list.IsValid())
    return nullptr;  // Parse error, invalid selector list

  // TODO(csharrison): How should we lazily parse css that needs the observer?
  if (observer_wrapper_) {
    ObserveSelectors(*observer_wrapper_, prelude);
  } else if (lazy_state_ && !lazy_state_->IsEmptyBlock(block)) {
    DCHECK(style_sheet_);
    return StyleRule::CreateLazy(std::move(selector_list),
                                 lazy_state_->CreateLazyParser(block));
  }
  ConsumeDeclarationList(block, StyleRule::kStyle);

  return StyleRule::Create(
      std::move(selector_list),
      CreateStylePropertySet(parsed_properties_, context_->Mode()));
}

void CSSParserImpl::ConsumeDeclarationList(CSSParserTokenStream& stream,
                                           StyleRule::RuleType rule_type) {
  DCHECK(parsed_properties_.IsEmpty());

  bool use_observer = observer_wrapper_ && (rule_type == StyleRule::kStyle ||
                                            rule_type == StyleRule::kKeyframe);
  if (use_observer) {
    observer_wrapper_->Observer().StartRuleBody(
        observer_wrapper_->PreviousTokenStartOffset(stream));
    observer_wrapper_->SkipCommentsBefore(stream, true);
  }

  while (!stream.AtEnd()) {
    switch (stream.UncheckedPeek().GetType()) {
      case kWhitespaceToken:
      case kSemicolonToken:
        stream.UncheckedConsume();
        break;
      case kIdentToken: {
        if (use_observer)
          observer_wrapper_->YieldCommentsBefore(stream);

        const auto declaration_start = stream.Position();
        while (!stream.AtEnd() &&
               stream.UncheckedPeek().GetType() != kSemicolonToken)
          stream.UncheckedConsumeComponentValue();

        // TODO(shend): Use streams instead of ranges
        ConsumeDeclaration(
            stream.MakeSubRange(declaration_start, stream.Position()),
            rule_type);

        if (use_observer)
          observer_wrapper_->SkipCommentsBefore(stream, false);
        break;
      }
      case kAtKeywordToken: {
        AllowedRulesType allowed_rules =
            rule_type == StyleRule::kStyle &&
                    RuntimeEnabledFeatures::CSSApplyAtRulesEnabled()
                ? kApplyRules
                : kNoRules;

        StyleRuleBase* rule = ConsumeAtRule(stream, allowed_rules);
        DCHECK(!rule);
        break;
      }
      default:  // Parse error, unexpected token in declaration list
        while (!stream.AtEnd() &&
               stream.UncheckedPeek().GetType() != kSemicolonToken)
          stream.UncheckedConsumeComponentValue();
        break;
    }
  }

  // Yield remaining comments
  if (use_observer) {
    observer_wrapper_->YieldCommentsBefore(stream);
    observer_wrapper_->Observer().EndRuleBody(
        observer_wrapper_->EndOffset(stream));
  }
}

void CSSParserImpl::ConsumeDeclarationList(CSSParserTokenRange range,
                                           StyleRule::RuleType rule_type) {
  DCHECK(parsed_properties_.IsEmpty());

  bool use_observer = observer_wrapper_ && (rule_type == StyleRule::kStyle ||
                                            rule_type == StyleRule::kKeyframe);
  if (use_observer) {
    observer_wrapper_->Observer().StartRuleBody(
        observer_wrapper_->PreviousTokenStartOffset(range));
    observer_wrapper_->SkipCommentsBefore(range, true);
  }

  while (!range.AtEnd()) {
    switch (range.Peek().GetType()) {
      case kWhitespaceToken:
      case kSemicolonToken:
        range.Consume();
        break;
      case kIdentToken: {
        const CSSParserToken* declaration_start = &range.Peek();

        if (use_observer)
          observer_wrapper_->YieldCommentsBefore(range);

        while (!range.AtEnd() && range.Peek().GetType() != kSemicolonToken)
          range.ConsumeComponentValue();

        ConsumeDeclaration(range.MakeSubRange(declaration_start, &range.Peek()),
                           rule_type);

        if (use_observer)
          observer_wrapper_->SkipCommentsBefore(range, false);
        break;
      }
      case kAtKeywordToken: {
        AllowedRulesType allowed_rules =
            rule_type == StyleRule::kStyle &&
                    RuntimeEnabledFeatures::CSSApplyAtRulesEnabled()
                ? kApplyRules
                : kNoRules;
        StyleRuleBase* rule = ConsumeAtRule(range, allowed_rules);
        DCHECK(!rule);
        break;
      }
      default:  // Parse error, unexpected token in declaration list
        while (!range.AtEnd() && range.Peek().GetType() != kSemicolonToken)
          range.ConsumeComponentValue();
        break;
    }
  }

  // Yield remaining comments
  if (use_observer) {
    observer_wrapper_->YieldCommentsBefore(range);
    observer_wrapper_->Observer().EndRuleBody(
        observer_wrapper_->EndOffset(range));
  }
}

void CSSParserImpl::ConsumeDeclaration(CSSParserTokenRange range,
                                       StyleRule::RuleType rule_type) {
  CSSParserTokenRange range_copy = range;  // For inspector callbacks

  DCHECK_EQ(range.Peek().GetType(), kIdentToken);
  const CSSParserToken& token = range.ConsumeIncludingWhitespace();
  CSSPropertyID unresolved_property = token.ParseAsUnresolvedCSSPropertyID();
  if (range.Consume().GetType() != kColonToken)
    return;  // Parse error

  bool important = false;
  const CSSParserToken* declaration_value_end = range.end();
  const CSSParserToken* last = range.end() - 1;
  while (last->GetType() == kWhitespaceToken)
    --last;
  if (last->GetType() == kIdentToken &&
      EqualIgnoringASCIICase(last->Value(), "important")) {
    --last;
    while (last->GetType() == kWhitespaceToken)
      --last;
    if (last->GetType() == kDelimiterToken && last->Delimiter() == '!') {
      important = true;
      declaration_value_end = last;
    }
  }

  if (important &&
      (rule_type == StyleRule::kFontFace || rule_type == StyleRule::kKeyframe))
    return;

  size_t properties_count = parsed_properties_.size();

  if (unresolved_property == CSSPropertyVariable) {
    if (rule_type != StyleRule::kStyle && rule_type != StyleRule::kKeyframe)
      return;
    AtomicString variable_name = token.Value().ToAtomicString();
    bool is_animation_tainted = rule_type == StyleRule::kKeyframe;
    ConsumeVariableValue(
        range.MakeSubRange(&range.Peek(), declaration_value_end), variable_name,
        important, is_animation_tainted);
  } else if (unresolved_property != CSSPropertyInvalid) {
    if (style_sheet_ && style_sheet_->SingleOwnerDocument())
      Deprecation::WarnOnDeprecatedProperties(
          style_sheet_->SingleOwnerDocument()->GetFrame(), unresolved_property);
    ConsumeDeclarationValue(
        range.MakeSubRange(&range.Peek(), declaration_value_end),
        unresolved_property, important, rule_type);
  }

  if (observer_wrapper_ &&
      (rule_type == StyleRule::kStyle || rule_type == StyleRule::kKeyframe)) {
    observer_wrapper_->Observer().ObserveProperty(
        observer_wrapper_->StartOffset(range_copy),
        observer_wrapper_->EndOffset(range_copy), important,
        parsed_properties_.size() != properties_count);
  }
}

void CSSParserImpl::ConsumeVariableValue(CSSParserTokenRange range,
                                         const AtomicString& variable_name,
                                         bool important,
                                         bool is_animation_tainted) {
  if (CSSCustomPropertyDeclaration* value =
          CSSVariableParser::ParseDeclarationValue(variable_name, range,
                                                   is_animation_tainted)) {
    parsed_properties_.push_back(
        CSSProperty(CSSPropertyVariable, *value, important));
    context_->Count(context_->Mode(), CSSPropertyVariable);
  }
}

void CSSParserImpl::ConsumeDeclarationValue(CSSParserTokenRange range,
                                            CSSPropertyID unresolved_property,
                                            bool important,
                                            StyleRule::RuleType rule_type) {
  CSSPropertyParser::ParseValue(unresolved_property, important, range, context_,
                                parsed_properties_, rule_type);
}

std::unique_ptr<Vector<double>> CSSParserImpl::ConsumeKeyframeKeyList(
    CSSParserTokenRange range) {
  std::unique_ptr<Vector<double>> result = WTF::WrapUnique(new Vector<double>);
  while (true) {
    range.ConsumeWhitespace();
    const CSSParserToken& token = range.ConsumeIncludingWhitespace();
    if (token.GetType() == kPercentageToken && token.NumericValue() >= 0 &&
        token.NumericValue() <= 100)
      result->push_back(token.NumericValue() / 100);
    else if (token.GetType() == kIdentToken &&
             EqualIgnoringASCIICase(token.Value(), "from"))
      result->push_back(0);
    else if (token.GetType() == kIdentToken &&
             EqualIgnoringASCIICase(token.Value(), "to"))
      result->push_back(1);
    else
      return nullptr;  // Parser error, invalid value in keyframe selector
    if (range.AtEnd())
      return result;
    if (range.Consume().GetType() != kCommaToken)
      return nullptr;  // Parser error
  }
}

}  // namespace blink
