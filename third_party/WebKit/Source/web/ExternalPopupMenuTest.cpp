// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/ExternalPopupMenu.h"

#include "platform/PopupMenu.h"
#include "platform/PopupMenuClient.h"
#include "public/web/WebPopupMenuInfo.h"
#include <gtest/gtest.h>

using namespace blink;

namespace {

const size_t kListSize = 7;

class TestPopupMenuClient : public PopupMenuClient {
public:
    TestPopupMenuClient() : m_listSize(0) { }
    virtual ~TestPopupMenuClient() { }

    virtual void valueChanged(unsigned listIndex, bool fireEvents = true) override { }
    virtual void selectionChanged(unsigned listIndex, bool fireEvents = true) override { }
    virtual void selectionCleared() override { }

    virtual String itemText(unsigned listIndex) const override { return emptyString(); }
    virtual String itemToolTip(unsigned listIndex) const override { return emptyString(); }
    virtual String itemAccessibilityText(unsigned listIndex) const override { return emptyString(); }
    virtual bool itemIsEnabled(unsigned listIndex) const override { return true; }
    virtual PopupMenuStyle itemStyle(unsigned listIndex) const override
    {
        FontDescription fontDescription;
        fontDescription.setComputedSize(12.0);
        Font font(fontDescription);
        font.update(nullptr);
        bool displayNone = m_displayNoneIndexSet.find(listIndex) != m_displayNoneIndexSet.end();
        return PopupMenuStyle(Color::black, Color::white, font, true, displayNone, Length(), TextDirection(), false);
    }
    virtual PopupMenuStyle menuStyle() const override { return itemStyle(0); }
    virtual LayoutUnit clientPaddingLeft() const override { return 0; }
    virtual LayoutUnit clientPaddingRight() const override { return 0; }
    virtual int listSize() const override { return m_listSize; }
    virtual int selectedIndex() const override { return 0; }
    virtual void popupDidHide() override { }
    virtual bool itemIsSeparator(unsigned listIndex) const override { return false;}
    virtual bool itemIsLabel(unsigned listIndex) const override { return false; }
    virtual bool itemIsSelected(unsigned listIndex) const override { return listIndex == 0;}
    virtual void setTextFromItem(unsigned listIndex) override { }
    virtual bool multiple() const override { return false; }

    void setListSize(size_t size) { m_listSize = size; }
    void setDisplayNoneIndex(unsigned index) { m_displayNoneIndexSet.insert(index); }
private:
    size_t m_listSize;
    std::set<unsigned> m_displayNoneIndexSet;
};

class ExternalPopupMenuDisplayNoneItemsTest : public testing::Test {
public:
    ExternalPopupMenuDisplayNoneItemsTest() { }

protected:
    virtual void SetUp() override
    {
        m_popupMenuClient.setListSize(kListSize);

        // Set the 4th an 5th items to have "display: none" property
        m_popupMenuClient.setDisplayNoneIndex(3);
        m_popupMenuClient.setDisplayNoneIndex(4);
    }

    TestPopupMenuClient m_popupMenuClient;
};

TEST_F(ExternalPopupMenuDisplayNoneItemsTest, PopupMenuInfoSizeTest)
{
    WebPopupMenuInfo info;
    ExternalPopupMenu::getPopupMenuInfo(info, m_popupMenuClient);
    EXPECT_EQ(5U, info.items.size());
}

TEST_F(ExternalPopupMenuDisplayNoneItemsTest, IndexMappingTest)
{
    // 6th indexed item in popupmenu would be the 4th item in ExternalPopupMenu,
    // and vice-versa.
    EXPECT_EQ(4, ExternalPopupMenu::toExternalPopupMenuItemIndex(6, m_popupMenuClient));
    EXPECT_EQ(6, ExternalPopupMenu::toPopupMenuItemIndex(4, m_popupMenuClient));

    // Invalid index, methods should return -1.
    EXPECT_EQ(-1, ExternalPopupMenu::toExternalPopupMenuItemIndex(8, m_popupMenuClient));
    EXPECT_EQ(-1, ExternalPopupMenu::toPopupMenuItemIndex(8, m_popupMenuClient));
}

} // namespace
