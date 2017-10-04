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

#include "core/html/parser/HTMLTreeBuilderSimulator.h"

#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/parser/HTMLTokenizer.h"
#include "core/html/parser/HTMLTreeBuilder.h"
#include "core/html_names.h"
#include "core/mathml_names.h"
#include "core/svg_names.h"

namespace blink {

using namespace HTMLNames;

static bool TokenExitsForeignContent(const CompactHTMLToken& token) {
  // FIXME: This is copied from HTMLTreeBuilder::processTokenInForeignContent
  // and changed to use threadSafeHTMLNamesMatch.
  const String& tag_name = token.Data();
  return ThreadSafeMatch(tag_name, bTag) || ThreadSafeMatch(tag_name, bigTag) ||
         ThreadSafeMatch(tag_name, blockquoteTag) ||
         ThreadSafeMatch(tag_name, bodyTag) ||
         ThreadSafeMatch(tag_name, brTag) ||
         ThreadSafeMatch(tag_name, centerTag) ||
         ThreadSafeMatch(tag_name, codeTag) ||
         ThreadSafeMatch(tag_name, ddTag) ||
         ThreadSafeMatch(tag_name, divTag) ||
         ThreadSafeMatch(tag_name, dlTag) || ThreadSafeMatch(tag_name, dtTag) ||
         ThreadSafeMatch(tag_name, emTag) ||
         ThreadSafeMatch(tag_name, embedTag) ||
         ThreadSafeMatch(tag_name, h1Tag) || ThreadSafeMatch(tag_name, h2Tag) ||
         ThreadSafeMatch(tag_name, h3Tag) || ThreadSafeMatch(tag_name, h4Tag) ||
         ThreadSafeMatch(tag_name, h5Tag) || ThreadSafeMatch(tag_name, h6Tag) ||
         ThreadSafeMatch(tag_name, headTag) ||
         ThreadSafeMatch(tag_name, hrTag) || ThreadSafeMatch(tag_name, iTag) ||
         ThreadSafeMatch(tag_name, imgTag) ||
         ThreadSafeMatch(tag_name, liTag) ||
         ThreadSafeMatch(tag_name, listingTag) ||
         ThreadSafeMatch(tag_name, menuTag) ||
         ThreadSafeMatch(tag_name, metaTag) ||
         ThreadSafeMatch(tag_name, nobrTag) ||
         ThreadSafeMatch(tag_name, olTag) || ThreadSafeMatch(tag_name, pTag) ||
         ThreadSafeMatch(tag_name, preTag) ||
         ThreadSafeMatch(tag_name, rubyTag) ||
         ThreadSafeMatch(tag_name, sTag) ||
         ThreadSafeMatch(tag_name, smallTag) ||
         ThreadSafeMatch(tag_name, spanTag) ||
         ThreadSafeMatch(tag_name, strongTag) ||
         ThreadSafeMatch(tag_name, strikeTag) ||
         ThreadSafeMatch(tag_name, subTag) ||
         ThreadSafeMatch(tag_name, supTag) ||
         ThreadSafeMatch(tag_name, tableTag) ||
         ThreadSafeMatch(tag_name, ttTag) || ThreadSafeMatch(tag_name, uTag) ||
         ThreadSafeMatch(tag_name, ulTag) ||
         ThreadSafeMatch(tag_name, varTag) ||
         (ThreadSafeMatch(tag_name, fontTag) &&
          (token.GetAttributeItem(colorAttr) ||
           token.GetAttributeItem(faceAttr) ||
           token.GetAttributeItem(sizeAttr)));
}

static bool TokenExitsSVG(const CompactHTMLToken& token) {
  // FIXME: It's very fragile that we special case foreignObject here to be
  // case-insensitive.
  return DeprecatedEqualIgnoringCase(token.Data(),
                                     SVGNames::foreignObjectTag.LocalName());
}

static bool TokenExitsMath(const CompactHTMLToken& token) {
  // FIXME: This is copied from HTMLElementStack::isMathMLTextIntegrationPoint
  // and changed to use threadSafeMatch.
  const String& tag_name = token.Data();
  return ThreadSafeMatch(tag_name, MathMLNames::miTag) ||
         ThreadSafeMatch(tag_name, MathMLNames::moTag) ||
         ThreadSafeMatch(tag_name, MathMLNames::mnTag) ||
         ThreadSafeMatch(tag_name, MathMLNames::msTag) ||
         ThreadSafeMatch(tag_name, MathMLNames::mtextTag);
}

static bool TokenExitsInSelect(const CompactHTMLToken& token) {
  // https://html.spec.whatwg.org/#parsing-main-inselect
  const String& tag_name = token.Data();
  return ThreadSafeMatch(tag_name, inputTag) ||
         ThreadSafeMatch(tag_name, keygenTag) ||
         ThreadSafeMatch(tag_name, textareaTag);
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
    if (record->NamespaceURI() == SVGNames::svgNamespaceURI)
      current_namespace = SVG;
    else if (record->NamespaceURI() == MathMLNames::mathmlNamespaceURI)
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
    if (ThreadSafeMatch(tag_name, SVGNames::svgTag))
      namespace_stack_.push_back(SVG);
    if (ThreadSafeMatch(tag_name, MathMLNames::mathTag))
      namespace_stack_.push_back(kMathML);
    if (InForeignContent() && TokenExitsForeignContent(token))
      namespace_stack_.pop_back();
    if ((namespace_stack_.back() == SVG && TokenExitsSVG(token)) ||
        (namespace_stack_.back() == kMathML && TokenExitsMath(token)))
      namespace_stack_.push_back(HTML);
    if (!InForeignContent()) {
      // FIXME: This is just a copy of Tokenizer::updateStateFor which uses
      // threadSafeMatches.
      if (ThreadSafeMatch(tag_name, textareaTag) ||
          ThreadSafeMatch(tag_name, titleTag)) {
        tokenizer->SetState(HTMLTokenizer::kRCDATAState);
      } else if (ThreadSafeMatch(tag_name, scriptTag)) {
        tokenizer->SetState(HTMLTokenizer::kScriptDataState);
        simulated_token = kScriptStart;
      } else if (ThreadSafeMatch(tag_name, linkTag)) {
        simulated_token = kLink;
      } else if (!in_select_insertion_mode_) {
        // If we're in the "in select" insertion mode, all of these tags are
        // ignored, so we shouldn't change the tokenizer state:
        // https://html.spec.whatwg.org/#parsing-main-inselect
        if (ThreadSafeMatch(tag_name, plaintextTag) &&
            !in_select_insertion_mode_) {
          tokenizer->SetState(HTMLTokenizer::kPLAINTEXTState);
        } else if (ThreadSafeMatch(tag_name, styleTag) ||
                   ThreadSafeMatch(tag_name, iframeTag) ||
                   ThreadSafeMatch(tag_name, xmpTag) ||
                   (ThreadSafeMatch(tag_name, noembedTag) &&
                    options_.plugins_enabled) ||
                   ThreadSafeMatch(tag_name, noframesTag) ||
                   (ThreadSafeMatch(tag_name, noscriptTag) &&
                    options_.script_enabled)) {
          tokenizer->SetState(HTMLTokenizer::kRAWTEXTState);
        }
      }

      // We need to track whether we're in the "in select" insertion mode
      // in order to determine whether '<plaintext>' will put the tokenizer
      // into PLAINTEXTState, and whether '<xmp>' and others will consume
      // textual content.
      //
      // https://html.spec.whatwg.org/#parsing-main-inselect
      if (ThreadSafeMatch(tag_name, selectTag)) {
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
         ThreadSafeMatch(tag_name, SVGNames::svgTag)) ||
        (namespace_stack_.back() == kMathML &&
         ThreadSafeMatch(tag_name, MathMLNames::mathTag)) ||
        (namespace_stack_.Contains(SVG) && namespace_stack_.back() == HTML &&
         TokenExitsSVG(token)) ||
        (namespace_stack_.Contains(kMathML) &&
         namespace_stack_.back() == HTML && TokenExitsMath(token))) {
      namespace_stack_.pop_back();
    }
    if (ThreadSafeMatch(tag_name, scriptTag)) {
      if (!InForeignContent())
        tokenizer->SetState(HTMLTokenizer::kDataState);
      return kScriptEnd;
    } else if (ThreadSafeMatch(tag_name, selectTag)) {
      in_select_insertion_mode_ = false;
    }
    if (ThreadSafeMatch(tag_name, styleTag))
      simulated_token = kStyleEnd;
  }

  // FIXME: Also setForceNullCharacterReplacement when in text mode.
  tokenizer->SetForceNullCharacterReplacement(InForeignContent());
  tokenizer->SetShouldAllowCDATA(InForeignContent());
  return simulated_token;
}

}  // namespace blink
