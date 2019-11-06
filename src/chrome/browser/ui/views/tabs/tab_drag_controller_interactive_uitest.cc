// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_drag_controller_interactive_uitest.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/window_finder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/env.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window_targeter.h"
#endif

#if defined(USE_AURA) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_state.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

using content::WebContents;
using display::Display;
using ui_test_utils::GetDisplays;

namespace test {

namespace {

const char kTabDragControllerInteractiveUITestUserDataKey[] =
    "TabDragControllerInteractiveUITestUserData";

class TabDragControllerInteractiveUITestUserData
    : public base::SupportsUserData::Data {
 public:
  explicit TabDragControllerInteractiveUITestUserData(int id) : id_(id) {}
  ~TabDragControllerInteractiveUITestUserData() override {}
  int id() { return id_; }

 private:
  int id_;
};

#if defined(OS_CHROMEOS)
aura::Window* GetWindowForTabStrip(TabStrip* tab_strip) {
  return tab_strip ? tab_strip->GetWidget()->GetNativeWindow() : nullptr;
}
#endif

}  // namespace

class QuitDraggingObserver : public content::NotificationObserver {
 public:
  QuitDraggingObserver() {
    registrar_.Add(this, chrome::NOTIFICATION_TAB_DRAG_LOOP_DONE,
                   content::NotificationService::AllSources());
  }
  ~QuitDraggingObserver() override {}

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_EQ(chrome::NOTIFICATION_TAB_DRAG_LOOP_DONE, type);
    run_loop_.QuitWhenIdle();
  }

  // The observer should be constructed prior to initiating the drag. To prevent
  // misuse via constructing a temporary object, Wait is marked lvalue-only.
  void Wait() & { run_loop_.Run(); }

 private:
  content::NotificationRegistrar registrar_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(QuitDraggingObserver);
};

void SetID(WebContents* web_contents, int id) {
  web_contents->SetUserData(
      &kTabDragControllerInteractiveUITestUserDataKey,
      std::make_unique<TabDragControllerInteractiveUITestUserData>(id));
}

void ResetIDs(TabStripModel* model, int start) {
  for (int i = 0; i < model->count(); ++i)
    SetID(model->GetWebContentsAt(i), start + i);
}

std::string IDString(TabStripModel* model) {
  std::string result;
  for (int i = 0; i < model->count(); ++i) {
    if (i != 0)
      result += " ";
    WebContents* contents = model->GetWebContentsAt(i);
    TabDragControllerInteractiveUITestUserData* user_data =
        static_cast<TabDragControllerInteractiveUITestUserData*>(
            contents->GetUserData(
                &kTabDragControllerInteractiveUITestUserDataKey));
    if (user_data)
      result += base::NumberToString(user_data->id());
    else
      result += "?";
  }
  return result;
}

TabStrip* GetTabStripForBrowser(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  return browser_view->tabstrip();
}

TabDragController* GetTabDragController(TabStrip* tab_strip) {
  return tab_strip->GetDragContext()->GetDragController();
}

}  // namespace test

using test::GetTabDragController;
using test::GetTabStripForBrowser;
using test::IDString;
using test::ResetIDs;
using test::SetID;
using ui_test_utils::GetCenterInScreenCoordinates;

TabDragControllerTest::TabDragControllerTest()
    : browser_list(BrowserList::GetInstance()) {}

TabDragControllerTest::~TabDragControllerTest() {
}

void TabDragControllerTest::StopAnimating(TabStrip* tab_strip) {
  tab_strip->StopAnimating(true);
}

void TabDragControllerTest::AddTabsAndResetBrowser(Browser* browser,
                                                   int additional_tabs) {
  for (int i = 0; i < additional_tabs; i++)
    AddBlankTabAndShow(browser);
  StopAnimating(GetTabStripForBrowser(browser));
  ResetIDs(browser->tab_strip_model(), 0);
}

Browser* TabDragControllerTest::CreateAnotherBrowserAndResize() {
  // Create another browser.
  Browser* browser2 = CreateBrowser(browser()->profile());
  ResetIDs(browser2->tab_strip_model(), 100);

  // Resize the two windows so they're right next to each other.
  const gfx::NativeWindow window = browser()->window()->GetNativeWindow();
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area();
  const gfx::Size size(work_area.width() / 3, work_area.height() / 2);
  gfx::Rect browser_rect(work_area.origin(), size);
  browser()->window()->SetBounds(browser_rect);
  browser_rect.set_x(browser_rect.right());
  browser2->window()->SetBounds(browser_rect);
  return browser2;
}

void TabDragControllerTest::SetWindowFinderForTabStrip(
    TabStrip* tab_strip,
    std::unique_ptr<WindowFinder> window_finder) {
  ASSERT_TRUE(GetTabDragController(tab_strip));
  GetTabDragController(tab_strip)->window_finder_ = std::move(window_finder);
}

void TabDragControllerTest::HandleGestureEvent(TabStrip* tab_strip,
                                               ui::GestureEvent* event) {
  tab_strip->OnGestureEvent(event);
}

bool TabDragControllerTest::HasDragStarted(TabStrip* tab_strip) const {
  return GetTabDragController(tab_strip) &&
         GetTabDragController(tab_strip)->started_drag();
}

void TabDragControllerTest::SetUp() {
#if defined(USE_AURA)
  // This needs to be disabled as it can interfere with when events are
  // processed. In particular if input throttling is turned on, then when an
  // event ack runs the event may not have been processed.
  aura::Env::set_initial_throttle_input_on_resize_for_testing(false);
#endif
  InProcessBrowserTest::SetUp();
}

namespace {

enum InputSource {
  INPUT_SOURCE_MOUSE = 0,
  INPUT_SOURCE_TOUCH = 1
};

int GetDetachY(TabStrip* tab_strip) {
  return std::max(TabDragController::kTouchVerticalDetachMagnetism,
                  TabDragController::kVerticalDetachMagnetism) +
      tab_strip->height() + 1;
}

bool GetIsDragged(Browser* browser) {
#if defined(OS_CHROMEOS)
  return ash::WindowState::Get(browser->window()->GetNativeWindow())
      ->is_dragged();
#endif
  return false;
}

}  // namespace

#if !defined(OS_CHROMEOS) && defined(USE_AURA)

// Following classes verify a crash scenario. Specifically on Windows when focus
// changes it can trigger capture being lost. This was causing a crash in tab
// dragging as it wasn't set up to handle this scenario. These classes
// synthesize this scenario.

// Allows making ClearNativeFocus() invoke ReleaseCapture().
class TestDesktopBrowserFrameAura : public DesktopBrowserFrameAura {
 public:
  TestDesktopBrowserFrameAura(
      BrowserFrame* browser_frame,
      BrowserView* browser_view)
      : DesktopBrowserFrameAura(browser_frame, browser_view),
        release_capture_(false) {}
  ~TestDesktopBrowserFrameAura() override {}

  void ReleaseCaptureOnNextClear() {
    release_capture_ = true;
  }

  void ClearNativeFocus() override {
    views::DesktopNativeWidgetAura::ClearNativeFocus();
    if (release_capture_) {
      release_capture_ = false;
      GetWidget()->ReleaseCapture();
    }
  }

 private:
  // If true ReleaseCapture() is invoked in ClearNativeFocus().
  bool release_capture_;

  DISALLOW_COPY_AND_ASSIGN(TestDesktopBrowserFrameAura);
};

// Factory for creating a TestDesktopBrowserFrameAura.
class TestNativeBrowserFrameFactory : public NativeBrowserFrameFactory {
 public:
  TestNativeBrowserFrameFactory() {}
  ~TestNativeBrowserFrameFactory() override {}

  NativeBrowserFrame* Create(BrowserFrame* browser_frame,
                             BrowserView* browser_view) override {
    return new TestDesktopBrowserFrameAura(browser_frame, browser_view);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNativeBrowserFrameFactory);
};

class TabDragCaptureLostTest : public TabDragControllerTest {
 public:
  TabDragCaptureLostTest() {
    NativeBrowserFrameFactory::Set(new TestNativeBrowserFrameFactory);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabDragCaptureLostTest);
};

// See description above for details.
IN_PROC_BROWSER_TEST_F(TabDragCaptureLostTest, ReleaseCaptureOnDrag) {
  AddTabsAndResetBrowser(browser(), 1);

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  gfx::Point tab_1_center(GetCenterInScreenCoordinates(tab_strip->tab_at(1)));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(tab_1_center) &&
              ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::DOWN));
  TestDesktopBrowserFrameAura* frame =
      static_cast<TestDesktopBrowserFrameAura*>(
          BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->
          native_widget_private());
  // Invoke ReleaseCaptureOnDrag() so that when the drag happens and focus
  // changes capture is released and the drag cancels.
  frame->ReleaseCaptureOnNextClear();
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  EXPECT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
}

IN_PROC_BROWSER_TEST_F(TabDragControllerTest, GestureEndShouldEndDragTest) {
  AddTabsAndResetBrowser(browser(), 1);

  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  Tab* tab1 = tab_strip->tab_at(1);
  gfx::Point tab_1_center(tab1->width() / 2, tab1->height() / 2);

  ui::GestureEvent gesture_tap_down(
      tab_1_center.x(), tab_1_center.x(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));
  tab_strip->MaybeStartDrag(tab1, gesture_tap_down,
    tab_strip->GetSelectionModel());
  EXPECT_TRUE(TabDragController::IsActive());

  ui::GestureEvent gesture_end(tab_1_center.x(), tab_1_center.x(), 0,
                               base::TimeTicks(),
                               ui::GestureEventDetails(ui::ET_GESTURE_END));
  HandleGestureEvent(tab_strip, &gesture_end);
  EXPECT_FALSE(TabDragController::IsActive());
  EXPECT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
}

#endif

class DetachToBrowserTabDragControllerTest
    : public TabDragControllerTest,
      public ::testing::WithParamInterface<const char*> {
 public:
  DetachToBrowserTabDragControllerTest() {}

  void SetUpOnMainThread() override {
#if defined(OS_CHROMEOS)
    root_ = browser()->window()->GetNativeWindow()->GetRootWindow();
    // Disable flings which might otherwise inadvertently be generated from
    // tests' touch events.
    ui::GestureConfiguration::GetInstance()->set_min_fling_velocity(
        std::numeric_limits<float>::max());
#endif
#if defined(OS_MACOSX)
    // Currently MacViews' browser windows are shown in the background and could
    // be obscured by other windows if there are any. This should be fixed in
    // order to be consistent with other platforms.
    EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
#endif  // OS_MACOSX
  }

  InputSource input_source() const {
    return strstr(GetParam(), "mouse") ?
        INPUT_SOURCE_MOUSE : INPUT_SOURCE_TOUCH;
  }

#if defined(OS_CHROMEOS)
  bool SendTouchEventsSync(int action, int id, const gfx::Point& location) {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    if (!ui_controls::SendTouchEventsNotifyWhenDone(
            action, id, location.x(), location.y(), run_loop.QuitClosure())) {
      return false;
    }
    run_loop.Run();
    return true;
  }
#endif

  // The following methods update one of the mouse or touch input depending upon
  // the InputSource.
  bool PressInput(const gfx::Point& location, int id = 0) {
    if (input_source() == INPUT_SOURCE_MOUSE) {
      return ui_test_utils::SendMouseMoveSync(location) &&
          ui_test_utils::SendMouseEventsSync(
              ui_controls::LEFT, ui_controls::DOWN);
    }
#if defined(OS_CHROMEOS)
    return SendTouchEventsSync(ui_controls::PRESS, id, location);
#else
    NOTREACHED();
    return false;
#endif
  }

  bool DragInputTo(const gfx::Point& location) {
    if (input_source() == INPUT_SOURCE_MOUSE)
      return ui_test_utils::SendMouseMoveSync(location);
#if defined(OS_CHROMEOS)
    return SendTouchEventsSync(ui_controls::MOVE, 0, location);
#else
    NOTREACHED();
    return false;
#endif
  }

  bool DragInputToAsync(const gfx::Point& location) {
    if (input_source() == INPUT_SOURCE_MOUSE)
      return ui_controls::SendMouseMove(location.x(), location.y());
#if defined(OS_CHROMEOS)
    return ui_controls::SendTouchEvents(ui_controls::MOVE, 0, location.x(),
                                        location.y());
#else
    NOTREACHED();
    return false;
#endif
  }

  bool DragInputToNotifyWhenDone(const gfx::Point& location,
                                 base::OnceClosure task) {
    if (input_source() == INPUT_SOURCE_MOUSE) {
      return ui_controls::SendMouseMoveNotifyWhenDone(
          location.x(), location.y(), std::move(task));
    }

#if defined(OS_CHROMEOS)
    return ui_controls::SendTouchEventsNotifyWhenDone(
        ui_controls::MOVE, 0, location.x(), location.y(), std::move(task));
#else
    NOTREACHED();
    return false;
#endif
  }

  bool ReleaseInput(int id = 0, bool async = false) {
    if (input_source() == INPUT_SOURCE_MOUSE) {
      return async ? ui_controls::SendMouseEvents(ui_controls::LEFT,
                                                  ui_controls::UP)
                   : ui_test_utils::SendMouseEventsSync(ui_controls::LEFT,
                                                        ui_controls::UP);
    }
#if defined(OS_CHROMEOS)
    return async ? ui_controls::SendTouchEvents(ui_controls::RELEASE, id, 0, 0)
                 : SendTouchEventsSync(ui_controls::RELEASE, id, gfx::Point());
#else
    NOTREACHED();
    return false;
#endif
  }

  void ReleaseInputAfterWindowDetached() {
    // On macOS, we want to avoid generating the input event [which requires an
    // associated window] until the window has been detached. Failure to do so
    // causes odd behavior [e.g. on macOS 10.10, the mouse-up will reactivate
    // the first window].
    if (browser_list->size() != 2u) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&DetachToBrowserTabDragControllerTest::
                             ReleaseInputAfterWindowDetached,
                         base::Unretained(this)),
          base::TimeDelta::FromMilliseconds(1));
      return;
    }

    // Windows hangs if you use a sync mouse event here.
    ASSERT_TRUE(ReleaseInput(0, true));
  }

  bool MoveInputTo(const gfx::Point& location) {
    aura::Env::GetInstance()->SetLastMouseLocation(location);
    if (input_source() == INPUT_SOURCE_MOUSE)
      return ui_test_utils::SendMouseMoveSync(location);
#if defined(OS_CHROMEOS)
    return SendTouchEventsSync(ui_controls::MOVE, 0, location);
#else
    NOTREACHED();
    return false;
#endif
  }

  void AddBlankTabAndShow(Browser* browser) {
    InProcessBrowserTest::AddBlankTabAndShow(browser);
  }

  // Returns true if the tab dragging info is correctly set on the attached
  // browser window. On Chrome OS, the info includes two window properties.
  bool IsTabDraggingInfoSet(TabStrip* attached_tabstrip,
                            TabStrip* source_tabstrip) {
    DCHECK(attached_tabstrip);
#if defined(OS_CHROMEOS)
    aura::Window* dragged_window =
        test::GetWindowForTabStrip(attached_tabstrip);
    attached_tabstrip->GetWidget()->GetNativeWindow();
    aura::Window* source_window =
        (source_tabstrip && source_tabstrip != attached_tabstrip)
            ? test::GetWindowForTabStrip(source_tabstrip)
            : nullptr;
    return dragged_window->GetProperty(ash::kIsDraggingTabsKey) &&
           dragged_window->GetProperty(ash::kTabDraggingSourceWindowKey) ==
               source_window;
#else
    return true;
#endif
  }

  // Returns true if the tab dragging info is correctly cleared on the attached
  // browser window.
  bool IsTabDraggingInfoCleared(TabStrip* attached_tabstrip) {
    DCHECK(attached_tabstrip);
#if defined(OS_CHROMEOS)
    aura::Window* dragged_window =
        test::GetWindowForTabStrip(attached_tabstrip);
    return !dragged_window->GetProperty(ash::kIsDraggingTabsKey) &&
           !dragged_window->GetProperty(ash::kTabDraggingSourceWindowKey);
#else
    return true;
#endif
  }

  void DragTabAndNotify(TabStrip* tab_strip,
                        base::OnceClosure task,
                        int tab = 0,
                        int drag_x_offset = 0) {
    test::QuitDraggingObserver observer;
    // Move to the tab and drag it enough so that it detaches.
    const gfx::Point tab_0_center =
        GetCenterInScreenCoordinates(tab_strip->tab_at(tab));
    ASSERT_TRUE(PressInput(tab_0_center));
    ASSERT_TRUE(DragInputToNotifyWhenDone(
        tab_0_center + gfx::Vector2d(drag_x_offset, GetDetachY(tab_strip)),
        std::move(task)));
    observer.Wait();
  }

  Browser* browser() const { return InProcessBrowserTest::browser(); }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
#if defined(OS_CHROMEOS)
  // The root window for the event generator.
  aura::Window* root_ = nullptr;
#endif

  DISALLOW_COPY_AND_ASSIGN(DetachToBrowserTabDragControllerTest);
};

// Creates a browser with two tabs, drags the second to the first.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest, DragInSameWindow) {
  AddTabsAndResetBrowser(browser(), 1);

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  // Test that the dragging info is correctly set on |tab_strip|.
  EXPECT_TRUE(IsTabDraggingInfoSet(tab_strip, tab_strip));
  ASSERT_TRUE(ReleaseInput());
  EXPECT_EQ("1 0", IDString(model));
  EXPECT_FALSE(TabDragController::IsActive());
  EXPECT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  // Test that the dragging info is properly cleared after dragging.
  EXPECT_TRUE(IsTabDraggingInfoCleared(tab_strip));

  // The tab strip should no longer have capture because the drag was ended and
  // mouse/touch was released.
  EXPECT_FALSE(tab_strip->GetWidget()->HasCapture());
}

// Creates a browser with four tabs. The first three belong in the same Tab
// Group. Dragging the third tab to after the fourth tab will result in a
// removal of the dragged tab from its group. Then dragging the second tab to
// after the third tab will also result in a removal of that dragged tab from
// its group.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragRightToUngroupTab) {
  scoped_feature_list_.InitAndEnableFeature(features::kTabGroups);

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  AddTabsAndResetBrowser(browser(), 3);
  TabGroupId group = model->AddToNewGroup({0, 1, 2});
  StopAnimating(tab_strip);

  EXPECT_EQ(4, model->count());
  EXPECT_EQ(3u, model->ListTabsInGroup(group).size());

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(3))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Dragging the tab in the second index to the tab in the third index switches
  // the tabs and removes the dragged tab from the group.
  EXPECT_EQ("0 1 3 2", IDString(model));
  EXPECT_THAT(model->ListTabsInGroup(group), testing::ElementsAre(0, 1));
  EXPECT_EQ(base::nullopt, model->GetTabGroupForTab(2));
  EXPECT_EQ(base::nullopt, model->GetTabGroupForTab(3));

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Dragging the tab in the first index to the tab in the second index (tab 3)
  // switches the tabs and removes the dragged tab from the group.
  EXPECT_EQ("0 3 1 2", IDString(model));
  EXPECT_THAT(model->ListTabsInGroup(group), testing::ElementsAre(0));
  EXPECT_EQ(base::nullopt, model->GetTabGroupForTab(1));
}

// Creates a browser with four tabs. The last three belong in the same Tab
// Group. Dragging the second tab to before the first tab will result in a
// removal of the dragged tab from its group. Then dragging the third tab to
// before the second tab will also result in a removal of that dragged tab from
// its group.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragLeftToUngroupTab) {
  scoped_feature_list_.InitAndEnableFeature(features::kTabGroups);

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  AddTabsAndResetBrowser(browser(), 3);
  TabGroupId group = model->AddToNewGroup({1, 2, 3});
  StopAnimating(tab_strip);

  EXPECT_EQ(4, model->count());
  EXPECT_EQ(3u, model->ListTabsInGroup(group).size());
  EXPECT_EQ(group, model->GetTabGroupForTab(1).value());

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Dragging the tab in the first index to the tab in the zero-th index
  // switches the tabs and removes the dragged tab from the group.
  EXPECT_EQ("1 0 2 3", IDString(model));
  EXPECT_THAT(model->ListTabsInGroup(group), testing::ElementsAre(2, 3));
  EXPECT_EQ(base::nullopt, model->GetTabGroupForTab(0));
  EXPECT_EQ(base::nullopt, model->GetTabGroupForTab(1));

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Dragging the tab in the second index to the tab in the first index switches
  // the tabs and removes the dragged tab from the group.
  EXPECT_EQ("1 2 0 3", IDString(model));
  EXPECT_THAT(model->ListTabsInGroup(group), testing::ElementsAre(3));
  EXPECT_EQ(base::nullopt, model->GetTabGroupForTab(2));
}

// Creates a browser with four tabs. The first three belong in the same Tab
// Group. Dragging tabs in a tab group while the group remains consecutive does
// not modify the group of the dragged tab.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragTabWithinGroupDoesNotModifyGroup) {
  scoped_feature_list_.InitAndEnableFeature(features::kTabGroups);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  AddTabsAndResetBrowser(browser(), 3);
  TabGroupId group = model->AddToNewGroup({0, 1, 2});
  StopAnimating(tab_strip);

  EXPECT_EQ(4, model->count());
  EXPECT_THAT(model->ListTabsInGroup(group), testing::ElementsAre(0, 1, 2));

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Dragging the tab in the zero-th index to the tab in the first index
  // switches the tabs but group membership does not change.
  EXPECT_EQ("1 0 2 3", IDString(model));
  EXPECT_THAT(model->ListTabsInGroup(group), testing::ElementsAre(0, 1, 2));

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Dragging the tab in the second index to the tab in the first index switches
  // the tabs but group membership does not change.
  EXPECT_EQ("1 2 0 3", IDString(model));
  EXPECT_THAT(model->ListTabsInGroup(group), testing::ElementsAre(0, 1, 2));
}

// Creates a browser with four tabs. The first tab is in a Tab Group. Dragging
// all tabs in a tab group (1 tab) will not modify the group of the dragged tab.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragSingleTabInGroupDoesNotModifyGroup) {
  scoped_feature_list_.InitAndEnableFeature(features::kTabGroups);

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  AddTabsAndResetBrowser(browser(), 3);
  TabGroupId group = model->AddToNewGroup({0});
  StopAnimating(tab_strip);

  EXPECT_EQ(4, model->count());
  EXPECT_THAT(model->ListTabsInGroup(group), testing::ElementsAre(0));

  // Dragging the tab in the zero-th index to the tab in the first index
  // switches the tabs but group membership does not change.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("1 0 2 3", IDString(model));
  EXPECT_THAT(model->ListTabsInGroup(group), testing::ElementsAre(1));

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Dragging the tab in the first index to the tab in the zero-th index
  // switches the tabs but group membership does not change.
  EXPECT_EQ("0 1 2 3", IDString(model));
  EXPECT_THAT(model->ListTabsInGroup(group), testing::ElementsAre(0));
}

// Drags a tab within the window (without dragging the whole window) then
// pressing a key ends the drag.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       KeyPressShouldEndDragTest) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));

  ASSERT_TRUE(TabDragController::IsActive());

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              false, false, false));
  EXPECT_EQ("1 0", IDString(browser()->tab_strip_model()));
  EXPECT_FALSE(TabDragController::IsActive());
  EXPECT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
}

#if defined(USE_AURA)
bool SubtreeShouldBeExplored(aura::Window* window,
                             const gfx::Point& local_point) {
  gfx::Point point_in_parent = local_point;
  aura::Window::ConvertPointToTarget(window, window->parent(),
                                     &point_in_parent);
  gfx::Point point_in_root = local_point;
  aura::Window::ConvertPointToTarget(window, window->GetRootWindow(),
                                     &point_in_root);
  ui::MouseEvent event(ui::ET_MOUSE_MOVED, point_in_parent, point_in_root,
                       base::TimeTicks::Now(), 0, 0);
  return window->targeter()->SubtreeShouldBeExploredForEvent(window, event);
}

// The logic to find the target tabstrip should take the window mask into
// account. This test hangs without the fix. crbug.com/473080.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragWithMaskedWindows) {
  AddTabsAndResetBrowser(browser(), 1);

  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  const gfx::Rect bounds = browser_window->GetBoundsInScreen();
  aura::test::TestWindowDelegate masked_window_delegate;
  masked_window_delegate.set_can_focus(false);
  std::unique_ptr<aura::Window> masked_window(
      aura::test::CreateTestWindowWithDelegate(
          &masked_window_delegate, 10, bounds, browser_window->parent()));
  masked_window->SetProperty(aura::client::kZOrderingKey,
                             ui::ZOrderLevel::kFloatingWindow);
  auto targeter = std::make_unique<aura::WindowTargeter>();
  targeter->SetInsets(gfx::Insets(0, bounds.width() - 10, 0, 0));
  masked_window->SetEventTargeter(std::move(targeter));

  ASSERT_FALSE(SubtreeShouldBeExplored(masked_window.get(),
                                       gfx::Point(bounds.width() - 11, 0)));
  ASSERT_TRUE(SubtreeShouldBeExplored(masked_window.get(),
                                      gfx::Point(bounds.width() - 9, 0)));
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(ReleaseInput());
  EXPECT_EQ("1 0", IDString(model));
  EXPECT_FALSE(TabDragController::IsActive());
  EXPECT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
}
#endif  // USE_AURA

namespace {

// Invoked from the nested run loop.
void DragToSeparateWindowStep2(DetachToBrowserTabDragControllerTest* test,
                               TabStrip* not_attached_tab_strip,
                               TabStrip* target_tab_strip) {
  ASSERT_FALSE(not_attached_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Test that after the tabs are detached from the source tabstrip (in this
  // case |not_attached_tab_strip|), the tab dragging info should be properly
  // cleared on the source tabstrip.
  EXPECT_TRUE(test->IsTabDraggingInfoCleared(not_attached_tab_strip));
  // At this moment there should be a new browser window for the dragged tabs.
  EXPECT_EQ(3u, test->browser_list->size());
  Browser* new_browser = test->browser_list->get(2);
  TabStrip* new_tab_strip = GetTabStripForBrowser(new_browser);
  ASSERT_TRUE(new_tab_strip->GetDragContext()->IsDragSessionActive());
  // Test that the tab dragging info should be correctly set on the new window.
  EXPECT_TRUE(
      test->IsTabDraggingInfoSet(new_tab_strip, not_attached_tab_strip));

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  //
  // Note: It's possible on small screens for the windows to overlap, so we want
  // to pick a point squarely within the second tab strip and not in the
  // starting window.
  const gfx::Rect old_window_bounds =
      not_attached_tab_strip->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect target_bounds = target_tab_strip->GetBoundsInScreen();
  gfx::Point target_point = target_bounds.CenterPoint();
  target_point.set_x(std::max(target_point.x(), old_window_bounds.right() + 1));
  ASSERT_TRUE(target_bounds.Contains(target_point));
  ASSERT_TRUE(test->DragInputToAsync(target_point));
}

}  // namespace

// Creates two browsers, drags from first into second.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragToSeparateWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  AddTabsAndResetBrowser(browser(), 1);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip, base::BindOnce(&DragToSeparateWindowStep2, this,
                                             tab_strip, tab_strip2));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  EXPECT_FALSE(GetIsDragged(browser()));
  EXPECT_TRUE(IsTabDraggingInfoCleared(tab_strip));
  EXPECT_TRUE(IsTabDraggingInfoSet(tab_strip2, tab_strip));

  // Release mouse or touch, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_TRUE(IsTabDraggingInfoCleared(tab_strip2));
  EXPECT_EQ("100 0", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
  EXPECT_FALSE(GetIsDragged(browser2));

  // Both windows should not be maximized
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_FALSE(browser2->window()->IsMaximized());

  // The tab strip should no longer have capture because the drag was ended and
  // mouse/touch was released.
  EXPECT_FALSE(tab_strip->GetWidget()->HasCapture());
  EXPECT_FALSE(tab_strip2->GetWidget()->HasCapture());
}

namespace {

// WindowFinder that calls OnMouseCaptureLost() from
// GetLocalProcessWindowAtPoint().
class CaptureLoseWindowFinder : public WindowFinder {
 public:
  explicit CaptureLoseWindowFinder(TabStrip* tab_strip)
      : tab_strip_(tab_strip) {}
  ~CaptureLoseWindowFinder() override {}

  // WindowFinder:
  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& screen_point,
      const std::set<gfx::NativeWindow>& ignore) override {
    static_cast<views::View*>(tab_strip_)->OnMouseCaptureLost();
    return nullptr;
  }

 private:
  TabStrip* tab_strip_;

  DISALLOW_COPY_AND_ASSIGN(CaptureLoseWindowFinder);
};

}  // namespace

// Calls OnMouseCaptureLost() from WindowFinder::GetLocalProcessWindowAtPoint()
// and verifies we don't crash. This simulates a crash seen on windows.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       CaptureLostDuringDrag) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  AddTabsAndResetBrowser(browser(), 1);

  // Press on first tab so drag is active. Reset WindowFinder to one that causes
  // capture to be lost from within GetLocalProcessWindowAtPoint(), then
  // continue drag. The capture lost should trigger the drag to cancel.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(tab_strip->GetDragContext()->IsDragSessionActive());
  SetWindowFinderForTabStrip(
      tab_strip, base::WrapUnique(new CaptureLoseWindowFinder(tab_strip)));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
}

namespace {

#if defined(OS_CHROMEOS)
bool IsWindowPositionManaged(aura::Window* window) {
  return window->GetProperty(ash::kWindowPositionManagedTypeKey);
}
bool HasUserChangedWindowPositionOrSize(aura::Window* window) {
  return ash::WindowState::Get(window)->bounds_changed_by_user();
}
#else
bool IsWindowPositionManaged(gfx::NativeWindow window) {
  return true;
}
bool HasUserChangedWindowPositionOrSize(gfx::NativeWindow window) {
  return false;
}
#endif

// Encapsulates waiting for the browser window to become maximized. This is
// needed for example on Chrome desktop linux, where window maximization is done
// asynchronously as an event received from a different process.
class MaximizedBrowserWindowWaiter {
 public:
  explicit MaximizedBrowserWindowWaiter(BrowserWindow* window)
      : window_(window) {}
  ~MaximizedBrowserWindowWaiter() = default;

  // Blocks until the browser window becomes maximized.
  void Wait() {
    if (CheckMaximized())
      return;

    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  bool CheckMaximized() {
    if (!window_->IsMaximized()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              base::IgnoreResult(&MaximizedBrowserWindowWaiter::CheckMaximized),
              base::Unretained(this)));
      return false;
    }

    // Quit the run_loop to end the wait.
    if (!quit_.is_null())
      std::move(quit_).Run();
    return true;
  }

  // The browser window observed by this waiter.
  BrowserWindow* window_;

  // The waiter's RunLoop quit closure.
  base::Closure quit_;

  DISALLOW_COPY_AND_ASSIGN(MaximizedBrowserWindowWaiter);
};

}  // namespace

// Drags from browser to separate window and releases mouse.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DetachToOwnWindow) {
  const gfx::Rect initial_bounds(browser()->window()->GetBounds());
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DetachToBrowserTabDragControllerTest::
                                      ReleaseInputAfterWindowDetached,
                                  base::Unretained(this)));

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);

  bool check_new_window_active = true;
#if defined(OS_MACOSX)
  // AppKit 10.10 asynchronously reactivates the first
  // window. This behavior is non-deterministic, and appears to be a test-only
  // issue. Thus, we just skip the test check. https://crbug.com/862859.
  if (base::mac::IsOS10_10())
    check_new_window_active = false;
#endif
  if (check_new_window_active) {
    EXPECT_TRUE(new_browser->window()->IsActive());
  }
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  EXPECT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // The bounds of the initial window should not have changed.
  EXPECT_EQ(initial_bounds.ToString(),
            browser()->window()->GetBounds().ToString());

  EXPECT_FALSE(GetIsDragged(browser()));
  EXPECT_FALSE(GetIsDragged(new_browser));
  // After this both windows should still be manageable.
  EXPECT_TRUE(IsWindowPositionManaged(browser()->window()->GetNativeWindow()));
  EXPECT_TRUE(IsWindowPositionManaged(
      new_browser->window()->GetNativeWindow()));

  // Both windows should not be maximized
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_FALSE(new_browser->window()->IsMaximized());

  // The tab strip should no longer have capture because the drag was ended and
  // mouse/touch was released.
  EXPECT_FALSE(tab_strip->GetWidget()->HasCapture());
  EXPECT_FALSE(tab_strip2->GetWidget()->HasCapture());
}

// Tests that a tab can be dragged from a browser window that is resized to full
// screen.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DetachFromFullsizeWindow) {
  // Resize the browser window so that it is as big as the work area.
  gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .work_area();
  browser()->window()->SetBounds(work_area);
  const gfx::Rect initial_bounds(browser()->window()->GetBounds());
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DetachToBrowserTabDragControllerTest::
                                      ReleaseInputAfterWindowDetached,
                                  base::Unretained(this)));

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // The bounds of the initial window should not have changed.
  EXPECT_EQ(initial_bounds.ToString(),
            browser()->window()->GetBounds().ToString());

  EXPECT_FALSE(GetIsDragged(browser()));
  EXPECT_FALSE(GetIsDragged(new_browser));
  // After this both windows should still be manageable.
  EXPECT_TRUE(IsWindowPositionManaged(browser()->window()->GetNativeWindow()));
  EXPECT_TRUE(
      IsWindowPositionManaged(new_browser->window()->GetNativeWindow()));

  // The tab strip should no longer have capture because the drag was ended and
  // mouse/touch was released.
  EXPECT_FALSE(tab_strip->GetWidget()->HasCapture());
  EXPECT_FALSE(tab_strip2->GetWidget()->HasCapture());
}

// This test doesn't make sense on Mac, since it has no concept of "maximized".
#if !defined(OS_MACOSX)
// Drags from browser to a separate window and releases mouse.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DetachToOwnWindowFromMaximizedWindow) {
  // Maximize the initial browser window.
  browser()->window()->Maximize();
  MaximizedBrowserWindowWaiter(browser()->window()).Wait();
  ASSERT_TRUE(browser()->window()->IsMaximized());

  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DetachToBrowserTabDragControllerTest::
                                      ReleaseInputAfterWindowDetached,
                                  base::Unretained(this)));

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // The bounds of the initial window should not have changed.
  EXPECT_TRUE(browser()->window()->IsMaximized());

  EXPECT_FALSE(GetIsDragged(browser()));
  EXPECT_FALSE(GetIsDragged(new_browser));
  // After this both windows should still be manageable.
  EXPECT_TRUE(IsWindowPositionManaged(browser()->window()->GetNativeWindow()));
  EXPECT_TRUE(IsWindowPositionManaged(
      new_browser->window()->GetNativeWindow()));

  // The new window should be maximized.
  MaximizedBrowserWindowWaiter(new_browser->window()).Wait();
  EXPECT_TRUE(new_browser->window()->IsMaximized());
}
#endif

#if defined(OS_CHROMEOS)

// This test makes sense only on Chrome OS where we have the immersive
// fullscreen mode. The detached tab to a new browser window should remain in
// immersive fullscreen mode.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DetachToOwnWindowWhileInImmersiveFullscreenMode) {
  // Toggle the immersive fullscreen mode for the initial browser.
  chrome::ToggleFullscreenMode(browser());
  ImmersiveModeController* controller =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->immersive_mode_controller();
  ASSERT_TRUE(controller->IsEnabled());

  // Forcively reveal the tabstrip immediately.
  std::unique_ptr<ImmersiveRevealedLock> lock(
      controller->GetRevealedLock(ImmersiveModeController::ANIMATE_REVEAL_NO));

  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DetachToBrowserTabDragControllerTest::
                                      ReleaseInputAfterWindowDetached,
                                  base::Unretained(this)));

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // The bounds of the initial window should not have changed.
  EXPECT_TRUE(browser()->window()->IsFullscreen());
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->immersive_mode_controller()
                  ->IsEnabled());

  EXPECT_FALSE(GetIsDragged(browser()));
  EXPECT_FALSE(GetIsDragged(new_browser));
  // After this both windows should still be manageable.
  EXPECT_TRUE(IsWindowPositionManaged(browser()->window()->GetNativeWindow()));
  EXPECT_TRUE(
      IsWindowPositionManaged(new_browser->window()->GetNativeWindow()));

  // The new browser should be in immersive fullscreen mode.
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(new_browser)
                  ->immersive_mode_controller()
                  ->IsEnabled());
  EXPECT_TRUE(new_browser->window()->IsFullscreen());
}

#endif

// Deletes a tab being dragged before the user moved enough to start a drag.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DeleteBeforeStartedDragging) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Click on the first tab, but don't move it.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));

  // Should be dragging.
  ASSERT_TRUE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Delete the tab being dragged.
  browser()->tab_strip_model()->DetachWebContentsAt(0);

  // Should have canceled dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
  EXPECT_FALSE(GetIsDragged(browser()));
}

IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragDoesntStartFromClick) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Click on the first tab, but don't move it.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  EXPECT_TRUE(PressInput(tab_0_center));

  // A drag session should exist, but the drag should not have started.
  EXPECT_TRUE(tab_strip->GetDragContext()->IsDragSessionActive());
  EXPECT_TRUE(TabDragController::IsActive());
  EXPECT_FALSE(HasDragStarted(tab_strip));

  // Move the mouse enough to start the drag.  It doesn't matter whether this
  // is enough to create a window or not.
  EXPECT_TRUE(DragInputTo(gfx::Point(tab_0_center.x() + 20, tab_0_center.y())));

  // Drag should now have started.
  EXPECT_TRUE(HasDragStarted(tab_strip));

  EXPECT_TRUE(ReleaseInput());
}

// Deletes a tab being dragged while still attached.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DeleteTabWhileAttached) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Click on the first tab and move it enough so that it starts dragging but is
  // still attached.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputTo(
                  gfx::Point(tab_0_center.x() + 20, tab_0_center.y())));

  // Should be dragging.
  ASSERT_TRUE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Delete the tab being dragged.
  browser()->tab_strip_model()->DetachWebContentsAt(0);

  // Should have canceled dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  EXPECT_FALSE(GetIsDragged(browser()));
}

namespace {

void CloseTabsWhileDetachedStep2(const BrowserList* browser_list) {
  ASSERT_EQ(2u, browser_list->size());
  Browser* old_browser = browser_list->get(0);
  EXPECT_EQ("0 3", IDString(old_browser->tab_strip_model()));
  Browser* new_browser = browser_list->get(1);
  EXPECT_EQ("1 2", IDString(new_browser->tab_strip_model()));
  chrome::CloseTab(new_browser);
}

}  // namespace

// Selects 2 tabs out of 4, drags them out and closes the new browser window
// while dragging tabs.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DeleteTabsWhileDetached) {
  AddTabsAndResetBrowser(browser(), 3);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  EXPECT_EQ("0 1 2 3", IDString(browser()->tab_strip_model()));

  // Click the first tab and select two middle tabs.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(ReleaseInput());
  browser()->tab_strip_model()->ToggleSelectionAt(2);

  // Press mouse button in the second tab and drag it enough to detach.
  DragTabAndNotify(
      tab_strip, base::BindOnce(&CloseTabsWhileDetachedStep2, browser_list), 2);

  // Should not be dragging.
  ASSERT_EQ(1u, browser_list->size());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // Both tabs "1" and "2" get closed.
  EXPECT_EQ("0 3", IDString(browser()->tab_strip_model()));

  EXPECT_FALSE(GetIsDragged(browser()));
}

namespace {

void PressEscapeWhileDetachedStep2(const BrowserList* browser_list) {
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ui_controls::SendKeyPress(
      new_browser->window()->GetNativeWindow(), ui::VKEY_ESCAPE, false, false,
      false, false);
}

}  // namespace

// Detaches a tab and while detached presses escape to revert the drag.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       PressEscapeWhileDetached) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  DragTabAndNotify(
      tab_strip, base::BindOnce(&PressEscapeWhileDetachedStep2, browser_list));

  // Should not be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // And there should only be one window.
  EXPECT_EQ(1u, browser_list->size());

  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Remaining browser window should not be maximized
  EXPECT_FALSE(browser()->window()->IsMaximized());

  // The tab strip should no longer have capture because the drag was ended and
  // mouse/touch was released.
  EXPECT_FALSE(tab_strip->GetWidget()->HasCapture());
}

namespace {

void DragAllStep2(DetachToBrowserTabDragControllerTest* test,
                  const BrowserList* browser_list) {
  // Should only be one window.
  ASSERT_EQ(1u, browser_list->size());
  // Windows hangs if you use a sync mouse event here.
  ASSERT_TRUE(test->ReleaseInput(0, true));
}

}  // namespace

// Selects multiple tabs and starts dragging the window.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest, DragAll) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  browser()->tab_strip_model()->ToggleSelectionAt(0);
  const gfx::Rect initial_bounds = browser()->window()->GetBounds();

  // Move to the first tab and drag it enough so that it would normally
  // detach.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DragAllStep2, this, browser_list));

  // Should not be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // And there should only be one window.
  EXPECT_EQ(1u, browser_list->size());

  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  EXPECT_FALSE(GetIsDragged(browser()));

  // Remaining browser window should not be maximized
  EXPECT_FALSE(browser()->window()->IsMaximized());

  const gfx::Rect final_bounds = browser()->window()->GetBounds();
  // Size unchanged, but it should have moved down.
  EXPECT_EQ(initial_bounds.size(), final_bounds.size());
  EXPECT_EQ(initial_bounds.origin().x(), final_bounds.origin().x());
  EXPECT_EQ(initial_bounds.origin().y() + GetDetachY(tab_strip),
            final_bounds.origin().y());
}

namespace {

// Invoked from the nested run loop.
void DragAllToSeparateWindowStep2(DetachToBrowserTabDragControllerTest* test,
                                  TabStrip* attached_tab_strip,
                                  TabStrip* target_tab_strip) {
  ASSERT_TRUE(attached_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, test->browser_list->size());

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  ASSERT_TRUE(
      test->DragInputToAsync(GetCenterInScreenCoordinates(target_tab_strip)));
}

}  // namespace

// Creates two browsers, selects all tabs in first and drags into second.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragAllToSeparateWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  AddTabsAndResetBrowser(browser(), 1);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  browser()->tab_strip_model()->ToggleSelectionAt(0);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip, base::BindOnce(&DragAllToSeparateWindowStep2,
                                             this, tab_strip, tab_strip2));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(1u, browser_list->size());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0 1", IDString(browser2->tab_strip_model()));

  EXPECT_FALSE(GetIsDragged(browser2));

  // Remaining browser window should not be maximized
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

namespace {

// Invoked from the nested run loop.
void DragAllToSeparateWindowAndCancelStep2(
    DetachToBrowserTabDragControllerTest* test,
    TabStrip* attached_tab_strip,
    TabStrip* target_tab_strip,
    const BrowserList* browser_list) {
  ASSERT_TRUE(attached_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  ASSERT_TRUE(
      test->DragInputToAsync(GetCenterInScreenCoordinates(target_tab_strip)));
}

}  // namespace

// Creates two browsers, selects all tabs in first, drags into second, then hits
// escape.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragAllToSeparateWindowAndCancel) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  AddTabsAndResetBrowser(browser(), 1);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  browser()->tab_strip_model()->ToggleSelectionAt(0);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DragAllToSeparateWindowAndCancelStep2, this,
                                  tab_strip, tab_strip2, browser_list));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(1u, browser_list->size());

  // Cancel the drag.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser2, ui::VKEY_ESCAPE, false, false, false, false));

  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0 1", IDString(browser2->tab_strip_model()));

  // browser() will have been destroyed, but browser2 should remain.
  ASSERT_EQ(1u, browser_list->size());

  EXPECT_FALSE(GetIsDragged(browser2));

  // Remaining browser window should not be maximized
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

// Creates two browsers, drags from first into the second in such a way that
// no detaching should happen.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragDirectlyToSecondWindow) {
  // TODO(pkasting): Crashes when detaching browser.  https://crbug.com/918733
  if (input_source() == INPUT_SOURCE_TOUCH) {
    VLOG(1) << "Test is DISABLED for touch input.";
    return;
  }

  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  AddTabsAndResetBrowser(browser(), 1);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  const gfx::Rect initial_bounds(browser2->window()->GetBounds());

  // Place the first browser directly below the second in such a way that
  // dragging a tab upwards will drag it directly into the second browser's
  // tabstrip.
  const BrowserView* const browser_view2 =
      BrowserView::GetBrowserViewForBrowser(browser2);
  const gfx::Rect tabstrip_region2_bounds =
      browser_view2->frame()->GetBoundsForTabStripRegion(
          browser_view2->tabstrip());
  gfx::Rect bounds = initial_bounds;
  bounds.Offset(0, tabstrip_region2_bounds.bottom());
  browser()->window()->SetBounds(bounds);

  // Ensure the first browser is on top so clicks go to it.
  ui_test_utils::BrowserActivationWaiter activation_waiter(browser());
  browser()->window()->Activate();
  activation_waiter.WaitForActivation();

  // Move to the first tab and drag it to browser2.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));

  const views::View* tab = tab_strip2->tab_at(0);
  gfx::Point b2_location(GetCenterInScreenCoordinates(tab));
  b2_location.Offset(-tab->width() / 4, 0);
  ASSERT_TRUE(DragInputTo(b2_location));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 100", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // Make sure that the window is still managed and not user moved.
  EXPECT_TRUE(IsWindowPositionManaged(browser2->window()->GetNativeWindow()));
  EXPECT_FALSE(HasUserChangedWindowPositionOrSize(
      browser2->window()->GetNativeWindow()));

  // Also make sure that the drag to window position has not changed.
  EXPECT_EQ(initial_bounds.ToString(),
            browser2->window()->GetBounds().ToString());
}

// Creates two browsers, the first browser has a single tab and drags into the
// second browser.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragSingleTabToSeparateWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  ResetIDs(browser()->tab_strip_model(), 0);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip, base::BindOnce(&DragAllToSeparateWindowStep2,
                                             this, tab_strip, tab_strip2));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(1u, browser_list->size());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0", IDString(browser2->tab_strip_model()));

  EXPECT_FALSE(GetIsDragged(browser2));

  // Remaining browser window should not be maximized
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

namespace {

// Invoked from the nested run loop.
void CancelOnNewTabWhenDraggingStep2(
    DetachToBrowserTabDragControllerTest* test,
    const BrowserList* browser_list) {
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  chrome::AddTabAt(browser_list->GetLastActive(), GURL(url::kAboutBlankURL),
                   0, false);
}

}  // namespace

// Adds another tab, detaches into separate window, adds another tab and
// verifies the run loop ends.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       CancelOnNewTabWhenDragging) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  AddTabsAndResetBrowser(browser(), 1);

  // Move to the first tab and drag it enough so that it detaches.
  const gfx::Point tab_0_center =
      GetCenterInScreenCoordinates(tab_strip->tab_at(0));
  ASSERT_TRUE(PressInput(tab_0_center));

  // Add another tab. This should trigger exiting the nested loop. Add at the
  // beginning to exercise past crash when model/tabstrip got out of sync.
  // crbug.com/474082
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center + gfx::Vector2d(0, GetDetachY(tab_strip)),
      base::BindOnce(&CancelOnNewTabWhenDraggingStep2, this, browser_list)));
  observer.Wait();

  // Should be two windows and not dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());
  for (auto* browser : *BrowserList::GetInstance()) {
    EXPECT_FALSE(GetIsDragged(browser));
    // Should not be maximized
    EXPECT_FALSE(browser->window()->IsMaximized());
  }
}

namespace {

TabStrip* GetAttachedTabstrip() {
  for (Browser* browser : *BrowserList::GetInstance()) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    if (TabDragController::IsAttachedTo(
            browser_view->tabstrip()->GetDragContext()))
      return browser_view->tabstrip();
  }
  return nullptr;
}

void DragWindowAndVerifyOffset(DetachToBrowserTabDragControllerTest* test,
                               TabStrip* tab_strip,
                               int tab_index) {
  test::QuitDraggingObserver observer;
  // Move to the tab and drag it enough so that it detaches.
  const gfx::Point tab_center =
      GetCenterInScreenCoordinates(tab_strip->tab_at(tab_index));
  // The expected offset; the horizontal position should be relative to the
  // pressed tab. The vertical position should be relative to the window itself
  // since the top margin can be different between the existing browser and
  // the dragged one.
  const gfx::Vector2d press_offset(
      tab_center.x() - tab_strip->tab_at(tab_index)->GetBoundsInScreen().x(),
      tab_center.y() - tab_strip->GetWidget()->GetWindowBoundsInScreen().y());
  const gfx::Point initial_move =
      tab_center + gfx::Vector2d(0, GetDetachY(tab_strip));
  const gfx::Point second_move = initial_move + gfx::Vector2d(20, 20);
  ASSERT_TRUE(test->PressInput(tab_center));
  ASSERT_TRUE(test->DragInputToNotifyWhenDone(
      initial_move, base::BindLambdaForTesting([&]() {
        // Moves slightly to cause the actual dragging effect on the system and
        // makes sure the window is positioned correctly.
        ASSERT_TRUE(test->DragInputToNotifyWhenDone(
            second_move, base::BindLambdaForTesting([&]() {
              TabStrip* attached = GetAttachedTabstrip();
              // Same computation for drag offset. This operation drags a single
              // tab, so the target tab index should be always 0.
              gfx::Vector2d drag_offset(
                  second_move.x() -
                      attached->tab_at(0)->GetBoundsInScreen().x(),
                  second_move.y() -
                      attached->GetWidget()->GetWindowBoundsInScreen().y());
              EXPECT_EQ(press_offset, drag_offset);
              ASSERT_TRUE(test->ReleaseInput());
            })));
      })));
  observer.Wait();
}

}  // namespace

#if defined(OS_WIN)
// TODO(mukai): enable those tests on Windows.
#define MAYBE_OffsetForDraggingTab DISABLED_OffsetForDraggingTab
#else
#define MAYBE_OffsetForDraggingTab OffsetForDraggingTab
#endif

IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_OffsetForDraggingTab) {
  DragWindowAndVerifyOffset(this, GetTabStripForBrowser(browser()), 0);
  ASSERT_FALSE(TabDragController::IsActive());
}

// TODO(960915): fix flakiness and re-enable this test on mac/linux.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DISABLED_OffsetForDraggingDetachedTab) {
  AddTabsAndResetBrowser(browser(), 1);

  DragWindowAndVerifyOffset(this, GetTabStripForBrowser(browser()), 1);
  ASSERT_FALSE(TabDragController::IsActive());
}

#if defined(OS_CHROMEOS)
namespace {

void DragInMaximizedWindowStep2(DetachToBrowserTabDragControllerTest* test,
                                Browser* browser,
                                TabStrip* tab_strip,
                                const BrowserList* browser_list) {
  // There should be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  EXPECT_NE(browser, new_browser);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);

  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());

  // Both windows should be visible.
  EXPECT_TRUE(tab_strip->GetWidget()->IsVisible());
  EXPECT_TRUE(tab_strip2->GetWidget()->IsVisible());

  // Stops dragging.
  ASSERT_TRUE(test->ReleaseInput());
}

}  // namespace

// Creates a browser with two tabs, maximizes it, drags the tab out.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragInMaximizedWindow) {
  AddTabsAndResetBrowser(browser(), 1);
  browser()->window()->Maximize();

  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DragInMaximizedWindowStep2, this, browser(),
                                  tab_strip, browser_list));

  ASSERT_FALSE(TabDragController::IsActive());

  // Should be two browsers.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());

  EXPECT_TRUE(browser()->window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(new_browser->window()->GetNativeWindow()->IsVisible());

  EXPECT_FALSE(GetIsDragged(browser()));
  EXPECT_FALSE(GetIsDragged(new_browser));

  // The source window should be maximized.
  EXPECT_TRUE(browser()->window()->IsMaximized());
  // The new window should be maximized.
  EXPECT_TRUE(new_browser->window()->IsMaximized());
}

IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       OffsetForDraggingInMaximizedWindow) {
  AddTabsAndResetBrowser(browser(), 1);
  // Moves the browser window slightly to ensure that the browser's restored
  // bounds are different from the maximized bound's origin.
  browser()->window()->SetBounds(browser()->window()->GetBounds() +
                                 gfx::Vector2d(100, 50));
  browser()->window()->Maximize();

  DragWindowAndVerifyOffset(this, GetTabStripForBrowser(browser()), 0);
  ASSERT_FALSE(TabDragController::IsActive());
}

IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       OffsetForDraggingInTabletMode) {
  AddTabsAndResetBrowser(browser(), 1);
  // Moves the browser window slightly to ensure that the browser's restored
  // bounds are different from the maximized bound's origin.
  browser()->window()->SetBounds(browser()->window()->GetBounds() +
                                 gfx::Vector2d(100, 50));
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  DragWindowAndVerifyOffset(this, GetTabStripForBrowser(browser()), 1);
  ASSERT_FALSE(TabDragController::IsActive());
}

IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       OffsetForDraggingRightSnappedWindowInTabletMode) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  // Right snap the browser window.
  aura::Window* window = browser()->window()->GetNativeWindow();
  ash::Shell::Get()->split_view_controller()->SnapWindow(
      window, ash::SplitViewController::RIGHT);
  EXPECT_NE(gfx::Point(), window->GetBoundsInScreen().origin());

  DragWindowAndVerifyOffset(this, GetTabStripForBrowser(browser()), 0);
  ASSERT_FALSE(TabDragController::IsActive());
}

namespace {

void DragToOverviewWindowStep2(DetachToBrowserTabDragControllerTest* test,
                               TabStrip* not_attached_tab_strip,
                               TabStrip* target_tab_strip) {
  ASSERT_FALSE(not_attached_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // And there should be three browser windows, including the newly created one
  // for the dragged tab.
  EXPECT_EQ(3u, test->browser_list->size());

  // Put the window that accociated with |target_tab_strip| in overview.
  test::GetWindowForTabStrip(target_tab_strip)
      ->SetProperty(ash::kIsShowingInOverviewKey, true);

  // Drag to target_tab_strip.
  ASSERT_TRUE(
      test->DragInputTo(GetCenterInScreenCoordinates(target_tab_strip)));

  // Test that the dragged tab did not attach to the overview window.
  EXPECT_EQ(3u, test->browser_list->size());

  ASSERT_TRUE(test->ReleaseInput());
}

}  // namespace

// Creates two browsers, drags from first into second. If the target window is
// currently showing in overview, we should not attaching the dragged tabs
// into the target window during dragging, but should do so until the drag ends.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragToOverviewWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Add another tab to browser().
  AddTabsAndResetBrowser(browser(), 1);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip, base::BindOnce(&DragToOverviewWindowStep2, this,
                                             tab_strip, tab_strip2));

  // Now the dragged tab should have been attached to the target tabstrip after
  // the drag ends.
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ(2u, browser_list->size());
}

namespace {

void DragToOverviewNewWindowItemStep2(
    DetachToBrowserTabDragControllerTest* test,
    TabStrip* attached_tab_strip) {
  ASSERT_TRUE(attached_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  EXPECT_TRUE(attached_tab_strip->GetWidget()->GetNativeWindow()->HasFocus());

  // Put the attached window in overview to simulate the "drop on the new
  // selector item" scenario.
  test::GetWindowForTabStrip(attached_tab_strip)
      ->SetProperty(ash::kIsShowingInOverviewKey, true);
  // At the same time we remove |attached_tab_strip|'s focus to simulate what
  // happens in overview (In overview, the window items in overview don't have
  // focus, it's the textfield in overview that has focus).
  attached_tab_strip->GetFocusManager()->SetFocusedView(nullptr);

  ASSERT_TRUE(test->ReleaseInput());
}

}  // namespace

// After dragging a window to drop it onto the new window selector item in
// overview mode, the window should be added to overview window grid, and should
// not restore its focus.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragToOverviewNewWindowItem) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Make |tab_strip| the focused view before dragging.
  tab_strip->GetFocusManager()->SetFocusedView(tab_strip);
  EXPECT_TRUE(tab_strip->HasFocus());

  // Drag the tab long enough so that it moves.
  DragTabAndNotify(tab_strip, base::BindOnce(&DragToOverviewNewWindowItemStep2,
                                             this, tab_strip));

  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ(1u, browser_list->size());

  // Test that the attached tabstrip doesn't restore focuas as it's currently
  // showing in overview.
  EXPECT_TRUE(test::GetWindowForTabStrip(tab_strip)->GetProperty(
      ash::kIsShowingInOverviewKey));
  EXPECT_FALSE(tab_strip->HasFocus());
}

namespace {

// A window observer that observes the dragged window's property
// ash::kIsDraggingTabsKey.
class DraggedWindowObserver : public aura::WindowObserver {
 public:
  DraggedWindowObserver(DetachToBrowserTabDragControllerTest* test,
                        aura::Window* window,
                        const gfx::Rect& bounds,
                        const gfx::Point& end_point)
      : test_(test), end_bounds_(bounds), end_point_(end_point) {}

  ~DraggedWindowObserver() override {
    if (window_)
      window_->RemoveObserver(this);
  }

  void StartObserving(aura::Window* window) {
    DCHECK(!window_);
    window_ = window;
    window_->AddObserver(this);
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(window_, window);
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK_EQ(window_, window);
    if (key == ash::kIsDraggingTabsKey) {
      if (!window_->GetProperty(ash::kIsDraggingTabsKey)) {
        // It should be triggered by TabDragController::ClearTabDraggingInfo()
        // from TabDragController::EndDragImpl(). Theoretically at this point
        // TabDragController should have removed itself as an observer of the
        // dragged tabstrip's widget. So changing its bounds should do nothing.

        // It's to ensure the current cursor location is within the bounds of
        // another browser's tabstrip.
        test_->MoveInputTo(end_point_);

        // Change window's bounds to simulate what might happen in ash. If
        // TabDragController is still an observer of the dragged tabstrip's
        // widget, OnWidgetBoundsChanged() will calls into ContinueDragging()
        // to attach the dragged tabstrip into another browser, which might
        // cause chrome crash.
        window_->SetBounds(end_bounds_);
      }
    }
  }

 private:
  DetachToBrowserTabDragControllerTest* test_;
  // The dragged window.
  aura::Window* window_ = nullptr;
  // The bounds that |window_| will change to when the drag ends.
  gfx::Rect end_bounds_;
  // The position that the mouse/touch event will move to when the drag ends.
  gfx::Point end_point_;

  DISALLOW_COPY_AND_ASSIGN(DraggedWindowObserver);
};

void DoNotObserveDraggedWidgetAfterDragEndsStep2(
    DetachToBrowserTabDragControllerTest* test,
    DraggedWindowObserver* observer,
    TabStrip* attached_tab_strip) {
  ASSERT_TRUE(attached_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Start observe the dragged window.
  observer->StartObserving(attached_tab_strip->GetWidget()->GetNativeWindow());

  ASSERT_TRUE(test->ReleaseInput());
}

}  // namespace

// Test that after the drag ends, TabDragController is no longer an observer of
// the dragged widget, so that if the bounds of the dragged widget change,
// TabDragController won't be put into dragging process again.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DoNotObserveDraggedWidgetAfterDragEnds) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  EXPECT_EQ(2u, browser_list->size());

  // Create an window observer to observe the dragged window.
  std::unique_ptr<DraggedWindowObserver> observer(new DraggedWindowObserver(
      this, test::GetWindowForTabStrip(tab_strip),
      tab_strip2->GetWidget()->GetNativeWindow()->bounds(),
      GetCenterInScreenCoordinates(tab_strip2)));

  // Drag the tab long enough so that it moves.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DoNotObserveDraggedWidgetAfterDragEndsStep2,
                                  this, observer.get(), tab_strip));

  // There should be still two browsers at this moment. |tab_strip| should not
  // be merged into |tab_strip2|.
  EXPECT_EQ(2u, browser_list->size());

  ASSERT_FALSE(TabDragController::IsActive());
}

namespace {

void DoNotAttachToOtherWindowTestStep2(
    DetachToBrowserTabDragControllerTest* test,
    TabStrip* not_attached_tab_strip,
    TabStrip* target_tab_strip) {
  ASSERT_TRUE(TabDragController::IsActive());

  // There should be three browser windows, including the newly created one for
  // the dragged tab.
  EXPECT_EQ(3u, test->browser_list->size());
  // Get this new created window and set it to non-attachable.
  Browser* new_browser = test->browser_list->get(2);
  new_browser->window()->GetNativeWindow()->SetProperty(
      ash::kCanAttachToAnotherWindowKey, false);

  // Now drag to target_tab_strip.
  ASSERT_TRUE(
      test->DragInputTo(GetCenterInScreenCoordinates(target_tab_strip)));

  ASSERT_TRUE(test->ReleaseInput());
}

}  // namespace

// Test that if the dragged window is not allowed to attach to another window
// during dragging, then it can't attach to another window.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DoNotAttachToOtherWindowTest) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  AddTabsAndResetBrowser(browser(), 1);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  EXPECT_EQ(2u, browser_list->size());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip, base::BindOnce(&DoNotAttachToOtherWindowTestStep2,
                                             this, tab_strip, tab_strip2));

  // Test that the newly created browser window doesn't attach to the target
  // browser window.
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ(3u, browser_list->size());
}

namespace {

void DeferredTargetTabStripTestStep2(DetachToBrowserTabDragControllerTest* test,
                                     TabStrip* not_attached_tab_strip,
                                     TabStrip* target_tab_strip) {
  ASSERT_FALSE(not_attached_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // And there should be three browser windows, including the newly created one
  // for the dragged tab.
  EXPECT_EQ(3u, test->browser_list->size());

  // Put the window that accociated with |target_tab_strip| in overview.
  test::GetWindowForTabStrip(target_tab_strip)
      ->SetProperty(ash::kIsShowingInOverviewKey, true);

  // Drag to target_tab_strip.
  ASSERT_TRUE(
      test->DragInputTo(GetCenterInScreenCoordinates(target_tab_strip)));

  // At this point, |target_tab_strip| should be the deferred target tabstip.
  // Theoratically the dragged tabstrip will merge into |target_tab_strip| after
  // the drag ends.
  EXPECT_TRUE(test::GetWindowForTabStrip(target_tab_strip)
                  ->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));

  // Now clear the property.
  test::GetWindowForTabStrip(target_tab_strip)
      ->ClearProperty(ash::kIsDeferredTabDraggingTargetWindowKey);

  ASSERT_TRUE(test->ReleaseInput());
}

}  // namespace

// Test that if a tabstrip is a deferred target tabstrip, and its corresponding
// window key is cleared to remove itself as the deferred target tabstrip, the
// dragged tabstrip should not attach into it after the drag ends.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DeferredTargetTabStripTest) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  AddTabsAndResetBrowser(browser(), 1);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip, base::BindOnce(&DeferredTargetTabStripTestStep2,
                                             this, tab_strip, tab_strip2));

  // Now the dragged tab should not be attached to the target tabstrip after
  // the drag ends.
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ(3u, browser_list->size());
}

namespace {

// Returns true if the web contents that's accociated with |browser| is using
// fast resize.
bool WebContentsIsFastResized(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  ContentsWebView* contents_web_view =
      static_cast<ContentsWebView*>(browser_view->GetContentsView());
  return contents_web_view->holder()->fast_resize();
}

void FastResizeDuringDraggingStep2(DetachToBrowserTabDragControllerTest* test,
                                   TabStrip* not_attached_tab_strip,
                                   TabStrip* target_tab_strip) {
  // There should be three browser windows, including the newly created one for
  // the dragged tab.
  EXPECT_EQ(3u, test->browser_list->size());

  // Get this new created window for the drag. It should have fast resize set.
  Browser* new_browser = test->browser_list->get(2);
  EXPECT_TRUE(WebContentsIsFastResized(new_browser));
  // The source window should also have fast resize set.
  EXPECT_TRUE(WebContentsIsFastResized(test->browser()));

  // Now drag to target_tab_strip.
  ASSERT_TRUE(
      test->DragInputTo(GetCenterInScreenCoordinates(target_tab_strip)));

  ASSERT_TRUE(test->ReleaseInput());
}

}  // namespace

// Tests that we use fast resize to resize the web contents of the dragged
// window and the source window during tab dragging process, and don't use fast
// resize after tab dragging ends.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       FastResizeDuringDragging) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  AddTabsAndResetBrowser(browser(), 1);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  EXPECT_EQ(2u, browser_list->size());

  EXPECT_FALSE(WebContentsIsFastResized(browser()));
  EXPECT_FALSE(WebContentsIsFastResized(browser2));

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip, base::BindOnce(&FastResizeDuringDraggingStep2,
                                             this, tab_strip, tab_strip2));

  EXPECT_FALSE(WebContentsIsFastResized(browser()));
  EXPECT_FALSE(WebContentsIsFastResized(browser2));
}

namespace {

void DragToMinimizedOverviewWindowStep2(
    DetachToBrowserTabDragControllerTest* test,
    TabStrip* dragged_tab_strip,
    TabStrip* target_tab_strip) {
  EXPECT_EQ(2u, test->browser_list->size());

  // Test that overview is open behind the dragged window and |target_window|
  // has been put in overview as expected.
  aura::Window* dragged_window = test::GetWindowForTabStrip(dragged_tab_strip);
  aura::Window* target_window = test::GetWindowForTabStrip(target_tab_strip);
  EXPECT_TRUE(target_window->GetProperty(ash::kIsShowingInOverviewKey));
  EXPECT_FALSE(dragged_window->GetProperty(ash::kIsShowingInOverviewKey));

  // Now drag the tabs to a point that is contained by |target_window|.
  gfx::RectF target_window_bounds(target_window->bounds());
  gfx::Transform transform = target_window->layer()->GetTargetTransform();
  transform.TransformRect(&target_window_bounds);
  gfx::Point target_point(target_window_bounds.CenterPoint().x(),
                          target_window_bounds.CenterPoint().y());

  // Minimize the |target_window|.
  target_window->SetProperty(aura::client::kShowStateKey,
                             ui::SHOW_STATE_MINIMIZED);

  ASSERT_TRUE(test->DragInputTo(target_point));
  EXPECT_TRUE(
      target_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));

  ASSERT_TRUE(test->ReleaseInput());
}

}  // namespace

// Test that the dragged tabs should be able to merge into an overview window
// that represents a minimized window.
// TODO(https://crbug.com/979013) Disabled due to flakiness.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DISABLED_DragToMinimizedOverviewWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  EXPECT_EQ(2u, browser_list->size());

  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  // Move to the first tab of |browser2| and drag it toward to browser(). Note
  // dragging on |browser2| which only has one tab in tablet mode will open
  // overview behind the |browser2|.
  DragTabAndNotify(tab_strip2,
                   base::BindOnce(&DragToMinimizedOverviewWindowStep2, this,
                                  tab_strip2, tab_strip));

  // |tab_strip| should have been merged into |browser2|. Thus there should only
  // be one browser now.
  EXPECT_EQ(browser_list->size(), 1u);
}

// Subclass of DetachToBrowserTabDragControllerTest that
// creates multiple displays.
class DetachToBrowserInSeparateDisplayTabDragControllerTest
    : public DetachToBrowserTabDragControllerTest {
 public:
  DetachToBrowserInSeparateDisplayTabDragControllerTest() {}
  virtual ~DetachToBrowserInSeparateDisplayTabDragControllerTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DetachToBrowserTabDragControllerTest::SetUpCommandLine(command_line);
    // Make screens sufficiently wide to host 2 browsers side by side.
    command_line->AppendSwitchASCII("ash-host-window-bounds",
                                    "0+0-800x600,800+0-800x600");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      DetachToBrowserInSeparateDisplayTabDragControllerTest);
};

namespace {

void DragSingleTabToSeparateWindowInSecondDisplayStep3(
    DetachToBrowserTabDragControllerTest* test) {
  ASSERT_TRUE(test->ReleaseInput());
}

void DragSingleTabToSeparateWindowInSecondDisplayStep2(
    DetachToBrowserTabDragControllerTest* test,
    const gfx::Point& target_point) {
  ASSERT_TRUE(test->DragInputToNotifyWhenDone(
      target_point,
      base::BindOnce(&DragSingleTabToSeparateWindowInSecondDisplayStep3,
                     test)));
}

}  // namespace

// Drags from browser to a second display and releases input.
IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DragSingleTabToSeparateWindowInSecondDisplay) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  // Then drag it to the final destination on the second screen.
  display::Screen* const screen = display::Screen::GetScreen();
  display::Display second_display = ui_test_utils::GetSecondaryDisplay(screen);
  const gfx::Point start = GetCenterInScreenCoordinates(tab_strip->tab_at(0));
  ASSERT_FALSE(second_display.bounds().Contains(start));
  const gfx::Point target(second_display.bounds().x() + 1,
                          start.y() + GetDetachY(tab_strip));
  ASSERT_TRUE(second_display.bounds().Contains(target));

  DragTabAndNotify(
      tab_strip,
      base::BindOnce(&DragSingleTabToSeparateWindowInSecondDisplayStep2, this,
                     target));

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());

  // This other browser should be on the second screen (with mouse drag)
  // With the touch input the browser cannot be dragged from one screen
  // to another and the window stays on the first screen.
  if (input_source() == INPUT_SOURCE_MOUSE) {
    EXPECT_EQ(
        ui_test_utils::GetSecondaryDisplay(screen).id(),
        screen
            ->GetDisplayNearestWindow(new_browser->window()->GetNativeWindow())
            .id());
  }

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // Both windows should not be maximized
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_FALSE(new_browser->window()->IsMaximized());
}

namespace {

// Invoked from the nested run loop.
void DragTabToWindowInSeparateDisplayStep2(
    DetachToBrowserTabDragControllerTest* test,
    TabStrip* not_attached_tab_strip,
    TabStrip* target_tab_strip) {
  ASSERT_FALSE(not_attached_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  gfx::Point target_point(
      GetCenterInScreenCoordinates(target_tab_strip->tab_at(0)));

  // Move it closer to the beginning of the tab so it will drop before that tab.
  target_point.Offset(-20, 0);
  ASSERT_TRUE(test->DragInputToAsync(target_point));
}

}  // namespace

// Drags from browser to another browser on a second display and releases input.
IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DragTabToWindowInSeparateDisplay) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateBrowser(browser()->profile());
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  ResetIDs(browser2->tab_strip_model(), 100);

  // Move the second browser to the second display.
  display::Screen* screen = display::Screen::GetScreen();
  Display second_display = ui_test_utils::GetSecondaryDisplay(screen);
  browser2->window()->SetBounds(second_display.work_area());
  EXPECT_EQ(
      second_display.id(),
      screen->GetDisplayNearestWindow(browser2->window()->GetNativeWindow())
          .id());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DragTabToWindowInSeparateDisplayStep2, this,
                                  tab_strip, tab_strip2));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 100", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // Both windows should not be maximized
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DragBrowserWindowWhenMajorityOfBoundsInSecondDisplay) {
  // Set the browser's window bounds such that the majority of its bounds
  // resides in the second display.
  const std::pair<Display, Display> displays =
      GetDisplays(display::Screen::GetScreen());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  {
    // Moves the browser window through dragging so that the majority of its
    // bounds are in the secondary display but it's still be in the primary
    // display. Do not use SetBounds() or related, it may move the browser
    // window to the secondary display in some configurations like Mash.
    int target_x = displays.first.bounds().right() -
                   browser()->window()->GetBounds().width() / 2 + 20;
    const gfx::Point target_point =
        GetCenterInScreenCoordinates(tab_strip->tab_at(0)) +
        gfx::Vector2d(target_x - browser()->window()->GetBounds().x(),
                      GetDetachY(tab_strip));
    DragTabAndNotify(
        tab_strip,
        base::BindOnce(&DragSingleTabToSeparateWindowInSecondDisplayStep2, this,
                       target_point));
    StopAnimating(tab_strip);
  }
  EXPECT_EQ(displays.first.id(),
            browser()->window()->GetNativeWindow()->GetHost()->GetDisplayId());

  // Start dragging the window by the tab strip, and move it only to the edge
  // of the first display. Expect at that point mouse would warp and the window
  // will therefore reside in the second display when mouse is released.
  const gfx::Point tab_0_center =
      GetCenterInScreenCoordinates(tab_strip->tab_at(0));
  const int offset_x = tab_0_center.x() - browser()->window()->GetBounds().x();
  const int detach_y = tab_0_center.y() + GetDetachY(tab_strip);
  const int first_display_warp_edge_x = displays.first.bounds().right() - 1;
  const gfx::Point warped_point(displays.second.bounds().x() + 1, detach_y);

  DragTabAndNotify(
      tab_strip, base::BindLambdaForTesting([&]() {
        // This makes another event on the warped location because the test
        // system does not create it automatically as the result of pointer
        // warp.
        ASSERT_TRUE(DragInputToNotifyWhenDone(
            gfx::Point(first_display_warp_edge_x, detach_y),
            base::BindOnce(&DragSingleTabToSeparateWindowInSecondDisplayStep2,
                           this, warped_point)));
      }));

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should only be a single browser.
  ASSERT_EQ(1u, browser_list->size());
  ASSERT_EQ(browser(), browser_list->get(0));
  ASSERT_TRUE(browser()->window()->IsActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());

  // Browser now resides in display 2.
  EXPECT_EQ(warped_point.x() - offset_x, browser()->window()->GetBounds().x());
  EXPECT_EQ(displays.second.id(),
            browser()->window()->GetNativeWindow()->GetHost()->GetDisplayId());
}

// Drags from browser to another browser on a second display and releases input.
IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DragTabToWindowOnSecondDisplay) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateBrowser(browser()->profile());
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  ResetIDs(browser2->tab_strip_model(), 100);

  // Move both browsers to be side by side on the second display.
  display::Screen* screen = display::Screen::GetScreen();
  Display second_display = ui_test_utils::GetSecondaryDisplay(screen);
  gfx::Rect work_area = second_display.work_area();
  work_area.set_width(work_area.width() / 2);
  browser()->window()->SetBounds(work_area);
  // It's possible the window will not fit in half the screen, in which case we
  // will position the windows as well as we can.
  work_area.set_x(browser()->window()->GetBounds().right());
  // Sanity check: second browser should still be on the second display.
  ASSERT_LT(work_area.x(), second_display.work_area().right());
  browser2->window()->SetBounds(work_area);
  EXPECT_EQ(
      second_display.id(),
      screen->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .id());
  EXPECT_EQ(
      second_display.id(),
      screen->GetDisplayNearestWindow(browser2->window()->GetNativeWindow())
          .id());

  // Sanity check: make sure the target position is also within in the screen
  // bounds:
  ASSERT_LT(GetCenterInScreenCoordinates(tab_strip2->tab_at(0)).x(),
            second_display.work_area().right());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DragTabToWindowInSeparateDisplayStep2, this,
                                  tab_strip, tab_strip2));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 100", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // Both windows should not be maximized
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

// Drags from a maximized browser to another non-maximized browser on a second
// display and releases input.
IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DragMaxTabToNonMaxWindowInSeparateDisplay) {
  AddTabsAndResetBrowser(browser(), 1);
  browser()->window()->Maximize();
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser on the second display.
  display::Screen* screen = display::Screen::GetScreen();
  ASSERT_EQ(2, screen->GetNumDisplays());
  const std::pair<Display, Display> displays = GetDisplays(screen);
  gfx::Rect work_area = displays.second.work_area();
  work_area.Inset(20, 20, 20, 60);
  Browser::CreateParams params(browser()->profile(), true);
  params.initial_show_state = ui::SHOW_STATE_NORMAL;
  params.initial_bounds = work_area;
  Browser* browser2 = new Browser(params);
  AddBlankTabAndShow(browser2);

  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  ResetIDs(browser2->tab_strip_model(), 100);

  EXPECT_EQ(
      displays.second.id(),
      screen->GetDisplayNearestWindow(browser2->window()->GetNativeWindow())
          .id());
  EXPECT_EQ(
      displays.first.id(),
      screen->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .id());
  EXPECT_EQ(2, tab_strip->tab_count());
  EXPECT_EQ(1, tab_strip2->tab_count());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DragTabToWindowInSeparateDisplayStep2, this,
                                  tab_strip, tab_strip2));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());

  // tab should have moved
  EXPECT_EQ(1, tab_strip->tab_count());
  EXPECT_EQ(2, tab_strip2->tab_count());

  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 100", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // Source browser should still be maximized, target should not
  EXPECT_TRUE(browser()->window()->IsMaximized());
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

// Drags from a restored browser to an immersive fullscreen browser on a
// second display and releases input.
// TODO(pkasting) https://crbug.com/910782 Hangs.
IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DISABLED_DragTabToImmersiveBrowserOnSeparateDisplay) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateBrowser(browser()->profile());
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  ResetIDs(browser2->tab_strip_model(), 100);

  // Move the second browser to the second display.
  display::Screen* screen = display::Screen::GetScreen();
  const std::pair<Display, Display> displays = GetDisplays(screen);
  browser2->window()->SetBounds(displays.second.work_area());
  EXPECT_EQ(
      displays.second.id(),
      screen->GetDisplayNearestWindow(browser2->window()->GetNativeWindow())
          .id());

  // Put the second browser into immersive fullscreen.
  BrowserView* browser_view2 = BrowserView::GetBrowserViewForBrowser(browser2);
  ImmersiveModeController* immersive_controller2 =
      browser_view2->immersive_mode_controller();
  ash::ImmersiveFullscreenControllerTestApi(
      static_cast<ImmersiveModeControllerAsh*>(immersive_controller2)
          ->controller())
      .SetupForTest();
  chrome::ToggleFullscreenMode(browser2);
  // For MD, the browser's top chrome is completely offscreen, with tabstrip
  // visible.
  ASSERT_TRUE(immersive_controller2->IsEnabled());
  ASSERT_FALSE(immersive_controller2->IsRevealed());
  ASSERT_TRUE(tab_strip2->GetVisible());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DragTabToWindowInSeparateDisplayStep2, this,
                                  tab_strip, tab_strip2));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // browser2's top chrome should be revealed and the tab strip should be
  // at normal height while user is tragging tabs_strip2's tabs.
  ASSERT_TRUE(immersive_controller2->IsRevealed());
  ASSERT_TRUE(tab_strip2->GetVisible());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 100", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // Move the mouse off of browser2's top chrome.
  ASSERT_TRUE(
      ui_test_utils::SendMouseMoveSync(displays.first.bounds().CenterPoint()));

  // The first browser window should not be in immersive fullscreen.
  // browser2 should still be in immersive fullscreen, but the top chrome should
  // no longer be revealed.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());

  EXPECT_TRUE(immersive_controller2->IsEnabled());
  EXPECT_FALSE(immersive_controller2->IsRevealed());
  EXPECT_TRUE(tab_strip2->GetVisible());
}

// Subclass of DetachToBrowserTabDragControllerTest that
// creates multiple displays with different device scale factors.
class DifferentDeviceScaleFactorDisplayTabDragControllerTest
    : public DetachToBrowserTabDragControllerTest {
 public:
  DifferentDeviceScaleFactorDisplayTabDragControllerTest() {}
  virtual ~DifferentDeviceScaleFactorDisplayTabDragControllerTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DetachToBrowserTabDragControllerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("ash-host-window-bounds",
                                    "800x600,800+0-800x600*2");
  }

  float GetCursorDeviceScaleFactor() const {
    return aura::client::GetCursorClient(
               browser()->window()->GetNativeWindow()->GetRootWindow())
        ->GetCursor()
        .device_scale_factor();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      DifferentDeviceScaleFactorDisplayTabDragControllerTest);
};

namespace {

// The points where a tab is dragged in CursorDeviceScaleFactorStep.
constexpr gfx::Point kDragPoints[] = {
    {300, 200}, {399, 200}, {500, 200}, {400, 200}, {300, 200},
};

// The expected device scale factors after the cursor is moved to the
// corresponding kDragPoints in CursorDeviceScaleFactorStep.
constexpr float kDeviceScaleFactorExpectations[] = {
    1.0f, 1.0f, 2.0f, 2.0f, 1.0f,
};

static_assert(
    base::size(kDragPoints) == base::size(kDeviceScaleFactorExpectations),
    "kDragPoints and kDeviceScaleFactorExpectations must have the same "
    "number of elements");

// Drags tab to |kDragPoints[index]|, then calls the next step function.
void CursorDeviceScaleFactorStep(
    DifferentDeviceScaleFactorDisplayTabDragControllerTest* test,
    TabStrip* not_attached_tab_strip,
    size_t index) {
  SCOPED_TRACE(index);
  ASSERT_FALSE(not_attached_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  if (index > 0) {
    EXPECT_EQ(kDragPoints[index - 1],
              aura::Env::GetInstance()->last_mouse_location());
    EXPECT_EQ(kDeviceScaleFactorExpectations[index - 1],
              test->GetCursorDeviceScaleFactor());
  }

  if (index < base::size(kDragPoints)) {
    ASSERT_TRUE(test->DragInputToNotifyWhenDone(
        kDragPoints[index], base::BindOnce(&CursorDeviceScaleFactorStep, test,
                                           not_attached_tab_strip, index + 1)));
  } else {
    // Finishes a series of CursorDeviceScaleFactorStep calls and ends drag.
    ASSERT_TRUE(
        ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::UP));
  }
}

}  // namespace

// Verifies cursor's device scale factor is updated when a tab is moved across
// displays with different device scale factors (http://crbug.com/154183).
// TODO(pkasting): In interactive_ui_tests, scale factor never changes to 2.
// https://crbug.com/918731
// TODO(pkasting): In non_single_process_mash_interactive_ui_tests, pointer is
// warped during the drag (which results in changing to scale factor 2 early),
// and scale factor doesn't change back to 1 at the end.
// https://crbug.com/918732
IN_PROC_BROWSER_TEST_P(DifferentDeviceScaleFactorDisplayTabDragControllerTest,
                       DISABLED_CursorDeviceScaleFactor) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move the second browser to the second display.
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

  // Move to the first tab and drag it enough so that it detaches.
  DragTabAndNotify(tab_strip, base::BindOnce(&CursorDeviceScaleFactorStep, this,
                                             tab_strip, 0));
}

class DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest
    : public DetachToBrowserTabDragControllerTest {
 public:
  DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DetachToBrowserTabDragControllerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("ash-host-window-bounds",
                                    "0+0-800x600,800+0-800x600");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest);
};

namespace {

// Invoked from the nested run loop.
void CancelDragTabToWindowInSeparateDisplayStep3(
    TabStrip* tab_strip,
    const BrowserList* browser_list) {
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  // Switching display mode should cancel the drag operation.
  ash::ShellTestApi().AddRemoveDisplay();
}

// Invoked from the nested run loop.
void CancelDragTabToWindowInSeparateDisplayStep2(
    DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest* test,
    TabStrip* tab_strip,
    Display current_display,
    gfx::Point final_destination,
    const BrowserList* browser_list) {
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  Browser* new_browser = browser_list->get(1);
  EXPECT_EQ(
      current_display.id(),
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(new_browser->window()->GetNativeWindow())
          .id());

  ASSERT_TRUE(test->DragInputToNotifyWhenDone(
      final_destination,
      base::BindOnce(&CancelDragTabToWindowInSeparateDisplayStep3, tab_strip,
                     browser_list)));
}

}  // namespace

// Drags from browser to a second display and releases input.
IN_PROC_BROWSER_TEST_P(
    DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest,
    CancelDragTabToWindowIn2ndDisplay) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Move the second browser to the second display.
  const std::pair<Display, Display> displays =
      GetDisplays(display::Screen::GetScreen());
  gfx::Point final_destination = displays.second.work_area().CenterPoint();

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough to move to another display.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&CancelDragTabToWindowInSeparateDisplayStep2,
                                  this, tab_strip, displays.first,
                                  final_destination, browser_list));

  ASSERT_EQ(1u, browser_list->size());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Release the mouse
  ASSERT_TRUE(
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::UP));
}

// Drags from browser from a second display to primary and releases input.
IN_PROC_BROWSER_TEST_P(
    DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest,
    CancelDragTabToWindowIn1stDisplay) {
  display::Screen* screen = display::Screen::GetScreen();
  const std::pair<Display, Display> displays = GetDisplays(screen);

  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));
  EXPECT_EQ(
      displays.first.id(),
      screen->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .id());

  browser()->window()->SetBounds(displays.second.work_area());
  EXPECT_EQ(
      displays.second.id(),
      screen->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .id());

  // Move the second browser to the display.
  gfx::Point final_destination = displays.first.work_area().CenterPoint();

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough to move to another display.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&CancelDragTabToWindowInSeparateDisplayStep2,
                                  this, tab_strip, displays.second,
                                  final_destination, browser_list));

  ASSERT_EQ(1u, browser_list->size());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Release the mouse
  ASSERT_TRUE(
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::UP));
}

// Subclass of DetachToBrowserTabDragControllerTest that runs tests only with
// touch input.
class DetachToBrowserTabDragControllerTestTouch
    : public DetachToBrowserTabDragControllerTest {
 public:
  DetachToBrowserTabDragControllerTestTouch() {}
  virtual ~DetachToBrowserTabDragControllerTestTouch() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DetachToBrowserTabDragControllerTestTouch);
};

namespace {
void PressSecondFingerWhileDetachedStep3(
    DetachToBrowserTabDragControllerTest* test) {
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, test->browser_list->size());
  ASSERT_TRUE(test->browser_list->get(1)->window()->IsActive());

  ASSERT_TRUE(test->ReleaseInput());
  ASSERT_TRUE(test->ReleaseInput(1));
}

void PressSecondFingerWhileDetachedStep2(
    DetachToBrowserTabDragControllerTest* test,
    const gfx::Point& target_point) {
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, test->browser_list->size());
  ASSERT_TRUE(test->browser_list->get(1)->window()->IsActive());

  // Continue dragging after adding a second finger.
  ASSERT_TRUE(test->PressInput(gfx::Point(), 1));
  ASSERT_TRUE(test->DragInputToNotifyWhenDone(
      target_point,
      base::BindOnce(&PressSecondFingerWhileDetachedStep3, test)));
}

}  // namespace

// Detaches a tab and while detached presses a second finger.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTestTouch,
                       PressSecondFingerWhileDetached) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Move to the first tab and drag it enough so that it detaches. Drag it
  // slightly more horizontally so that it does not generate a swipe down
  // gesture that minimizes the detached browser window.
  const int touch_move_delta = GetDetachY(tab_strip);
  const gfx::Point target = GetCenterInScreenCoordinates(tab_strip->tab_at(0)) +
                            gfx::Vector2d(0, 2 * touch_move_delta);
  DragTabAndNotify(
      tab_strip,
      base::BindOnce(&PressSecondFingerWhileDetachedStep2, this, target), 0,
      touch_move_delta + 5);

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
}

namespace {

void SecondFingerPressTestStep3(DetachToBrowserTabDragControllerTest* test) {
  ASSERT_TRUE(test->ReleaseInput());
}

void SecondFingerPressTestStep2(DetachToBrowserTabDragControllerTest* test,
                                TabStrip* not_attached_tab_strip,
                                TabStrip* target_tab_strip) {
  ASSERT_TRUE(TabDragController::IsActive());

  // And there should be three browser windows, including the newly created one
  // for the dragged tab.
  EXPECT_EQ(3u, test->browser_list->size());

  // Put the window that accociated with |target_tab_strip| in overview.
  test::GetWindowForTabStrip(target_tab_strip)
      ->SetProperty(ash::kIsShowingInOverviewKey, true);

  // Drag to |target_tab_strip|.
  const gfx::Point target_point =
      GetCenterInScreenCoordinates(target_tab_strip);
  ASSERT_TRUE(test->DragInputTo(target_point));

  // Now add a second finger to tap on it.
  aura::Env::GetInstance()->set_touch_down(true);
  ASSERT_TRUE(test->PressInput(gfx::Point(), 1));
  ASSERT_TRUE(test->ReleaseInput(1));

  ASSERT_TRUE(test->DragInputToNotifyWhenDone(
      target_point, base::BindOnce(&SecondFingerPressTestStep3, test)));
}

}  // namespace

// Tests that when drgging a tab to a browser window that's currently in
// overview, press the second finger should not cause chrome crash.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTestTouch,
                       SecondFingerPressTest) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  AddTabsAndResetBrowser(browser(), 1);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip, base::BindOnce(&SecondFingerPressTestStep2, this,
                                             tab_strip, tab_strip2));

  // Test that after dragging there is no crash and the dragged tab should now
  // be merged into the target tabstrip.
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ(2u, browser_list->size());
}

IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTestTouch,
                       LeftSnapShouldntCauseMergeAtEnd) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  AddTabsAndResetBrowser(browser(), 1);

  // Set the last mouse location at the center of tab 0. This shouldn't affect
  // the touch behavior below. See https://crbug.com/914527#c1 for the details
  // of how this can affect the result.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  base::RunLoop run_loop;
  ui_controls::SendMouseMoveNotifyWhenDone(tab_0_center.x(), tab_0_center.y(),
                                           run_loop.QuitClosure());
  run_loop.Run();

  // Drag the tab 1 to left-snapping.
  DragTabAndNotify(
      tab_strip, base::BindLambdaForTesting([&]() {
        const gfx::Rect display_bounds =
            display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
        const gfx::Point target(display_bounds.x(),
                                display_bounds.CenterPoint().y());
        ASSERT_TRUE(
            DragInputToNotifyWhenDone(target, base::BindLambdaForTesting([&]() {
                                        ASSERT_TRUE(ReleaseInput());
                                      })));
      }),
      1);

  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ(2u, browser_list->size());
}

IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTestTouch,
                       FlingDownAtEndOfDrag) {
  // Reduce the minimum fling velocity for this specific test case to cause the
  // fling-down gesture in the middle of tab-dragging. This should end up with
  // minimizing the window. See https://crbug.com/902897 for the details.
  ui::GestureConfiguration::GetInstance()->set_min_fling_velocity(1);

  test::QuitDraggingObserver observer;
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  const gfx::Point tab_0_center =
      GetCenterInScreenCoordinates(tab_strip->tab_at(0));
  const gfx::Vector2d detach(0, GetDetachY(tab_strip));
  base::SimpleTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks::Now());
  ui::SetEventTickClockForTesting(&clock);
  ASSERT_TRUE(PressInput(tab_0_center));
  clock.Advance(base::TimeDelta::FromMilliseconds(5));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center + detach, base::BindLambdaForTesting([&]() {
        // Drag down again; this should cause a fling-down event.
        clock.Advance(base::TimeDelta::FromMilliseconds(5));
        ASSERT_TRUE(DragInputToNotifyWhenDone(
            tab_0_center + detach + detach, base::BindLambdaForTesting([&]() {
              clock.Advance(base::TimeDelta::FromMilliseconds(5));
              ASSERT_TRUE(ReleaseInput());
            })));
      })));
  observer.Wait();

  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_TRUE(browser()->window()->IsMinimized());
  EXPECT_FALSE(browser()->window()->IsVisible());
  ui::SetEventTickClockForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTestTouch,
                       FlingOnStartingDrag) {
  ui::GestureConfiguration::GetInstance()->set_min_fling_velocity(1);
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  const gfx::Point tab_0_center =
      GetCenterInScreenCoordinates(tab_strip->tab_at(0));
  const gfx::Vector2d detach(0, GetDetachY(tab_strip));

  // Sends events to the server without waiting for its reply, which will cause
  // extra touch events before PerformWindowMove starts handling events.
  test::QuitDraggingObserver observer;
  base::SimpleTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks::Now());
  ui::SetEventTickClockForTesting(&clock);
  ASSERT_TRUE(PressInput(tab_0_center));
  clock.Advance(base::TimeDelta::FromMilliseconds(5));
  ASSERT_TRUE(DragInputToAsync(tab_0_center + detach));
  clock.Advance(base::TimeDelta::FromMilliseconds(5));
  ASSERT_TRUE(DragInputToAsync(tab_0_center + detach + detach));
  clock.Advance(base::TimeDelta::FromMilliseconds(2));
  ASSERT_TRUE(ReleaseInput());
  observer.Wait();

  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ(2u, browser_list->size());
  ui::SetEventTickClockForTesting(nullptr);
}

#endif  // OS_CHROMEOS

#if defined(OS_CHROMEOS)
INSTANTIATE_TEST_SUITE_P(TabDragging,
                         DetachToBrowserTabDragControllerTest,
                         ::testing::Values("mouse", "touch"));
INSTANTIATE_TEST_SUITE_P(TabDragging,
                         DetachToBrowserInSeparateDisplayTabDragControllerTest,
                         ::testing::Values("mouse"));
INSTANTIATE_TEST_SUITE_P(TabDragging,
                         DifferentDeviceScaleFactorDisplayTabDragControllerTest,
                         ::testing::Values("mouse"));
INSTANTIATE_TEST_SUITE_P(
    TabDragging,
    DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest,
    ::testing::Values("mouse"));
INSTANTIATE_TEST_SUITE_P(TabDragging,
                         DetachToBrowserTabDragControllerTestTouch,
                         ::testing::Values("touch"));
#else
INSTANTIATE_TEST_SUITE_P(TabDragging,
                         DetachToBrowserTabDragControllerTest,
                         ::testing::Values("mouse"));
#endif
