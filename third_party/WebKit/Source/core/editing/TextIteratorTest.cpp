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
#include "core/frame/DOMWindow.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/loader/EmptyClients.h"
#include "core/page/Page.h"
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
    virtual void TearDown() OVERRIDE;

    HTMLDocument& document() const;

    void setBodyInnerHTML(const char*);
    PassRefPtr<Range> getBodyRange() const;

private:
    Page::PageClients m_pageClients;
    OwnPtr<Page> m_page;

    EmptyFrameLoaderClient m_frameLoaderClient;
    RefPtr<Frame> m_frame;

    HTMLDocument* m_document;
};

void TextIteratorTest::SetUp()
{
    // FIXME: Creation of fake frames should be factored out and should be done in one central place.
    fillWithEmptyClients(m_pageClients);
    m_page = adoptPtr(new Page(m_pageClients));

    m_frame = Frame::create(FrameInit::create(0, m_page.get(), &m_frameLoaderClient));
    m_frame->setView(FrameView::create(m_frame.get(), IntSize(800, 600)));
    m_frame->init();

    m_document = toHTMLDocument(m_frame->domWindow()->document());
    ASSERT(m_document);
}

void TextIteratorTest::TearDown()
{
    m_document = 0;
    m_frame.clear();
    m_page.clear();
}

HTMLDocument& TextIteratorTest::document() const
{
    return *m_document;
}

void TextIteratorTest::setBodyInnerHTML(const char* bodyContent)
{
    document().body()->setInnerHTML(String::fromUTF8(bodyContent), ASSERT_NO_EXCEPTION);
    m_document->view()->layout(); // Force renderers to be created; TextIterator needs them.
}

PassRefPtr<Range> TextIteratorTest::getBodyRange() const
{
    RefPtr<Range> range(Range::create(document()));
    range->selectNode(document().body());
    return range.release();
}

TEST_F(TextIteratorTest, BasicIteration)
{
    setBodyInnerHTML("<p>Hello, \ntext</p><p>iterator.</p>");
    static const char* expectedTextChunksRawString[] = {
        "Hello, ",
        "text",
        "\n",
        "\n",
        "iterator."
    };
    static const size_t numberOfExpectedTextChunks = WTF_ARRAY_LENGTH(expectedTextChunksRawString);
    Vector<String> expectedTextChunks;
    expectedTextChunks.append(expectedTextChunksRawString, numberOfExpectedTextChunks);

    RefPtr<Range> range = getBodyRange();
    TextIterator textIterator(range.get());
    Vector<String> actualTextChunks;

    while (!textIterator.atEnd()) {
        actualTextChunks.append(textIterator.substring(0, textIterator.length()));
        textIterator.advance();
    }

    EXPECT_EQ(expectedTextChunks, actualTextChunks);
}

}
