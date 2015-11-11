// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/HTMLInputElement.h"

#include "core/dom/Document.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/testing/DummyPageHolder.h"
#include <gtest/gtest.h>

namespace blink {

TEST(HTMLInputElementTest, create)
{
    const RefPtrWillBeRawPtr<Document> document = Document::create();
    RefPtrWillBeRawPtr<HTMLInputElement> input = HTMLInputElement::create(*document, nullptr, /* createdByParser */ false);
    EXPECT_NE(nullptr, input->userAgentShadowRoot());

    input = HTMLInputElement::create(*document, nullptr, /* createdByParser */ true);
    EXPECT_EQ(nullptr, input->userAgentShadowRoot());
    input->parserSetAttributes(Vector<Attribute>());
    EXPECT_NE(nullptr, input->userAgentShadowRoot());
}

TEST(HTMLInputElementTest, NoAssertWhenMovedInNewDocument)
{
    const RefPtrWillBeRawPtr<Document> documentWithoutFrame = Document::create();
    EXPECT_EQ(nullptr, documentWithoutFrame->frameHost());
    RefPtrWillBeRawPtr<HTMLHtmlElement> html = HTMLHtmlElement::create(*documentWithoutFrame);
    html->appendChild(HTMLBodyElement::create(*documentWithoutFrame));

    // Create an input element with type "range" inside a document without frame.
    toHTMLBodyElement(html->firstChild())->setInnerHTML("<input type='range' />", ASSERT_NO_EXCEPTION);
    documentWithoutFrame->appendChild(html.release());

    OwnPtr<DummyPageHolder> pageHolder = DummyPageHolder::create();
    auto& document = pageHolder->document();
    EXPECT_NE(nullptr, document.frameHost());

    // Put the input element inside a document with frame.
    document.body()->appendChild(documentWithoutFrame->body()->firstChild());

    // Remove the input element and all refs to it so it gets deleted before the document.
    // The assert in |EventHandlerRegistry::updateEventHandlerTargets()| should not be triggered.
    document.body()->removeChild(document.body()->firstChild());
}

TEST(HTMLInputElementTest, DefaultToolTip)
{
    RefPtrWillBeRawPtr<Document> document = Document::create();
    RefPtrWillBeRawPtr<HTMLHtmlElement> html = HTMLHtmlElement::create(*document);
    html->appendChild(HTMLBodyElement::create(*document));
    RefPtrWillBeRawPtr<HTMLInputElement> input = HTMLInputElement::create(*document, nullptr, false);
    input->setBooleanAttribute(HTMLNames::requiredAttr, true);
    toHTMLBodyElement(html->firstChild())->appendChild(input.get());
    document->appendChild(html.release());
    EXPECT_EQ("<<ValidationValueMissing>>", input->defaultToolTip());
}

} // namespace blink
