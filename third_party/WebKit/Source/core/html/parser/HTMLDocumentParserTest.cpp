// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/HTMLDocumentParser.h"

#include "core/html/HTMLDocument.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/loader/PrerendererClient.h"
#include "core/loader/TextResourceDecoderBuilder.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

namespace {

class TestPrerendererClient : public PrerendererClient {
 public:
  TestPrerendererClient(Page& page, bool is_prefetch_only)
      : PrerendererClient(page, nullptr), is_prefetch_only_(is_prefetch_only) {}

 private:
  void WillAddPrerender(Prerender*) override {}
  bool IsPrefetchOnly() override { return is_prefetch_only_; }

  bool is_prefetch_only_;
};

class HTMLDocumentParserTest : public testing::Test {
 protected:
  HTMLDocumentParserTest() : dummy_page_holder_(DummyPageHolder::Create()) {
    dummy_page_holder_->GetDocument().SetURL(
        KURL(NullURL(), "https://example.test"));
  }

  HTMLDocumentParser* CreateParser(HTMLDocument& document) {
    HTMLDocumentParser* parser =
        HTMLDocumentParser::Create(document, kForceSynchronousParsing);
    TextResourceDecoderBuilder decoder_builder("text/html", g_null_atom);
    std::unique_ptr<TextResourceDecoder> decoder(
        decoder_builder.BuildFor(&document));
    parser->SetDecoder(std::move(decoder));
    return parser;
  }

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

}  // namespace

TEST_F(HTMLDocumentParserTest, AppendPrefetch) {
  HTMLDocument& document = ToHTMLDocument(dummy_page_holder_->GetDocument());
  ProvidePrerendererClientTo(
      *document.GetPage(),
      new TestPrerendererClient(*document.GetPage(), true));
  EXPECT_TRUE(document.IsPrefetchOnly());
  HTMLDocumentParser* parser = CreateParser(document);

  const char kBytes[] = "<ht";
  parser->AppendBytes(kBytes, sizeof(kBytes));
  // The bytes are forwarded to the preload scanner, not to the tokenizer.
  HTMLParserScriptRunnerHost* script_runner_host =
      parser->AsHTMLParserScriptRunnerHostForTesting();
  EXPECT_TRUE(script_runner_host->HasPreloadScanner());
  EXPECT_EQ(HTMLTokenizer::kDataState, parser->Tokenizer()->GetState());
}

TEST_F(HTMLDocumentParserTest, AppendNoPrefetch) {
  HTMLDocument& document = ToHTMLDocument(dummy_page_holder_->GetDocument());
  EXPECT_FALSE(document.IsPrefetchOnly());
  // Use ForceSynchronousParsing to allow calling append().
  HTMLDocumentParser* parser = CreateParser(document);

  const char kBytes[] = "<ht";
  parser->AppendBytes(kBytes, sizeof(kBytes));
  // The bytes are forwarded to the tokenizer.
  HTMLParserScriptRunnerHost* script_runner_host =
      parser->AsHTMLParserScriptRunnerHostForTesting();
  EXPECT_FALSE(script_runner_host->HasPreloadScanner());
  EXPECT_EQ(HTMLTokenizer::kTagNameState, parser->Tokenizer()->GetState());
}

}  // namespace blink
