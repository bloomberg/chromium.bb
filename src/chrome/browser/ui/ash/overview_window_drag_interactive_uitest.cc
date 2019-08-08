// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/window_state_type.mojom.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/ash/ash_test_util.h"
#include "chrome/browser/ui/ash/tablet_mode_client_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/perf/performance_test.h"
#include "content/public/browser/notification_service.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/tween.h"

namespace {

// The frame duration for 60fps.
constexpr base::TimeDelta kFrameDuration =
    base::TimeDelta::FromMicroseconds(16666);

// TODO(oshima): Move this to utility lib.
// Producer produces the point for given progress value.
class PointProducer {
 public:
  virtual ~PointProducer() = default;
  virtual gfx::Point GetPosition(float progress) = 0;
};

// InterporateProducer produces the interpolated location between two points
// based on tween type.
class InterporateProducer : public PointProducer {
 public:
  InterporateProducer(const gfx::Point& start,
                      const gfx::Point& end,
                      gfx::Tween::Type type = gfx::Tween::LINEAR)
      : start_(start), end_(end), type_(type) {}
  ~InterporateProducer() override = default;

  // PointProducer:
  gfx::Point GetPosition(float progress) override {
    float value = gfx::Tween::CalculateValue(type_, progress);
    return gfx::Point(
        gfx::Tween::LinearIntValueBetween(value, start_.x(), end_.x()),
        gfx::Tween::LinearIntValueBetween(value, start_.y(), end_.y()));
  }

 private:
  gfx::Point start_, end_;
  gfx::Tween::Type type_;

  DISALLOW_COPY_AND_ASSIGN(InterporateProducer);
};

// A utility class that generates drag events using |producer| logic.
class DragEventGenerator {
 public:
  DragEventGenerator(std::unique_ptr<PointProducer> producer,
                     const base::TimeDelta duration,
                     bool touch = true)
      : producer_(std::move(producer)),
        start_(base::TimeTicks::Now()),
        expected_next_time_(start_ + kFrameDuration),
        duration_(duration),
        touch_(touch) {
    gfx::Point initial_position = producer_->GetPosition(0.f);
    if (touch_) {
      ui_controls::SendTouchEvents(ui_controls::PRESS, 0, initial_position.x(),
                                   initial_position.y());
    } else {
      ui_controls::SendMouseMove(initial_position.x(), initial_position.y());
      ui_controls::SendMouseEvents(ui_controls::LEFT, ui_controls::DOWN);
    }
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DragEventGenerator::GenerateNext,
                       base::Unretained(this)),
        kFrameDuration);
  }

  ~DragEventGenerator() {
    VLOG(1) << "Effective Event per seconds="
            << (count_ * 1000) / duration_.InMilliseconds();
  }

  void GenerateNext() {
    auto now = base::TimeTicks::Now();
    auto elapsed = now - start_;

    expected_next_time_ += kFrameDuration;
    count_++;

    if (elapsed >= duration_) {
      gfx::Point position = producer_->GetPosition(1.0f);
      if (touch_) {
        ui_controls::SendTouchEventsNotifyWhenDone(
            ui_controls::MOVE, 0, position.x(), position.y(),
            base::BindOnce(&DragEventGenerator::Done, base::Unretained(this),
                           position));
      } else {
        ui_controls::SendMouseMoveNotifyWhenDone(
            position.x(), position.y(),
            base::BindOnce(&DragEventGenerator::Done, base::Unretained(this),
                           position));
      }
      return;
    }

    float progress = static_cast<float>(elapsed.InMilliseconds()) /
                     duration_.InMilliseconds();
    gfx::Point position = producer_->GetPosition(progress);
    if (touch_) {
      ui_controls::SendTouchEvents(ui_controls::MOVE, 0, position.x(),
                                   position.y());
    } else {
      ui_controls::SendMouseMove(position.x(), position.y());
    }

    auto delta = expected_next_time_ - now;
    if (delta.InMilliseconds() > 0) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&DragEventGenerator::GenerateNext,
                         base::Unretained(this)),
          delta);
    } else {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&DragEventGenerator::GenerateNext,
                                    base::Unretained(this)));
    }
  }

  void Done(const gfx::Point position) {
    if (touch_) {
      ui_controls::SendTouchEventsNotifyWhenDone(ui_controls::RELEASE, 0,
                                                 position.x(), position.y(),
                                                 run_loop_.QuitClosure());
    } else {
      ui_controls::SendMouseEventsNotifyWhenDone(
          ui_controls::LEFT, ui_controls::UP, run_loop_.QuitClosure());
    }
  }

  void Wait() { run_loop_.Run(); }

 private:
  std::unique_ptr<PointProducer> producer_;
  int count_ = 0;

  base::TimeTicks start_;
  base::TimeTicks expected_next_time_;
  base::TimeDelta duration_;
  bool touch_;

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(DragEventGenerator);
};

// Wait until the window's state changed to left snapped.
// The window should stay alive, so no need to observer destroying.
class LeftSnapWaiter : public aura::WindowObserver {
 public:
  explicit LeftSnapWaiter(aura::Window* window) : window_(window) {
    window->AddObserver(this);
  }
  ~LeftSnapWaiter() override { window_->RemoveObserver(this); }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key == ash::kWindowStateTypeKey && IsLeftSnapped())
      run_loop_.Quit();
  }

  void Wait() {
    if (!IsLeftSnapped())
      run_loop_.Run();
  }

  bool IsLeftSnapped() {
    return window_->GetProperty(ash::kWindowStateTypeKey) ==
           ash::mojom::WindowStateType::LEFT_SNAPPED;
  }

 private:
  aura::Window* window_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(LeftSnapWaiter);
};

}  // namespace

// Test window drag performance in overview mode.
// int: number of windows : 2, 8
// bool: the tab content (about:blank, chrome://newtab)
// bool: touch (true) or mouse(false)
class OverviewWindowDragTest
    : public UIPerformanceTest,
      public testing::WithParamInterface<::testing::tuple<int, bool>> {
 public:
  OverviewWindowDragTest() = default;
  ~OverviewWindowDragTest() override = default;

  // UIPerformanceTest:
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();
    test::SetAndWaitForTabletMode(true);

    int additional_browsers = std::get<0>(GetParam()) - 1;
    bool blank_page = std::get<1>(GetParam());

    GURL ntp_url("chrome://newtab");
    // The default is blank page.
    if (blank_page)
      ui_test_utils::NavigateToURL(browser(), ntp_url);

    for (int i = additional_browsers; i > 0; i--) {
      Browser* new_browser = CreateBrowser(browser()->profile());
      if (blank_page)
        ui_test_utils::NavigateToURL(new_browser, ntp_url);
    }

    float cost_per_browser = blank_page ? 0.5 : 0.1;
    int wait_seconds = (base::SysInfo::IsRunningOnChromeOS() ? 5 : 0) +
                       additional_browsers * cost_per_browser;
    base::RunLoop run_loop;
    base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                          base::TimeDelta::FromSeconds(wait_seconds));
    run_loop.Run();
  }

  // UIPerformanceTest:
  std::vector<std::string> GetUMAHistogramNames() const override {
    return {
        "Ash.Overview.WindowDrag.PresentationTime.TabletMode",
    };
  }

  gfx::Size GetDisplaySize(aura::Window* window) const {
    return display::Screen::GetScreen()->GetDisplayNearestWindow(window).size();
  }

  // Returns the location within the top / left most window.
  gfx::Point GetStartLocation(const gfx::Size& size) const {
    int num_browsers = std::get<0>(GetParam());
    return num_browsers == 2 ? gfx::Point(size.width() / 3, size.height() / 2)
                             : gfx::Point(size.width() / 5, size.height() / 4);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverviewWindowDragTest);
};

IN_PROC_BROWSER_TEST_P(OverviewWindowDragTest, NormalDrag) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ui_controls::SendKeyPress(browser_window, ui::VKEY_MEDIA_LAUNCH_APP1,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /*command=*/false);
  test::WaitForOverviewAnimationState(
      ash::mojom::OverviewAnimationState::kEnterAnimationComplete);
  gfx::Size display_size = GetDisplaySize(browser_window);
  gfx::Point start_point = GetStartLocation(display_size);
  gfx::Point end_point(start_point);
  end_point.set_x(end_point.x() + display_size.width() / 2);
  DragEventGenerator generator(
      std::make_unique<InterporateProducer>(start_point, end_point),
      base::TimeDelta::FromMilliseconds(1000));
  generator.Wait();
}

// The test is flaky because close notification is not the right singal.
// crbug.com/953355
IN_PROC_BROWSER_TEST_P(OverviewWindowDragTest, DISABLED_DragToClose) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ui_controls::SendKeyPress(browser_window, ui::VKEY_MEDIA_LAUNCH_APP1,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /*command=*/false);
  test::WaitForOverviewAnimationState(
      ash::mojom::OverviewAnimationState::kEnterAnimationComplete);

  content::WindowedNotificationObserver waiter(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(chrome::FindLastActive()));

  gfx::Point start_point = GetStartLocation(GetDisplaySize(browser_window));
  gfx::Point end_point(start_point);
  end_point.set_y(0);
  end_point.set_x(end_point.x() + 10);
  DragEventGenerator generator(
      std::make_unique<InterporateProducer>(start_point, end_point),
      base::TimeDelta::FromMilliseconds(500), gfx::Tween::EASE_IN_2);
  generator.Wait();

  // Wait for the window to close.
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_P(OverviewWindowDragTest, DragToSnap) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ui_controls::SendKeyPress(browser_window, ui::VKEY_MEDIA_LAUNCH_APP1,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /*command=*/false);
  test::WaitForOverviewAnimationState(
      ash::mojom::OverviewAnimationState::kEnterAnimationComplete);

  gfx::Point start_point = GetStartLocation(GetDisplaySize(browser_window));
  gfx::Point end_point(start_point);
  end_point.set_x(0);
  DragEventGenerator generator(
      std::make_unique<InterporateProducer>(start_point, end_point),
      base::TimeDelta::FromMilliseconds(1000));
  generator.Wait();

  Browser* active = chrome::FindLastActive();
  LeftSnapWaiter waiter(
      features::IsUsingWindowService()
          ? active->window()->GetNativeWindow()->GetRootWindow()
          : active->window()->GetNativeWindow());

  // Wait for the window to be snapped.
  waiter.Wait();
}

INSTANTIATE_TEST_SUITE_P(,
                         OverviewWindowDragTest,
                         ::testing::Combine(::testing::Values(2, 8),
                                            /*blank=*/testing::Bool()));
