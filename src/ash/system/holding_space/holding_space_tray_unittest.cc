// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray.h"

#include <array>
#include <deque>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "url/gurl.h"

namespace ash {

namespace {

using testing::_;
using testing::ElementsAre;

constexpr char kTestUser[] = "user@test";

// Helpers ---------------------------------------------------------------------

// A wrapper around `views::View::GetVisible()` with a null check for `view`.
bool IsViewVisible(const views::View* view) {
  return view && view->GetVisible();
}

void Click(const views::View* view, int flags = ui::EF_NONE) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator.set_flags(flags);
  event_generator.ClickLeftButton();
}

void DoubleClick(const views::View* view, int flags = ui::EF_NONE) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator.set_flags(flags);
  event_generator.DoubleClickLeftButton();
}

void GestureTap(const views::View* view) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.GestureTapAt(view->GetBoundsInScreen().CenterPoint());
}

ui::GestureEvent BuildGestureEvent(const gfx::Point& event_location,
                                   ui::EventType gesture_type) {
  return ui::GestureEvent(event_location.x(), event_location.y(), ui::EF_NONE,
                          ui::EventTimeForNow(),
                          ui::GestureEventDetails(gesture_type));
}

void LongPress(const views::View* view) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveTouch(view->GetBoundsInScreen().CenterPoint());
  const gfx::Point& press_location = event_generator.current_screen_location();
  ui::GestureEvent long_press =
      BuildGestureEvent(press_location, ui::ET_GESTURE_LONG_PRESS);
  event_generator.Dispatch(&long_press);

  ui::GestureEvent gesture_end =
      BuildGestureEvent(press_location, ui::ET_GESTURE_END);
  event_generator.Dispatch(&gesture_end);
}

void PressAndReleaseKey(const views::View* view,
                        ui::KeyboardCode key_code,
                        int flags = ui::EF_NONE) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.PressKey(key_code, flags);
  event_generator.ReleaseKey(key_code, flags);
}

bool PressTabUntilFocused(const views::View* view, int max_count = 10) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  while (!view->HasFocus() && --max_count >= 0)
    event_generator.PressKey(ui::VKEY_TAB, ui::EF_NONE);
  return view->HasFocus();
}

std::unique_ptr<HoldingSpaceImage> CreateStubHoldingSpaceImage(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  return std::make_unique<HoldingSpaceImage>(
      HoldingSpaceImage::GetMaxSizeForType(type), file_path,
      /*async_bitmap_resolver=*/base::DoNothing());
}

// Mocks -----------------------------------------------------------------------

class MockHoldingSpaceClient : public HoldingSpaceClient {
 public:
  // HoldingSpaceClient:
  MOCK_METHOD(void,
              AddScreenshot,
              (const base::FilePath& file_path),
              (override));
  MOCK_METHOD(void,
              AddScreenRecording,
              (const base::FilePath& file_path),
              (override));
  MOCK_METHOD(void,
              CopyImageToClipboard,
              (const HoldingSpaceItem& item, SuccessCallback callback),
              (override));
  MOCK_METHOD(base::FilePath,
              CrackFileSystemUrl,
              (const GURL& file_system_url),
              (const, override));
  MOCK_METHOD(void, OpenDownloads, (SuccessCallback callback), (override));
  MOCK_METHOD(void, OpenMyFiles, (SuccessCallback callback), (override));
  MOCK_METHOD(void,
              OpenItems,
              (const std::vector<const HoldingSpaceItem*>& items,
               SuccessCallback callback),
              (override));
  MOCK_METHOD(void,
              ShowItemInFolder,
              (const HoldingSpaceItem& item, SuccessCallback callback),
              (override));
  MOCK_METHOD(void,
              PinFiles,
              (const std::vector<base::FilePath>& file_paths),
              (override));
  MOCK_METHOD(void,
              PinItems,
              (const std::vector<const HoldingSpaceItem*>& items),
              (override));
  MOCK_METHOD(void,
              UnpinItems,
              (const std::vector<const HoldingSpaceItem*>& items),
              (override));
};

// TransformRecordingLayerDelegate ---------------------------------------------

// A scoped `ui::LayerDelegate` which records information about transforms.
class ScopedTransformRecordingLayerDelegate : public ui::LayerDelegate {
 public:
  explicit ScopedTransformRecordingLayerDelegate(ui::Layer* layer)
      : layer_(layer), layer_delegate_(layer_->delegate()) {
    layer_->set_delegate(this);
    Reset();
  }

  ScopedTransformRecordingLayerDelegate(
      const ScopedTransformRecordingLayerDelegate&) = delete;
  ScopedTransformRecordingLayerDelegate& operator=(
      const ScopedTransformRecordingLayerDelegate&) = delete;

  ~ScopedTransformRecordingLayerDelegate() override {
    layer_->set_delegate(layer_delegate_);
  }

  // Resets recorded information.
  void Reset() {
    const gfx::Transform& transform = layer_->transform();
    start_scale_ = end_scale_ = min_scale_ = max_scale_ = transform.Scale2d();
    start_translation_ = end_translation_ = min_translation_ =
        max_translation_ = transform.To2dTranslation();
  }

  // Returns true if a scale occurred.
  bool DidScale() const {
    return start_scale_ != min_scale_ || start_scale_ != max_scale_;
  }

  // Returns true if a translation occurred.
  bool DidTranslate() const {
    return start_translation_ != min_translation_ ||
           start_translation_ != max_translation_;
  }

  // Returns true if `layer_` scaled from `start` to `end`.
  bool ScaledFrom(const gfx::Vector2dF& start,
                  const gfx::Vector2dF& end) const {
    return start == start_scale_ && end == end_scale_;
  }

  // Returns true if `layer_` scaled within `min` and `max`.
  bool ScaledInRange(const gfx::Vector2dF& min,
                     const gfx::Vector2dF& max) const {
    return min == min_scale_ && max == max_scale_;
  }

  // Returns true if `layer_` translated from `start` to `end`.
  bool TranslatedFrom(const gfx::Vector2dF& start,
                      const gfx::Vector2dF& end) const {
    return start == start_translation_ && end == end_translation_;
  }

  // Returns true if `layer_` translated within `min` to `max`.
  bool TranslatedInRange(const gfx::Vector2dF& min,
                         const gfx::Vector2dF& max) const {
    return min == min_translation_ && max == max_translation_;
  }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {}
  void OnDeviceScaleFactorChanged(float old_scale, float new_scale) override {}

  void OnLayerTransformed(const gfx::Transform& old_transform,
                          ui::PropertyChangeReason reason) override {
    const gfx::Transform& transform = layer_->transform();
    end_scale_ = transform.Scale2d();
    end_translation_ = transform.To2dTranslation();
    min_scale_.SetToMin(end_scale_);
    max_scale_.SetToMax(end_scale_);
    min_translation_.SetToMin(end_translation_);
    max_translation_.SetToMax(end_translation_);
  }

  ui::Layer* const layer_;
  ui::LayerDelegate* const layer_delegate_;

  gfx::Vector2dF start_scale_;
  gfx::Vector2dF start_translation_;
  gfx::Vector2dF end_scale_;
  gfx::Vector2dF end_translation_;
  gfx::Vector2dF min_scale_;
  gfx::Vector2dF max_scale_;
  gfx::Vector2dF min_translation_;
  gfx::Vector2dF max_translation_;
};

}  // namespace

// HoldingSpaceTrayTest --------------------------------------------------------

// Parameterized by whether the previews feature is enabled.
class HoldingSpaceTrayTest : public AshTestBase,
                             public testing::WithParamInterface<bool> {
 public:
  HoldingSpaceTrayTest() {
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;

    enabled_features.push_back(features::kTemporaryHoldingSpace);

    if (IsPreviewsFeatureEnabled())
      enabled_features.push_back(features::kTemporaryHoldingSpacePreviews);
    else
      disabled_features.push_back(features::kTemporaryHoldingSpacePreviews);

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    test_api_ = std::make_unique<HoldingSpaceTestApi>();
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    HoldingSpaceController::Get()->RegisterClientAndModelForUser(
        user_account, client(), model());
    GetSessionControllerClient()->AddUserSession(kTestUser);
    holding_space_prefs::MarkTimeOfFirstAvailability(
        GetSessionControllerClient()->GetUserPrefService(user_account));
  }

  void TearDown() override {
    test_api_.reset();
    AshTestBase::TearDown();
  }

  HoldingSpaceItem* AddItem(HoldingSpaceItem::Type type,
                            const base::FilePath& path) {
    return AddItemToModel(model(), type, path);
  }

  HoldingSpaceItem* AddItemToModel(HoldingSpaceModel* target_model,
                                   HoldingSpaceItem::Type type,
                                   const base::FilePath& path) {
    GURL file_system_url(
        base::StrCat({"filesystem:", path.BaseName().value()}));
    std::unique_ptr<HoldingSpaceItem> item =
        HoldingSpaceItem::CreateFileBackedItem(
            type, path, file_system_url,
            base::BindOnce(&CreateStubHoldingSpaceImage));
    HoldingSpaceItem* item_ptr = item.get();
    target_model->AddItem(std::move(item));
    return item_ptr;
  }

  HoldingSpaceItem* AddPartiallyInitializedItem(HoldingSpaceItem::Type type,
                                                const base::FilePath& path) {
    // Create a holding space item, and use it to create a serialized item
    // dictionary.
    std::unique_ptr<HoldingSpaceItem> item =
        HoldingSpaceItem::CreateFileBackedItem(
            type, path, GURL("filesystem:ignored"),
            base::BindOnce(&CreateStubHoldingSpaceImage));
    const base::DictionaryValue serialized_holding_space_item =
        item->Serialize();
    std::unique_ptr<HoldingSpaceItem> deserialized_item =
        HoldingSpaceItem::Deserialize(
            serialized_holding_space_item,
            /*image_resolver=*/
            base::BindOnce(&CreateStubHoldingSpaceImage));

    HoldingSpaceItem* deserialized_item_ptr = deserialized_item.get();
    model()->AddItem(std::move(deserialized_item));
    return deserialized_item_ptr;
  }

  void RemoveAllItems() {
    model()->RemoveIf(
        base::BindRepeating([](const HoldingSpaceItem* item) { return true; }));
  }

  // The holding space tray is only visible in the shelf after the first holding
  // space item has been added. Most tests do not care about this so, as a
  // convenience, the time of first add will be marked prior to starting the
  // session when `pre_mark_time_of_first_add` is true.
  void StartSession(bool pre_mark_time_of_first_add = true) {
    if (pre_mark_time_of_first_add)
      MarkTimeOfFirstAdd();

    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    GetSessionControllerClient()->SwitchActiveUser(user_account);
  }

  void MarkTimeOfFirstAdd() {
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    holding_space_prefs::MarkTimeOfFirstAdd(
        GetSessionControllerClient()->GetUserPrefService(user_account));
  }

  void MarkTimeOfFirstPin() {
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    holding_space_prefs::MarkTimeOfFirstPin(
        GetSessionControllerClient()->GetUserPrefService(user_account));
  }

  void SwitchToSecondaryUser(const std::string& user_id,
                             HoldingSpaceClient* client,
                             HoldingSpaceModel* model) {
    AccountId user_account = AccountId::FromUserEmail(user_id);
    HoldingSpaceController::Get()->RegisterClientAndModelForUser(user_account,
                                                                 client, model);
    GetSessionControllerClient()->AddUserSession(user_id);

    holding_space_prefs::MarkTimeOfFirstAvailability(
        GetSessionControllerClient()->GetUserPrefService(user_account));
    holding_space_prefs::MarkTimeOfFirstAdd(
        GetSessionControllerClient()->GetUserPrefService(user_account));
    holding_space_prefs::MarkTimeOfFirstPin(
        GetSessionControllerClient()->GetUserPrefService(user_account));

    GetSessionControllerClient()->SwitchActiveUser(user_account);
  }

  void UnregisterModelForUser(const std::string& user_id) {
    AccountId user_account = AccountId::FromUserEmail(user_id);
    HoldingSpaceController::Get()->RegisterClientAndModelForUser(
        user_account, nullptr, nullptr);
  }

  bool IsPreviewsFeatureEnabled() const { return GetParam(); }

  HoldingSpaceTestApi* test_api() { return test_api_.get(); }

  testing::NiceMock<MockHoldingSpaceClient>* client() {
    return &holding_space_client_;
  }

  HoldingSpaceModel* model() { return &holding_space_model_; }

  Shelf* GetShelf(const display::Display& display) {
    auto* const manager = Shell::Get()->window_tree_host_manager();
    auto* const window = manager->GetRootWindowForDisplayId(display.id());
    return Shelf::ForWindow(window);
  }

  HoldingSpaceTray* GetTray() {
    return GetTray(Shelf::ForWindow(Shell::GetRootWindowForNewWindows()));
  }

  HoldingSpaceTray* GetTray(Shelf* shelf) {
    return shelf->shelf_widget()->status_area_widget()->holding_space_tray();
  }

 private:
  std::unique_ptr<HoldingSpaceTestApi> test_api_;
  testing::NiceMock<MockHoldingSpaceClient> holding_space_client_;
  HoldingSpaceModel holding_space_model_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests -----------------------------------------------------------------------

TEST_P(HoldingSpaceTrayTest, ShowTrayButtonOnFirstUse) {
  StartSession(/*pre_mark_time_of_first_add=*/false);
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();

  // The tray button should *not* be shown for users that have never added
  // anything to the holding space.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item. This should cause the tray button to show.
  HoldingSpaceItem* item =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake"));
  MarkTimeOfFirstAdd();
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  // Show the bubble - both the pinned files and recent files child bubbles
  // should be shown.
  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  // Remove the download item and verify the pinned files bubble, and the
  // tray button are still shown.
  model()->RemoveItem(item->id());
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  test_api()->Close();
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  EXPECT_TRUE(IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_FALSE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  test_api()->Show();

  // Add and remove a pinned item.
  HoldingSpaceItem* pinned_item =
      AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/pin"));
  MarkTimeOfFirstPin();
  model()->RemoveItem(pinned_item->id());

  // Verify that the pinned files bubble, and the tray button get hidden.
  EXPECT_FALSE(test_api()->PinnedFilesBubbleShown());
  test_api()->Close();
  EXPECT_FALSE(test_api()->IsShowingInShelf());
}

TEST_P(HoldingSpaceTrayTest, HideButtonWhenModelDetached) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item - the button should be shown.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  SwitchToSecondaryUser("user@secondary", /*client=*/nullptr,
                        /*model=*/nullptr);
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();

  EXPECT_FALSE(test_api()->IsShowingInShelf());
  UnregisterModelForUser("user@secondary");
}

TEST_P(HoldingSpaceTrayTest, HideButtonOnChangeToEmptyModel) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item - the button should be shown.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  HoldingSpaceModel secondary_holding_space_model;
  SwitchToSecondaryUser("user@secondary", /*client=*/nullptr,
                        /*model=*/&secondary_holding_space_model);
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  AddItemToModel(&secondary_holding_space_model,
                 HoldingSpaceItem::Type::kDownload,
                 base::FilePath("/tmp/fake_2"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  UnregisterModelForUser("user@secondary");
}

TEST_P(HoldingSpaceTrayTest, HideButtonOnChangeToNonEmptyModel) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  HoldingSpaceModel secondary_holding_space_model;
  AddItemToModel(&secondary_holding_space_model,
                 HoldingSpaceItem::Type::kDownload,
                 base::FilePath("/tmp/fake_2"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  SwitchToSecondaryUser("user@secondary", /*client=*/nullptr,
                        /*model=*/&secondary_holding_space_model);
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  UnregisterModelForUser("user@secondary");
}

TEST_P(HoldingSpaceTrayTest, HideButtonOnUserAddingScreen) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // The tray button should be showing if the user has an item in holding space.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // The tray button should be hidden if the user adding screen is running.
  SetUserAddingScreenRunning(true);
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // The tray button should be showing if the user adding screen is finished.
  SetUserAddingScreenRunning(false);
  EXPECT_TRUE(test_api()->IsShowingInShelf());
}

TEST_P(HoldingSpaceTrayTest, AddingItemShowsTrayBubble) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item - the button should be shown.
  HoldingSpaceItem* item_1 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  // Remove the only item - the button should be hidden.
  model()->RemoveItem(item_1->id());
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a screen capture item - the button should be shown.
  HoldingSpaceItem* item_2 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  // Remove the only item - the button should be hidden.
  model()->RemoveItem(item_2->id());
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a pinned item - the button should be shown.
  HoldingSpaceItem* item_3 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_3"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  // Remove the only item - the button should be hidden.
  model()->RemoveItem(item_3->id());
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());
}

TEST_P(HoldingSpaceTrayTest, TrayButtonNotShownForPartialItemsOnly) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add few partial items - the tray button should remain hidden.
  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kDownload,
                              base::FilePath("/tmp/fake_1"));
  EXPECT_FALSE(test_api()->IsShowingInShelf());
  HoldingSpaceItem* item_2 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));
  EXPECT_FALSE(test_api()->IsShowingInShelf());
  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kScreenshot,
                              base::FilePath("/tmp/fake_3"));
  EXPECT_FALSE(test_api()->IsShowingInShelf());
  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kPinnedFile,
                              base::FilePath("/tmp/fake_4"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Finalize one item, and verify the tray button gets shown.
  model()->FinalizeOrRemoveItem(item_2->id(), GURL("filesystem:fake_2"));

  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  // Remove the finalized item - the shelf button should get hidden.
  model()->RemoveItem(item_2->id());
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());
}

// Tests that the tray icon size changes on in-app shelf.
TEST_P(HoldingSpaceTrayTest, UpdateTrayIconSizeForInAppShelf) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item - the button should be shown.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();

  EXPECT_TRUE(test_api()->IsShowingInShelf());
  if (IsPreviewsFeatureEnabled()) {
    ASSERT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));
    EXPECT_EQ(gfx::Size(kHoldingSpaceTrayIconDefaultPreviewSize, kTrayItemSize),
              test_api()->GetPreviewsTrayIcon()->size());
  } else {
    ASSERT_TRUE(IsViewVisible(test_api()->GetDefaultTrayIcon()));
    EXPECT_EQ(gfx::Size(kHoldingSpaceTrayIconDefaultPreviewSize, kTrayItemSize),
              test_api()->GetDefaultTrayIcon()->size());
  }

  TabletModeControllerTestApi().EnterTabletMode();

  // Create a test widget to force in-app shelf.
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  ASSERT_TRUE(widget);

  EXPECT_TRUE(test_api()->IsShowingInShelf());
  if (IsPreviewsFeatureEnabled()) {
    ASSERT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));
    EXPECT_EQ(gfx::Size(kHoldingSpaceTrayIconSmallPreviewSize, kTrayItemSize),
              test_api()->GetPreviewsTrayIcon()->size());
  } else {
    ASSERT_TRUE(IsViewVisible(test_api()->GetDefaultTrayIcon()));
    EXPECT_EQ(gfx::Size(kHoldingSpaceTrayIconDefaultPreviewSize, kTrayItemSize),
              test_api()->GetDefaultTrayIcon()->size());
  }

  // Transition to home screen.
  widget->Minimize();

  if (IsPreviewsFeatureEnabled()) {
    ASSERT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));
    EXPECT_EQ(gfx::Size(kHoldingSpaceTrayIconDefaultPreviewSize, kTrayItemSize),
              test_api()->GetPreviewsTrayIcon()->size());
  } else {
    ASSERT_TRUE(IsViewVisible(test_api()->GetDefaultTrayIcon()));
    EXPECT_EQ(gfx::Size(kHoldingSpaceTrayIconDefaultPreviewSize, kTrayItemSize),
              test_api()->GetDefaultTrayIcon()->size());
  }
}

// Tests that a shelf config change just after an item has been removed does
// not cause a crash.
TEST_P(HoldingSpaceTrayTest, ShelfConfigChangeWithDelayedItemRemoval) {
  MarkTimeOfFirstPin();
  StartSession();

  // Create a test widget to force in-app shelf in tablet mode.
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  ASSERT_TRUE(widget);

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  HoldingSpaceItem* item_1 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  HoldingSpaceItem* item_2 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();

  EXPECT_TRUE(test_api()->IsShowingInShelf());

  model()->RemoveItem(item_1->id());
  TabletModeControllerTestApi().EnterTabletMode();
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();

  EXPECT_TRUE(test_api()->IsShowingInShelf());

  model()->RemoveItem(item_2->id());
  TabletModeControllerTestApi().LeaveTabletMode();
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());
}

// Tests that a shelf alignment change will behave as expected when there are
// multiple displays (and therefore multiple shelves/trays).
TEST_P(HoldingSpaceTrayTest, ShelfAlignmentChangeWithMultipleDisplays) {
  // This test is only relevant when previews are enabled.
  if (!IsPreviewsFeatureEnabled())
    return;

  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // This test requires multiple displays. Create two.
  UpdateDisplay("1280x768,1280x768");

  MarkTimeOfFirstPin();
  StartSession();

  // Cache shelves/trays for each display.
  Shelf* const primary_shelf = GetShelf(GetPrimaryDisplay());
  Shelf* const secondary_shelf = GetShelf(GetSecondaryDisplay());
  HoldingSpaceTray* const primary_tray = GetTray(primary_shelf);
  HoldingSpaceTray* const secondary_tray = GetTray(secondary_shelf);

  // Trays should not initially be visible.
  ASSERT_FALSE(primary_tray->GetVisible());
  ASSERT_FALSE(secondary_tray->GetVisible());

  // Add a few holding space items to cause trays to show in shelves.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_3"));

  // Trays should now be visible.
  ASSERT_TRUE(primary_tray->GetVisible());
  ASSERT_TRUE(secondary_tray->GetVisible());

  // Immediately update previews for each tray.
  primary_tray->FirePreviewsUpdateTimerIfRunningForTesting();
  secondary_tray->FirePreviewsUpdateTimerIfRunningForTesting();

  // Cache previews for each tray.
  views::View* const primary_icon_previews_container =
      primary_tray->GetViewByID(kHoldingSpaceTrayPreviewsIconId)->children()[0];
  views::View* const secondary_icon_previews_container =
      secondary_tray->GetViewByID(kHoldingSpaceTrayPreviewsIconId)
          ->children()[0];
  const std::vector<ui::Layer*>& primary_icon_previews =
      primary_icon_previews_container->layer()->children();
  const std::vector<ui::Layer*>& secondary_icon_previews =
      secondary_icon_previews_container->layer()->children();

  // Verify each tray contains three previews.
  ASSERT_EQ(primary_icon_previews.size(), 3u);
  ASSERT_EQ(secondary_icon_previews.size(), 3u);

  // Verify initial preview transforms. Since both shelves currently are bottom
  // aligned, previews should be positioned horizontally.
  for (int i = 0; i < 3; ++i) {
    const int main_axis_offset =
        (2 - i) * kHoldingSpaceTrayIconDefaultPreviewSize / 2;
    ASSERT_EQ(primary_icon_previews[i]->transform().To2dTranslation(),
              gfx::Vector2d(main_axis_offset, 0));
    ASSERT_EQ(secondary_icon_previews[i]->transform().To2dTranslation(),
              gfx::Vector2d(main_axis_offset, 0));
  }

  // Change the secondary shelf to a vertical alignment.
  secondary_shelf->SetAlignment(ShelfAlignment::kRight);

  // Verify preview transforms. The primary shelf should still position its
  // previews horizontally but the secondary shelf should now position its
  // previews vertically.
  for (int i = 0; i < 3; ++i) {
    const int main_axis_offset =
        (2 - i) * kHoldingSpaceTrayIconDefaultPreviewSize / 2;
    ASSERT_EQ(primary_icon_previews[i]->transform().To2dTranslation(),
              gfx::Vector2d(main_axis_offset, 0));
    ASSERT_EQ(secondary_icon_previews[i]->transform().To2dTranslation(),
              gfx::Vector2d(0, main_axis_offset));
  }

  // Change the secondary shelf back to a horizontal alignment.
  secondary_shelf->SetAlignment(ShelfAlignment::kBottom);

  // Verify preview transforms. Since both shelves are bottom aligned once
  // again, previews should be positioned horizontally.
  for (int i = 0; i < 3; ++i) {
    const int main_axis_offset =
        (2 - i) * kHoldingSpaceTrayIconDefaultPreviewSize / 2;
    ASSERT_EQ(primary_icon_previews[i]->transform().To2dTranslation(),
              gfx::Vector2d(main_axis_offset, 0));
    ASSERT_EQ(secondary_icon_previews[i]->transform().To2dTranslation(),
              gfx::Vector2d(main_axis_offset, 0));
  }
}

// Tests how download chips are updated during item addition, removal and
// finalization.
TEST_P(HoldingSpaceTrayTest, DownloadsSection) {
  StartSession();

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());

  // Add a download item and verify recent file bubble gets shown.
  HoldingSpaceItem* item_1 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));

  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  ASSERT_EQ(1u, test_api()->GetDownloadChips().size());

  // Add partially initialized download item - verify it doesn't get shown in
  // the UI yet.
  HoldingSpaceItem* item_2 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(1u, download_chips.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());

  // Add another download, and verify it's shown in the UI.
  HoldingSpaceItem* item_3 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_3"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Finalize partially initialized item, and verify it gets added to the
  // section, in the order of addition, replacing the oldest item.
  model()->FinalizeOrRemoveItem(item_2->id(), GURL("filesystem:fake_2"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove the newest item, and verify the section gets updated.
  model()->RemoveItem(item_3->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove other items, and verify the recent files bubble gets hidden.
  model()->RemoveItem(item_2->id());

  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(1u, download_chips.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());

  model()->RemoveItem(item_1->id());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  // Pinned bubble is showing "educational" info, and it should remain shown.
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
}

// Verifies the downloads section is shown and orders items as expected when the
// model contains a number of finalized items prior to showing UI.
TEST_P(HoldingSpaceTrayTest, DownloadsSectionWithFinalizedItemsOnly) {
  MarkTimeOfFirstPin();
  StartSession();

  // Add a number of finalized download items.
  std::deque<HoldingSpaceItem*> items;
  for (size_t i = 0; i < kMaxDownloads; ++i) {
    items.push_back(
        AddItem(HoldingSpaceItem::Type::kDownload,
                base::FilePath("/tmp/fake_" + base::NumberToString(i))));
  }

  test_api()->Show();
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  std::vector<views::View*> download_files = test_api()->GetDownloadChips();
  ASSERT_EQ(items.size(), download_files.size());

  while (!items.empty()) {
    // View order is expected to be reverse of item order.
    auto* download_file = HoldingSpaceItemView::Cast(download_files.back());
    EXPECT_EQ(download_file->item()->id(), items.front()->id());

    items.pop_front();
    download_files.pop_back();
  }

  test_api()->Close();
}

TEST_P(HoldingSpaceTrayTest, FinalizingDownloadItemThatShouldBeInvisible) {
  StartSession();
  test_api()->Show();

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());

  // Add partially initialized download item - verify it doesn't get shown in
  // the UI yet.
  HoldingSpaceItem* item_1 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));

  // Add two download items.
  HoldingSpaceItem* item_2 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));
  HoldingSpaceItem* item_3 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_3"));
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Finalize partially initialized item, and verify it's not added to the
  // section.
  model()->FinalizeOrRemoveItem(item_1->id(), GURL("filesystem:fake_1"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove the oldest item, and verify the section doesn't get updated.
  model()->RemoveItem(item_1->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());
}

// Tests that a partially initialized download item does not get shown if a full
// download item gets removed from the holding space.
TEST_P(HoldingSpaceTrayTest, PartialItemNowShownOnRemovingADownloadItem) {
  StartSession();
  test_api()->Show();

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());

  // Add partially initialized download item - verify it doesn't get shown in
  // the UI yet.
  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kDownload,
                              base::FilePath("/tmp/fake_1"));

  // Add two download items.
  HoldingSpaceItem* item_2 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));
  HoldingSpaceItem* item_3 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_3"));
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove one of the fully initialized items, and verify the partially
  // initialized item is no shown.
  model()->RemoveItem(item_2->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(1u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
}

// Tests how screen captures section is updated during item addition, removal
// and finalization.
TEST_P(HoldingSpaceTrayTest, ScreenCapturesSection) {
  StartSession();
  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  // Add a screenshot item and verify recent file bubble gets shown.
  HoldingSpaceItem* item_1 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_1"));

  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  ASSERT_EQ(1u, test_api()->GetScreenCaptureViews().size());

  // Add partially initialized download item - verify it doesn't get shown in
  // the UI yet.
  HoldingSpaceItem* item_2 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake_2"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_captures =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(1u, screen_captures.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());

  // Add more items to fill up the section.
  HoldingSpaceItem* item_3 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_3"));
  HoldingSpaceItem* item_4 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_4"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Finalize partially initialized item, and verify it gets added to the
  // section, in the order of addition, replacing the oldest item.
  model()->FinalizeOrRemoveItem(item_2->id(), GURL("filesystem:fake_2"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Remove the newest item, and verify the section gets updated.
  model()->RemoveItem(item_4->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Remove other items, and verify the recent files bubble gets hidden.
  model()->RemoveItem(item_2->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(2u, screen_captures.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());

  model()->RemoveItem(item_3->id());
  model()->RemoveItem(item_1->id());

  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  // Pinned bubble is showing "educational" info, and it should remain shown.
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
}

// Verifies the screen captures section is shown and orders items as expected
// when the model contains a number of finalized items prior to showing UI.
TEST_P(HoldingSpaceTrayTest, ScreenCapturesSectionWithFinalizedItemsOnly) {
  MarkTimeOfFirstPin();
  StartSession();

  // Add a number of finalized screen capture items.
  std::deque<HoldingSpaceItem*> items;
  for (size_t i = 0; i < kMaxScreenCaptures; ++i) {
    items.push_back(
        AddItem(HoldingSpaceItem::Type::kScreenshot,
                base::FilePath("/tmp/fake_" + base::NumberToString(i))));
  }

  test_api()->Show();
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  std::vector<views::View*> screenshots = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(items.size(), screenshots.size());

  while (!items.empty()) {
    // View order is expected to be reverse of item order.
    auto* screenshot = HoldingSpaceItemView::Cast(screenshots.back());
    EXPECT_EQ(screenshot->item()->id(), items.front()->id());

    items.pop_front();
    screenshots.pop_back();
  }

  test_api()->Close();
}

TEST_P(HoldingSpaceTrayTest, FinalizingScreenCaptureItemThatShouldBeInvisible) {
  StartSession();
  test_api()->Show();

  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add partially initialized download item - verify it doesn't get shown in
  // the UI yet.
  HoldingSpaceItem* item_1 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake_1"));

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add enough screenshot items to fill up the section.
  HoldingSpaceItem* item_2 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_2"));
  HoldingSpaceItem* item_3 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_3"));
  HoldingSpaceItem* item_4 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_4"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_captures =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Finalize partially initialized item, and verify it's not added to the
  // section.
  model()->FinalizeOrRemoveItem(item_1->id(), GURL("filesystem:fake_1"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Remove the oldest item, and verify the section doesn't get updated.
  model()->RemoveItem(item_1->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  test_api()->Close();
}

// Tests that a partially initialized screenshot item does not get shown if a
// fully initialized screenshot item gets removed from the holding space.
TEST_P(HoldingSpaceTrayTest, PartialItemNowShownOnRemovingAScreenCapture) {
  StartSession();
  test_api()->Show();

  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add partially initialized item - verify it doesn't get shown in the UI yet.
  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kScreenshot,
                              base::FilePath("/tmp/fake_1"));

  HoldingSpaceItem* item_2 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_2"));
  HoldingSpaceItem* item_3 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_3"));
  HoldingSpaceItem* item_4 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_4"));
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_captures =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Remove one of the fully initialized items, and verify the partially
  // initialized item is no shown.
  model()->RemoveItem(item_2->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(2u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());

  test_api()->Close();
}

// Tests how the pinned item section is updated during item addition, removal
// and finalization.
TEST_P(HoldingSpaceTrayTest, PinnedFilesSection) {
  MarkTimeOfFirstPin();
  StartSession();

  HoldingSpaceItem* item_1 = AddItem(HoldingSpaceItem::Type::kPinnedFile,
                                     base::FilePath("/tmp/fake_1"));

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(1u, pinned_files.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());

  // Add a partially initialized item - verify it doesn't get shown in the UI
  // yet.
  HoldingSpaceItem* item_2 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake_2"));

  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(1u, pinned_files.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());

  // Add more items to the section.
  HoldingSpaceItem* item_3 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake_3"));
  HoldingSpaceItem* item_4 = AddItem(HoldingSpaceItem::Type::kPinnedFile,
                                     base::FilePath("/tmp/fake_4"));

  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(2u, pinned_files.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[1])->item()->id());

  // Finalize partially initialized item, and verify it gets shown.
  model()->FinalizeOrRemoveItem(item_2->id(), GURL("filesystem:fake_2"));

  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(3u, pinned_files.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(pinned_files[1])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[2])->item()->id());

  // Remove a partial item.
  model()->RemoveItem(item_3->id());

  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(3u, pinned_files.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(pinned_files[1])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[2])->item()->id());

  // Remove the newest item, and verify the section gets updated.
  model()->RemoveItem(item_4->id());

  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(2u, pinned_files.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[1])->item()->id());

  // Remove other items, and verify the files section gets hidden.
  model()->RemoveItem(item_2->id());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(1u, pinned_files.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());

  model()->RemoveItem(item_1->id());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  EXPECT_FALSE(test_api()->PinnedFilesBubbleShown());
}

// Verifies the pinned files bubble is not shown if it only contains partially
// initialized items.
TEST_P(HoldingSpaceTrayTest,
       PinnedFilesBubbleWithPartiallyInitializedItemsOnly) {
  MarkTimeOfFirstPin();
  StartSession();

  // Add a download item to show the tray button.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/download"));

  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kPinnedFile,
                              base::FilePath("/tmp/fake_1"));

  test_api()->Show();
  EXPECT_FALSE(test_api()->PinnedFilesBubbleShown());

  // Add another partially initialized item.
  HoldingSpaceItem* item_2 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake_2"));
  EXPECT_FALSE(test_api()->PinnedFilesBubbleShown());

  // Add a fully initialized item, and verify it gets shown.
  HoldingSpaceItem* item_3 = AddItem(HoldingSpaceItem::Type::kPinnedFile,
                                     base::FilePath("/tmp/fake_3"));
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());

  std::vector<views::View*> pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(1u, pinned_files.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
  EXPECT_TRUE(HoldingSpaceItemView::Cast(pinned_files[0])->GetVisible());

  // Finalize a partially initialized item with an empty URL - it should get
  // removed.
  model()->FinalizeOrRemoveItem(item_2->id(), GURL());

  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(1u, pinned_files.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
}

// Verifies the pinned items section is shown and orders items as expected when
// the model contains a number of finalized items prior to showing UI.
TEST_P(HoldingSpaceTrayTest, PinnedFilesSectionWithFinalizedItemsOnly) {
  MarkTimeOfFirstPin();
  StartSession();

  // Add a number of finalized pinned items.
  std::deque<HoldingSpaceItem*> items;
  for (int i = 0; i < 10; ++i) {
    items.push_back(
        AddItem(HoldingSpaceItem::Type::kPinnedFile,
                base::FilePath("/tmp/fake_" + base::NumberToString(i))));
  }

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());

  std::vector<views::View*> pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(items.size(), pinned_files.size());

  while (!items.empty()) {
    // View order is expected to be reverse of item order.
    auto* pinned_file = HoldingSpaceItemView::Cast(pinned_files.back());
    EXPECT_EQ(pinned_file->item()->id(), items.front()->id());

    items.pop_front();
    pinned_files.pop_back();
  }
  test_api()->Close();
}

// Tests that as nearby shared files are added to the model, they show on the
// downloads section.
TEST_P(HoldingSpaceTrayTest, DownloadsSectionWithNearbySharedFiles) {
  StartSession();

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  ASSERT_TRUE(test_api()->GetDownloadChips().empty());

  // Add a nearby share item and verify recent files bubble gets shown.
  HoldingSpaceItem* item_1 = AddItem(HoldingSpaceItem::Type::kNearbyShare,
                                     base::FilePath("/tmp/fake_1"));
  ASSERT_TRUE(item_1->IsFinalized());

  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  ASSERT_EQ(1u, test_api()->GetDownloadChips().size());

  // Add a download item, and verify it's also shown in the UI in the order they
  // were added.
  HoldingSpaceItem* item_2 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove the first item, and verify the section gets updated.
  model()->RemoveItem(item_1->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(1u, download_chips.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());

  test_api()->Close();
}

// Tests that a partially initialized nearby share item does not get shown if a
// full download item gets removed from the holding space.
TEST_P(HoldingSpaceTrayTest, PartialNearbyShareItemWithExistingDownloadItems) {
  StartSession();
  test_api()->Show();

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  ASSERT_TRUE(test_api()->GetDownloadChips().empty());

  // Add partially initialized nearby share item - verify it doesn't get shown
  // in the UI yet.
  HoldingSpaceItem* nearby_share_item =
      AddPartiallyInitializedItem(HoldingSpaceItem::Type::kNearbyShare,
                                  base::FilePath("/tmp/nearby_share"));
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());

  // Add partially initialized screenshot item - verify it doesn't get shown in
  // the UI yet.
  HoldingSpaceItem* screenshot_item = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/screenshot"));
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  // Add two download items.
  HoldingSpaceItem* download_item_1 = AddItem(
      HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/download_1"));
  HoldingSpaceItem* download_item_2 = AddItem(
      HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/download_2"));
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(download_item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(download_item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Finalize the nearby share item and verify it is not shown.
  model()->FinalizeOrRemoveItem(nearby_share_item->id(),
                                GURL("filesystem:nearby_share"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(download_item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(download_item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Finalize the screenshot item and verify it is shown. Note that the
  // finalized screenshot item should not affect appearance of the downloads
  // section of holding space UI. It shows in the screen captures section.
  model()->FinalizeOrRemoveItem(screenshot_item->id(),
                                GURL("filesystem:screenshot"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_EQ(1u, test_api()->GetScreenCaptureViews().size());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(download_item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(download_item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove one of the fully initialized items, and verify the nearby share item
  // that was finalized late is shown.
  model()->RemoveItem(download_item_1->id());

  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(download_item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(nearby_share_item->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  test_api()->Close();
}

// Tests that a partially initialized download item does not get shown if a
// full download item gets removed from the holding space.
TEST_P(HoldingSpaceTrayTest, PartialDownloadItemWithExistingNearbyShareItems) {
  StartSession();
  test_api()->Show();

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  ASSERT_TRUE(test_api()->GetDownloadChips().empty());

  // Add partially initialized download item - verify it doesn't get shown
  // in the UI yet.
  HoldingSpaceItem* item_1 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());

  // Add two nearby share items.
  HoldingSpaceItem* item_2 = AddItem(HoldingSpaceItem::Type::kNearbyShare,
                                     base::FilePath("/tmp/fake_2"));
  HoldingSpaceItem* item_3 = AddItem(HoldingSpaceItem::Type::kNearbyShare,
                                     base::FilePath("/tmp/fake_3"));
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Finalize the download item and verify it is not shown.
  model()->FinalizeOrRemoveItem(item_1->id(), GURL("filesystem:fake_1"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove one of the fully initialized items, and verify the partially
  // initialized item is not shown.
  model()->RemoveItem(item_2->id());

  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  test_api()->Close();
}

// Right clicking the holding space tray should show a context menu if the
// previews feature is enabled. Otherwise it should do nothing.
TEST_P(HoldingSpaceTrayTest, ShouldMaybeShowContextMenuOnRightClick) {
  StartSession();

  views::View* tray = test_api()->GetTray();
  ASSERT_TRUE(tray);

  EXPECT_FALSE(views::MenuController::GetActiveInstance());

  // Move the mouse to and perform a right click on `tray`.
  auto* root_window = tray->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(tray->GetBoundsInScreen().CenterPoint());
  event_generator.ClickRightButton();

  EXPECT_EQ(!!views::MenuController::GetActiveInstance(),
            IsPreviewsFeatureEnabled());
}

// Tests that as screen recording files are added to the model, they show in the
// screen captures section.
TEST_P(HoldingSpaceTrayTest, ScreenCapturesSectionWithScreenRecordingFiles) {
  StartSession();

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  ASSERT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add a screen recording item and verify recent files section gets shown.
  HoldingSpaceItem* item_1 = AddItem(HoldingSpaceItem::Type::kScreenRecording,
                                     base::FilePath("/tmp/fake_1"));
  ASSERT_TRUE(item_1->IsFinalized());

  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  ASSERT_EQ(1u, test_api()->GetScreenCaptureViews().size());

  // Add a screenshot item, and verify it's also shown in the UI in the reverse
  // order they were added.
  HoldingSpaceItem* item_2 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_2"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_capture_chips =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(2u, screen_capture_chips.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());

  // Remove the first item, and verify the section gets updated.
  model()->RemoveItem(item_1->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(1u, screen_capture_chips.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());

  test_api()->Close();
}

// Tests that a partially initialized screen recording item shows in the UI in
// the reverse order from added time rather than finalization time.
TEST_P(HoldingSpaceTrayTest,
       PartialScreenRecordingItemWithExistingScreenshotItems) {
  StartSession();
  test_api()->Show();

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  ASSERT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add partially initialized screen recording item - verify it doesn't get
  // shown in the UI yet.
  HoldingSpaceItem* screen_recording_item =
      AddPartiallyInitializedItem(HoldingSpaceItem::Type::kScreenRecording,
                                  base::FilePath("/tmp/screen_recording"));
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add three screenshot items to fill up the section.
  HoldingSpaceItem* screenshot_item_1 = AddItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/screenshot_1"));
  HoldingSpaceItem* screenshot_item_2 = AddItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/screenshot_2"));
  HoldingSpaceItem* screenshot_item_3 = AddItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/screenshot_3"));
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_capture_chips =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screenshot_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screenshot_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screenshot_item_1->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Finalize the screen recording item and verify it is not shown.
  model()->FinalizeOrRemoveItem(screen_recording_item->id(),
                                GURL("filesystem:screen_recording"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screenshot_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screenshot_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screenshot_item_1->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Remove one of the fully initialized items, and verify the screen recording
  // item that was finalized late is shown.
  model()->RemoveItem(screenshot_item_1->id());

  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screenshot_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screenshot_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screen_recording_item->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Add partially initialized screen recording item - verify it doesn't get
  // shown in the UI yet.
  HoldingSpaceItem* screen_recording_item_last =
      AddPartiallyInitializedItem(HoldingSpaceItem::Type::kScreenRecording,
                                  base::FilePath("/tmp/screen_recording_last"));
  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screenshot_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screenshot_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screen_recording_item->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Finalize the screen recording item and verify it is shown first.
  model()->FinalizeOrRemoveItem(screen_recording_item_last->id(),
                                GURL("filesystem:screen_recording"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screen_recording_item_last->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screenshot_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screenshot_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  test_api()->Close();
}

// Tests that partially initialized screenshot item shows in the UI in the
// reverse order from added time rather than finalization time.
TEST_P(HoldingSpaceTrayTest,
       PartialScreenshotItemWithExistingScreenRecordingItems) {
  StartSession();
  test_api()->Show();

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  ASSERT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add partially initialized screenshot item - verify it doesn't get shown
  // in the UI yet.
  HoldingSpaceItem* screenshot_item = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake_1"));
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add three screenshot recording items to fill up the section.
  HoldingSpaceItem* screen_recording_item_1 = AddItem(
      HoldingSpaceItem::Type::kScreenRecording, base::FilePath("/tmp/fake_2"));
  HoldingSpaceItem* screen_recording_item_2 = AddItem(
      HoldingSpaceItem::Type::kScreenRecording, base::FilePath("/tmp/fake_3"));
  HoldingSpaceItem* screen_recording_item_3 = AddItem(
      HoldingSpaceItem::Type::kScreenRecording, base::FilePath("/tmp/fake_4"));
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_capture_chips =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screen_recording_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screen_recording_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screen_recording_item_1->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Finalize the screenshot item and verify it is not shown.
  model()->FinalizeOrRemoveItem(screenshot_item->id(),
                                GURL("filesystem:fake_1"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screen_recording_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screen_recording_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screen_recording_item_1->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Remove one of the fully initialized items, and verify the partially
  // initialized item is not shown.
  model()->RemoveItem(screen_recording_item_1->id());

  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screen_recording_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screen_recording_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screenshot_item->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  test_api()->Close();
}

// Screen recordings should have an overlaying play icon.
TEST_P(HoldingSpaceTrayTest, PlayIconForScreenRecordings) {
  StartSession();
  test_api()->Show();

  // Add one screenshot item and one screen recording item.
  HoldingSpaceItem* screenshot_item = AddItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake_1"));
  HoldingSpaceItem* screen_recording_item = AddItem(
      HoldingSpaceItem::Type::kScreenRecording, base::FilePath("/tmp/fake_2"));
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  std::vector<views::View*> screen_capture_chips =
      test_api()->GetScreenCaptureViews();

  EXPECT_EQ(2u, screen_capture_chips.size());

  EXPECT_EQ(screenshot_item->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_FALSE(screen_capture_chips[1]->GetViewByID(
      kHoldingSpaceScreenCapturePlayIconId));
  EXPECT_EQ(screen_recording_item->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_TRUE(screen_capture_chips[0]->GetViewByID(
      kHoldingSpaceScreenCapturePlayIconId));
}

// Until the user has pinned an item, a placeholder should exist in the pinned
// files bubble which contains a chip to open the Files app.
TEST_P(HoldingSpaceTrayTest, PlaceholderContainsFilesAppChip) {
  StartSession(/*pre_mark_time_of_first_add=*/false);

  // The tray button should *not* be shown for users that have never added
  // anything to the holding space.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item. This should cause the tray button to show.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake"));
  MarkTimeOfFirstAdd();
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Show the bubble. Both the pinned files and recent files child bubbles
  // should be shown.
  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  // A chip to open the Files app should exist in the pinned files bubble.
  views::View* pinned_files_bubble = test_api()->GetPinnedFilesBubble();
  ASSERT_TRUE(pinned_files_bubble);
  views::View* files_app_chip =
      pinned_files_bubble->GetViewByID(kHoldingSpaceFilesAppChipId);
  ASSERT_TRUE(files_app_chip);

  // Prior to being acted upon by the user, there should be no events logged to
  // the Files app chip histogram.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.FilesAppChip.Action.All",
      holding_space_metrics::FilesAppChipAction::kClick, 0);

  // Click the chip and expect a call to open the Files app.
  EXPECT_CALL(*client(), OpenMyFiles);
  Click(files_app_chip);

  // After having been acted upon by the user, there should be a single click
  // event logged to the Files app chip histogram.
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.FilesAppChip.Action.All",
      holding_space_metrics::FilesAppChipAction::kClick, 1);
}

// User should be able to open the Downloads folder in the Files app by pressing
// the enter key on the Downloads section header.
TEST_P(HoldingSpaceTrayTest, EnterKeyOpensDownloads) {
  StartSession();

  // Add a download item.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake1"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Show the bubble.
  test_api()->Show();
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);

  // Select the download item. Previously there was a bug where if a holding
  // space item view was selected, the enter key would *not* open Downloads.
  Click(download_chips[0]);
  EXPECT_TRUE(HoldingSpaceItemView::Cast(download_chips[0])->selected());

  // Focus the downloads section header.
  auto* downloads_section_header = test_api()->GetDownloadsSectionHeader();
  ASSERT_TRUE(downloads_section_header);
  EXPECT_TRUE(PressTabUntilFocused(downloads_section_header));

  // Press ENTER and expect an attempt to open the Downloads folder in the Files
  // app. There should be *no* attempts to open an holding space items.
  EXPECT_CALL(*client(), OpenItems).Times(0);
  EXPECT_CALL(*client(), OpenDownloads);
  PressAndReleaseKey(downloads_section_header, ui::KeyboardCode::VKEY_RETURN);
}

// User should be able to launch selected holding space items by pressing the
// enter key.
TEST_P(HoldingSpaceTrayTest, EnterKeyOpensSelectedFiles) {
  StartSession();

  // Add three holding space items.
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake1"));
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake2"));
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake3"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Show the bubble.
  test_api()->Show();
  const std::vector<views::View*> pinned_file_chips =
      test_api()->GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 3u);
  const std::array<HoldingSpaceItemView*, 3> item_views = {
      HoldingSpaceItemView::Cast(pinned_file_chips[0]),
      HoldingSpaceItemView::Cast(pinned_file_chips[1]),
      HoldingSpaceItemView::Cast(pinned_file_chips[2]),
  };

  // Press the enter key. The client should *not* attempt to open any items.
  EXPECT_CALL(*client(), OpenItems).Times(0);
  PressAndReleaseKey(item_views[0], ui::KeyboardCode::VKEY_RETURN);
  testing::Mock::VerifyAndClearExpectations(client());

  // Click an item. The view should be selected.
  Click(item_views[0]);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());

  // Press the enter key. We expect the client to open the selected item.
  EXPECT_CALL(*client(), OpenItems(testing::ElementsAre(item_views[0]->item()),
                                   testing::_));
  PressAndReleaseKey(item_views[0], ui::KeyboardCode::VKEY_RETURN);
  testing::Mock::VerifyAndClearExpectations(client());

  // Shift-click on the second item. Both views should be selected.
  Click(item_views[1], ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_TRUE(item_views[1]->selected());

  // Press the enter key. We expect the client to open the selected items.
  EXPECT_CALL(*client(), OpenItems(testing::ElementsAre(item_views[0]->item(),
                                                        item_views[1]->item()),
                                   testing::_));
  PressAndReleaseKey(item_views[1], ui::KeyboardCode::VKEY_RETURN);
  testing::Mock::VerifyAndClearExpectations(client());

  // Tab traverse to the last item.
  EXPECT_TRUE(PressTabUntilFocused(item_views[2]));

  // Press the enter key. The client should open only the focused item since
  // it was *not* selected prior to pressing the enter key.
  EXPECT_CALL(*client(), OpenItems(testing::ElementsAre(item_views[2]->item()),
                                   testing::_));
  PressAndReleaseKey(item_views[2], ui::KeyboardCode::VKEY_RETURN);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());
}

// Clicking on tote bubble background should deselect any selected items.
TEST_P(HoldingSpaceTrayTest, ClickBackgroundToDeselectItems) {
  StartSession();

  // Add two items.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake1"));
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake2"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Show the bubble and cache holding space item views.
  test_api()->Show();
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  std::array<HoldingSpaceItemView*, 2> item_views = {
      HoldingSpaceItemView::Cast(download_chips[0]),
      HoldingSpaceItemView::Cast(download_chips[1])};

  // Click an item chip. The view should be selected.
  Click(download_chips[0]);
  ASSERT_TRUE(item_views[0]->selected());
  ASSERT_FALSE(item_views[1]->selected());

  // Clicking on the parent view should deselect item.
  Click(download_chips[0]->parent());
  ASSERT_FALSE(item_views[0]->selected());
  ASSERT_FALSE(item_views[1]->selected());

  // Click on both items to select them both.
  Click(download_chips[0], ui::EF_SHIFT_DOWN);
  Click(download_chips[1], ui::EF_SHIFT_DOWN);
  ASSERT_TRUE(item_views[0]->selected());
  ASSERT_TRUE(item_views[1]->selected());

  // Clicking on the parent view should deselect both items.
  Click(download_chips[0]->parent());
  ASSERT_FALSE(item_views[0]->selected());
  ASSERT_FALSE(item_views[1]->selected());
}

// It should be possible to select multiple items in clamshell mode.
TEST_P(HoldingSpaceTrayTest, MultiselectInClamshellMode) {
  StartSession();

  // Add a few holding space items to populate each section.
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake1"));
  AddItem(HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake2"));
  AddItem(HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake3"));
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake4"));
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake5"));

  // Show the bubble and cache holding space item views.
  test_api()->Show();
  std::vector<views::View*> pinned_file_chips =
      test_api()->GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 1u);
  std::vector<views::View*> screen_capture_views =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(screen_capture_views.size(), 2u);
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 2u);
  std::vector<HoldingSpaceItemView*> item_views(
      {HoldingSpaceItemView::Cast(pinned_file_chips[0]),
       HoldingSpaceItemView::Cast(screen_capture_views[0]),
       HoldingSpaceItemView::Cast(screen_capture_views[1]),
       HoldingSpaceItemView::Cast(download_chips[0]),
       HoldingSpaceItemView::Cast(download_chips[1])});

  // Shift-click the middle view. It should become selected.
  Click(item_views[2], ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());  // The clicked view.
  EXPECT_FALSE(item_views[3]->selected());
  EXPECT_FALSE(item_views[4]->selected());

  // Click the middle view. It should *not* become unselected.
  Click(item_views[2]);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());  // The clicked view.
  EXPECT_FALSE(item_views[3]->selected());
  EXPECT_FALSE(item_views[4]->selected());

  // Shift-click the bottom view. We should now have selected a range.
  Click(item_views[4], ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());  // The previously clicked view.
  EXPECT_TRUE(item_views[3]->selected());
  EXPECT_TRUE(item_views[4]->selected());  // The clicked view.

  // Shift-click the top view. The previous range should be cleared and the
  // new range selected.
  Click(item_views[0], ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(item_views[0]->selected());  // The clicked view.
  EXPECT_TRUE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());  // The previously clicked view.
  EXPECT_FALSE(item_views[3]->selected());
  EXPECT_FALSE(item_views[4]->selected());

  // Control-click the bottom view. The previous range should still be selected
  // as well as the view that was just clicked.
  Click(item_views[4], ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_TRUE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());
  EXPECT_FALSE(item_views[3]->selected());
  EXPECT_TRUE(item_views[4]->selected());  // The clicked view.

  // Shift-click the second-from-the-bottom view. A new range should be selected
  // from the bottom view to the view that was just clicked. The previous range
  // that was selected should still be selected.
  Click(item_views[3], ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_TRUE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());
  EXPECT_TRUE(item_views[3]->selected());  // The clicked view.
  EXPECT_TRUE(item_views[4]->selected());  // The previously clicked view.

  // Control-click the second-from-the-top view. The view that was just clicked
  // should now be unselected. No other views that were selected should change.
  Click(item_views[1], ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());  // The clicked view.
  EXPECT_TRUE(item_views[2]->selected());
  EXPECT_TRUE(item_views[3]->selected());
  EXPECT_TRUE(item_views[4]->selected());

  // Add another holding space item. This should cause views for existing
  // holding space items to be destroyed and recreated.
  AddItem(HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake6"));
  pinned_file_chips = test_api()->GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 1u);
  screen_capture_views = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(screen_capture_views.size(), 3u);
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 2u);
  item_views = {HoldingSpaceItemView::Cast(pinned_file_chips[0]),
                HoldingSpaceItemView::Cast(screen_capture_views[0]),
                HoldingSpaceItemView::Cast(screen_capture_views[1]),
                HoldingSpaceItemView::Cast(screen_capture_views[2]),
                HoldingSpaceItemView::Cast(download_chips[0]),
                HoldingSpaceItemView::Cast(download_chips[1])};

  // Views for items previously selected should have selection restored.
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());  // The view for the new item.
  EXPECT_FALSE(item_views[2]->selected());  // The previously clicked view.
  EXPECT_TRUE(item_views[3]->selected());
  EXPECT_TRUE(item_views[4]->selected());
  EXPECT_TRUE(item_views[5]->selected());

  // Shift-click the second-from-the-bottom view. A new range should be selected
  // from the previously clicked view to the view that was just clicked.
  Click(item_views[4], ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());  // The previously clicked view.
  EXPECT_TRUE(item_views[3]->selected());
  EXPECT_TRUE(item_views[4]->selected());  // The clicked view.
  EXPECT_TRUE(item_views[5]->selected());

  // Click the third-from-the-bottom view. Even though it was already selected
  // it should now be the only view selected.
  Click(item_views[3]);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());
  EXPECT_TRUE(item_views[3]->selected());  // The clicked view.
  EXPECT_FALSE(item_views[4]->selected());
  EXPECT_FALSE(item_views[5]->selected());

  // Control-click the third-from-the-bottom view. There should no longer be
  // any views selected.
  Click(item_views[3], ui::EF_CONTROL_DOWN);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());
  EXPECT_FALSE(item_views[3]->selected());  // The clicked view.
  EXPECT_FALSE(item_views[4]->selected());
  EXPECT_FALSE(item_views[5]->selected());
}

// It should be possible to select multiple items in touch mode.
TEST_P(HoldingSpaceTrayTest, MultiselectInTouchMode) {
  StartSession();

  // Add a few holding space items.
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake1"));
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake2"));
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake3"));

  // Show the bubble and cache holding space item views.
  test_api()->Show();
  const std::vector<views::View*> pinned_file_chips =
      test_api()->GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 3u);
  std::array<HoldingSpaceItemView*, 3> item_views = {
      HoldingSpaceItemView::Cast(pinned_file_chips[0]),
      HoldingSpaceItemView::Cast(pinned_file_chips[1]),
      HoldingSpaceItemView::Cast(pinned_file_chips[2])};

  // Long press an item. The view should be selected and a context menu shown.
  LongPress(item_views[0]);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());
  EXPECT_TRUE(views::MenuController::GetActiveInstance());

  // Close the context menu. The view that was long pressed should still be
  // selected.
  PressAndReleaseKey(item_views[0], ui::KeyboardCode::VKEY_ESCAPE);
  EXPECT_FALSE(views::MenuController::GetActiveInstance());
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());

  // Long press another item. Both views that were long pressed should be
  // selected and a context menu shown.
  LongPress(item_views[1]);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_TRUE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());
  EXPECT_TRUE(views::MenuController::GetActiveInstance());

  // Close the context menu. Both views that were long pressed should still be
  // selected.
  PressAndReleaseKey(item_views[0], ui::KeyboardCode::VKEY_ESCAPE);
  EXPECT_FALSE(views::MenuController::GetActiveInstance());
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_TRUE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());

  // Tap one of the selected views. It should no longer be selected.
  GestureTap(item_views[0]);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_TRUE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());

  // Tap one of the unselected views. It should become selected.
  GestureTap(item_views[2]);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_TRUE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());

  // Tap both selected views. No views should be selected.
  GestureTap(item_views[1]);
  GestureTap(item_views[2]);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());

  // Tap an unselected view. This is the only way to open an item via touch.
  // There must be *no* views currently selected when tapping a view.
  EXPECT_CALL(*client(), OpenItems)
      .WillOnce(
          testing::Invoke([&](const std::vector<const HoldingSpaceItem*>& items,
                              HoldingSpaceClient::SuccessCallback callback) {
            ASSERT_EQ(items.size(), 1u);
            EXPECT_EQ(items[0], item_views[2]->item());
          }));
  GestureTap(item_views[2]);
  testing::Mock::VerifyAndClearExpectations(client());
}

// Verifies that selection UI is correctly represented depending on device state
// and the number of selected holding space item views.
TEST_P(HoldingSpaceTrayTest, SelectionUi) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  StartSession();

  // Add both a chip-style and screen-capture-style holding space item.
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake1"));
  AddItem(HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake2"));

  // Show holding space UI.
  test_api()->Show();
  ASSERT_TRUE(test_api()->IsShowing());

  // Cache holding space item views.
  std::vector<views::View*> pinned_file_chips =
      test_api()->GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 1u);
  std::vector<views::View*> screen_capture_views =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(screen_capture_views.size(), 1u);
  std::vector<HoldingSpaceItemView*> item_views = {
      HoldingSpaceItemView::Cast(pinned_file_chips[0]),
      HoldingSpaceItemView::Cast(screen_capture_views[0])};

  // Expects visibility of `view` to match `visible`.
  auto expect_visible = [](views::View* view, bool visible) {
    ASSERT_TRUE(view);
    EXPECT_EQ(view->GetVisible(), visible);
  };

  // Expects visibility of `item_view`'s checkmark to match `visible`.
  auto expect_checkmark_visible = [&](HoldingSpaceItemView* item_view,
                                      bool visible) {
    auto* checkmark = item_view->GetViewByID(kHoldingSpaceItemCheckmarkId);
    expect_visible(checkmark, visible);
  };

  // Expects visibility of `item_view`'s image to match `visible`.
  auto expect_image_visible = [&](HoldingSpaceItemView* item_view,
                                  bool visible) {
    auto* image = item_view->GetViewByID(kHoldingSpaceItemImageId);
    expect_visible(image, visible);
  };

  // Initially no holding space item views are selected.
  for (HoldingSpaceItemView* item_view : item_views) {
    EXPECT_FALSE(item_view->selected());
    expect_checkmark_visible(item_view, false);
    expect_image_visible(item_view, true);
  }

  // Select the first holding space item view.
  Click(item_views[0]);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());

  // Since the device is not in tablet mode and only a single holding space item
  // view is selected, no checkmarks should be shown.
  for (HoldingSpaceItemView* item_view : item_views) {
    expect_checkmark_visible(item_view, false);
    expect_image_visible(item_view, true);
  }

  // Add the second holding space item view to the selection.
  Click(item_views[1], ui::EF_CONTROL_DOWN);

  // Because there are multiple holding space item views selected, checkmarks
  // should be shown. For chip-style holding space item views the checkmark
  // replaces the image.
  for (HoldingSpaceItemView* item_view : item_views) {
    EXPECT_TRUE(item_view->selected());
    expect_checkmark_visible(item_view, true);
    expect_image_visible(item_view, item_view->item()->IsScreenCapture());
  }

  // Remove the second holding space item. Note that its view was selected.
  HoldingSpaceController::Get()->model()->RemoveItem(item_views[1]->item_id());

  // Re-cache holding space item views as they will have been destroyed and
  // recreated when animating item view removal.
  pinned_file_chips = test_api()->GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 1u);
  screen_capture_views = test_api()->GetScreenCaptureViews();
  EXPECT_EQ(screen_capture_views.size(), 0u);
  item_views = {HoldingSpaceItemView::Cast(pinned_file_chips[0])};

  // The first (and only) holding space item view should still be selected
  // although it should no longer show its checkmark since now only a single
  // holding space item view is selected.
  EXPECT_TRUE(item_views[0]->selected());
  expect_checkmark_visible(item_views[0], false);
  expect_image_visible(item_views[0], true);

  // Switch to tablet mode. Note that this closes holding space UI.
  ShellTestApi().SetTabletModeEnabledForTest(true);
  EXPECT_FALSE(test_api()->IsShowing());

  // Re-show holding space UI.
  test_api()->Show();
  ASSERT_TRUE(test_api()->IsShowing());

  // Cache holding space item views.
  pinned_file_chips = test_api()->GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 1u);
  screen_capture_views = test_api()->GetScreenCaptureViews();
  EXPECT_EQ(screen_capture_views.size(), 0u);
  item_views = {HoldingSpaceItemView::Cast(pinned_file_chips[0])};

  // Initially no holding space item views are selected.
  EXPECT_FALSE(item_views[0]->selected());

  // Select the first (and only) holding space item view.
  Click(item_views[0]);

  // In tablet mode, a selected holding space item view should always show its
  // checkmark even if it is the only holding space item view selected.
  EXPECT_TRUE(item_views[0]->selected());
  expect_checkmark_visible(item_views[0], true);
  expect_image_visible(item_views[0], false);

  // Switch out of tablet mode. Note that this *doesn't* close holding space UI.
  ShellTestApi().SetTabletModeEnabledForTest(false);
  ASSERT_TRUE(test_api()->IsShowing());

  // The first (and only) holding space item should still be selected but it
  // should update checkmark/image visibility given that it is the only holding
  // space item view selected.
  EXPECT_TRUE(item_views[0]->selected());
  expect_checkmark_visible(item_views[0], false);
  expect_image_visible(item_views[0], true);
}

// Verifies that attempting to open holding space items via double click works
// as expected with event modifiers.
TEST_P(HoldingSpaceTrayTest, OpenItemsViaDoubleClickWithEventModifiers) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  StartSession();

  // Add multiple holding space items.
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake1"));
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake2"));

  const auto show_holding_space_and_cache_views =
      [&](std::vector<HoldingSpaceItemView*>* item_views) {
        // Show holding space UI.
        test_api()->Show();
        ASSERT_TRUE(test_api()->IsShowing());

        // Cache holding space item views.
        const std::vector<views::View*> views =
            test_api()->GetPinnedFileChips();
        ASSERT_EQ(views.size(), 2u);
        *item_views = {HoldingSpaceItemView::Cast(views[0]),
                       HoldingSpaceItemView::Cast(views[1])};
      };

  std::vector<HoldingSpaceItemView*> item_views;
  show_holding_space_and_cache_views(&item_views);

  // Double click an item with the control key down. Expect the clicked holding
  // space item to be opened.
  EXPECT_CALL(*client(), OpenItems(ElementsAre(item_views[0]->item()), _));
  DoubleClick(item_views[0], ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(client());

  // Reset.
  test_api()->Close();
  show_holding_space_and_cache_views(&item_views);

  // Double click an item with the shift key down. Expect the clicked holding
  // space item to be opened.
  EXPECT_CALL(*client(), OpenItems(ElementsAre(item_views[0]->item()), _));
  DoubleClick(item_views[0], ui::EF_SHIFT_DOWN);
  testing::Mock::VerifyAndClearExpectations(client());

  // Reset.
  test_api()->Close();
  show_holding_space_and_cache_views(&item_views);

  // Click a holding space item. Then double click the same item with the
  // control key down. Expect the clicked holding space item to be opened.
  EXPECT_CALL(*client(), OpenItems(ElementsAre(item_views[0]->item()), _));
  Click(item_views[0]);
  DoubleClick(item_views[0], ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(client());

  // Reset.
  test_api()->Close();
  show_holding_space_and_cache_views(&item_views);

  // Click a holding space item. Then double click the same item with the
  // shift key down. Expect the clicked holding space item to be opened.
  EXPECT_CALL(*client(), OpenItems(ElementsAre(item_views[0]->item()), _));
  Click(item_views[0]);
  DoubleClick(item_views[0], ui::EF_SHIFT_DOWN);
  testing::Mock::VerifyAndClearExpectations(client());

  // Reset.
  test_api()->Close();
  show_holding_space_and_cache_views(&item_views);

  // Click a holding space item. Then double click a different item with the
  // control key down. Expect both holding space items to be opened.
  EXPECT_CALL(
      *client(),
      OpenItems(ElementsAre(item_views[0]->item(), item_views[1]->item()), _));
  Click(item_views[0]);
  DoubleClick(item_views[1], ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(client());

  // Reset.
  test_api()->Close();
  show_holding_space_and_cache_views(&item_views);

  // Click a holding space item. Then double click a different item with the
  // shift key down. Expect both holding space items to be opened.
  EXPECT_CALL(
      *client(),
      OpenItems(ElementsAre(item_views[0]->item(), item_views[1]->item()), _));
  Click(item_views[0]);
  DoubleClick(item_views[1], ui::EF_SHIFT_DOWN);
  testing::Mock::VerifyAndClearExpectations(client());
}

// Verifies that the holding space tray animates in and out as expected.
TEST_P(HoldingSpaceTrayTest, EnterAndExitAnimations) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Prior to session start, the tray should not be showing.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  views::View* const tray = test_api()->GetTray();
  ASSERT_TRUE(tray && tray->layer());

  // Record transforms performed to the `tray` layer.
  ScopedTransformRecordingLayerDelegate transform_recorder(tray->layer());

  // Start the session. Because a holding space item was added in a previous
  // session (according to prefs state), the tray should animate in.
  StartSession();
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // The entry animation should be the default entry animation in which the
  // tray translates in from the right without scaling.
  EXPECT_FALSE(transform_recorder.DidScale());
  EXPECT_TRUE(transform_recorder.TranslatedFrom({0.f, 0.f}, {0.f, 0.f}));
  EXPECT_TRUE(transform_recorder.TranslatedInRange({0.f, 0.f}, {44.f, 0.f}));
  transform_recorder.Reset();

  // Pin a holding space item. Because the tray was already showing there
  // should be no change in tray visibility.
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake1"));
  MarkTimeOfFirstPin();
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Because there was no change in visibility, there should be no transform.
  EXPECT_FALSE(transform_recorder.DidScale());
  EXPECT_FALSE(transform_recorder.DidTranslate());
  transform_recorder.Reset();

  // Remove all holding space items. Because a holding space item was
  // previously pinned, the tray should animate out.
  RemoveAllItems();
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // The exit animation should be the default exit animation in which the tray
  // scales down and pivots about its center point.
  EXPECT_TRUE(transform_recorder.ScaledFrom({1.f, 1.f}, {0.5f, 0.5f}));
  EXPECT_TRUE(transform_recorder.ScaledInRange({0.5f, 0.5f}, {1.f, 1.f}));
  EXPECT_TRUE(transform_recorder.TranslatedFrom({0.f, 0.f}, {11.f, 12.f}));
  EXPECT_TRUE(transform_recorder.TranslatedInRange({0.f, 0.f}, {11.f, 12.f}));
  transform_recorder.Reset();

  // Pin a holding space item. The tray should animate in.
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake2"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // The entry animation should be the bounce in animation in which the tray
  // translates in vertically with scaling (since it previously scaled out).
  EXPECT_TRUE(transform_recorder.ScaledFrom({0.5f, 0.5f}, {1.f, 1.f}));
  EXPECT_TRUE(transform_recorder.ScaledInRange({0.5f, 0.5f}, {1.f, 1.f}));
  EXPECT_TRUE(transform_recorder.TranslatedFrom({11.f, 12.f}, {0.f, 0.f}));
  EXPECT_TRUE(transform_recorder.TranslatedInRange({0.f, -16.f}, {11.f, 12.f}));
  transform_recorder.Reset();

  // Lock the screen. The tray should animate out.
  auto* session_controller =
      ash_test_helper()->test_session_controller_client();
  session_controller->LockScreen();
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // The exit animation should be the default exit animation in which the tray
  // scales down and pivots about its center point.
  EXPECT_TRUE(transform_recorder.ScaledFrom({1.0f, 1.0f}, {0.5f, 0.5f}));
  EXPECT_TRUE(transform_recorder.ScaledInRange({0.5f, 0.5f}, {1.f, 1.f}));
  EXPECT_TRUE(transform_recorder.TranslatedFrom({0.f, 0.f}, {11.f, 12.f}));
  EXPECT_TRUE(transform_recorder.TranslatedInRange({0.f, 0.f}, {11.f, 12.f}));
  transform_recorder.Reset();

  // Unlock the screen. The tray should animate in.
  session_controller->UnlockScreen();
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // The entry animation should be the default entry animation in which the
  // tray translates in from the right.
  EXPECT_TRUE(transform_recorder.ScaledFrom({0.5f, 0.5f}, {1.f, 1.f}));
  EXPECT_TRUE(transform_recorder.ScaledInRange({0.5f, 0.5f}, {1.f, 1.f}));
  EXPECT_TRUE(transform_recorder.TranslatedFrom({11.f, 12.f}, {0.f, 0.f}));
  EXPECT_TRUE(transform_recorder.TranslatedInRange({0.f, 0.f}, {44.f, 12.f}));
  transform_recorder.Reset();

  // Switch to another user with a populated model. The tray should animate in.
  constexpr char kSecondaryUserId[] = "user@secondary";
  HoldingSpaceModel secondary_holding_space_model;
  AddItemToModel(&secondary_holding_space_model,
                 HoldingSpaceItem::Type::kPinnedFile,
                 base::FilePath("/tmp/fake3"));
  SwitchToSecondaryUser(kSecondaryUserId, /*client=*/nullptr,
                        &secondary_holding_space_model);
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // The entry animation should be the default entry animation in which the
  // tray translates in from the right.
  EXPECT_TRUE(transform_recorder.ScaledFrom({1.f, 1.f}, {1.f, 1.f}));
  EXPECT_TRUE(transform_recorder.ScaledInRange({0.5f, 0.5f}, {1.f, 1.f}));
  EXPECT_TRUE(transform_recorder.TranslatedFrom({0.f, 0.f}, {0.f, 0.f}));
  EXPECT_TRUE(transform_recorder.TranslatedInRange({0.f, 0.f}, {44.f, 12.f}));
  transform_recorder.Reset();

  // Clean up.
  UnregisterModelForUser(kSecondaryUserId);
}

INSTANTIATE_TEST_SUITE_P(All, HoldingSpaceTrayTest, testing::Bool());

}  // namespace ash
