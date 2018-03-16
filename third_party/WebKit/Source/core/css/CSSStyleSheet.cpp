/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2012 Apple Inc. All rights reserved.
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

#include "core/css/CSSStyleSheet.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/media_list_or_string.h"
#include "core/css/CSSImportRule.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleSheetInit.h"
#include "core/css/MediaList.h"
#include "core/css/StyleEngine.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/Node.h"
#include "core/frame/Deprecation.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/html_names.h"
#include "core/probe/CoreProbes.h"
#include "core/svg/SVGStyleElement.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

class StyleSheetCSSRuleList final : public CSSRuleList {
 public:
  static StyleSheetCSSRuleList* Create(CSSStyleSheet* sheet) {
    return new StyleSheetCSSRuleList(sheet);
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(style_sheet_);
    CSSRuleList::Trace(visitor);
  }

 private:
  StyleSheetCSSRuleList(CSSStyleSheet* sheet) : style_sheet_(sheet) {}

  unsigned length() const override { return style_sheet_->length(); }
  CSSRule* item(unsigned index) const override {
    return style_sheet_->item(index);
  }

  CSSStyleSheet* GetStyleSheet() const override { return style_sheet_; }

  Member<CSSStyleSheet> style_sheet_;
};

#if DCHECK_IS_ON()
static bool IsAcceptableCSSStyleSheetParent(const Node& parent_node) {
  // Only these nodes can be parents of StyleSheets, and they need to call
  // clearOwnerNode() when moved out of document. Note that destructor of
  // the nodes don't call clearOwnerNode() with Oilpan.
  return parent_node.IsDocumentNode() || IsHTMLLinkElement(parent_node) ||
         IsHTMLStyleElement(parent_node) || IsSVGStyleElement(parent_node) ||
         parent_node.getNodeType() == Node::kProcessingInstructionNode;
}
#endif

// static
const Document* CSSStyleSheet::SingleOwnerDocument(
    const CSSStyleSheet* style_sheet) {
  if (style_sheet)
    return StyleSheetContents::SingleOwnerDocument(style_sheet->Contents());
  return nullptr;
}

CSSStyleSheet* CSSStyleSheet::Create(Document& document,
                                     const String& text,
                                     ExceptionState& exception_state) {
  return CSSStyleSheet::Create(document, text, CSSStyleSheetInit(),
                               exception_state);
}

CSSStyleSheet* CSSStyleSheet::Create(Document& document,
                                     const String& text,
                                     const CSSStyleSheetInit& options,
                                     ExceptionState& exception_state) {
  if (!RuntimeEnabledFeatures::ConstructableStylesheetsEnabled()) {
    exception_state.ThrowTypeError("Illegal constructor");
    return nullptr;
  }
  // Folowing steps at spec draft
  // https://wicg.github.io/construct-stylesheets/#dom-cssstylesheet-cssstylesheet
  CSSParserContext* parser_context = CSSParserContext::Create(document);
  StyleSheetContents* contents = StyleSheetContents::Create(parser_context);
  CSSStyleSheet* sheet = new CSSStyleSheet(contents, nullptr);
  sheet->SetTitle(options.title());
  sheet->ClearOwnerNode();
  sheet->ClearOwnerRule();
  if (options.media().IsString()) {
    MediaList* media_list = MediaList::Create(
        MediaQuerySet::Create(), const_cast<CSSStyleSheet*>(sheet));
    media_list->setMediaText(options.media().GetAsString());
    sheet->SetMedia(media_list);
  } else {
    sheet->SetMedia(options.media().GetAsMediaList());
  }
  if (options.alternate())
    sheet->SetAlternateFromConstructor(true);
  if (options.disabled())
    sheet->setDisabled(true);
  sheet->SetText(text);
  return sheet;
}

CSSStyleSheet* CSSStyleSheet::Create(StyleSheetContents* sheet,
                                     CSSImportRule* owner_rule) {
  return new CSSStyleSheet(sheet, owner_rule);
}

CSSStyleSheet* CSSStyleSheet::Create(StyleSheetContents* sheet,
                                     Node& owner_node) {
  return new CSSStyleSheet(sheet, owner_node, false,
                           TextPosition::MinimumPosition());
}

CSSStyleSheet* CSSStyleSheet::CreateInline(StyleSheetContents* sheet,
                                           Node& owner_node,
                                           const TextPosition& start_position) {
  DCHECK(sheet);
  return new CSSStyleSheet(sheet, owner_node, true, start_position);
}

CSSStyleSheet* CSSStyleSheet::CreateInline(Node& owner_node,
                                           const KURL& base_url,
                                           const TextPosition& start_position,
                                           const WTF::TextEncoding& encoding) {
  CSSParserContext* parser_context = CSSParserContext::Create(
      owner_node.GetDocument(), owner_node.GetDocument().BaseURL(),
      owner_node.GetDocument().GetReferrerPolicy(), encoding);
  StyleSheetContents* sheet =
      StyleSheetContents::Create(base_url.GetString(), parser_context);
  return new CSSStyleSheet(sheet, owner_node, true, start_position);
}

CSSStyleSheet::CSSStyleSheet(StyleSheetContents* contents,
                             CSSImportRule* owner_rule)
    : contents_(contents),
      owner_rule_(owner_rule),
      start_position_(TextPosition::MinimumPosition()) {
  contents_->RegisterClient(this);
}

CSSStyleSheet::CSSStyleSheet(StyleSheetContents* contents,
                             Node& owner_node,
                             bool is_inline_stylesheet,
                             const TextPosition& start_position)
    : contents_(contents),
      is_inline_stylesheet_(is_inline_stylesheet),
      owner_node_(&owner_node),
      start_position_(start_position) {
#if DCHECK_IS_ON()
  DCHECK(IsAcceptableCSSStyleSheetParent(owner_node));
#endif
  contents_->RegisterClient(this);
}

CSSStyleSheet::~CSSStyleSheet() = default;

void CSSStyleSheet::WillMutateRules() {
  // If we are the only client it is safe to mutate.
  if (!contents_->IsUsedFromTextCache() &&
      !contents_->IsReferencedFromResource()) {
    contents_->ClearRuleSet();
    contents_->SetMutable();
    return;
  }
  // Only cacheable stylesheets should have multiple clients.
  DCHECK(contents_->IsCacheableForStyleElement() ||
         contents_->IsCacheableForResource());

  // Copy-on-write.
  contents_->UnregisterClient(this);
  contents_ = contents_->Copy();
  contents_->RegisterClient(this);

  contents_->SetMutable();

  // Any existing CSSOM wrappers need to be connected to the copied child rules.
  ReattachChildRuleCSSOMWrappers();
}

void CSSStyleSheet::DidMutateRules() {
  DCHECK(contents_->IsMutable());
  DCHECK_LE(contents_->ClientSize(), 1u);

  Document* owner = OwnerDocument();
  if (owner && ownerNode() && ownerNode()->isConnected()) {
    owner->GetStyleEngine().SetNeedsActiveStyleUpdate(
        ownerNode()->GetTreeScope());
    if (StyleResolver* resolver = owner->GetStyleEngine().Resolver())
      resolver->InvalidateMatchedPropertiesCache();
  } else if (!constructed_tree_scopes_.IsEmpty()) {
    for (auto tree_scope : constructed_tree_scopes_) {
      tree_scope->GetDocument().GetStyleEngine().SetNeedsActiveStyleUpdate(
          *tree_scope);
      if (StyleResolver* resolver =
              tree_scope->GetDocument().GetStyleEngine().Resolver())
        resolver->InvalidateMatchedPropertiesCache();
    }
  }
}

void CSSStyleSheet::DidMutate() {
  Document* owner = OwnerDocument();
  if (!owner)
    return;
  if (ownerNode() && ownerNode()->isConnected())
    owner->GetStyleEngine().SetNeedsActiveStyleUpdate(
        ownerNode()->GetTreeScope());
}

void CSSStyleSheet::ReattachChildRuleCSSOMWrappers() {
  for (unsigned i = 0; i < child_rule_cssom_wrappers_.size(); ++i) {
    if (!child_rule_cssom_wrappers_[i])
      continue;
    child_rule_cssom_wrappers_[i]->Reattach(contents_->RuleAt(i));
  }
}

void CSSStyleSheet::setDisabled(bool disabled) {
  if (disabled == is_disabled_)
    return;
  is_disabled_ = disabled;

  DidMutate();
}

void CSSStyleSheet::SetMediaQueries(
    scoped_refptr<MediaQuerySet> media_queries) {
  media_queries_ = std::move(media_queries);
  if (media_cssom_wrapper_ && media_queries_)
    media_cssom_wrapper_->Reattach(media_queries_.get());
}

bool CSSStyleSheet::MatchesMediaQueries(const MediaQueryEvaluator& evaluator) {
  viewport_dependent_media_query_results_.clear();
  device_dependent_media_query_results_.clear();

  if (!media_queries_)
    return true;
  return evaluator.Eval(*media_queries_,
                        &viewport_dependent_media_query_results_,
                        &device_dependent_media_query_results_);
}

unsigned CSSStyleSheet::length() const {
  return contents_->RuleCount();
}

CSSRule* CSSStyleSheet::item(unsigned index) {
  unsigned rule_count = length();
  if (index >= rule_count)
    return nullptr;

  if (child_rule_cssom_wrappers_.IsEmpty())
    child_rule_cssom_wrappers_.Grow(rule_count);
  DCHECK_EQ(child_rule_cssom_wrappers_.size(), rule_count);

  Member<CSSRule>& css_rule = child_rule_cssom_wrappers_[index];
  if (!css_rule)
    css_rule = contents_->RuleAt(index)->CreateCSSOMWrapper(this);
  return css_rule.Get();
}

void CSSStyleSheet::ClearOwnerNode() {
  DidMutate();
  if (owner_node_)
    contents_->UnregisterClient(this);
  owner_node_ = nullptr;
}

bool CSSStyleSheet::CanAccessRules() const {
  if (is_inline_stylesheet_)
    return true;
  KURL base_url = contents_->BaseURL();
  if (base_url.IsEmpty())
    return true;
  Document* document = OwnerDocument();
  if (!document)
    return true;
  if (document->GetStyleEngine().InspectorStyleSheet() == this)
    return true;
  if (document->GetSecurityOrigin()->CanReadContent(base_url))
    return true;
  if (allow_rule_access_from_origin_ &&
      document->GetSecurityOrigin()->CanAccess(
          allow_rule_access_from_origin_.get())) {
    return true;
  }
  return false;
}

CSSRuleList* CSSStyleSheet::rules(ExceptionState& exception_state) {
  return cssRules(exception_state);
}

unsigned CSSStyleSheet::insertRule(const String& rule_string,
                                   unsigned index,
                                   ExceptionState& exception_state) {
  if (!CanAccessRules()) {
    exception_state.ThrowSecurityError(
        "Cannot access StyleSheet to insertRule");
    return 0;
  }

  DCHECK(child_rule_cssom_wrappers_.IsEmpty() ||
         child_rule_cssom_wrappers_.size() == contents_->RuleCount());

  if (index > length()) {
    exception_state.ThrowDOMException(
        kIndexSizeError, "The index provided (" + String::Number(index) +
                             ") is larger than the maximum index (" +
                             String::Number(length()) + ").");
    return 0;
  }
  const CSSParserContext* context =
      CSSParserContext::CreateWithStyleSheet(contents_->ParserContext(), this);
  StyleRuleBase* rule =
      CSSParser::ParseRule(context, contents_.Get(), rule_string);

  if (!rule) {
    exception_state.ThrowDOMException(
        kSyntaxError, "Failed to parse the rule '" + rule_string + "'.");
    return 0;
  }
  RuleMutationScope mutation_scope(this);
  bool success = contents_->WrapperInsertRule(rule, index);
  if (!success) {
    if (rule->IsNamespaceRule())
      exception_state.ThrowDOMException(kInvalidStateError,
                                        "Failed to insert the rule");
    else
      exception_state.ThrowDOMException(kHierarchyRequestError,
                                        "Failed to insert the rule.");
    return 0;
  }
  if (!child_rule_cssom_wrappers_.IsEmpty())
    child_rule_cssom_wrappers_.insert(index, Member<CSSRule>(nullptr));

  return index;
}

void CSSStyleSheet::deleteRule(unsigned index,
                               ExceptionState& exception_state) {
  if (!CanAccessRules()) {
    exception_state.ThrowSecurityError(
        "Cannot access StyleSheet to deleteRule");
    return;
  }

  DCHECK(child_rule_cssom_wrappers_.IsEmpty() ||
         child_rule_cssom_wrappers_.size() == contents_->RuleCount());

  if (index >= length()) {
    exception_state.ThrowDOMException(
        kIndexSizeError, "The index provided (" + String::Number(index) +
                             ") is larger than the maximum index (" +
                             String::Number(length() - 1) + ").");
    return;
  }
  RuleMutationScope mutation_scope(this);

  bool success = contents_->WrapperDeleteRule(index);
  if (!success) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Failed to delete rule");
    return;
  }

  if (!child_rule_cssom_wrappers_.IsEmpty()) {
    if (child_rule_cssom_wrappers_[index])
      child_rule_cssom_wrappers_[index]->SetParentStyleSheet(nullptr);
    child_rule_cssom_wrappers_.EraseAt(index);
  }
}

int CSSStyleSheet::addRule(const String& selector,
                           const String& style,
                           int index,
                           ExceptionState& exception_state) {
  StringBuilder text;
  text.Append(selector);
  text.Append(" { ");
  text.Append(style);
  if (!style.IsEmpty())
    text.Append(' ');
  text.Append('}');
  insertRule(text.ToString(), index, exception_state);

  // As per Microsoft documentation, always return -1.
  return -1;
}

int CSSStyleSheet::addRule(const String& selector,
                           const String& style,
                           ExceptionState& exception_state) {
  return addRule(selector, style, length(), exception_state);
}

CSSRuleList* CSSStyleSheet::cssRules(ExceptionState& exception_state) {
  if (!CanAccessRules()) {
    exception_state.ThrowSecurityError("Cannot access rules");
    return nullptr;
  }
  if (!rule_list_cssom_wrapper_)
    rule_list_cssom_wrapper_ = StyleSheetCSSRuleList::Create(this);
  return rule_list_cssom_wrapper_.Get();
}

String CSSStyleSheet::href() const {
  return contents_->OriginalURL();
}

KURL CSSStyleSheet::BaseURL() const {
  return contents_->BaseURL();
}

bool CSSStyleSheet::IsLoading() const {
  return contents_->IsLoading();
}

MediaList* CSSStyleSheet::media() {
  if (!media_queries_)
    media_queries_ = MediaQuerySet::Create();

  if (!media_cssom_wrapper_)
    media_cssom_wrapper_ = MediaList::Create(media_queries_.get(),
                                             const_cast<CSSStyleSheet*>(this));
  return media_cssom_wrapper_.Get();
}

void CSSStyleSheet::SetMedia(MediaList* media_list) {
  media_cssom_wrapper_ = media_list;
}

CSSStyleSheet* CSSStyleSheet::parentStyleSheet() const {
  return owner_rule_ ? owner_rule_->parentStyleSheet() : nullptr;
}

Document* CSSStyleSheet::OwnerDocument() const {
  const CSSStyleSheet* root = this;
  while (root->parentStyleSheet())
    root = root->parentStyleSheet();
  return root->ownerNode() ? &root->ownerNode()->GetDocument() : nullptr;
}

void CSSStyleSheet::SetAllowRuleAccessFromOrigin(
    scoped_refptr<const SecurityOrigin> allowed_origin) {
  allow_rule_access_from_origin_ = std::move(allowed_origin);
}

bool CSSStyleSheet::SheetLoaded() {
  DCHECK(owner_node_);
  SetLoadCompleted(owner_node_->SheetLoaded());
  return load_completed_;
}

void CSSStyleSheet::StartLoadingDynamicSheet() {
  SetLoadCompleted(false);
  owner_node_->StartLoadingDynamicSheet();
}

void CSSStyleSheet::SetLoadCompleted(bool completed) {
  if (completed == load_completed_)
    return;

  load_completed_ = completed;

  if (completed)
    contents_->ClientLoadCompleted(this);
  else
    contents_->ClientLoadStarted(this);
}

void CSSStyleSheet::SetText(const String& text) {
  child_rule_cssom_wrappers_.clear();

  CSSStyleSheet::RuleMutationScope mutation_scope(this);
  contents_->ClearRules();
  contents_->ParseString(text);
}

void CSSStyleSheet::SetAlternateFromConstructor(
    bool alternate_from_constructor) {
  alternate_from_constructor_ = alternate_from_constructor;
}

bool CSSStyleSheet::IsAlternate() const {
  if (owner_node_) {
    return owner_node_->IsElementNode() &&
           ToElement(owner_node_)->getAttribute(relAttr).Contains("alternate");
  }
  return alternate_from_constructor_;
}

bool CSSStyleSheet::CanBeActivated(
    const String& current_preferrable_name) const {
  if (disabled())
    return false;

  if (owner_node_ && owner_node_->IsInShadowTree()) {
    if (IsHTMLStyleElement(owner_node_) || IsSVGStyleElement(owner_node_))
      return true;
    if (IsHTMLLinkElement(owner_node_) &&
        ToHTMLLinkElement(owner_node_)->IsImport())
      return !IsAlternate();
  }

  if (!owner_node_ ||
      owner_node_->getNodeType() == Node::kProcessingInstructionNode ||
      !IsHTMLLinkElement(owner_node_) ||
      !ToHTMLLinkElement(owner_node_)->IsEnabledViaScript()) {
    if (!title_.IsEmpty() && title_ != current_preferrable_name)
      return false;
  }

  if (IsAlternate() && title_.IsEmpty())
    return false;

  return true;
}

void CSSStyleSheet::Trace(blink::Visitor* visitor) {
  visitor->Trace(contents_);
  visitor->Trace(owner_node_);
  visitor->Trace(owner_rule_);
  visitor->Trace(constructed_tree_scopes_);
  visitor->Trace(media_cssom_wrapper_);
  visitor->Trace(child_rule_cssom_wrappers_);
  visitor->Trace(rule_list_cssom_wrapper_);
  StyleSheet::Trace(visitor);
}

}  // namespace blink
