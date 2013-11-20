/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/editing/TextIterator.h"

#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Range.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/geometry/IntSize.h"
#include "wtf/Compiler.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class TextIteratorTest : public ::testing::Test {
protected:
    virtual void SetUp() OVERRIDE;

    HTMLDocument& document() const;

    Vector<String> iterate(const char* bodyInnerHTML, TextIteratorBehavior = TextIteratorDefaultBehavior);

    void setBodyInnerHTML(const char*);
    PassRefPtr<Range> getBodyRange() const;

private:
    OwnPtr<DummyPageHolder> m_dummyPageHolder;

    HTMLDocument* m_document;
};

void TextIteratorTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_document = toHTMLDocument(&m_dummyPageHolder->document());
    ASSERT(m_document);
}

Vector<String> TextIteratorTest::iterate(const char* bodyInnerHTML, TextIteratorBehavior iteratorBehavior)
{
    setBodyInnerHTML(bodyInnerHTML);

    RefPtr<Range> range = getBodyRange();
    TextIterator textIterator(range.get(), iteratorBehavior);
    Vector<String> textChunks;
    while (!textIterator.atEnd()) {
        textChunks.append(textIterator.substring(0, textIterator.length()));
        textIterator.advance();
    }
    return textChunks;
}

HTMLDocument& TextIteratorTest::document() const
{
    return *m_document;
}

void TextIteratorTest::setBodyInnerHTML(const char* bodyContent)
{
    document().body()->setInnerHTML(String::fromUTF8(bodyContent), ASSERT_NO_EXCEPTION);
    document().view()->layout(); // Force renderers to be created; TextIterator needs them.
}

PassRefPtr<Range> TextIteratorTest::getBodyRange() const
{
    RefPtr<Range> range(Range::create(document()));
    range->selectNode(document().body());
    return range.release();
}

TEST_F(TextIteratorTest, BasicIteration)
{
    static const char* input = "<p>Hello, \ntext</p><p>iterator.</p>";
    static const char* expectedTextChunksRawString[] = {
        "Hello, ",
        "text",
        "\n",
        "\n",
        "iterator."
    };
    Vector<String> expectedTextChunks;
    expectedTextChunks.append(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));
    Vector<String> actualTextChunks = iterate(input);
    EXPECT_EQ(expectedTextChunks, actualTextChunks);
}

TEST_F(TextIteratorTest, NotEnteringTextControls)
{
    static const char* input = "<p>Hello <input type=\"text\" value=\"input\">!</p>";
    static const char* expectedTextChunksRawString[] = {
        "Hello ",
        "",
        "!",
    };
    Vector<String> expectedTextChunks;
    expectedTextChunks.append(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));
    Vector<String> actualTextChunks = iterate(input);
    EXPECT_EQ(expectedTextChunks, actualTextChunks);
}

TEST_F(TextIteratorTest, EnteringTextControlsWithOption)
{
    static const char* input = "<p>Hello <input type=\"text\" value=\"input\">!</p>";
    static const char* expectedTextChunksRawString[] = {
        "Hello ",
        "\n",
        "input",
        "!",
    };
    Vector<String> expectedTextChunks;
    expectedTextChunks.append(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));
    Vector<String> actualTextChunks = iterate(input, TextIteratorEntersTextControls);
    EXPECT_EQ(expectedTextChunks, actualTextChunks);
}

}
