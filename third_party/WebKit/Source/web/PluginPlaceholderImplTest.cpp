// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/PluginPlaceholderImpl.h"

#include "core/HTMLNames.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/TagCollection.h"
#include "core/testing/DummyPageHolder.h"
#include "public/web/WebPluginPlaceholder.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::Return;

namespace blink {
namespace {

using HTMLNames::scriptTag;

class MockWebPluginPlaceholder : public WebPluginPlaceholder {
public:
    static PassOwnPtr<MockWebPluginPlaceholder> create() { return adoptPtr(new MockWebPluginPlaceholder); }
    virtual ~MockWebPluginPlaceholder() { }

    MOCK_CONST_METHOD0(message, WebString());

private:
    MockWebPluginPlaceholder() { }
};

// Fixture which creates a dummy context for running these this test.
// Notably creates a document fragment, since Document returns nothing for
// textContent, and mocks out the underlying WebPluginPlaceholder.
class PluginPlaceholderImplTest : public ::testing::Test {
protected:
    PluginPlaceholderImplTest()
        : m_pageHolder(DummyPageHolder::create())
        , m_documentFragment(m_pageHolder->document().createDocumentFragment())
    {
        OwnPtr<MockWebPluginPlaceholder> webPluginPlaceholder = MockWebPluginPlaceholder::create();
        m_webPluginPlaceholder = webPluginPlaceholder.get();
        m_pluginPlaceholderImpl = PluginPlaceholderImpl::create(webPluginPlaceholder.release(), document());
    }

    Document& document() { return m_pageHolder->document(); }
    DocumentFragment& documentFragment() { return *m_documentFragment; }
    MockWebPluginPlaceholder& webPluginPlaceholder() { return *m_webPluginPlaceholder; }
    PluginPlaceholder& pluginPlaceholder() { return *m_pluginPlaceholderImpl; }

private:
    OwnPtr<DummyPageHolder> m_pageHolder;
    RefPtrWillBePersistent<DocumentFragment> m_documentFragment;
    MockWebPluginPlaceholder* m_webPluginPlaceholder; // owned by PluginPlaceholderImpl
    OwnPtrWillBePersistent<PluginPlaceholderImpl> m_pluginPlaceholderImpl;
};

TEST_F(PluginPlaceholderImplTest, MessageIsShown)
{
    String message = "Hello world!";
    EXPECT_CALL(webPluginPlaceholder(), message()).WillOnce(Return(message));
    pluginPlaceholder().loadIntoContainer(documentFragment());
    EXPECT_TRUE(documentFragment().textContent().contains(message));
}

TEST_F(PluginPlaceholderImplTest, MessageDoesNotRunScripts)
{
    String message = "<script>console.log('this should not run');</script>";
    EXPECT_CALL(webPluginPlaceholder(), message()).WillOnce(Return(message));
    pluginPlaceholder().loadIntoContainer(documentFragment());
    EXPECT_TRUE(documentFragment().textContent().contains(message));
    EXPECT_TRUE(documentFragment().getElementsByTagName(scriptTag.localName())->isEmpty());
}

TEST_F(PluginPlaceholderImplTest, MessageDoesNotAcceptElements)
{
    String message = "<h1 id='sentinel'>sentinel</h1>";
    EXPECT_CALL(webPluginPlaceholder(), message()).WillOnce(Return(message));
    pluginPlaceholder().loadIntoContainer(documentFragment());
    EXPECT_TRUE(documentFragment().textContent().contains(message));
    EXPECT_FALSE(documentFragment().getElementById("sentinel"));
}

} // namespace
} // namespace blink
