// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/test/widget_test.h"

using base::ASCIIToUTF16;

namespace {

gfx::ImageSkia CreateTestImage(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

}  // namespace

namespace views {

class LabelButtonTest : public test::WidgetTest {
 public:
  LabelButtonTest() {}

  // testing::Test:
  void SetUp() override {
    WidgetTest::SetUp();
    // Make a Widget to host the button. This ensures appropriate borders are
    // used (which could be derived from the Widget's NativeTheme).
    test_widget_ = CreateTopLevelPlatformWidget();

    button_ = new LabelButton(nullptr, base::string16());
    test_widget_->GetContentsView()->AddChildView(button_);
  }

  void TearDown() override {
    test_widget_->CloseNow();
    WidgetTest::TearDown();
  }

 protected:
  LabelButton* button_ = nullptr;

 private:
  Widget* test_widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LabelButtonTest);
};

TEST_F(LabelButtonTest, Init) {
  const base::string16 text(ASCIIToUTF16("abc"));
  LabelButton button(NULL, text);

  EXPECT_TRUE(button.GetImage(Button::STATE_NORMAL).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_HOVERED).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_PRESSED).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_DISABLED).isNull());

  EXPECT_EQ(text, button.GetText());

  ui::AXViewState accessible_state;
  button.GetAccessibleState(&accessible_state);
  EXPECT_EQ(ui::AX_ROLE_BUTTON, accessible_state.role);
  EXPECT_EQ(text, accessible_state.name);

  EXPECT_FALSE(button.is_default());
  EXPECT_EQ(button.style(), Button::STYLE_TEXTBUTTON);
  EXPECT_EQ(Button::STATE_NORMAL, button.state());

  EXPECT_EQ(button.image_->parent(), &button);
  EXPECT_EQ(button.label_->parent(), &button);
}

TEST_F(LabelButtonTest, Label) {
  EXPECT_TRUE(button_->GetText().empty());

  const gfx::FontList font_list;
  const base::string16 short_text(ASCIIToUTF16("abcdefghijklm"));
  const base::string16 long_text(ASCIIToUTF16("abcdefghijklmnopqrstuvwxyz"));
  const int short_text_width = gfx::GetStringWidth(short_text, font_list);
  const int long_text_width = gfx::GetStringWidth(long_text, font_list);

  // The width increases monotonically with string size (it does not shrink).
  EXPECT_LT(button_->GetPreferredSize().width(), short_text_width);
  button_->SetText(short_text);
  EXPECT_GT(button_->GetPreferredSize().height(), font_list.GetHeight());
  EXPECT_GT(button_->GetPreferredSize().width(), short_text_width);
  EXPECT_LT(button_->GetPreferredSize().width(), long_text_width);
  button_->SetText(long_text);
  EXPECT_GT(button_->GetPreferredSize().width(), long_text_width);
  button_->SetText(short_text);
  EXPECT_GT(button_->GetPreferredSize().width(), long_text_width);

  // Clamp the size to a maximum value.
  button_->SetMaxSize(gfx::Size(long_text_width, 1));
  EXPECT_EQ(button_->GetPreferredSize(), gfx::Size(long_text_width, 1));

  // Clear the monotonically increasing minimum size.
  button_->SetMinSize(gfx::Size());
  EXPECT_GT(button_->GetPreferredSize().width(), short_text_width);
  EXPECT_LT(button_->GetPreferredSize().width(), long_text_width);
}

// Test behavior of View::GetAccessibleState() for buttons when setting a label.
TEST_F(LabelButtonTest, AccessibleState) {
  ui::AXViewState accessible_state;

  button_->GetAccessibleState(&accessible_state);
  EXPECT_EQ(ui::AX_ROLE_BUTTON, accessible_state.role);
  EXPECT_EQ(base::string16(), accessible_state.name);

  // Without a label (e.g. image-only), the accessible name should automatically
  // be set from the tooltip.
  const base::string16 tooltip_text = ASCIIToUTF16("abc");
  button_->SetTooltipText(tooltip_text);
  button_->GetAccessibleState(&accessible_state);
  EXPECT_EQ(tooltip_text, accessible_state.name);
  EXPECT_EQ(base::string16(), button_->GetText());

  // Setting a label overrides the tooltip text.
  const base::string16 label_text = ASCIIToUTF16("def");
  button_->SetText(label_text);
  button_->GetAccessibleState(&accessible_state);
  EXPECT_EQ(label_text, accessible_state.name);
  EXPECT_EQ(label_text, button_->GetText());

  base::string16 tooltip;
  EXPECT_TRUE(button_->GetTooltipText(gfx::Point(), &tooltip));
  EXPECT_EQ(tooltip_text, tooltip);
}

TEST_F(LabelButtonTest, Image) {
  const int small_size = 50, large_size = 100;
  const gfx::ImageSkia small_image = CreateTestImage(small_size, small_size);
  const gfx::ImageSkia large_image = CreateTestImage(large_size, large_size);

  // The width increases monotonically with image size (it does not shrink).
  EXPECT_LT(button_->GetPreferredSize().width(), small_size);
  EXPECT_LT(button_->GetPreferredSize().height(), small_size);
  button_->SetImage(Button::STATE_NORMAL, small_image);
  EXPECT_GT(button_->GetPreferredSize().width(), small_size);
  EXPECT_GT(button_->GetPreferredSize().height(), small_size);
  EXPECT_LT(button_->GetPreferredSize().width(), large_size);
  EXPECT_LT(button_->GetPreferredSize().height(), large_size);
  button_->SetImage(Button::STATE_NORMAL, large_image);
  EXPECT_GT(button_->GetPreferredSize().width(), large_size);
  EXPECT_GT(button_->GetPreferredSize().height(), large_size);
  button_->SetImage(Button::STATE_NORMAL, small_image);
  EXPECT_GT(button_->GetPreferredSize().width(), large_size);
  EXPECT_GT(button_->GetPreferredSize().height(), large_size);

  // Clamp the size to a maximum value.
  button_->SetMaxSize(gfx::Size(large_size, 1));
  EXPECT_EQ(button_->GetPreferredSize(), gfx::Size(large_size, 1));

  // Clear the monotonically increasing minimum size.
  button_->SetMinSize(gfx::Size());
  EXPECT_GT(button_->GetPreferredSize().width(), small_size);
  EXPECT_LT(button_->GetPreferredSize().width(), large_size);
}

TEST_F(LabelButtonTest, LabelAndImage) {
  const gfx::FontList font_list;
  const base::string16 text(ASCIIToUTF16("abcdefghijklm"));
  const int text_width = gfx::GetStringWidth(text, font_list);

  const int image_size = 50;
  const gfx::ImageSkia image = CreateTestImage(image_size, image_size);
  ASSERT_LT(font_list.GetHeight(), image_size);

  // The width increases monotonically with content size (it does not shrink).
  EXPECT_LT(button_->GetPreferredSize().width(), text_width);
  EXPECT_LT(button_->GetPreferredSize().width(), image_size);
  EXPECT_LT(button_->GetPreferredSize().height(), image_size);
  button_->SetText(text);
  EXPECT_GT(button_->GetPreferredSize().width(), text_width);
  EXPECT_GT(button_->GetPreferredSize().height(), font_list.GetHeight());
  EXPECT_LT(button_->GetPreferredSize().width(), text_width + image_size);
  EXPECT_LT(button_->GetPreferredSize().height(), image_size);
  button_->SetImage(Button::STATE_NORMAL, image);
  EXPECT_GT(button_->GetPreferredSize().width(), text_width + image_size);
  EXPECT_GT(button_->GetPreferredSize().height(), image_size);

  // Layout and ensure the image is left of the label except for ALIGN_RIGHT.
  // (A proper parent view or layout manager would Layout on its invalidations).
  // Also make sure CENTER alignment moves the label compared to LEFT alignment.
  gfx::Size button_size = button_->GetPreferredSize();
  button_size.Enlarge(50, 0);
  button_->SetSize(button_size);
  button_->Layout();
  EXPECT_LT(button_->image_->bounds().right(), button_->label_->bounds().x());
  int left_align_label_midpoint = button_->label_->bounds().CenterPoint().x();
  button_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  button_->Layout();
  EXPECT_LT(button_->image_->bounds().right(), button_->label_->bounds().x());
  int center_align_label_midpoint = button_->label_->bounds().CenterPoint().x();
  EXPECT_LT(left_align_label_midpoint, center_align_label_midpoint);
  button_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  button_->Layout();
  EXPECT_LT(button_->label_->bounds().right(), button_->image_->bounds().x());

  button_->SetText(base::string16());
  EXPECT_GT(button_->GetPreferredSize().width(), text_width + image_size);
  EXPECT_GT(button_->GetPreferredSize().height(), image_size);
  button_->SetImage(Button::STATE_NORMAL, gfx::ImageSkia());
  EXPECT_GT(button_->GetPreferredSize().width(), text_width + image_size);
  EXPECT_GT(button_->GetPreferredSize().height(), image_size);

  // Clamp the size to a maximum value.
  button_->SetMaxSize(gfx::Size(image_size, 1));
  EXPECT_EQ(button_->GetPreferredSize(), gfx::Size(image_size, 1));

  // Clear the monotonically increasing minimum size.
  button_->SetMinSize(gfx::Size());
  EXPECT_LT(button_->GetPreferredSize().width(), text_width);
  EXPECT_LT(button_->GetPreferredSize().width(), image_size);
  EXPECT_LT(button_->GetPreferredSize().height(), image_size);
}

TEST_F(LabelButtonTest, FontList) {
  button_->SetText(base::ASCIIToUTF16("abc"));

  const gfx::FontList original_font_list = button_->GetFontList();
  const gfx::FontList large_font_list =
      original_font_list.DeriveWithSizeDelta(100);
  const int original_width = button_->GetPreferredSize().width();
  const int original_height = button_->GetPreferredSize().height();

  // The button size increases when the font size is increased.
  button_->SetFontList(large_font_list);
  EXPECT_GT(button_->GetPreferredSize().width(), original_width);
  EXPECT_GT(button_->GetPreferredSize().height(), original_height);

  // The button returns to its original size when the minimal size is cleared
  // and the original font size is restored.
  button_->SetMinSize(gfx::Size());
  button_->SetFontList(original_font_list);
  EXPECT_EQ(original_width, button_->GetPreferredSize().width());
  EXPECT_EQ(original_height, button_->GetPreferredSize().height());
}

TEST_F(LabelButtonTest, ChangeTextSize) {
  const base::string16 text(ASCIIToUTF16("abc"));
  const base::string16 longer_text(ASCIIToUTF16("abcdefghijklm"));
  button_->SetText(text);

  const int original_width = button_->GetPreferredSize().width();

  // The button size increases when the text size is increased.
  button_->SetText(longer_text);
  EXPECT_GT(button_->GetPreferredSize().width(), original_width);

  // The button returns to its original size when the original text is restored.
  button_->SetMinSize(gfx::Size());
  button_->SetText(text);
  EXPECT_EQ(original_width, button_->GetPreferredSize().width());
}

TEST_F(LabelButtonTest, ChangeLabelImageSpacing) {
  button_->SetText(ASCIIToUTF16("abc"));
  button_->SetImage(Button::STATE_NORMAL, CreateTestImage(50, 50));

  const int kOriginalSpacing = 5;
  button_->SetImageLabelSpacing(kOriginalSpacing);
  const int original_width = button_->GetPreferredSize().width();

  // Increasing the spacing between the text and label should increase the size.
  button_->SetImageLabelSpacing(2 * kOriginalSpacing);
  EXPECT_GT(button_->GetPreferredSize().width(), original_width);

  // The button shrinks if the original spacing is restored.
  button_->SetMinSize(gfx::Size());
  button_->SetImageLabelSpacing(kOriginalSpacing);
  EXPECT_EQ(original_width, button_->GetPreferredSize().width());
}

// Make sure the label gets the width it asks for and bolding it (via
// SetDefault) causes the size to update. Regression test for crbug.com/578722
TEST_F(LabelButtonTest, ButtonStyleIsDefaultSize) {
  LabelButton* button = new LabelButton(nullptr, base::ASCIIToUTF16("Save"));
  button->SetStyle(CustomButton::STYLE_BUTTON);
  button_->GetWidget()->GetContentsView()->AddChildView(button);
  button->SizeToPreferredSize();
  button->Layout();
  gfx::Size non_default_size = button->label_->size();
  EXPECT_EQ(button->label_->GetPreferredSize().width(),
            non_default_size.width());
  button->SetIsDefault(true);
  button->SizeToPreferredSize();
  button->Layout();
  EXPECT_NE(non_default_size, button->label_->size());
}

}  // namespace views
