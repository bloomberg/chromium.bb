// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_controller.h"

#include "base/run_loop.h"
#include "ui/aura/window.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/views/test/views_test_base.h"
#include "ui/wm/public/dispatcher_client.h"

#if defined(OS_WIN)
#include "base/message_loop/message_pump_dispatcher.h"
#elif defined(USE_X11)
#include <X11/Xlib.h>
#undef Bool
#undef None
#endif

namespace views {

namespace {

class TestPlatformEventSource : public ui::PlatformEventSource {
 public:
  TestPlatformEventSource() {}
  virtual ~TestPlatformEventSource() {}

  uint32_t Dispatch(const ui::PlatformEvent& event) {
    return DispatchEvent(event);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPlatformEventSource);
};

class TestDispatcherClient : public aura::client::DispatcherClient {
 public:
  TestDispatcherClient() : dispatcher_(NULL) {}
  virtual ~TestDispatcherClient() {}

  base::MessagePumpDispatcher* dispatcher() {
    return dispatcher_;
  }

  // aura::client::DispatcherClient:
  virtual void RunWithDispatcher(
      base::MessagePumpDispatcher* dispatcher) OVERRIDE {
    base::AutoReset<base::MessagePumpDispatcher*> reset_dispatcher(&dispatcher_,
                                                                   dispatcher);
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    base::RunLoop run_loop;
    quit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  virtual void QuitNestedMessageLoop() OVERRIDE { quit_callback_.Run(); }

 private:
  base::MessagePumpDispatcher* dispatcher_;
  base::Closure quit_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestDispatcherClient);
};

}  // namespace

class MenuControllerTest : public ViewsTestBase {
 public:
  MenuControllerTest() : controller_(NULL) {}
  virtual ~MenuControllerTest() {}

  // Dispatches |count| number of items, each in a separate iteration of the
  // message-loop, by posting a task.
  void Step3_DispatchEvents(int count) {
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    controller_->exit_type_ = MenuController::EXIT_ALL;

#if defined(USE_X11)
    XEvent xevent;
    memset(&xevent, 0, sizeof(xevent));
    event_source_.Dispatch(&xevent);
#else
    MSG msg;
    memset(&msg, 0, sizeof(MSG));
    dispatcher_client_.dispatcher()->Dispatch(msg);
#endif

    if (count) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&MenuControllerTest::Step3_DispatchEvents,
                     base::Unretained(this),
                     count - 1));
    } else {
      EXPECT_TRUE(run_loop_->running());
      run_loop_->Quit();
    }
  }

  // Runs a nested message-loop that does not involve the menu itself.
  void Step2_RunNestedLoop() {
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MenuControllerTest::Step3_DispatchEvents,
                   base::Unretained(this),
                   3));
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  void Step1_RunMenu() {
    Widget widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget.Init(params);
    widget.Show();

    aura::client::SetDispatcherClient(widget.GetNativeWindow()->GetRootWindow(),
                                      &dispatcher_client_);

    controller_ = new MenuController(NULL, true, NULL);
    controller_->owner_ = &widget;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MenuControllerTest::Step2_RunNestedLoop,
                   base::Unretained(this)));
    controller_->RunMessageLoop(false);
  }

 private:
  MenuController* controller_;
  scoped_ptr<base::RunLoop> run_loop_;
  TestPlatformEventSource event_source_;
  TestDispatcherClient dispatcher_client_;

  DISALLOW_COPY_AND_ASSIGN(MenuControllerTest);
};

TEST_F(MenuControllerTest, Basic) {
  base::MessageLoop::ScopedNestableTaskAllower allow_nested(
      base::MessageLoop::current());
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MenuControllerTest::Step1_RunMenu, base::Unretained(this)));
}

}  // namespace views
