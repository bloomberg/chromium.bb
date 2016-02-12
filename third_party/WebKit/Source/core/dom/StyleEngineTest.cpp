// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/StyleEngine.h"

#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class StyleEngineTest : public ::testing::Test {
protected:
    void SetUp() override;

    Document& document() { return m_dummyPageHolder->document(); }
    StyleEngine& styleEngine() { return document().styleEngine(); }

    bool isDocumentStyleSheetCollectionClean() { return !styleEngine().shouldUpdateDocumentStyleSheetCollection(AnalyzedStyleUpdate); }

private:
    OwnPtr<DummyPageHolder> m_dummyPageHolder;
};

void StyleEngineTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
}

TEST_F(StyleEngineTest, DocumentDirtyAfterInject)
{
    RefPtrWillBeRawPtr<StyleSheetContents> parsedSheet = StyleSheetContents::create(CSSParserContext(document(), nullptr));
    parsedSheet->parseString("div {}");
    styleEngine().injectAuthorSheet(parsedSheet);
    document().view()->updateAllLifecyclePhases();

    EXPECT_TRUE(isDocumentStyleSheetCollectionClean());
}

} // namespace blink
