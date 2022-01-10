// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

// Manages a Browser instance created by BrowserWithTestWindowTest beyond the
// default instance it creates in SetUp.
class AdditionalBrowser {
 public:
  explicit AdditionalBrowser(std::unique_ptr<Browser> browser)
      : browser_(std::move(browser)),
        browser_view_(BrowserView::GetBrowserViewForBrowser(browser_.get())) {}

  ~AdditionalBrowser() {
    // Tear down |browser_|, similar to TestWithBrowserView::TearDown.
    browser_.release();
    browser_view_->GetWidget()->CloseNow();
  }

  ExtensionsToolbarContainer* extensions_container() {
    return browser_view_->toolbar()->extensions_container();
  }

 private:
  std::unique_ptr<Browser> browser_;
  raw_ptr<BrowserView> browser_view_;
};

}  // namespace

class ExtensionsMenuViewUnitTest : public ExtensionsToolbarUnitTest {
 public:
  ExtensionsMenuViewUnitTest()
      : allow_extension_menu_instances_(
            ExtensionsMenuView::AllowInstancesForTesting()) {}

  ExtensionsMenuViewUnitTest(const ExtensionsMenuViewUnitTest&) = delete;
  ExtensionsMenuViewUnitTest& operator=(const ExtensionsMenuViewUnitTest&) =
      delete;

  ~ExtensionsMenuViewUnitTest() override = default;

  // ExtensionsToolbarUnitTest:
  void SetUp() override;

  scoped_refptr<const extensions::Extension> InstallExtensionAndLayout(
      const std::string& name);

  ExtensionsMenuView* extensions_menu() {
    return ExtensionsMenuView::GetExtensionsMenuViewForTesting();
  }

  // Asserts there is exactly 1 menu item and then returns it.
  ExtensionsMenuItemView* GetOnlyMenuItem();

  void ClickPinButton(ExtensionsMenuItemView* menu_item) const;
  void ClickContextMenuButton(ExtensionsMenuItemView* menu_item) const;

  std::vector<ToolbarActionView*> GetPinnedExtensionViews();

  ToolbarActionView* GetPinnedExtensionView(const std::string& name);

  ExtensionsMenuItemView* GetExtensionsMenuItemView(const std::string& name);

  // Returns a list of the names of the currently pinned extensions, in order
  // from left to right.
  std::vector<std::string> GetPinnedExtensionNames();

  // Since this is a unittest (and doesn't have as much "real" rendering),
  // the ExtensionsMenuView sometimes needs a nudge to re-layout the views.
  void LayoutMenuIfNecessary();

 private:
  base::AutoReset<bool> allow_extension_menu_instances_;
};

void ExtensionsMenuViewUnitTest::SetUp() {
  ExtensionsToolbarUnitTest::SetUp();

  ExtensionsMenuView::ShowBubble(extensions_container()->GetExtensionsButton(),
                                 browser(), extensions_container(), true);
}

scoped_refptr<const extensions::Extension>
ExtensionsMenuViewUnitTest::InstallExtensionAndLayout(const std::string& name) {
  scoped_refptr<const extensions::Extension> extension = InstallExtension(name);
  LayoutMenuIfNecessary();
  return extension;
}

ExtensionsMenuItemView* ExtensionsMenuViewUnitTest::GetOnlyMenuItem() {
  base::flat_set<ExtensionsMenuItemView*> menu_items =
      extensions_menu()->extensions_menu_items_for_testing();
  if (menu_items.size() != 1u) {
    ADD_FAILURE() << "Not exactly one item; size is: " << menu_items.size();
    return nullptr;
  }
  return *menu_items.begin();
}

void ExtensionsMenuViewUnitTest::ClickPinButton(
    ExtensionsMenuItemView* menu_item) const {
  ClickButton(menu_item->pin_button_for_testing());
}

void ExtensionsMenuViewUnitTest::ClickContextMenuButton(
    ExtensionsMenuItemView* menu_item) const {
  ClickButton(menu_item->context_menu_button_for_testing());
}

std::vector<ToolbarActionView*>
ExtensionsMenuViewUnitTest::GetPinnedExtensionViews() {
  std::vector<ToolbarActionView*> result;
  for (views::View* child : extensions_container()->children()) {
    // Ensure we don't downcast the ExtensionsToolbarButton.
    if (views::IsViewClass<ToolbarActionView>(child)) {
      ToolbarActionView* const action = static_cast<ToolbarActionView*>(child);
#if defined(OS_MAC)
      // TODO(crbug.com/1045212): Use IsActionVisibleOnToolbar() because it
      // queries the underlying model and not GetVisible(), as that relies on an
      // animation running, which is not reliable in unit tests on Mac.
      const bool is_visible = extensions_container()->IsActionVisibleOnToolbar(
          action->view_controller());
#else
      const bool is_visible = action->GetVisible();
#endif
      if (is_visible)
        result.push_back(action);
    }
  }
  return result;
}

ToolbarActionView* ExtensionsMenuViewUnitTest::GetPinnedExtensionView(
    const std::string& name) {
  std::vector<ToolbarActionView*> actions = GetPinnedExtensionViews();
  auto it = std::find_if(
      actions.begin(), actions.end(), [name](ToolbarActionView* action) {
        return base::UTF16ToUTF8(action->view_controller()->GetActionName()) ==
               name;
      });
  if (it == actions.end())
    return nullptr;
  return *it;
}

ExtensionsMenuItemView* ExtensionsMenuViewUnitTest::GetExtensionsMenuItemView(
    const std::string& name) {
  base::flat_set<ExtensionsMenuItemView*> menu_items =
      extensions_menu()->extensions_menu_items_for_testing();
  auto iter =
      base::ranges::find_if(menu_items, [name](ExtensionsMenuItemView* item) {
        return base::UTF16ToUTF8(item->view_controller()->GetActionName()) ==
               name;
      });
  return iter == menu_items.end() ? nullptr : *iter;
}

std::vector<std::string> ExtensionsMenuViewUnitTest::GetPinnedExtensionNames() {
  std::vector<ToolbarActionView*> views = GetPinnedExtensionViews();
  std::vector<std::string> result;
  result.resize(views.size());
  std::transform(
      views.begin(), views.end(), result.begin(), [](ToolbarActionView* view) {
        return base::UTF16ToUTF8(view->view_controller()->GetActionName());
      });
  return result;
}

void ExtensionsMenuViewUnitTest::LayoutMenuIfNecessary() {
  extensions_menu()->GetWidget()->LayoutRootViewIfNecessary();
}

TEST_F(ExtensionsMenuViewUnitTest, ExtensionsAreShownInTheMenu) {
  // To start, there should be no extensions in the menu.
  EXPECT_EQ(0u, extensions_menu()->extensions_menu_items_for_testing().size());

  // Add an extension, and verify that it's added to the menu.
  constexpr char kExtensionName[] = "Test 1";
  InstallExtensionAndLayout(kExtensionName);

  {
    base::flat_set<ExtensionsMenuItemView*> menu_items =
        extensions_menu()->extensions_menu_items_for_testing();
    ASSERT_EQ(1u, menu_items.size());
    EXPECT_EQ(kExtensionName,
              base::UTF16ToUTF8((*menu_items.begin())
                                    ->primary_action_button_for_testing()
                                    ->label_text_for_testing()));
  }
}

TEST_F(ExtensionsMenuViewUnitTest, ExtensionsAreSortedInTheMenu) {
  constexpr char kExtensionZName[] = "Z Extension";
  InstallExtension(kExtensionZName);
  constexpr char kExtensionAName[] = "A Extension";
  InstallExtension(kExtensionAName);
  constexpr char kExtensionBName[] = "b Extension";
  InstallExtension(kExtensionBName);
  constexpr char kExtensionCName[] = "C Extension";
  InstallExtensionAndLayout(kExtensionCName);

  std::vector<ExtensionsMenuItemView*> menu_items =
      ExtensionsMenuView::GetSortedItemsForSectionForTesting(
          ToolbarActionViewController::PageInteractionStatus::kNone);
  ASSERT_EQ(4u, menu_items.size());

  std::vector<std::string> item_names;
  for (auto* menu_item : menu_items) {
    item_names.push_back(
        base::UTF16ToUTF8(menu_item->primary_action_button_for_testing()
                              ->label_text_for_testing()));
  }

  // Basic std::sort would do A,C,Z,b however we want A,b,C,Z
  EXPECT_THAT(item_names,
              testing::ElementsAre(kExtensionAName, kExtensionBName,
                                   kExtensionCName, kExtensionZName));
}

TEST_F(ExtensionsMenuViewUnitTest, PinnedExtensionAppearsInToolbar) {
  constexpr char kName[] = "Test Name";
  InstallExtensionAndLayout(kName);

  ExtensionsMenuItemView* menu_item = GetOnlyMenuItem();
  ASSERT_TRUE(menu_item);
  ToolbarActionViewController* controller = menu_item->view_controller();
  EXPECT_FALSE(extensions_container()->IsActionVisibleOnToolbar(controller));
  EXPECT_THAT(GetPinnedExtensionNames(), testing::IsEmpty());

  ClickPinButton(menu_item);
  WaitForAnimation();

  EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(controller));
  EXPECT_THAT(GetPinnedExtensionNames(), testing::ElementsAre(kName));

  ClickPinButton(menu_item);  // Unpin.
  WaitForAnimation();

  EXPECT_FALSE(extensions_container()->IsActionVisibleOnToolbar(
      menu_item->view_controller()));
  EXPECT_THAT(GetPinnedExtensionNames(), testing::IsEmpty());
}

TEST_F(ExtensionsMenuViewUnitTest, PinnedExtensionAppearsInAnotherWindow) {
  InstallExtensionAndLayout("Test Name");

  AdditionalBrowser browser2(
      CreateBrowser(browser()->profile(), browser()->type(),
                    /* hosted_app */ false, /* browser_window */ nullptr));

  ExtensionsMenuItemView* menu_item = GetOnlyMenuItem();
  ASSERT_TRUE(menu_item);
  ClickPinButton(menu_item);

  // Window that was already open gets the pinned extension.
  EXPECT_TRUE(browser2.extensions_container()->IsActionVisibleOnToolbar(
      menu_item->view_controller()));

  AdditionalBrowser browser3(
      CreateBrowser(browser()->profile(), browser()->type(),
                    /* hosted_app */ false, /* browser_window */ nullptr));

  // Brand-new window also gets the pinned extension.
  EXPECT_TRUE(browser3.extensions_container()->IsActionVisibleOnToolbar(
      menu_item->view_controller()));
}

TEST_F(ExtensionsMenuViewUnitTest, PinnedExtensionRemovedWhenDisabled) {
  constexpr char kName[] = "Test Name";
  const extensions::ExtensionId id = InstallExtensionAndLayout(kName)->id();

  {
    ExtensionsMenuItemView* menu_item = GetOnlyMenuItem();
    ASSERT_TRUE(menu_item);
    ClickPinButton(menu_item);
  }

  DisableExtension(id);
  WaitForAnimation();

  ASSERT_EQ(0u, extensions_menu()->extensions_menu_items_for_testing().size());
  EXPECT_THAT(GetPinnedExtensionNames(), testing::IsEmpty());

  EnableExtension(id);
  WaitForAnimation();

  ASSERT_EQ(1u, extensions_menu()->extensions_menu_items_for_testing().size());
  EXPECT_THAT(GetPinnedExtensionNames(), testing::ElementsAre(kName));
}

TEST_F(ExtensionsMenuViewUnitTest, ReorderPinnedExtensions) {
  constexpr char kName1[] = "Test 1";
  InstallExtension(kName1);
  constexpr char kName2[] = "Test 2";
  InstallExtension(kName2);
  constexpr char kName3[] = "Test 3";
  InstallExtensionAndLayout(kName3);

  EXPECT_EQ(3u, extensions_menu()->extensions_menu_items_for_testing().size());

  for (const char* name : {kName1, kName2, kName3}) {
    ExtensionsMenuItemView* item = GetExtensionsMenuItemView(name);
    ASSERT_TRUE(item) << name;
    ClickPinButton(item);
    EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(
        item->view_controller()));
  }

  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kName1, kName2, kName3));

  // Simulate dragging "Test 3" to the first slot.
  ToolbarActionView* drag_view = GetPinnedExtensionView(kName3);
  ui::OSExchangeData drag_data;
  extensions_container()->WriteDragDataForView(drag_view, gfx::Point(),
                                               &drag_data);
  gfx::PointF drop_point(GetPinnedExtensionView(kName1)->origin());
  ui::DropTargetEvent drop_event(drag_data, drop_point, drop_point,
                                 ui::DragDropTypes::DRAG_MOVE);
  extensions_container()->OnDragUpdated(drop_event);
  extensions_container()->OnPerformDrop(drop_event);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kName3, kName1, kName2));
}

TEST_F(ExtensionsMenuViewUnitTest, RunDropCallback) {
  constexpr char kName1[] = "Test 1";
  auto ext1 = InstallExtension(kName1);
  constexpr char kName2[] = "Test 2";
  auto ext2 = InstallExtension(kName2);
  constexpr char kName3[] = "Test 3";
  auto ext3 = InstallExtensionAndLayout(kName3);

  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);

  toolbar_model->SetActionVisibility(ext1->id(), true);
  toolbar_model->SetActionVisibility(ext2->id(), true);
  toolbar_model->SetActionVisibility(ext3->id(), true);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kName1, kName2, kName3));

  // Simulate dragging "Test 3" to the first slot.
  ToolbarActionView* drag_view = GetPinnedExtensionView(kName3);
  ui::OSExchangeData drag_data;
  extensions_container()->WriteDragDataForView(drag_view, gfx::Point(),
                                               &drag_data);
  gfx::PointF drop_point(GetPinnedExtensionView(kName1)->origin());
  ui::DropTargetEvent drop_event(drag_data, drop_point, drop_point,
                                 ui::DragDropTypes::DRAG_MOVE);
  extensions_container()->OnDragUpdated(drop_event);
  auto cb = extensions_container()->GetDropCallback(drop_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(cb).Run(drop_event, output_drag_op);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kName3, kName1, kName2));
  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kMove);
}

TEST_F(ExtensionsMenuViewUnitTest, ResetDropCallback) {
  constexpr char kName1[] = "Test 1";
  auto ext1 = InstallExtension(kName1);
  constexpr char kName2[] = "Test 2";
  auto ext2 = InstallExtension(kName2);
  constexpr char kName3[] = "Test 3";
  auto ext3 = InstallExtensionAndLayout(kName3);

  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);

  toolbar_model->SetActionVisibility(ext1->id(), true);
  toolbar_model->SetActionVisibility(ext2->id(), true);
  toolbar_model->SetActionVisibility(ext3->id(), true);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kName1, kName2, kName3));

  // Simulate dragging "Test 3" to the first slot.
  ToolbarActionView* drag_view = GetPinnedExtensionView(kName3);
  ui::OSExchangeData drag_data;
  extensions_container()->WriteDragDataForView(drag_view, gfx::Point(),
                                               &drag_data);
  gfx::PointF drop_point(GetPinnedExtensionView(kName1)->origin());
  ui::DropTargetEvent drop_event(drag_data, drop_point, drop_point,
                                 ui::DragDropTypes::DRAG_MOVE);
  extensions_container()->OnDragUpdated(drop_event);
  auto cb = extensions_container()->GetDropCallback(drop_event);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kName3, kName1, kName2));

  // If the drop callback is reset (and never invoked), the drag should be
  // aborted, and items should be back in their original order.
  cb.Reset();
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kName1, kName2, kName3));
}

TEST_F(ExtensionsMenuViewUnitTest, InvalidateDropCallbackOnActionAdded) {
  constexpr char kName1[] = "Test 1";
  auto ext1 = InstallExtension(kName1);
  constexpr char kName2[] = "Test 2";
  auto ext2 = InstallExtensionAndLayout(kName2);

  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);

  toolbar_model->SetActionVisibility(ext1->id(), true);
  toolbar_model->SetActionVisibility(ext2->id(), true);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(), testing::ElementsAre(kName1, kName2));

  // Simulate dragging "Test 2" to the first slot.
  ToolbarActionView* drag_view = GetPinnedExtensionView(kName2);
  ui::OSExchangeData drag_data;
  extensions_container()->WriteDragDataForView(drag_view, gfx::Point(),
                                               &drag_data);
  gfx::PointF drop_point(GetPinnedExtensionView(kName1)->origin());
  ui::DropTargetEvent drop_event(drag_data, drop_point, drop_point,
                                 ui::DragDropTypes::DRAG_MOVE);
  extensions_container()->OnDragUpdated(drop_event);
  auto cb = extensions_container()->GetDropCallback(drop_event);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(), testing::ElementsAre(kName2, kName1));

  constexpr char kName3[] = "Test 3";
  auto ext3 = InstallExtensionAndLayout(kName3);
  toolbar_model->SetActionVisibility(ext3->id(), true);
  WaitForAnimation();

  // The drop callback should be invalidated, and items should be back in their
  // original order.
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(cb).Run(drop_event, output_drag_op);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kName1, kName2, kName3));
}

// ToolbarActionsModel::MovePinnedAction crashes if pinned extensions changes
// while the drop callback isn't invalidated. This test makes sure this doesn't
// happen anymore. https://crbug.com/1268239.
TEST_F(ExtensionsMenuViewUnitTest, InvalidateDropCallbackOnPrefChange) {
  constexpr char kName1[] = "Test 1";
  auto ext1 = InstallExtension(kName1);
  constexpr char kName2[] = "Test 2";
  auto ext2 = InstallExtensionAndLayout(kName2);

  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);

  toolbar_model->SetActionVisibility(ext1->id(), true);
  toolbar_model->SetActionVisibility(ext2->id(), true);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(), testing::ElementsAre(kName1, kName2));

  // Simulate dragging "Test 2" to the first slot.
  ToolbarActionView* drag_view = GetPinnedExtensionView(kName2);
  ui::OSExchangeData drag_data;
  extensions_container()->WriteDragDataForView(drag_view, gfx::Point(),
                                               &drag_data);
  gfx::PointF drop_point(GetPinnedExtensionView(kName1)->origin());
  ui::DropTargetEvent drop_event(drag_data, drop_point, drop_point,
                                 ui::DragDropTypes::DRAG_MOVE);
  extensions_container()->OnDragUpdated(drop_event);
  auto cb = extensions_container()->GetDropCallback(drop_event);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(), testing::ElementsAre(kName2, kName1));

  extensions::ExtensionPrefs::Get(profile())->SetPinnedExtensions({});
  WaitForAnimation();

  // The drop callback should be invalidated, and items should be back in their
  // original order.
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(cb).Run(drop_event, output_drag_op);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(), testing::ElementsAre());
}

TEST_F(ExtensionsMenuViewUnitTest, PinnedExtensionsReorderOnPrefChange) {
  constexpr char kName1[] = "Test 1";
  const extensions::ExtensionId id1 = InstallExtension(kName1)->id();
  constexpr char kName2[] = "Test 2";
  const extensions::ExtensionId id2 = InstallExtension(kName2)->id();
  constexpr char kName3[] = "Test 3";
  const extensions::ExtensionId id3 = InstallExtensionAndLayout(kName3)->id();

  for (const char* name : {kName1, kName2, kName3}) {
    ExtensionsMenuItemView* item = GetExtensionsMenuItemView(name);
    ASSERT_TRUE(item) << name;
    ClickPinButton(item);
  }
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kName1, kName2, kName3));

  extensions::ExtensionPrefs::Get(profile())->SetPinnedExtensions(
      {id2, id3, id1});
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kName2, kName3, kName1));
}

TEST_F(ExtensionsMenuViewUnitTest, PinnedExtensionLayout) {
  for (int i = 0; i < 3; i++)
    InstallExtensionAndLayout(base::StringPrintf("Test %d", i));
  for (auto* menu_item :
       extensions_menu()->extensions_menu_items_for_testing()) {
    ClickPinButton(menu_item);
  }
  WaitForAnimation();

  std::vector<ToolbarActionView*> action_views = GetPinnedExtensionViews();
  ASSERT_EQ(3u, action_views.size());
  ExtensionsToolbarButton* menu_button =
      extensions_container()->GetExtensionsButton();

  // All views should be lined up horizontally with the menu button.
  EXPECT_EQ(action_views[0]->y(), action_views[1]->y());
  EXPECT_EQ(action_views[1]->y(), action_views[2]->y());
  EXPECT_EQ(action_views[2]->y(), menu_button->y());

  // Views are ordered left-to-right (in LTR mode).
  EXPECT_LE(action_views[0]->x() + action_views[0]->width(),
            action_views[1]->x());
  EXPECT_LE(action_views[1]->x() + action_views[1]->width(),
            action_views[2]->x());
  EXPECT_LE(action_views[2]->x() + action_views[2]->width(), menu_button->x());
}

// Tests that when an extension is reloaded it remains visible in the toolbar
// and extensions menu.
TEST_F(ExtensionsMenuViewUnitTest, ReloadExtension) {
  // The extension must have a manifest to be reloaded.
  extensions::TestExtensionDir extension_directory;
  constexpr char kManifest[] = R"({
        "name": "Test",
        "version": "1",
        "manifest_version": 2
      })";
  extension_directory.WriteManifest(kManifest);
  extensions::ChromeTestExtensionLoader loader(profile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_directory.UnpackedPath());
  // Force the menu to re-layout, since a new item was added.
  LayoutMenuIfNecessary();
  ASSERT_EQ(1u, extensions_menu()->extensions_menu_items_for_testing().size());

  {
    ExtensionsMenuItemView* menu_item = GetOnlyMenuItem();
    ClickPinButton(menu_item);
    EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(
        menu_item->view_controller()));
    // |menu_item| will not be valid after the extension reloads.
  }

  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile()));
  ReloadExtension(extension->id());
  ASSERT_TRUE(registry_observer.WaitForExtensionLoaded());
  LayoutMenuIfNecessary();

  ASSERT_EQ(1u, extensions_menu()->extensions_menu_items_for_testing().size());
  EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(
      GetOnlyMenuItem()->view_controller()));
}

// Tests that a when an extension is reloaded with manifest errors, and
// therefore fails to be loaded into Chrome, it's removed from the toolbar and
// extensions menu.
TEST_F(ExtensionsMenuViewUnitTest, ReloadExtensionFailed) {
  extensions::TestExtensionDir extension_directory;
  constexpr char kManifest[] = R"({
        "name": "Test",
        "version": "1",
        "manifest_version": 2
      })";
  extension_directory.WriteManifest(kManifest);
  extensions::ChromeTestExtensionLoader loader(profile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_directory.UnpackedPath());
  LayoutMenuIfNecessary();
  ExtensionsMenuItemView* menu_item = GetOnlyMenuItem();
  ASSERT_TRUE(menu_item);
  ClickPinButton(menu_item);

  // Replace the extension's valid manifest with one containing errors. In this
  // case, the error is that both the 'browser_action' and 'page_action' keys
  // are specified instead of only one.
  constexpr char kManifestWithErrors[] = R"({
        "name": "Test",
        "version": "1",
        "manifest_version": 2,
        "page_action" : {},
        "browser_action" : {}
      })";
  extension_directory.WriteManifest(kManifestWithErrors);

  // Reload the extension. It should fail due to the manifest errors.
  extension_service()->ReloadExtensionWithQuietFailure(extension->id());
  base::RunLoop().RunUntilIdle();

  // Since the extension is removed it's no longer visible on the toolbar or in
  // the menu.
  for (views::View* child : extensions_container()->children())
    EXPECT_FALSE(views::IsViewClass<ToolbarActionView>(child));
  EXPECT_EQ(0u, extensions_menu()->extensions_menu_items_for_testing().size());
}

TEST_F(ExtensionsMenuViewUnitTest, PinButtonUserActionWithAccessibility) {
  base::UserActionTester user_action_tester;
  InstallExtensionAndLayout("Test Extension");
  ExtensionsMenuItemView* menu_item = GetOnlyMenuItem();
  ASSERT_NE(nullptr, menu_item);
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  constexpr char kPinButtonUserAction[] = "Extensions.Toolbar.PinButtonPressed";

  // Verify behavior before pin, after pin, and after unpin.
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(i, user_action_tester.GetActionCount(kPinButtonUserAction));
#if defined(OS_MAC)
    // TODO(crbug.com/1045212): No Mac animations in unit tests cause errors.
#else
    EXPECT_EQ(i, counter.GetCount(ax::mojom::Event::kAlert));
    EXPECT_EQ(i, counter.GetCount(ax::mojom::Event::kTextChanged));
#endif
    ClickPinButton(menu_item);
  }
}

TEST_F(ExtensionsMenuViewUnitTest, WindowTitle) {
  InstallExtensionAndLayout("Test Extension");

  ExtensionsMenuView* const menu_view = extensions_menu();
  EXPECT_FALSE(menu_view->GetWindowTitle().empty());
  EXPECT_TRUE(menu_view->GetAccessibleWindowTitle().empty());
}

// TODO(crbug.com/984654): When supported, add a test to verify the
// ExtensionsToolbarContainer shrinks when the window is too small to show all
// pinned extensions.
// TODO(crbug.com/984654): When supported, add a test to verify an extension
// is shown when a bubble pops up and needs to draw attention to it.
