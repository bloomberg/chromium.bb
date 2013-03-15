// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/styled_label_listener.h"

namespace views {

class StyledLabelTest : public testing::Test, public StyledLabelListener {
 public:
  StyledLabelTest() {}
  virtual ~StyledLabelTest() {}

  // StyledLabelListener implementation.
  virtual void StyledLabelLinkClicked(const ui::Range& range,
                                      int event_flags) OVERRIDE {}

 protected:
  StyledLabel* styled() { return styled_.get(); }

  void InitStyledLabel(const std::string& ascii_text) {
    styled_.reset(new StyledLabel(ASCIIToUTF16(ascii_text), this));
  }

  int StyledLabelContentHeightForWidth(int w) {
    return styled_->GetHeightForWidth(w) - styled_->GetInsets().height();
  }

 private:
  scoped_ptr<StyledLabel> styled_;

  DISALLOW_COPY_AND_ASSIGN(StyledLabelTest);
};

TEST_F(StyledLabelTest, NoWrapping) {
  const std::string text("This is a test block of text");
  InitStyledLabel(text);
  Label label(ASCIIToUTF16(text));
  const gfx::Size label_preferred_size = label.GetPreferredSize();
  EXPECT_EQ(label_preferred_size.height(),
            StyledLabelContentHeightForWidth(label_preferred_size.width() * 2));
}

TEST_F(StyledLabelTest, BasicWrapping) {
  const std::string text("This is a test block of text");
  InitStyledLabel(text);
  Label label(ASCIIToUTF16(text.substr(0, text.size() * 2 / 3)));
  gfx::Size label_preferred_size = label.GetPreferredSize();
  EXPECT_EQ(label_preferred_size.height() * 2,
            StyledLabelContentHeightForWidth(label_preferred_size.width()));

  // Also respect the border.
  styled()->set_border(Border::CreateEmptyBorder(3, 3, 3, 3));
  styled()->SetBounds(
      0,
      0,
      styled()->GetInsets().width() + label_preferred_size.width(),
      styled()->GetInsets().height() + 2 * label_preferred_size.height());
  styled()->Layout();
  ASSERT_EQ(2, styled()->child_count());
  EXPECT_EQ(3, styled()->child_at(0)->bounds().x());
  EXPECT_EQ(3, styled()->child_at(0)->bounds().y());
  EXPECT_EQ(styled()->bounds().height() - 3,
            styled()->child_at(1)->bounds().bottom());
}

TEST_F(StyledLabelTest, CreateLinks) {
  const std::string text("This is a test block of text.");
  InitStyledLabel(text);
  styled()->AddLink(ui::Range(0, 1));
  styled()->AddLink(ui::Range(1, 2));
  styled()->AddLink(ui::Range(10, 11));
  styled()->AddLink(ui::Range(12, 13));

  styled()->SetBounds(0, 0, 1000, 1000);
  styled()->Layout();
  ASSERT_EQ(7, styled()->child_count());
}

TEST_F(StyledLabelTest, DontBreakLinks) {
  const std::string text("This is a test block of text, ");
  const std::string link_text("and this should be a link");
  InitStyledLabel(text + link_text);
  styled()->AddLink(ui::Range(text.size(), text.size() + link_text.size()));

  Label label(ASCIIToUTF16(text + link_text.substr(0, link_text.size() / 2)));
  gfx::Size label_preferred_size = label.GetPreferredSize();
  int pref_height = styled()->GetHeightForWidth(label_preferred_size.width());
  EXPECT_EQ(label_preferred_size.height() * 2,
            pref_height - styled()->GetInsets().height());

  styled()->SetBounds(0, 0, label_preferred_size.width(), pref_height);
  styled()->Layout();
  ASSERT_EQ(2, styled()->child_count());
  EXPECT_EQ(0, styled()->child_at(0)->bounds().x());
  EXPECT_EQ(0, styled()->child_at(1)->bounds().x());
}

TEST_F(StyledLabelTest, HandleEmptyLayout) {
  const std::string text("This is a test block of text, ");
  InitStyledLabel(text);
  styled()->Layout();
  ASSERT_EQ(0, styled()->child_count());
}

}  // namespace
