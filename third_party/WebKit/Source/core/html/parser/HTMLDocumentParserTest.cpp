// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/HTMLDocumentParser.h"

#include <memory>
#include "core/html/HTMLDocument.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/loader/PrerendererClient.h"
#include "core/loader/TextResourceDecoderBuilder.h"
#include "core/testing/PageTestBase.h"
#include "public/platform/WebPrerenderingSupport.h"
#include "testing/gtest/include/gtest/gtest.h"

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

class TestPrerenderingSupport : public WebPrerenderingSupport {
 public:
  TestPrerenderingSupport() { Initialize(this); }

  virtual void Add(const WebPrerender&) {}
  virtual void Cancel(const WebPrerender&) {}
  virtual void Abandon(const WebPrerender&) {}
  virtual void PrefetchFinished() { prefetch_finished_ = true; }

  bool IsPrefetchFinished() const { return prefetch_finished_; }

 private:
  bool prefetch_finished_ = false;
};

class HTMLDocumentParserTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().SetURL(KURL("https://example.test"));
  }

  HTMLDocumentParser* CreateParser(HTMLDocument& document) {
    HTMLDocumentParser* parser =
        HTMLDocumentParser::Create(document, kForceSynchronousParsing);
    std::unique_ptr<TextResourceDecoder> decoder(
        BuildTextResourceDecoderFor(&document, "text/html", g_null_atom));
    parser->SetDecoder(std::move(decoder));
    return parser;
  }

  bool PrefetchFinishedCleanly() {
    return prerendering_support_.IsPrefetchFinished();
  }

 private:
  TestPrerenderingSupport prerendering_support_;
};

}  // namespace

TEST_F(HTMLDocumentParserTest, AppendPrefetch) {
  HTMLDocument& document = ToHTMLDocument(GetDocument());
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
  // Finishing should not cause parsing to start (verified via an internal
  // DCHECK).
  static_cast<DocumentParser*>(parser)->Finish();
  EXPECT_EQ(HTMLTokenizer::kDataState, parser->Tokenizer()->GetState());
  EXPECT_TRUE(PrefetchFinishedCleanly());
}

TEST_F(HTMLDocumentParserTest, AppendNoPrefetch) {
  HTMLDocument& document = ToHTMLDocument(GetDocument());
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
