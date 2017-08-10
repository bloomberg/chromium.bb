// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSSelectorParser.h"

#include <memory>
#include "core/css/CSSSelectorList.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/frame/Deprecation.h"
#include "core/frame/UseCounter.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

// static
CSSSelectorList CSSSelectorParser::ParseSelector(
    CSSParserTokenRange range,
    const CSSParserContext* context,
    StyleSheetContents* style_sheet) {
  CSSSelectorParser parser(context, style_sheet);
  range.ConsumeWhitespace();
  CSSSelectorList result = parser.ConsumeComplexSelectorList(range);
  if (!range.AtEnd())
    return CSSSelectorList();

  parser.RecordUsageAndDeprecations(result);
  return result;
}

CSSSelectorParser::CSSSelectorParser(const CSSParserContext* context,
                                     StyleSheetContents* style_sheet)
    : context_(context), style_sheet_(style_sheet) {}

CSSSelectorList CSSSelectorParser::ConsumeComplexSelectorList(
    CSSParserTokenRange& range) {
  Vector<std::unique_ptr<CSSParserSelector>> selector_list;
  std::unique_ptr<CSSParserSelector> selector = ConsumeComplexSelector(range);
  if (!selector)
    return CSSSelectorList();
  selector_list.push_back(std::move(selector));
  while (!range.AtEnd() && range.Peek().GetType() == kCommaToken) {
    range.ConsumeIncludingWhitespace();
    selector = ConsumeComplexSelector(range);
    if (!selector)
      return CSSSelectorList();
    selector_list.push_back(std::move(selector));
  }

  if (failed_parsing_)
    return CSSSelectorList();

  return CSSSelectorList::AdoptSelectorVector(selector_list);
}

CSSSelectorList CSSSelectorParser::ConsumeCompoundSelectorList(
    CSSParserTokenRange& range) {
  Vector<std::unique_ptr<CSSParserSelector>> selector_list;
  std::unique_ptr<CSSParserSelector> selector = ConsumeCompoundSelector(range);
  range.ConsumeWhitespace();
  if (!selector)
    return CSSSelectorList();
  selector_list.push_back(std::move(selector));
  while (!range.AtEnd() && range.Peek().GetType() == kCommaToken) {
    range.ConsumeIncludingWhitespace();
    selector = ConsumeCompoundSelector(range);
    range.ConsumeWhitespace();
    if (!selector)
      return CSSSelectorList();
    selector_list.push_back(std::move(selector));
  }

  if (failed_parsing_)
    return CSSSelectorList();

  return CSSSelectorList::AdoptSelectorVector(selector_list);
}

namespace {

enum CompoundSelectorFlags {
  kHasPseudoElementForRightmostCompound = 1 << 0,
  kHasContentPseudoElement = 1 << 1
};

unsigned ExtractCompoundFlags(const CSSParserSelector& simple_selector,
                              CSSParserMode parser_mode) {
  if (simple_selector.Match() != CSSSelector::kPseudoElement)
    return 0;
  if (simple_selector.GetPseudoType() == CSSSelector::kPseudoContent)
    return kHasContentPseudoElement;
  if (simple_selector.GetPseudoType() == CSSSelector::kPseudoShadow)
    return 0;
  // TODO(rune@opera.com): crbug.com/578131
  // The UASheetMode check is a work-around to allow this selector in
  // mediaControls(New).css:
  // input[type="range" i]::-webkit-media-slider-container > div {
  if (parser_mode == kUASheetMode &&
      simple_selector.GetPseudoType() ==
          CSSSelector::kPseudoWebKitCustomElement)
    return 0;
  return kHasPseudoElementForRightmostCompound;
}

}  // namespace

std::unique_ptr<CSSParserSelector> CSSSelectorParser::ConsumeComplexSelector(
    CSSParserTokenRange& range) {
  std::unique_ptr<CSSParserSelector> selector = ConsumeCompoundSelector(range);
  if (!selector)
    return nullptr;

  unsigned previous_compound_flags = 0;

  for (CSSParserSelector* simple = selector.get();
       simple && !previous_compound_flags; simple = simple->TagHistory())
    previous_compound_flags |= ExtractCompoundFlags(*simple, context_->Mode());

  while (CSSSelector::RelationType combinator = ConsumeCombinator(range)) {
    std::unique_ptr<CSSParserSelector> next_selector =
        ConsumeCompoundSelector(range);
    if (!next_selector)
      return combinator == CSSSelector::kDescendant ? std::move(selector)
                                                    : nullptr;
    if (previous_compound_flags & kHasPseudoElementForRightmostCompound)
      return nullptr;
    CSSParserSelector* end = next_selector.get();
    unsigned compound_flags = ExtractCompoundFlags(*end, context_->Mode());
    while (end->TagHistory()) {
      end = end->TagHistory();
      compound_flags |= ExtractCompoundFlags(*end, context_->Mode());
    }
    end->SetRelation(combinator);
    if (previous_compound_flags & kHasContentPseudoElement)
      end->SetRelationIsAffectedByPseudoContent();
    previous_compound_flags = compound_flags;
    end->SetTagHistory(std::move(selector));

    selector = std::move(next_selector);
  }

  return selector;
}

namespace {

bool IsScrollbarPseudoClass(CSSSelector::PseudoType pseudo) {
  switch (pseudo) {
    case CSSSelector::kPseudoEnabled:
    case CSSSelector::kPseudoDisabled:
    case CSSSelector::kPseudoHover:
    case CSSSelector::kPseudoActive:
    case CSSSelector::kPseudoHorizontal:
    case CSSSelector::kPseudoVertical:
    case CSSSelector::kPseudoDecrement:
    case CSSSelector::kPseudoIncrement:
    case CSSSelector::kPseudoStart:
    case CSSSelector::kPseudoEnd:
    case CSSSelector::kPseudoDoubleButton:
    case CSSSelector::kPseudoSingleButton:
    case CSSSelector::kPseudoNoButton:
    case CSSSelector::kPseudoCornerPresent:
    case CSSSelector::kPseudoWindowInactive:
      return true;
    default:
      return false;
  }
}

bool IsUserActionPseudoClass(CSSSelector::PseudoType pseudo) {
  switch (pseudo) {
    case CSSSelector::kPseudoHover:
    case CSSSelector::kPseudoFocus:
    case CSSSelector::kPseudoFocusWithin:
    case CSSSelector::kPseudoActive:
      return true;
    default:
      return false;
  }
}

bool IsPseudoClassValidAfterPseudoElement(
    CSSSelector::PseudoType pseudo_class,
    CSSSelector::PseudoType compound_pseudo_element) {
  switch (compound_pseudo_element) {
    case CSSSelector::kPseudoResizer:
    case CSSSelector::kPseudoScrollbar:
    case CSSSelector::kPseudoScrollbarCorner:
    case CSSSelector::kPseudoScrollbarButton:
    case CSSSelector::kPseudoScrollbarThumb:
    case CSSSelector::kPseudoScrollbarTrack:
    case CSSSelector::kPseudoScrollbarTrackPiece:
      return IsScrollbarPseudoClass(pseudo_class);
    case CSSSelector::kPseudoSelection:
      return pseudo_class == CSSSelector::kPseudoWindowInactive;
    case CSSSelector::kPseudoWebKitCustomElement:
    case CSSSelector::kPseudoBlinkInternalElement:
      return IsUserActionPseudoClass(pseudo_class);
    default:
      return false;
  }
}

bool IsSimpleSelectorValidAfterPseudoElement(
    const CSSParserSelector& simple_selector,
    CSSSelector::PseudoType compound_pseudo_element) {
  if (compound_pseudo_element == CSSSelector::kPseudoUnknown)
    return true;
  if (compound_pseudo_element == CSSSelector::kPseudoContent)
    return simple_selector.Match() != CSSSelector::kPseudoElement;
  if (simple_selector.Match() != CSSSelector::kPseudoClass)
    return false;
  CSSSelector::PseudoType pseudo = simple_selector.GetPseudoType();
  if (pseudo == CSSSelector::kPseudoNot) {
    DCHECK(simple_selector.SelectorList());
    DCHECK(simple_selector.SelectorList()->First());
    DCHECK(!simple_selector.SelectorList()->First()->TagHistory());
    pseudo = simple_selector.SelectorList()->First()->GetPseudoType();
  }
  return IsPseudoClassValidAfterPseudoElement(pseudo, compound_pseudo_element);
}

}  // namespace

std::unique_ptr<CSSParserSelector> CSSSelectorParser::ConsumeCompoundSelector(
    CSSParserTokenRange& range) {
  std::unique_ptr<CSSParserSelector> compound_selector;

  AtomicString namespace_prefix;
  AtomicString element_name;
  CSSSelector::PseudoType compound_pseudo_element = CSSSelector::kPseudoUnknown;
  if (!ConsumeName(range, element_name, namespace_prefix)) {
    compound_selector = ConsumeSimpleSelector(range);
    if (!compound_selector)
      return nullptr;
    if (compound_selector->Match() == CSSSelector::kPseudoElement)
      compound_pseudo_element = compound_selector->GetPseudoType();
  }
  if (context_->IsHTMLDocument())
    element_name = element_name.LowerASCII();

  while (std::unique_ptr<CSSParserSelector> simple_selector =
             ConsumeSimpleSelector(range)) {
    // TODO(rune@opera.com): crbug.com/578131
    // The UASheetMode check is a work-around to allow this selector in
    // mediaControls(New).css:
    // video::-webkit-media-text-track-region-container.scrolling
    if (context_->Mode() != kUASheetMode &&
        !IsSimpleSelectorValidAfterPseudoElement(*simple_selector.get(),
                                                 compound_pseudo_element)) {
      failed_parsing_ = true;
      return nullptr;
    }
    if (simple_selector->Match() == CSSSelector::kPseudoElement)
      compound_pseudo_element = simple_selector->GetPseudoType();

    if (compound_selector)
      compound_selector = AddSimpleSelectorToCompound(
          std::move(compound_selector), std::move(simple_selector));
    else
      compound_selector = std::move(simple_selector);
  }

  if (!compound_selector) {
    AtomicString namespace_uri = DetermineNamespace(namespace_prefix);
    if (namespace_uri.IsNull()) {
      failed_parsing_ = true;
      return nullptr;
    }
    if (namespace_uri == DefaultNamespace())
      namespace_prefix = g_null_atom;
    return CSSParserSelector::Create(
        QualifiedName(namespace_prefix, element_name, namespace_uri));
  }
  // TODO(rune@opera.com): Prepending a type selector to the compound is
  // unnecessary if this compound is an argument to a pseudo selector like
  // :not(), since a type selector will be prepended at the top level of the
  // selector if necessary. We need to propagate that context information here
  // to tell if we are at the top level.
  PrependTypeSelectorIfNeeded(namespace_prefix, element_name,
                              compound_selector.get());
  return SplitCompoundAtImplicitShadowCrossingCombinator(
      std::move(compound_selector));
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::ConsumeSimpleSelector(
    CSSParserTokenRange& range) {
  const CSSParserToken& token = range.Peek();
  std::unique_ptr<CSSParserSelector> selector;
  if (token.GetType() == kHashToken)
    selector = ConsumeId(range);
  else if (token.GetType() == kDelimiterToken && token.Delimiter() == '.')
    selector = ConsumeClass(range);
  else if (token.GetType() == kLeftBracketToken)
    selector = ConsumeAttribute(range);
  else if (token.GetType() == kColonToken)
    selector = ConsumePseudo(range);
  else
    return nullptr;
  if (!selector)
    failed_parsing_ = true;
  return selector;
}

bool CSSSelectorParser::ConsumeName(CSSParserTokenRange& range,
                                    AtomicString& name,
                                    AtomicString& namespace_prefix) {
  name = g_null_atom;
  namespace_prefix = g_null_atom;

  const CSSParserToken& first_token = range.Peek();
  if (first_token.GetType() == kIdentToken) {
    name = first_token.Value().ToAtomicString();
    range.Consume();
  } else if (first_token.GetType() == kDelimiterToken &&
             first_token.Delimiter() == '*') {
    name = g_star_atom;
    range.Consume();
  } else if (first_token.GetType() == kDelimiterToken &&
             first_token.Delimiter() == '|') {
    // This is an empty namespace, which'll get assigned this value below
    name = g_empty_atom;
  } else {
    return false;
  }

  if (range.Peek().GetType() != kDelimiterToken ||
      range.Peek().Delimiter() != '|')
    return true;
  range.Consume();

  namespace_prefix = name;
  const CSSParserToken& name_token = range.Consume();
  if (name_token.GetType() == kIdentToken) {
    name = name_token.Value().ToAtomicString();
  } else if (name_token.GetType() == kDelimiterToken &&
             name_token.Delimiter() == '*') {
    name = g_star_atom;
  } else {
    name = g_null_atom;
    namespace_prefix = g_null_atom;
    return false;
  }

  return true;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::ConsumeId(
    CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().GetType(), kHashToken);
  if (range.Peek().GetHashTokenType() != kHashTokenId)
    return nullptr;
  std::unique_ptr<CSSParserSelector> selector = CSSParserSelector::Create();
  selector->SetMatch(CSSSelector::kId);
  AtomicString value = range.Consume().Value().ToAtomicString();
  selector->SetValue(value, IsQuirksModeBehavior(context_->MatchMode()));
  return selector;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::ConsumeClass(
    CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().GetType(), kDelimiterToken);
  DCHECK_EQ(range.Peek().Delimiter(), '.');
  range.Consume();
  if (range.Peek().GetType() != kIdentToken)
    return nullptr;
  std::unique_ptr<CSSParserSelector> selector = CSSParserSelector::Create();
  selector->SetMatch(CSSSelector::kClass);
  AtomicString value = range.Consume().Value().ToAtomicString();
  selector->SetValue(value, IsQuirksModeBehavior(context_->MatchMode()));
  return selector;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::ConsumeAttribute(
    CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().GetType(), kLeftBracketToken);
  CSSParserTokenRange block = range.ConsumeBlock();
  block.ConsumeWhitespace();

  AtomicString namespace_prefix;
  AtomicString attribute_name;
  if (!ConsumeName(block, attribute_name, namespace_prefix))
    return nullptr;
  if (attribute_name == g_star_atom)
    return nullptr;
  block.ConsumeWhitespace();

  if (context_->IsHTMLDocument())
    attribute_name = attribute_name.LowerASCII();

  AtomicString namespace_uri = DetermineNamespace(namespace_prefix);
  if (namespace_uri.IsNull())
    return nullptr;

  QualifiedName qualified_name =
      namespace_prefix.IsNull()
          ? QualifiedName(g_null_atom, attribute_name, g_null_atom)
          : QualifiedName(namespace_prefix, attribute_name, namespace_uri);

  std::unique_ptr<CSSParserSelector> selector = CSSParserSelector::Create();

  if (block.AtEnd()) {
    selector->SetAttribute(qualified_name, CSSSelector::kCaseSensitive);
    selector->SetMatch(CSSSelector::kAttributeSet);
    return selector;
  }

  selector->SetMatch(ConsumeAttributeMatch(block));

  const CSSParserToken& attribute_value = block.ConsumeIncludingWhitespace();
  if (attribute_value.GetType() != kIdentToken &&
      attribute_value.GetType() != kStringToken)
    return nullptr;
  selector->SetValue(attribute_value.Value().ToAtomicString());
  selector->SetAttribute(qualified_name, ConsumeAttributeFlags(block));

  if (!block.AtEnd())
    return nullptr;
  return selector;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::ConsumePseudo(
    CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().GetType(), kColonToken);
  range.Consume();

  int colons = 1;
  if (range.Peek().GetType() == kColonToken) {
    range.Consume();
    colons++;
  }

  const CSSParserToken& token = range.Peek();
  if (token.GetType() != kIdentToken && token.GetType() != kFunctionToken)
    return nullptr;

  std::unique_ptr<CSSParserSelector> selector = CSSParserSelector::Create();
  selector->SetMatch(colons == 1 ? CSSSelector::kPseudoClass
                                 : CSSSelector::kPseudoElement);

  AtomicString value = token.Value().ToAtomicString().LowerASCII();
  bool has_arguments = token.GetType() == kFunctionToken;
  selector->UpdatePseudoType(value, has_arguments, context_->Mode());

  if (!RuntimeEnabledFeatures::CSSSelectorsFocusWithinEnabled() &&
      selector->GetPseudoType() == CSSSelector::kPseudoFocusWithin)
    return nullptr;

  if (selector->Match() == CSSSelector::kPseudoElement &&
      disallow_pseudo_elements_)
    return nullptr;

  if (token.GetType() == kIdentToken) {
    range.Consume();
    if (selector->GetPseudoType() == CSSSelector::kPseudoUnknown)
      return nullptr;
    return selector;
  }

  CSSParserTokenRange block = range.ConsumeBlock();
  block.ConsumeWhitespace();
  if (selector->GetPseudoType() == CSSSelector::kPseudoUnknown)
    return nullptr;

  switch (selector->GetPseudoType()) {
    case CSSSelector::kPseudoHost:
    case CSSSelector::kPseudoHostContext:
    case CSSSelector::kPseudoAny:
    case CSSSelector::kPseudoCue: {
      DisallowPseudoElementsScope scope(this);

      std::unique_ptr<CSSSelectorList> selector_list =
          WTF::MakeUnique<CSSSelectorList>();
      *selector_list = ConsumeCompoundSelectorList(block);
      if (!selector_list->IsValid() || !block.AtEnd())
        return nullptr;
      selector->SetSelectorList(std::move(selector_list));
      return selector;
    }
    case CSSSelector::kPseudoNot: {
      std::unique_ptr<CSSParserSelector> inner_selector =
          ConsumeCompoundSelector(block);
      block.ConsumeWhitespace();
      if (!inner_selector || !inner_selector->IsSimple() || !block.AtEnd())
        return nullptr;
      Vector<std::unique_ptr<CSSParserSelector>> selector_vector;
      selector_vector.push_back(std::move(inner_selector));
      selector->AdoptSelectorVector(selector_vector);
      return selector;
    }
    case CSSSelector::kPseudoSlotted: {
      DisallowPseudoElementsScope scope(this);

      std::unique_ptr<CSSParserSelector> inner_selector =
          ConsumeCompoundSelector(block);
      block.ConsumeWhitespace();
      if (!inner_selector || !block.AtEnd())
        return nullptr;
      Vector<std::unique_ptr<CSSParserSelector>> selector_vector;
      selector_vector.push_back(std::move(inner_selector));
      selector->AdoptSelectorVector(selector_vector);
      return selector;
    }
    case CSSSelector::kPseudoLang: {
      // FIXME: CSS Selectors Level 4 allows :lang(*-foo)
      const CSSParserToken& ident = block.ConsumeIncludingWhitespace();
      if (ident.GetType() != kIdentToken || !block.AtEnd())
        return nullptr;
      selector->SetArgument(ident.Value().ToAtomicString());
      return selector;
    }
    case CSSSelector::kPseudoNthChild:
    case CSSSelector::kPseudoNthLastChild:
    case CSSSelector::kPseudoNthOfType:
    case CSSSelector::kPseudoNthLastOfType: {
      std::pair<int, int> ab;
      if (!ConsumeANPlusB(block, ab))
        return nullptr;
      block.ConsumeWhitespace();
      if (!block.AtEnd())
        return nullptr;
      selector->SetNth(ab.first, ab.second);
      return selector;
    }
    default:
      break;
  }

  return nullptr;
}

CSSSelector::RelationType CSSSelectorParser::ConsumeCombinator(
    CSSParserTokenRange& range) {
  CSSSelector::RelationType fallback_result = CSSSelector::kSubSelector;
  while (range.Peek().GetType() == kWhitespaceToken) {
    range.Consume();
    fallback_result = CSSSelector::kDescendant;
  }

  if (range.Peek().GetType() != kDelimiterToken)
    return fallback_result;

  switch (range.Peek().Delimiter()) {
    case '+':
      range.ConsumeIncludingWhitespace();
      return CSSSelector::kDirectAdjacent;

    case '~':
      range.ConsumeIncludingWhitespace();
      return CSSSelector::kIndirectAdjacent;

    case '>':
      if (!RuntimeEnabledFeatures::
              ShadowPiercingDescendantCombinatorEnabled() ||
          context_->IsDynamicProfile() ||
          range.Peek(1).GetType() != kDelimiterToken ||
          range.Peek(1).Delimiter() != '>') {
        range.ConsumeIncludingWhitespace();
        return CSSSelector::kChild;
      }
      range.Consume();

      // Check the 3rd '>'.
      if (range.Peek(1).GetType() != kDelimiterToken ||
          range.Peek(1).Delimiter() != '>') {
        // TODO: Treat '>>' as a CSSSelector::kDescendant here.
        return CSSSelector::kChild;
      }
      range.Consume();
      range.ConsumeIncludingWhitespace();
      return CSSSelector::kShadowPiercingDescendant;

    case '/': {
      // Match /deep/
      range.Consume();
      const CSSParserToken& ident = range.Consume();
      if (ident.GetType() != kIdentToken ||
          !EqualIgnoringASCIICase(ident.Value(), "deep"))
        failed_parsing_ = true;
      const CSSParserToken& slash = range.ConsumeIncludingWhitespace();
      if (slash.GetType() != kDelimiterToken || slash.Delimiter() != '/')
        failed_parsing_ = true;
      return CSSSelector::kShadowDeep;
    }

    default:
      break;
  }
  return fallback_result;
}

CSSSelector::MatchType CSSSelectorParser::ConsumeAttributeMatch(
    CSSParserTokenRange& range) {
  const CSSParserToken& token = range.ConsumeIncludingWhitespace();
  switch (token.GetType()) {
    case kIncludeMatchToken:
      return CSSSelector::kAttributeList;
    case kDashMatchToken:
      return CSSSelector::kAttributeHyphen;
    case kPrefixMatchToken:
      return CSSSelector::kAttributeBegin;
    case kSuffixMatchToken:
      return CSSSelector::kAttributeEnd;
    case kSubstringMatchToken:
      return CSSSelector::kAttributeContain;
    case kDelimiterToken:
      if (token.Delimiter() == '=')
        return CSSSelector::kAttributeExact;
    default:
      failed_parsing_ = true;
      return CSSSelector::kAttributeExact;
  }
}

CSSSelector::AttributeMatchType CSSSelectorParser::ConsumeAttributeFlags(
    CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kIdentToken)
    return CSSSelector::kCaseSensitive;
  const CSSParserToken& flag = range.ConsumeIncludingWhitespace();
  if (EqualIgnoringASCIICase(flag.Value(), "i"))
    return CSSSelector::kCaseInsensitive;
  failed_parsing_ = true;
  return CSSSelector::kCaseSensitive;
}

bool CSSSelectorParser::ConsumeANPlusB(CSSParserTokenRange& range,
                                       std::pair<int, int>& result) {
  const CSSParserToken& token = range.Consume();
  if (token.GetType() == kNumberToken &&
      token.GetNumericValueType() == kIntegerValueType) {
    result = std::make_pair(0, clampTo<int>(token.NumericValue()));
    return true;
  }
  if (token.GetType() == kIdentToken) {
    if (EqualIgnoringASCIICase(token.Value(), "odd")) {
      result = std::make_pair(2, 1);
      return true;
    }
    if (EqualIgnoringASCIICase(token.Value(), "even")) {
      result = std::make_pair(2, 0);
      return true;
    }
  }

  // The 'n' will end up as part of an ident or dimension. For a valid <an+b>,
  // this will store a string of the form 'n', 'n-', or 'n-123'.
  String n_string;

  if (token.GetType() == kDelimiterToken && token.Delimiter() == '+' &&
      range.Peek().GetType() == kIdentToken) {
    result.first = 1;
    n_string = range.Consume().Value().ToString();
  } else if (token.GetType() == kDimensionToken &&
             token.GetNumericValueType() == kIntegerValueType) {
    result.first = clampTo<int>(token.NumericValue());
    n_string = token.Value().ToString();
  } else if (token.GetType() == kIdentToken) {
    if (token.Value()[0] == '-') {
      result.first = -1;
      n_string = token.Value().ToString().Substring(1);
    } else {
      result.first = 1;
      n_string = token.Value().ToString();
    }
  }

  range.ConsumeWhitespace();

  if (n_string.IsEmpty() || !IsASCIIAlphaCaselessEqual(n_string[0], 'n'))
    return false;
  if (n_string.length() > 1 && n_string[1] != '-')
    return false;

  if (n_string.length() > 2) {
    bool valid;
    result.second = n_string.Substring(1).ToIntStrict(&valid);
    return valid;
  }

  NumericSign sign = n_string.length() == 1 ? kNoSign : kMinusSign;
  if (sign == kNoSign && range.Peek().GetType() == kDelimiterToken) {
    char delimiter_sign = range.ConsumeIncludingWhitespace().Delimiter();
    if (delimiter_sign == '+')
      sign = kPlusSign;
    else if (delimiter_sign == '-')
      sign = kMinusSign;
    else
      return false;
  }

  if (sign == kNoSign && range.Peek().GetType() != kNumberToken) {
    result.second = 0;
    return true;
  }

  const CSSParserToken& b = range.Consume();
  if (b.GetType() != kNumberToken ||
      b.GetNumericValueType() != kIntegerValueType)
    return false;
  if ((b.GetNumericSign() == kNoSign) == (sign == kNoSign))
    return false;
  result.second = clampTo<int>(b.NumericValue());
  if (sign == kMinusSign) {
    // Negating minimum integer returns itself, instead return max integer.
    if (UNLIKELY(result.second == std::numeric_limits<int>::min()))
      result.second = std::numeric_limits<int>::max();
    else
      result.second = -result.second;
  }
  return true;
}

const AtomicString& CSSSelectorParser::DefaultNamespace() const {
  if (!style_sheet_)
    return g_star_atom;
  return style_sheet_->DefaultNamespace();
}

const AtomicString& CSSSelectorParser::DetermineNamespace(
    const AtomicString& prefix) {
  if (prefix.IsNull())
    return DefaultNamespace();
  if (prefix.IsEmpty())
    return g_empty_atom;  // No namespace. If an element/attribute has a
                          // namespace, we won't match it.
  if (prefix == g_star_atom)
    return g_star_atom;  // We'll match any namespace.
  if (!style_sheet_)
    return g_null_atom;  // Cannot resolve prefix to namespace without a
                         // stylesheet, syntax error.
  return style_sheet_->NamespaceURIFromPrefix(prefix);
}

void CSSSelectorParser::PrependTypeSelectorIfNeeded(
    const AtomicString& namespace_prefix,
    const AtomicString& element_name,
    CSSParserSelector* compound_selector) {
  if (element_name.IsNull() && DefaultNamespace() == g_star_atom &&
      !compound_selector->NeedsImplicitShadowCombinatorForMatching())
    return;

  AtomicString determined_element_name =
      element_name.IsNull() ? g_star_atom : element_name;
  AtomicString namespace_uri = DetermineNamespace(namespace_prefix);
  if (namespace_uri.IsNull()) {
    failed_parsing_ = true;
    return;
  }
  AtomicString determined_prefix = namespace_prefix;
  if (namespace_uri == DefaultNamespace())
    determined_prefix = g_null_atom;
  QualifiedName tag =
      QualifiedName(determined_prefix, determined_element_name, namespace_uri);

  // *:host/*:host-context never matches, so we can't discard the *,
  // otherwise we can't tell the difference between *:host and just :host.
  //
  // Also, selectors where we use a ShadowPseudo combinator between the
  // element and the pseudo element for matching (custom pseudo elements,
  // ::cue, ::shadow), we need a universal selector to set the combinator
  // (relation) on in the cases where there are no simple selectors preceding
  // the pseudo element.
  bool is_host_pseudo = compound_selector->IsHostPseudoSelector();
  if (is_host_pseudo && element_name.IsNull() && namespace_prefix.IsNull())
    return;
  if (tag != AnyQName() || is_host_pseudo ||
      compound_selector->NeedsImplicitShadowCombinatorForMatching()) {
    compound_selector->PrependTagSelector(
        tag, determined_prefix == g_null_atom &&
                 determined_element_name == g_star_atom && !is_host_pseudo);
  }
}

std::unique_ptr<CSSParserSelector>
CSSSelectorParser::AddSimpleSelectorToCompound(
    std::unique_ptr<CSSParserSelector> compound_selector,
    std::unique_ptr<CSSParserSelector> simple_selector) {
  compound_selector->AppendTagHistory(CSSSelector::kSubSelector,
                                      std::move(simple_selector));
  return compound_selector;
}

std::unique_ptr<CSSParserSelector>
CSSSelectorParser::SplitCompoundAtImplicitShadowCrossingCombinator(
    std::unique_ptr<CSSParserSelector> compound_selector) {
  // The tagHistory is a linked list that stores combinator separated compound
  // selectors from right-to-left. Yet, within a single compound selector,
  // stores the simple selectors from left-to-right.
  //
  // ".a.b > div#id" is stored in a tagHistory as [div, #id, .a, .b], each
  // element in the list stored with an associated relation (combinator or
  // SubSelector).
  //
  // ::cue, ::shadow, and custom pseudo elements have an implicit ShadowPseudo
  // combinator to their left, which really makes for a new compound selector,
  // yet it's consumed by the selector parser as a single compound selector.
  //
  // Example:
  //
  // input#x::-webkit-clear-button -> [ ::-webkit-clear-button, input, #x ]
  //
  // Likewise, ::slotted() pseudo element has an implicit ShadowSlot combinator
  // to its left for finding matching slot element in other TreeScope.
  //
  // Example:
  //
  // slot[name=foo]::slotted(div) -> [ ::slotted(div), slot, [name=foo] ]
  CSSParserSelector* split_after = compound_selector.get();

  while (split_after->TagHistory() &&
         !split_after->TagHistory()->NeedsImplicitShadowCombinatorForMatching())
    split_after = split_after->TagHistory();

  if (!split_after || !split_after->TagHistory())
    return compound_selector;

  std::unique_ptr<CSSParserSelector> second_compound =
      split_after->ReleaseTagHistory();
  second_compound->AppendTagHistory(
      second_compound->GetPseudoType() == CSSSelector::kPseudoSlotted
          ? CSSSelector::kShadowSlot
          : CSSSelector::kShadowPseudo,
      std::move(compound_selector));
  return second_compound;
}

void CSSSelectorParser::RecordUsageAndDeprecations(
    const CSSSelectorList& selector_list) {
  if (!context_->IsUseCounterRecordingEnabled())
    return;

  for (const CSSSelector* selector = selector_list.First(); selector;
       selector = CSSSelectorList::Next(*selector)) {
    for (const CSSSelector* current = selector; current;
         current = current->TagHistory()) {
      WebFeature feature = WebFeature::kNumberOfFeatures;
      switch (current->GetPseudoType()) {
        case CSSSelector::kPseudoAny:
          feature = WebFeature::kCSSSelectorPseudoAny;
          break;
        case CSSSelector::kPseudoUnresolved:
          feature = WebFeature::kCSSSelectorPseudoUnresolved;
          break;
        case CSSSelector::kPseudoDefined:
          feature = WebFeature::kCSSSelectorPseudoDefined;
          break;
        case CSSSelector::kPseudoSlotted:
          feature = WebFeature::kCSSSelectorPseudoSlotted;
          break;
        case CSSSelector::kPseudoContent:
          feature = WebFeature::kCSSSelectorPseudoContent;
          break;
        case CSSSelector::kPseudoHost:
          feature = WebFeature::kCSSSelectorPseudoHost;
          break;
        case CSSSelector::kPseudoHostContext:
          feature = WebFeature::kCSSSelectorPseudoHostContext;
          break;
        case CSSSelector::kPseudoFullScreenAncestor:
          feature = WebFeature::kCSSSelectorPseudoFullScreenAncestor;
          break;
        case CSSSelector::kPseudoFullScreen:
          feature = WebFeature::kCSSSelectorPseudoFullScreen;
          break;
        case CSSSelector::kPseudoListBox:
          if (context_->Mode() != kUASheetMode)
            feature = WebFeature::kCSSSelectorInternalPseudoListBox;
          break;
        case CSSSelector::kPseudoWebKitCustomElement:
          if (context_->Mode() != kUASheetMode) {
            if (current->Value() ==
                "-internal-media-controls-overlay-cast-button") {
              feature = WebFeature::
                  kCSSSelectorInternalMediaControlsOverlayCastButton;
            } else if (current->Value() == "-webkit-media-controls") {
              feature = WebFeature::kCSSSelectorWebkitMediaControls;
            } else if (current->Value() ==
                       "-webkit-media-controls-overlay-enclosure") {
              feature =
                  WebFeature::kCSSSelectorWebkitMediaControlsOverlayEnclosure;
            } else if (current->Value() ==
                       "-webkit-media-controls-overlay-play-button") {
              feature =
                  WebFeature::kCSSSelectorWebkitMediaControlsOverlayPlayButton;
            } else if (current->Value() == "-webkit-media-controls-enclosure") {
              feature = WebFeature::kCSSSelectorWebkitMediaControlsEnclosure;
            } else if (current->Value() == "-webkit-media-controls-panel") {
              feature = WebFeature::kCSSSelectorWebkitMediaControlsPanel;
            } else if (current->Value() ==
                       "-webkit-media-controls-play-button") {
              feature = WebFeature::kCSSSelectorWebkitMediaControlsPlayButton;
            } else if (current->Value() ==
                       "-webkit-media-controls-current-time-display") {
              feature =
                  WebFeature::kCSSSelectorWebkitMediaControlsCurrentTimeDisplay;
            } else if (current->Value() ==
                       "-webkit-media-controls-time-remaining-display") {
              feature = WebFeature::
                  kCSSSelectorWebkitMediaControlsTimeRemainingDisplay;
            } else if (current->Value() == "-webkit-media-controls-timeline") {
              feature = WebFeature::kCSSSelectorWebkitMediaControlsTimeline;
            } else if (current->Value() ==
                       "-webkit-media-controls-timeline-container") {
              feature =
                  WebFeature::kCSSSelectorWebkitMediaControlsTimelineContainer;
            } else if (current->Value() ==
                       "-webkit-media-controls-mute-button") {
              feature = WebFeature::kCSSSelectorWebkitMediaControlsMuteButton;
            } else if (current->Value() ==
                       "-webkit-media-controls-volume-slider") {
              feature = WebFeature::kCSSSelectorWebkitMediaControlsVolumeSlider;
            } else if (current->Value() ==
                       "-webkit-media-controls-fullscreen-button") {
              feature =
                  WebFeature::kCSSSelectorWebkitMediaControlsFullscreenButton;
            } else if (current->Value() ==
                       "-webkit-media-controls-toggle-closed-captions-button") {
              feature = WebFeature::
                  kCSSSelectorWebkitMediaControlsToggleClosedCaptionsButton;
            }
          }
          break;
        case CSSSelector::kPseudoSpatialNavigationFocus:
          if (context_->Mode() != kUASheetMode) {
            feature =
                WebFeature::kCSSSelectorInternalPseudoSpatialNavigationFocus;
          }
          break;
        case CSSSelector::kPseudoReadOnly:
          if (context_->Mode() != kUASheetMode)
            feature = WebFeature::kCSSSelectorPseudoReadOnly;
          break;
        case CSSSelector::kPseudoReadWrite:
          if (context_->Mode() != kUASheetMode)
            feature = WebFeature::kCSSSelectorPseudoReadWrite;
          break;
        default:
          break;
      }
      if (feature != WebFeature::kNumberOfFeatures) {
        if (!Deprecation::DeprecationMessage(feature).IsEmpty() &&
            style_sheet_->AnyOwnerDocument()) {
          Deprecation::CountDeprecation(*style_sheet_->AnyOwnerDocument(),
                                        feature);
        } else {
          context_->Count(feature);
        }
      }
      if (current->Relation() == CSSSelector::kIndirectAdjacent)
        context_->Count(WebFeature::kCSSSelectorIndirectAdjacent);
      if (current->SelectorList())
        RecordUsageAndDeprecations(*current->SelectorList());
    }
  }
}

}  // namespace blink
