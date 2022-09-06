// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/intent_picker_bubble_view.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "chrome/browser/apps/intent_helper/intent_picker_helpers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/widget/widget_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/arc/common/intent_helper/arc_intent_helper_package.h"
#endif

using AppInfo = apps::IntentPickerAppInfo;
using BubbleType = apps::IntentPickerBubbleType;
using content::WebContents;
using content::OpenURLParams;
using content::Referrer;

class IntentPickerBubbleViewTest : public TestWithBrowserView {
 public:
  IntentPickerBubbleViewTest() = default;

  IntentPickerBubbleViewTest(const IntentPickerBubbleViewTest&) = delete;
  IntentPickerBubbleViewTest& operator=(const IntentPickerBubbleViewTest&) =
      delete;

  void TearDown() override {
    // Make sure the bubble is destroyed before the profile to avoid a crash.
    if (bubble_)
      bubble_->GetWidget()->CloseNow();

    TestWithBrowserView::TearDown();
  }

 protected:
  void CreateBubbleView(bool use_icons,
                        bool show_stay_in_chrome,
                        BubbleType bubble_type,
                        const absl::optional<url::Origin>& initiating_origin) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    anchor_view_ =
        browser_view->toolbar()->AddChildView(std::make_unique<views::View>());

    // Pushing a couple of fake apps just to check they are created on the UI.
    app_info_.emplace_back(apps::PickerEntryType::kArc, ui::ImageModel(),
                           "package_1", "dank app 1");
    app_info_.emplace_back(apps::PickerEntryType::kArc, ui::ImageModel(),
                           "package_2", "dank_app_2");

    if (use_icons)
      FillAppListWithDummyIcons();

    // We create |web_contents| since the Bubble UI has an Observer that
    // depends on this, otherwise it wouldn't work.
    GURL url("http://www.google.com");
    WebContents* web_contents = browser()->OpenURL(
        OpenURLParams(url, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false));
    CommitPendingLoad(&web_contents->GetController());

    std::vector<AppInfo> app_info;

    // AppInfo is move only. Manually create a new app_info array to pass into
    // the bubble constructor.
    for (const auto& app : app_info_) {
      app_info.emplace_back(app.type, app.icon_model, app.launch_name,
                            app.display_name);
    }

    auto* widget = IntentPickerBubbleView::ShowBubble(
        anchor_view_, /*highlighted_button=*/nullptr, bubble_type, web_contents,
        std::move(app_info), show_stay_in_chrome,
        /*show_remember_selection=*/true, initiating_origin,
        base::BindOnce(&IntentPickerBubbleViewTest::OnBubbleClosed,
                       base::Unretained(this)));
    bubble_ = IntentPickerBubbleView::intent_picker_bubble();
    event_generator_.reset();  // Mac only allows one EventGenerator at a time.
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        views::GetRootWindow(widget));
  }

  // Add an app to the bubble opened by CreateBubbleView. Manually added apps
  // will appear before automatic placeholder apps.
  void AddApp(apps::PickerEntryType app_type,
              const std::string& launch_name,
              const std::string& title) {
    app_info_.emplace_back(app_type, ui::ImageModel(), launch_name, title);
  }

  void FillAppListWithDummyIcons() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    ui::ImageModel dummy_icon_model =
        ui::ImageModel::FromImage(rb.GetImageNamed(IDR_CLOSE));
    for (auto& app : app_info_)
      app.icon_model = dummy_icon_model;
  }

  views::Button* GetButtonAtIndex(size_t index) {
    auto children =
        bubble_->GetViewByID(IntentPickerBubbleView::ViewId::kItemContainer)
            ->children();
    CHECK_LT(index, children.size());
    return static_cast<views::Button*>(children[index]);
  }

  views::LabelButton* GetLabelButtonAtIndex(size_t index) {
    CHECK(!apps::features::LinkCapturingUiUpdateEnabled());
    return static_cast<views::LabelButton*>(GetButtonAtIndex(index));
  }

  void ClickApp(size_t index) {
    event_generator_->MoveMouseTo(
        GetButtonAtIndex(index)->GetBoundsInScreen().CenterPoint());
    event_generator_->ClickLeftButton();
  }

  views::InkDropState GetInkDropState(size_t index) {
    return views::InkDrop::Get(GetButtonAtIndex(index))
        ->GetInkDrop()
        ->GetTargetInkDropState();
  }

  size_t GetScrollViewSize() {
    return bubble_->GetViewByID(IntentPickerBubbleView::ViewId::kItemContainer)
        ->children()
        .size();
  }

  void OnBubbleClosed(const std::string& selected_app_launch_name,
                      apps::PickerEntryType entry_type,
                      apps::IntentPickerCloseReason close_reason,
                      bool should_persist) {
    last_selected_launch_name_ = selected_app_launch_name;
    last_close_reason_ = close_reason;
    last_selection_should_persist_ = should_persist;
  }

  raw_ptr<IntentPickerBubbleView> bubble_ = nullptr;
  raw_ptr<views::View> anchor_view_;
  std::vector<AppInfo> app_info_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  std::string last_selected_launch_name_;
  apps::IntentPickerCloseReason last_close_reason_;
  bool last_selection_should_persist_;
};

// Verifies that we didn't set up an image for any LabelButton.
TEST_F(IntentPickerBubbleViewTest, NullIcons) {
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);
  size_t size = GetScrollViewSize();
  for (size_t i = 0; i < size; ++i) {
    gfx::ImageSkia image =
        GetLabelButtonAtIndex(i)->GetImage(views::Button::STATE_NORMAL);
    EXPECT_TRUE(image.isNull()) << i;
  }
}

// Verifies that all the icons contain a non-null icon.
TEST_F(IntentPickerBubbleViewTest, NonNullIcons) {
  CreateBubbleView(/*use_icons=*/true, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);
  size_t size = GetScrollViewSize();
  for (size_t i = 0; i < size; ++i) {
    gfx::ImageSkia image =
        GetLabelButtonAtIndex(i)->GetImage(views::Button::STATE_NORMAL);
    EXPECT_FALSE(image.isNull()) << i;
  }
}

// Verifies that the bubble contains as many rows as |app_info_| with one
// exception, if the Chrome package is present on the input list it won't be
// shown to the user on the picker UI, so there could be a difference
// represented by |chrome_package_repetitions|.
TEST_F(IntentPickerBubbleViewTest, LabelsPtrVectorSize) {
  CreateBubbleView(/*use_icons=*/true, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);
  size_t size = app_info_.size();

  EXPECT_EQ(size, GetScrollViewSize());
}

// Verifies that the first item is activated by default when creating a new
// bubble.
TEST_F(IntentPickerBubbleViewTest, VerifyStartingInkDrop) {
  CreateBubbleView(/*use_icons=*/true, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);
  size_t size = GetScrollViewSize();
  EXPECT_EQ(GetInkDropState(0), views::InkDropState::ACTIVATED);
  for (size_t i = 1; i < size; ++i) {
    EXPECT_EQ(GetInkDropState(i), views::InkDropState::HIDDEN);
  }
}

// Press each button at a time and make sure it goes to ACTIVATED state,
// followed by HIDDEN state after selecting other button.
TEST_F(IntentPickerBubbleViewTest, InkDropStateTransition) {
  CreateBubbleView(/*use_icons=*/true, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);
  size_t size = GetScrollViewSize();
  for (size_t i = 0; i < size; ++i) {
    ClickApp((i + 1) % size);
    EXPECT_EQ(GetInkDropState(i), views::InkDropState::HIDDEN);
    EXPECT_EQ(GetInkDropState((i + 1) % size), views::InkDropState::ACTIVATED);
  }
}

// Arbitrary press a button twice, check that the InkDropState remains the same.
TEST_F(IntentPickerBubbleViewTest, PressButtonTwice) {
  CreateBubbleView(/*use_icons=*/true, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);
  EXPECT_EQ(GetInkDropState(1), views::InkDropState::HIDDEN);
  ClickApp(1);
  EXPECT_EQ(GetInkDropState(1), views::InkDropState::ACTIVATED);
  // Move mouse to prevent the mouse events being interpreted as a double-click.
  event_generator_->MoveMouseBy(10, 0);
  event_generator_->ClickLeftButton();
  EXPECT_EQ(GetInkDropState(1), views::InkDropState::ACTIVATED);
}

// Check that a non nullptr WebContents() has been created and observed.
TEST_F(IntentPickerBubbleViewTest, WebContentsTiedToBubble) {
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);
  EXPECT_TRUE(bubble_->web_contents());

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);
  EXPECT_TRUE(bubble_->web_contents());
}

// Check that that the correct window title is shown.
TEST_F(IntentPickerBubbleViewTest, WindowTitle) {
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN_WITH),
            bubble_->GetWindowTitle());

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kClickToCall,
                   /*initiating_origin=*/absl::nullopt);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_LABEL),
            bubble_->GetWindowTitle());
}

// Check that that the correct button labels are used.
TEST_F(IntentPickerBubbleViewTest, ButtonLabels) {
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN),
            bubble_->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_STAY_IN_CHROME),
      bubble_->GetDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL));

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kClickToCall,
                   /*initiating_origin=*/absl::nullopt);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_CALL_BUTTON_LABEL),
            bubble_->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_STAY_IN_CHROME),
      bubble_->GetDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL));
}

TEST_F(IntentPickerBubbleViewTest, InitiatingOriginView) {
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);
  const int children_without_origin = bubble_->children().size();

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   url::Origin::Create(GURL("https://example.com")));
  const int children_with_origin = bubble_->children().size();
  EXPECT_EQ(children_without_origin + 1, children_with_origin);

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   url::Origin::Create(GURL("http://www.google.com")));
  const int children_with_same_origin = bubble_->children().size();
  EXPECT_EQ(children_without_origin, children_with_same_origin);
}

enum class BubbleInterfaceType { kListView, kGridView };

class IntentPickerBubbleViewLayoutTest
    : public IntentPickerBubbleViewTest,
      public ::testing::WithParamInterface<BubbleInterfaceType> {
 public:
  void SetUp() override {
    if (GetParam() == BubbleInterfaceType::kGridView) {
      feature_list_.InitAndEnableFeature(
          apps::features::kLinkCapturingUiUpdate);
    } else {
      feature_list_.InitAndDisableFeature(
          apps::features::kLinkCapturingUiUpdate);
    }

    IntentPickerBubbleViewTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(IntentPickerBubbleViewLayoutTest, RememberCheckbox) {
  AddApp(apps::PickerEntryType::kDevice, "device_id", "Android Phone");
  AddApp(apps::PickerEntryType::kWeb, "web_app_id", "Web App");
  AddApp(apps::PickerEntryType::kArc, "arc_app_id", "Arc App");

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);

  views::Checkbox* checkbox = static_cast<views::Checkbox*>(
      bubble_->GetViewByID(IntentPickerBubbleView::ViewId::kRememberCheckbox));

  // kDevice entries should not allow persistence.
  ClickApp(0);
  ASSERT_FALSE(checkbox->GetEnabled());

  // kWeb entries should allow persistence when PWA persistence is enabled.
  ClickApp(1);
  ASSERT_EQ(checkbox->GetEnabled(), apps::IntentPickerPwaPersistenceEnabled());

  // Other app types can be persisted.
  ClickApp(2);
  ASSERT_TRUE(checkbox->GetEnabled());
}

TEST_P(IntentPickerBubbleViewLayoutTest, AcceptDialog) {
  AddApp(apps::PickerEntryType::kWeb, "web_app_id_1", "Web App");
  AddApp(apps::PickerEntryType::kWeb, "web_app_id_2", "Web App");
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);

  ClickApp(1);
  bubble_->AcceptDialog();

  EXPECT_EQ(last_selected_launch_name_, "web_app_id_2");
  EXPECT_FALSE(last_selection_should_persist_);
  EXPECT_EQ(last_close_reason_, apps::IntentPickerCloseReason::OPEN_APP);
}

TEST_P(IntentPickerBubbleViewLayoutTest, AcceptDialogWithRememberSelection) {
  AddApp(apps::PickerEntryType::kArc, "arc_app_id", "ARC App");
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);

  views::Checkbox* checkbox = static_cast<views::Checkbox*>(
      bubble_->GetViewByID(IntentPickerBubbleView::ViewId::kRememberCheckbox));
  checkbox->SetChecked(true);

  bubble_->AcceptDialog();

  EXPECT_EQ(last_selected_launch_name_, "arc_app_id");
  EXPECT_TRUE(last_selection_should_persist_);
  EXPECT_EQ(last_close_reason_, apps::IntentPickerCloseReason::OPEN_APP);
}

TEST_P(IntentPickerBubbleViewLayoutTest, CancelDialog) {
  AddApp(apps::PickerEntryType::kWeb, "web_app_id_1", "Web App");
  AddApp(apps::PickerEntryType::kWeb, "web_app_id_2", "Web App");
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);

  ClickApp(1);
  bubble_->CancelDialog();

  EXPECT_EQ(last_selected_launch_name_, apps_util::kUseBrowserForLink);
  EXPECT_FALSE(last_selection_should_persist_);
  EXPECT_EQ(last_close_reason_, apps::IntentPickerCloseReason::STAY_IN_CHROME);
}

TEST_P(IntentPickerBubbleViewLayoutTest, CloseDialog) {
  AddApp(apps::PickerEntryType::kWeb, "web_app_id_1", "Web App");
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);

  bubble_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kLostFocus);

  ASSERT_EQ(last_close_reason_,
            apps::IntentPickerCloseReason::DIALOG_DEACTIVATED);
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_KeyboardNavigation DISABLED_KeyboardNavigation
#else
#define MAYBE_KeyboardNavigation KeyboardNavigation
#endif
TEST_P(IntentPickerBubbleViewLayoutTest, MAYBE_KeyboardNavigation) {
  if (GetParam() == BubbleInterfaceType::kGridView) {
    // TODO(crbug.com/1321501): Support keyboard navigation in grid view.
    GTEST_SKIP();
  }
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);

  GetButtonAtIndex(0)->RequestFocus();

  event_generator_->PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_EQ(bubble_->GetSelectedIndex(), 1u);
  event_generator_->PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_EQ(bubble_->GetSelectedIndex(), 0u);
  // Pressing up/left from the first item should wrap around to the last item.
  event_generator_->PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_EQ(bubble_->GetSelectedIndex(),
            bubble_->app_info_for_testing().size() - 1);
  // Pressing down/right from the last item should wrap around to the first
  // item.
  event_generator_->PressAndReleaseKey(ui::VKEY_RIGHT);
  EXPECT_EQ(bubble_->GetSelectedIndex(), 0u);
}

TEST_P(IntentPickerBubbleViewLayoutTest, DoubleClickToAccept) {
  AddApp(apps::PickerEntryType::kWeb, "web_app_id", "Web App");
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/absl::nullopt);

  event_generator_->MoveMouseTo(
      GetButtonAtIndex(0)->GetBoundsInScreen().CenterPoint());
  event_generator_->DoubleClickLeftButton();

  EXPECT_EQ(last_selected_launch_name_, "web_app_id");
  EXPECT_EQ(last_close_reason_, apps::IntentPickerCloseReason::OPEN_APP);
}

INSTANTIATE_TEST_SUITE_P(All,
                         IntentPickerBubbleViewLayoutTest,
                         testing::Values(BubbleInterfaceType::kListView,
                                         BubbleInterfaceType::kGridView));
