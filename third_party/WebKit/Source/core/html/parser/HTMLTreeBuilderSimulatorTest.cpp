// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/HTMLTreeBuilderSimulator.h"

#include "core/html/parser/CompactHTMLToken.h"
#include "core/html/parser/HTMLParserOptions.h"
#include "core/html/parser/HTMLToken.h"
#include "core/html/parser/HTMLTokenizer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/TextPosition.h"

#include <memory>

namespace blink {

namespace {

static void startTag(HTMLToken& token, const char* name)
{
    token.clear();
    token.beginStartTag(name[0]);
    size_t length = strlen(name);
    for (size_t i = 1; i < length; ++i)
        token.appendToName(name[i]);
}

static void simulate(HTMLTreeBuilderSimulator& simulator, HTMLToken& token, HTMLTokenizer* tokenizer)
{
    CompactHTMLToken compactToken(&token, TextPosition::minimumPosition());
    simulator.simulate(compactToken, tokenizer);
}

// This is a regression test for crbug.com/542803
TEST(HTMLTreeBuilderSimulatorTest, ChangeStateInForeignContentAtHTMLIntegrationPoint)
{
    HTMLParserOptions options;
    HTMLTreeBuilderSimulator simulator(options);
    std::unique_ptr<HTMLTokenizer> tokenizer = HTMLTokenizer::create(options);
    HTMLToken token;

    startTag(token, "svg");
    simulate(simulator, token, tokenizer.get()); // open elements: svg
    EXPECT_EQ(HTMLTokenizer::DataState, tokenizer->getState())
        << "the svg start tag should have put the simulator into 'in body' "
        << "state";

    startTag(token, "title");
    simulate(simulator, token, tokenizer.get()); // svg > title
    EXPECT_EQ(HTMLTokenizer::DataState, tokenizer->getState())
        << "the title start tag should not have flipped the simulator into "
        << "RCDATA state because svg is not a HTML integration point";

    startTag(token, "desc");
    simulate(simulator, token, tokenizer.get()); // svg > title > desc
    EXPECT_EQ(HTMLTokenizer::DataState, tokenizer->getState())
        << "the desc tag should not have flipped the simulator into the "
        << "any unusual state because desc parsing rules do not change state, "
        << "despite title being a HTML integration point";

    startTag(token, "title");
    simulate(simulator, token, tokenizer.get()); // svg > title > desc > title
    EXPECT_EQ(HTMLTokenizer::RCDATAState, tokenizer->getState())
        << "the title start tag should have flipped the simulator into the "
        << "RCDATA state for title, despite being in foreign content mode, "
        << "because desc is a HTML integration point";
}

}

} // namespace blink
