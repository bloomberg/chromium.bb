/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/parser/html_tree_builder_simulator.h"

#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"
#include "third_party/blink/renderer/core/html/parser/html_tree_builder.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

using namespace html_names;

static bool TokenExitsForeignContent(const CompactHTMLToken& token) {
  // FIXME: This is copied from HTMLTreeBuilder::processTokenInForeignContent
  // and changed to use threadSafeHTMLNamesMatch.
  const String& tag_name = token.Data();
  return ThreadSafeMatch(tag_name, kBTag) ||
         ThreadSafeMatch(tag_name, kBigTag) ||
         ThreadSafeMatch(tag_name, kBlockquoteTag) ||
         ThreadSafeMatch(tag_name, kBodyTag) ||
         ThreadSafeMatch(tag_name, kBrTag) ||
         ThreadSafeMatch(tag_name, kCenterTag) ||
         ThreadSafeMatch(tag_name, kCodeTag) ||
         ThreadSafeMatch(tag_name, kDdTag) ||
         ThreadSafeMatch(tag_name, kDivTag) ||
         ThreadSafeMatch(tag_name, kDlTag) ||
         ThreadSafeMatch(tag_name, kDtTag) ||
         ThreadSafeMatch(tag_name, kEmTag) ||
         ThreadSafeMatch(tag_name, kEmbedTag) ||
         ThreadSafeMatch(tag_name, kH1Tag) ||
         ThreadSafeMatch(tag_name, kH2Tag) ||
         ThreadSafeMatch(tag_name, kH3Tag) ||
         ThreadSafeMatch(tag_name, kH4Tag) ||
         ThreadSafeMatch(tag_name, kH5Tag) ||
         ThreadSafeMatch(tag_name, kH6Tag) ||
         ThreadSafeMatch(tag_name, kHeadTag) ||
         ThreadSafeMatch(tag_name, kHrTag) ||
         ThreadSafeMatch(tag_name, kITag) ||
         ThreadSafeMatch(tag_name, kImgTag) ||
         ThreadSafeMatch(tag_name, kLiTag) ||
         ThreadSafeMatch(tag_name, kListingTag) ||
         ThreadSafeMatch(tag_name, kMenuTag) ||
         ThreadSafeMatch(tag_name, kMetaTag) ||
         ThreadSafeMatch(tag_name, kNobrTag) ||
         ThreadSafeMatch(tag_name, kOlTag) ||
         ThreadSafeMatch(tag_name, kPTag) ||
         ThreadSafeMatch(tag_name, kPreTag) ||
         ThreadSafeMatch(tag_name, kRubyTag) ||
         ThreadSafeMatch(tag_name, kSTag) ||
         ThreadSafeMatch(tag_name, kSmallTag) ||
         ThreadSafeMatch(tag_name, kSpanTag) ||
         ThreadSafeMatch(tag_name, kStrongTag) ||
         ThreadSafeMatch(tag_name, kStrikeTag) ||
         ThreadSafeMatch(tag_name, kSubTag) ||
         ThreadSafeMatch(tag_name, kSupTag) ||
         ThreadSafeMatch(tag_name, kTableTag) ||
         ThreadSafeMatch(tag_name, kTtTag) ||
         ThreadSafeMatch(tag_name, kUTag) ||
         ThreadSafeMatch(tag_name, kUlTag) ||
         ThreadSafeMatch(tag_name, kVarTag) ||
         (ThreadSafeMatch(tag_name, kFontTag) &&
          (token.GetAttributeItem(kColorAttr) ||
           token.GetAttributeItem(kFaceAttr) ||
           token.GetAttributeItem(kSizeAttr)));
}

static bool TokenExitsMath(const CompactHTMLToken& token) {
  // FIXME: This is copied from HTMLElementStack::isMathMLTextIntegrationPoint
  // and changed to use threadSafeMatch.
  const String& tag_name = token.Data();
  return ThreadSafeMatch(tag_name, mathml_names::kMiTag) ||
         ThreadSafeMatch(tag_name, mathml_names::kMoTag) ||
         ThreadSafeMatch(tag_name, mathml_names::kMnTag) ||
         ThreadSafeMatch(tag_name, mathml_names::kMsTag) ||
         ThreadSafeMatch(tag_name, mathml_names::kMtextTag);
}

static bool TokenExitsInSelect(const CompactHTMLToken& token) {
  // https://html.spec.whatwg.org/C/#parsing-main-inselect
  const String& tag_name = token.Data();
  return ThreadSafeMatch(tag_name, kInputTag) ||
         ThreadSafeMatch(tag_name, kKeygenTag) ||
         ThreadSafeMatch(tag_name, kTextareaTag);
}

HTMLTreeBuilderSimulator::HTMLTreeBuilderSimulator(
    const HTMLParserOptions& options)
    : options_(options), in_select_insertion_mode_(false) {
  namespace_stack_.push_back(HTML);
}

HTMLTreeBuilderSimulator::State HTMLTreeBuilderSimulator::StateFor(
    HTMLTreeBuilder* tree_builder) {
  DCHECK(IsMainThread());
  State namespace_stack;
  for (HTMLElementStack::ElementRecord* record =
           tree_builder->OpenElements()->TopRecord();
       record; record = record->Next()) {
    Namespace current_namespace = HTML;
    if (record->NamespaceURI() == svg_names::kNamespaceURI)
      current_namespace = SVG;
    else if (record->NamespaceURI() == mathml_names::kNamespaceURI)
      current_namespace = kMathML;

    if (namespace_stack.IsEmpty() ||
        namespace_stack.back() != current_namespace)
      namespace_stack.push_back(current_namespace);
  }
  namespace_stack.Reverse();
  return namespace_stack;
}

HTMLTreeBuilderSimulator::SimulatedToken HTMLTreeBuilderSimulator::Simulate(
    const CompactHTMLToken& token,
    HTMLTokenizer* tokenizer) {
  SimulatedToken simulated_token = kOtherToken;

  if (token.GetType() == HTMLToken::kStartTag) {
    const String& tag_name = token.Data();
    if (ThreadSafeMatch(tag_name, svg_names::kSVGTag))
      namespace_stack_.push_back(SVG);
    if (ThreadSafeMatch(tag_name, mathml_names::kMathTag))
      namespace_stack_.push_back(kMathML);
    if (InForeignContent() && TokenExitsForeignContent(token))
      namespace_stack_.pop_back();
    if (IsHTMLIntegrationPointForStartTag(token) ||
        (namespace_stack_.back() == kMathML && TokenExitsMath(token))) {
      namespace_stack_.push_back(HTML);
    } else if (!InForeignContent()) {
      // FIXME: This is just a copy of Tokenizer::updateStateFor which uses
      // threadSafeMatches.
      if (ThreadSafeMatch(tag_name, kTextareaTag) ||
          ThreadSafeMatch(tag_name, kTitleTag)) {
        tokenizer->SetState(HTMLTokenizer::kRCDATAState);
      } else if (ThreadSafeMatch(tag_name, kScriptTag)) {
        tokenizer->SetState(HTMLTokenizer::kScriptDataState);

        String type_attribute_value;
        if (auto* item = token.GetAttributeItem(kTypeAttr)) {
          type_attribute_value = item->Value();
        }

        String language_attribute_value;
        if (auto* item = token.GetAttributeItem(kLanguageAttr)) {
          language_attribute_value = item->Value();
        }

        if (ScriptLoader::IsValidScriptTypeAndLanguage(
                type_attribute_value, language_attribute_value,
                ScriptLoader::kAllowLegacyTypeInTypeAttribute)) {
          simulated_token = kValidScriptStart;
        }
      } else if (ThreadSafeMatch(tag_name, kLinkTag)) {
        simulated_token = kLink;
      } else if (!in_select_insertion_mode_) {
        // If we're in the "in select" insertion mode, all of these tags are
        // ignored, so we shouldn't change the tokenizer state:
        // https://html.spec.whatwg.org/C/#parsing-main-inselect
        if (ThreadSafeMatch(tag_name, kPlaintextTag) &&
            !in_select_insertion_mode_) {
          tokenizer->SetState(HTMLTokenizer::kPLAINTEXTState);
        } else if (ThreadSafeMatch(tag_name, kStyleTag) ||
                   ThreadSafeMatch(tag_name, kIFrameTag) ||
                   ThreadSafeMatch(tag_name, kXmpTag) ||
                   ThreadSafeMatch(tag_name, kNoembedTag) ||
                   ThreadSafeMatch(tag_name, kNoframesTag) ||
                   (ThreadSafeMatch(tag_name, kNoscriptTag) &&
                    options_.script_enabled)) {
          tokenizer->SetState(HTMLTokenizer::kRAWTEXTState);
        }
      }

      // We need to track whether we're in the "in select" insertion mode
      // in order to determine whether '<plaintext>' will put the tokenizer
      // into PLAINTEXTState, and whether '<xmp>' and others will consume
      // textual content.
      //
      // https://html.spec.whatwg.org/C/#parsing-main-inselect
      if (ThreadSafeMatch(tag_name, kSelectTag)) {
        in_select_insertion_mode_ = true;
      } else if (in_select_insertion_mode_ && TokenExitsInSelect(token)) {
        in_select_insertion_mode_ = false;
      }
    }
  }

  if (token.GetType() == HTMLToken::kEndTag ||
      (token.GetType() == HTMLToken::kStartTag && token.SelfClosing() &&
       InForeignContent())) {
    const String& tag_name = token.Data();
    if ((namespace_stack_.back() == SVG &&
         ThreadSafeMatch(tag_name, svg_names::kSVGTag)) ||
        (namespace_stack_.back() == kMathML &&
         ThreadSafeMatch(tag_name, mathml_names::kMathTag)) ||
        IsHTMLIntegrationPointForEndTag(token) ||
        (namespace_stack_.Contains(kMathML) &&
         namespace_stack_.back() == HTML && TokenExitsMath(token))) {
      namespace_stack_.pop_back();
    }
    if (ThreadSafeMatch(tag_name, kScriptTag)) {
      if (!InForeignContent())
        tokenizer->SetState(HTMLTokenizer::kDataState);
      return kScriptEnd;
    }
    if (ThreadSafeMatch(tag_name, kSelectTag))
      in_select_insertion_mode_ = false;
    if (ThreadSafeMatch(tag_name, kStyleTag))
      simulated_token = kStyleEnd;
  }
  if (token.GetType() == HTMLToken::kStartTag &&
      simulated_token == kOtherToken) {
    const String& tag_name = token.Data();
    // Use the presence of a dash in the tag name as a proxy for
    // "is a custom element".
    if (tag_name.find('-') != kNotFound)
      simulated_token = kCustomElementBegin;
  }

  // FIXME: Also setForceNullCharacterReplacement when in text mode.
  tokenizer->SetForceNullCharacterReplacement(InForeignContent());
  tokenizer->SetShouldAllowCDATA(InForeignContent());
  return simulated_token;
}

// https://html.spec.whatwg.org/C/#html-integration-point
bool HTMLTreeBuilderSimulator::IsHTMLIntegrationPointForStartTag(
    const CompactHTMLToken& token) const {
  DCHECK(token.GetType() == HTMLToken::kStartTag) << token.GetType();

  Namespace tokens_ns = namespace_stack_.back();
  const String& tag_name = token.Data();
  if (tokens_ns == kMathML) {
    if (!ThreadSafeMatch(tag_name, mathml_names::kAnnotationXmlTag))
      return false;
    if (auto* encoding = token.GetAttributeItem(mathml_names::kEncodingAttr)) {
      return EqualIgnoringASCIICase(encoding->Value(), "text/html") ||
             EqualIgnoringASCIICase(encoding->Value(), "application/xhtml+xml");
    }
  } else if (tokens_ns == SVG) {
    // FIXME: It's very fragile that we special case foreignObject here to be
    // case-insensitive.
    if (DeprecatedEqualIgnoringCase(tag_name,
                                    svg_names::kForeignObjectTag.LocalName()))
      return true;
    return ThreadSafeMatch(tag_name, svg_names::kDescTag) ||
           ThreadSafeMatch(tag_name, svg_names::kTitleTag);
  }
  return false;
}

// https://html.spec.whatwg.org/C/#html-integration-point
bool HTMLTreeBuilderSimulator::IsHTMLIntegrationPointForEndTag(
    const CompactHTMLToken& token) const {
  if (token.GetType() != HTMLToken::kEndTag)
    return false;

  // If it's inside an HTML integration point, the top namespace is
  // HTML, and its next namespace is not HTML.
  if (namespace_stack_.back() != HTML)
    return false;
  if (namespace_stack_.size() < 2)
    return false;
  Namespace tokens_ns = namespace_stack_[namespace_stack_.size() - 2];

  const String& tag_name = token.Data();
  if (tokens_ns == kMathML)
    return ThreadSafeMatch(tag_name, mathml_names::kAnnotationXmlTag);
  if (tokens_ns == SVG) {
    // FIXME: It's very fragile that we special case foreignObject here to be
    // case-insensitive.
    if (DeprecatedEqualIgnoringCase(tag_name,
                                    svg_names::kForeignObjectTag.LocalName()))
      return true;
    return ThreadSafeMatch(tag_name, svg_names::kDescTag) ||
           ThreadSafeMatch(tag_name, svg_names::kTitleTag);
  }
  return false;
}

}  // namespace blink
