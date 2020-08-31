// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"

namespace {

const int kWindowWidth = 500;
const int kMinimizeButtonWidth = 26;
const int kMaximizeButtonWidth = 25;
const int kCloseButtonWidth = 43;
const int kMaximizedExtraCloseWidth =
    OpaqueBrowserFrameViewLayout::kFrameBorderThickness -
    views::NonClientFrameView::kFrameShadowThickness;
const int kCaptionButtonsWidth =
    kMinimizeButtonWidth + kMaximizeButtonWidth + kCloseButtonWidth;
const int kCaptionButtonHeight = 18;

class TestLayoutDelegate : public OpaqueBrowserFrameViewLayoutDelegate {
 public:
  TestLayoutDelegate() : show_caption_buttons_(true), maximized_(false) {}
  ~TestLayoutDelegate() override {}

  void set_window_title(const base::string16& title) { window_title_ = title; }
  void set_show_caption_buttons(bool show_caption_buttons) {
    show_caption_buttons_ = show_caption_buttons;
  }
  void set_maximized(bool maximized) { maximized_ = maximized; }

  // OpaqueBrowserFrameViewLayoutDelegate:
  bool ShouldShowWindowIcon() const override { return !window_title_.empty(); }
  bool ShouldShowWindowTitle() const override { return !window_title_.empty(); }
  base::string16 GetWindowTitle() const override { return window_title_; }
  int GetIconSize() const override { return 17; }
  gfx::Size GetBrowserViewMinimumSize() const override {
    return gfx::Size(168, 64);
  }
  bool ShouldShowCaptionButtons() const override {
    return show_caption_buttons_;
  }
  bool IsRegularOrGuestSession() const override { return true; }
  bool IsMaximized() const override { return maximized_; }
  bool IsMinimized() const override { return false; }
  bool IsTabStripVisible() const override { return window_title_.empty(); }
  int GetTabStripHeight() const override {
    return IsTabStripVisible() ? GetLayoutConstant(TAB_HEIGHT) : 0;
  }
  bool IsToolbarVisible() const override { return true; }
  gfx::Size GetTabstripMinimumSize() const override {
    return IsTabStripVisible() ? gfx::Size(78, 29) : gfx::Size();
  }
  int GetTopAreaHeight() const override { return 0; }
  bool UseCustomFrame() const override { return true; }
  bool IsFrameCondensed() const override {
    return !show_caption_buttons_ || maximized_;
  }
  bool EverHasVisibleBackgroundTabShapes() const override { return false; }

 private:
  base::string16 window_title_;
  bool show_caption_buttons_;
  bool maximized_;

  DISALLOW_COPY_AND_ASSIGN(TestLayoutDelegate);
};

}  // namespace

class OpaqueBrowserFrameViewLayoutTest
    : public ChromeViewsTestBase,
      public testing::WithParamInterface<bool> {
 public:
  OpaqueBrowserFrameViewLayoutTest() {}
  ~OpaqueBrowserFrameViewLayoutTest() override {}

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    delegate_ = std::make_unique<TestLayoutDelegate>();
    auto layout = std::make_unique<OpaqueBrowserFrameViewLayout>();
    layout->set_delegate(delegate_.get());
    layout->set_forced_window_caption_spacing_for_test(0);
    widget_ = CreateTestWidget();
    root_view_ = widget_->GetRootView();
    root_view_->SetSize(gfx::Size(kWindowWidth, kWindowWidth));
    layout_manager_ = root_view_->SetLayoutManager(std::move(layout));

    // Add the caption buttons. We use fake images because we're modeling the
    // Windows assets here, while the linux version uses differently sized
    // assets.
    //
    // TODO(erg): In a follow up patch, separate these sizes out into virtual
    // accessors so we can test both the windows and linux behaviours once we
    // start modifying the code.
    minimize_button_ = InitWindowCaptionButton(
        VIEW_ID_MINIMIZE_BUTTON,
        gfx::Size(kMinimizeButtonWidth, kCaptionButtonHeight));
    maximize_button_ = InitWindowCaptionButton(
        VIEW_ID_MAXIMIZE_BUTTON,
        gfx::Size(kMaximizeButtonWidth, kCaptionButtonHeight));
    restore_button_ = InitWindowCaptionButton(
        VIEW_ID_RESTORE_BUTTON,
        gfx::Size(kMaximizeButtonWidth, kCaptionButtonHeight));
    close_button_ = InitWindowCaptionButton(
        VIEW_ID_CLOSE_BUTTON,
        gfx::Size(kCloseButtonWidth, kCaptionButtonHeight));

    delegate_->set_maximized(GetParam());
  }

  void TearDown() override {
    widget_.reset();

    ChromeViewsTestBase::TearDown();
  }

 protected:
  views::ImageButton* InitWindowCaptionButton(ViewID view_id,
                                              const gfx::Size& size) {
    views::ImageButton* button = new views::ImageButton(nullptr);
    gfx::ImageSkiaRep rep(size, 1.0f);
    gfx::ImageSkia image(rep);
    button->SetImage(views::Button::STATE_NORMAL, &image);
    button->SetID(view_id);
    root_view_->AddChildView(button);
    return button;
  }

  void AddWindowTitleIcons() {
    tab_icon_view_ = new TabIconView(nullptr, nullptr);
    tab_icon_view_->set_is_light(true);
    tab_icon_view_->SetID(VIEW_ID_WINDOW_ICON);
    root_view_->AddChildView(tab_icon_view_);

    window_title_ = new views::Label(delegate_->GetWindowTitle());
    window_title_->SetVisible(delegate_->ShouldShowWindowTitle());
    window_title_->SetEnabledColor(SK_ColorWHITE);
    window_title_->SetSubpixelRenderingEnabled(false);
    window_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    window_title_->SetID(VIEW_ID_WINDOW_TITLE);
    root_view_->AddChildView(window_title_);
  }

  int CaptionY() const {
    return delegate_->IsMaximized() ?
        0 : views::NonClientFrameView::kFrameShadowThickness;
  }

  int CaptionLeft() const {
    return kWindowWidth -
           (delegate_->IsMaximized()
                ? kMaximizedExtraCloseWidth
                : OpaqueBrowserFrameViewLayout::kFrameBorderThickness) -
           kCaptionButtonsWidth - OpaqueBrowserFrameViewLayout::kCaptionSpacing;
  }

  int IconAndTitleY() const {
    // This approximates the real positioning algorithm, which is complicated.
    const int unavailable_px_at_top =
        delegate_->IsMaximized()
            ? 0
            : OpaqueBrowserFrameViewLayout::kTopFrameEdgeThickness;
    return (unavailable_px_at_top + CaptionY() + kCaptionButtonHeight +
            OpaqueBrowserFrameViewLayout::kCaptionButtonBottomPadding -
            delegate_->GetIconSize()) /
           2;
  }

  void ExpectCaptionButtons(bool caption_buttons_on_left, int extra_height) {
    if (!delegate_->ShouldShowCaptionButtons()) {
      EXPECT_FALSE(maximize_button_->GetVisible());
      EXPECT_FALSE(minimize_button_->GetVisible());
      EXPECT_FALSE(restore_button_->GetVisible());
      EXPECT_FALSE(close_button_->GetVisible());
      return;
    }

    bool maximized = delegate_->IsMaximized();
    int frame_thickness =
        maximized ? 0 : OpaqueBrowserFrameViewLayout::kFrameBorderThickness;
    int close_width =
        kCloseButtonWidth + (maximized ? kMaximizedExtraCloseWidth : 0);
    int close_x = caption_buttons_on_left ?
        frame_thickness : (kWindowWidth - frame_thickness - close_width);
    EXPECT_EQ(close_x, close_button_->x());
    EXPECT_EQ(CaptionY(), close_button_->y());
    EXPECT_EQ(close_width, close_button_->width());
    EXPECT_EQ(kCaptionButtonHeight + extra_height, close_button_->height());
    EXPECT_TRUE(close_button_->GetVisible());
    views::ImageButton* visible_button = maximize_button_;
    views::ImageButton* hidden_button = restore_button_;
    if (maximized)
      std::swap(visible_button, hidden_button);
    if (caption_buttons_on_left)
      EXPECT_EQ(minimize_button_->bounds().right(), visible_button->x());
    else
      EXPECT_EQ(close_button_->x(), visible_button->bounds().right());
    EXPECT_EQ(close_button_->y(), visible_button->y());
    EXPECT_EQ(kMaximizeButtonWidth, visible_button->width());
    EXPECT_EQ(close_button_->height(), visible_button->height());
    EXPECT_TRUE(visible_button->GetVisible());
    if (caption_buttons_on_left)
      EXPECT_EQ(close_button_->bounds().right(), minimize_button_->x());
    else
      EXPECT_EQ(visible_button->x(), minimize_button_->bounds().right());
    EXPECT_EQ(visible_button->y(), minimize_button_->y());
    EXPECT_EQ(kMinimizeButtonWidth, minimize_button_->width());
    EXPECT_EQ(visible_button->height(), minimize_button_->height());
    EXPECT_TRUE(minimize_button_->GetVisible());
    EXPECT_FALSE(hidden_button->GetVisible());
  }

  void ExpectTabStripAndMinimumSize(bool caption_buttons_on_left) {
    bool show_caption_buttons = delegate_->ShouldShowCaptionButtons();
    bool maximized = delegate_->IsMaximized() || !show_caption_buttons;
    int tabstrip_x = 0;
    if (show_caption_buttons && caption_buttons_on_left) {
      int right_of_close =
          maximized ? kMaximizedExtraCloseWidth
                    : OpaqueBrowserFrameViewLayout::kFrameBorderThickness;
      tabstrip_x += kCaptionButtonsWidth + right_of_close;
    } else if (!maximized) {
      tabstrip_x += OpaqueBrowserFrameViewLayout::kFrameBorderThickness;
    }
    gfx::Size tabstrip_min_size(delegate_->GetTabstripMinimumSize());
    gfx::Rect tabstrip_region_bounds(
        layout_manager_->GetBoundsForTabStripRegion(tabstrip_min_size,
                                                    kWindowWidth));
    EXPECT_EQ(tabstrip_x, tabstrip_region_bounds.x());
    if (maximized) {
      EXPECT_EQ(0, tabstrip_region_bounds.y());
    } else {
      const int tabstrip_nonexcluded_y =
          OpaqueBrowserFrameViewLayout::kFrameBorderThickness +
          layout_manager_->GetNonClientRestoredExtraThickness() +
          OpaqueBrowserFrameViewLayout::kNonClientExtraTopThickness;
      EXPECT_LE(tabstrip_region_bounds.y(), tabstrip_nonexcluded_y);
    }
    const bool showing_caption_buttons_on_right =
        show_caption_buttons && !caption_buttons_on_left;
    const int caption_width =
        showing_caption_buttons_on_right ? kCaptionButtonsWidth : 0;
    int maximized_spacing =
        showing_caption_buttons_on_right ? kMaximizedExtraCloseWidth : 0;
    int restored_spacing = OpaqueBrowserFrameViewLayout::kFrameBorderThickness;
    int spacing = maximized ? maximized_spacing : restored_spacing;
    const int tabstrip_width =
        kWindowWidth - tabstrip_x - caption_width - spacing;
    EXPECT_EQ(tabstrip_width, tabstrip_region_bounds.width());
    EXPECT_EQ(tabstrip_min_size.height(), tabstrip_region_bounds.height());
    gfx::Size browser_view_min_size(delegate_->GetBrowserViewMinimumSize());

    // The tabs and window control buttons (if present) sit above the toolstrip
    // in the browser window. The only one of these that can really change size
    // is the tabstrip, so we should be able to find the minimum width of this
    // region by subtracting out the difference between the current tab strip
    // width and the minimum tab strip width.
    const int top_bar_minimum_width = kWindowWidth -
                                      tabstrip_region_bounds.width() +
                                      tabstrip_min_size.width();
    // The minimum window width is then the minimum overall browser contents
    // or the minimum tab strip/control buttons size, whichever is larger, plus
    // the frame width.
    const int frame_width =
        delegate_->IsFrameCondensed()
            ? 0
            : 2 * OpaqueBrowserFrameViewLayout::kFrameBorderThickness;
    const int min_width =
        std::max(browser_view_min_size.width(), top_bar_minimum_width) +
        frame_width;
    gfx::Size min_size(layout_manager_->GetMinimumSize(root_view_));
    EXPECT_EQ(min_width, min_size.width());

    int restored_border_height =
        2 * OpaqueBrowserFrameViewLayout::kFrameBorderThickness +
        OpaqueBrowserFrameViewLayout::kNonClientExtraTopThickness;
    int top_border_height = maximized ? 0 : restored_border_height;
    int min_height = top_border_height + browser_view_min_size.height();
    EXPECT_EQ(min_height, min_size.height());
  }

  void ExpectWindowIcon(bool caption_buttons_on_left) {
    if (caption_buttons_on_left) {
      EXPECT_TRUE(layout_manager_->IconBounds().IsEmpty());
      return;
    }

    int border_thickness =
        (delegate_->IsMaximized() || !delegate_->ShouldShowCaptionButtons())
            ? 0
            : OpaqueBrowserFrameViewLayout::kFrameBorderThickness;
    gfx::Rect icon_bounds(layout_manager_->IconBounds());
    EXPECT_EQ(border_thickness + OpaqueBrowserFrameViewLayout::kIconLeftSpacing,
              icon_bounds.x());
    int icon_y =
        delegate_->ShouldShowWindowTitle() ? IconAndTitleY() : border_thickness;
    EXPECT_EQ(icon_y, icon_bounds.y());
    int icon_size = delegate_->GetIconSize();
    EXPECT_EQ(icon_size, icon_bounds.width());
    EXPECT_EQ(icon_size, icon_bounds.height());
  }

  void ExpectWindowTitle() {
    int icon_size = delegate_->GetIconSize();
    int title_x = (delegate_->IsMaximized()
                       ? 0
                       : OpaqueBrowserFrameViewLayout::kFrameBorderThickness) +
                  OpaqueBrowserFrameViewLayout::kIconLeftSpacing + icon_size +
                  OpaqueBrowserFrameViewLayout::kIconTitleSpacing;
    gfx::Rect title_bounds(window_title_->bounds());
    EXPECT_EQ(title_x, title_bounds.x());
    EXPECT_EQ(IconAndTitleY(), title_bounds.y());
    EXPECT_EQ(CaptionLeft() - title_x, title_bounds.width());
    EXPECT_EQ(icon_size, title_bounds.height());
  }

  std::unique_ptr<views::Widget> widget_;
  views::View* root_view_ = nullptr;
  OpaqueBrowserFrameViewLayout* layout_manager_ = nullptr;
  std::unique_ptr<TestLayoutDelegate> delegate_;

  // Widgets:
  views::ImageButton* minimize_button_ = nullptr;
  views::ImageButton* maximize_button_ = nullptr;
  views::ImageButton* restore_button_ = nullptr;
  views::ImageButton* close_button_ = nullptr;

  TabIconView* tab_icon_view_ = nullptr;
  views::Label* window_title_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(OpaqueBrowserFrameViewLayoutTest);
};

TEST_P(OpaqueBrowserFrameViewLayoutTest, BasicWindow) {
  // Tests the layout of a default chrome window with a tabstrip and no window
  // title.
  root_view_->Layout();
  ExpectCaptionButtons(false, 0);
  ExpectTabStripAndMinimumSize(false);
  ExpectWindowIcon(false);
}

TEST_P(OpaqueBrowserFrameViewLayoutTest, WindowButtonsOnLeft) {
  // Tests the layout of a chrome window with caption buttons on the left.
  std::vector<views::FrameButton> leading_buttons;
  std::vector<views::FrameButton> trailing_buttons;
  leading_buttons.push_back(views::FrameButton::kClose);
  leading_buttons.push_back(views::FrameButton::kMinimize);
  leading_buttons.push_back(views::FrameButton::kMaximize);
  layout_manager_->SetButtonOrdering(leading_buttons, trailing_buttons);

  root_view_->Layout();
  ExpectCaptionButtons(true, 0);
  ExpectTabStripAndMinimumSize(true);
  ExpectWindowIcon(true);
}

TEST_P(OpaqueBrowserFrameViewLayoutTest, WithoutCaptionButtons) {
  // Tests the layout of a default chrome window with no caption buttons (which
  // should force the tab strip to be condensed).
  delegate_->set_show_caption_buttons(false);

  root_view_->Layout();
  ExpectCaptionButtons(false, 0);
  ExpectTabStripAndMinimumSize(false);
  ExpectWindowIcon(false);
}

TEST_P(OpaqueBrowserFrameViewLayoutTest, WindowWithTitleAndIcon) {
  // Tests the layout of pop up windows.
  delegate_->set_window_title(base::ASCIIToUTF16("Window Title"));
  AddWindowTitleIcons();

  root_view_->Layout();
  ExpectCaptionButtons(false, 0);
  ExpectWindowIcon(false);
  ExpectWindowTitle();
}

INSTANTIATE_TEST_SUITE_P(All,
                         OpaqueBrowserFrameViewLayoutTest,
                         ::testing::Values(false, true),
                         [](const testing::TestParamInfo<bool>& param_info) {
                           return std::string(param_info.param ? "Maximized"
                                                               : "Restored");
                         });
