/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_mainmessagepump.h>
#include <blpwtk2_queuepumpscheduler.h>
#include <blpwtk2_timerpumpscheduler.h>
#include <blpwtk2_statics.h>

#include <base/run_loop.h>
#include <base/lazy_instance.h>
#include <base/message_loop/message_loop.h>
#include <base/threading/thread_local.h>
#include <base/win/wrapped_window_proc.h>
#include <base/time/time.h>
#include <third_party/blink/renderer/core/page/bb_window_hooks.h>
#include <v8.h>

namespace blpwtk2 {
namespace {

const int kMsgHaveWork = WM_USER + 1;
const int kPumpMessage = WM_USER + 2;

inline
bool isModalCode(int code)
{
    return MSGF_DIALOGBOX == code
        || 3 == code // Window move
        || 4 == code // Window resize
        || MSGF_MENU == code
        || MSGF_SCROLLBAR == code;
}

base::LazyInstance<base::ThreadLocalPointer<MainMessagePump>>::DestructorAtExit
    g_lazy_tls = LAZY_INSTANCE_INITIALIZER;

}

                        // ---------------------
                        // class MainMessagePump
                        // ---------------------

// static
const wchar_t* MainMessagePump::getClassName()
{
    static const wchar_t* className;

    if (nullptr == className) {
        className = L"blpwtk2::MainMessagePump";

        WNDCLASSW windowClass = {
            0,                  // style
            &windowProcedure,   // lpfnWndProc
            0,                  // cbClsExtra
            0,                  // cbWndExtra
            0,                  // hInstance
            0,                  // hIcon
            0,                  // hCursor
            0,                  // hbrBackground
            0,                  // lpszMenuName
            className           // lpszClassName
        };

        ATOM atom = ::RegisterClassW(&windowClass);
        CHECK(atom);
    }

    return className;
}

// static
LRESULT CALLBACK MainMessagePump::windowProcedure(NativeView window_handle,
                                                  UINT message,
                                                  WPARAM wparam,
                                                  LPARAM lparam)
{
    // Note that 'pump' is only valid under specific conditions.
    MainMessagePump* pump = reinterpret_cast<MainMessagePump*>(wparam);

    if (kPumpMessage == message) {
        pump->doWork();
    }
    else if (WM_TIMER == message) {
        DCHECK(pump->d_isInsideModalLoop);

        // Inside an OS modal loop, we have the ability to install a
        // preHandleMessage as a message filter hook but we don't have the
        // ability to install a postHandleMessage handler.  This prevents us from
        // unintrusively posting a pump message that is directly triggered by
        // MessagePumpForUI.
        //
        // We use a timer to continuously schedule a pump.  It is possible for
        // timer messages to be starved, especially when the renderer's UI message
        // loop and the browser's UI message loop are running in the same thread.
        // To get around this starvation, we conditionally schedule a pump from
        // the window procedure hook.
        DWORD scheduleTime = pump->d_scheduleTime;
        DWORD currentTime = ::GetTickCount();
        if (pump->d_minTimer < currentTime - scheduleTime) {
            pump->d_allowIdleWork = false;
            pump->schedulePumpIfNecessary();
        }
    }
    else {
        return DefWindowProc(window_handle, message, wparam, lparam);
    }

    return S_OK;
}

// static
LRESULT CALLBACK MainMessagePump::messageFilter(int code,
                                                WPARAM wParam,
                                                LPARAM lParam)
{
    MainMessagePump* pump = current();

    if (!pump->d_isInsideModalLoop && isModalCode(code)) {
        DebugWithTime("ENTERING MODAL LOOP\n");
        pump->modalLoop(true);
    }
    return CallNextHookEx(pump->d_messageFilter, code, wParam, lParam);
}

// static
LRESULT CALLBACK MainMessagePump::windowProcedureHook(int code,
                                                      WPARAM wParam,
                                                      LPARAM lParam)
{
    MainMessagePump* pump = current();

    CWPSTRUCT* cwp = (CWPSTRUCT*)lParam;
    switch (cwp->message) {
    case WM_ENTERSIZEMOVE:
        DebugWithTime("HOOK ENTER SIZEMOVE\n");
        pump->modalLoop(true);
        break;
    case WM_EXITSIZEMOVE:
        DebugWithTime("HOOK EXIT SIZEMOVE\n");
        pump->modalLoop(false);
        break;
    case WM_ENTERMENULOOP:
        DebugWithTime("HOOK ENTER MENU\n");
        pump->modalLoop(true);
        break;
    case WM_EXITMENULOOP:
        DebugWithTime("HOOK EXIT MENU\n");
        pump->modalLoop(false);
        break;
    case kMsgHaveWork:
        break;
    default:
        if (pump->d_nestLevel > 0) {
            DebugWithTime("DETECTED INNER PUMP\n");
            pump->modalLoop(true);
        }
    }

    DWORD needRepost = pump->d_needRepost;
    if (needRepost) {
        pump->schedulePumpIfNecessary();
    }
    else if (USER_TIMER_MAXIMUM != pump->d_maxTimer &&
            pump->d_isInsideModalLoop) {
        // Schedule a pump if the timer message was throttled for too long.
        DWORD scheduleTime = pump->d_scheduleTime;
        DWORD currentTime = ::GetTickCount();

        if (0 != scheduleTime &&
            pump->d_maxTimer < currentTime - scheduleTime) {
            pump->d_allowIdleWork = false;
            pump->schedulePumpIfNecessary();
        }
    }

    return CallNextHookEx(pump->d_windowProcedureHook, code, wParam, lParam);
}

void MainMessagePump::schedulePumpIfNecessary()
{
    // Store the atomic<bool> into a local variable. This helps us to see a
    // snapshot of the variable during debugging in case another thread
    // changes the value.
    bool work_scheduled = work_scheduled_;

    if (work_scheduled) {
        schedulePump();
    }
}

void MainMessagePump::schedulePump()
{
    int wasPumped = ::InterlockedExchange(&d_isPumped, 1);
    if (0 != wasPumped) {
        // Already pumped.
        return;
    }

    BOOL ret = ::PostMessageW(d_window,
                              kPumpMessage,
                              reinterpret_cast<WPARAM>(this),
                              0);

    // Keep note on whether the pump message was successfully posted.  If it
    // failed to post, we will try again from the window procedure hook.
    ::InterlockedExchange(&d_needRepost, 0 == ret? 1 : 0);

    if (ret) {
        // Successfully pumped.
        return;
    }

    // Failed to post pump message.  The message queue may be full.  We'll try
    // again later
    ::InterlockedExchange(&d_isPumped, 0);
}

void MainMessagePump::doWork()
{
    DCHECK(d_nestLevel >= 0);
    ++d_nestLevel;

    int wasPumped = ::InterlockedExchange(&d_isPumped, 0);
    DCHECK_EQ(1, wasPumped);

    unsigned int startTime1 = ::GetTickCount();

    // Since MessagePumpForUI::DoRunLoop is not called when MainMessagePump is
    // used, the functions MessagePumpForUI::ProcessNextWindowsMessage,
    // MessagePumpForUI::ProcessMessageHelper, and
    // MessagePumpForUI::ProcessPumpReplacementMessage are also not called.
    // Part of the job for MessagePumpForUI::ProcessMessageHelper was to
    // dispatch the work message to its own window message handler, which then
    // calls into MessageLoop to flush the task queue.  We will do the same thing
    // by sending a synchronous message to the same window message handler.

    unsigned int maxPumpCount;
    bool work_scheduled = false;

    if (!d_isInsideModalLoop) {
        maxPumpCount = 1;
    }
    else {
        maxPumpCount = d_maxPumpCountInsideModalLoop;
    }

    for (unsigned int i=0; i<maxPumpCount; ++i) {
        resetWorkState();
        MessagePumpForUI::HandleWorkMessage();

        // Store the atomic<bool> into a local variable. This helps us to see a
        // snapshot of the variable during debugging in case another thread
        // changes the value.
        work_scheduled = work_scheduled_;
        if (!work_scheduled) {
            // Break out of the loop if no more work is scheduled.
            break;
        }
    }

    if (!work_scheduled && d_allowIdleWork) {
        unsigned int startTime2 = ::GetTickCount();
        MessagePumpForUI::DoIdleWork();
        unsigned int endTime2 = ::GetTickCount();

        if (shouldTrace(endTime2 - startTime2)) {
            LOG(WARNING) << "blpwtk2::MainMessagePump::doWork:  MainMessagePumpForUI::DoIdleWork took "
                         << (endTime2 - startTime2) << " ms to run";
        }
    }
    else {
        d_allowIdleWork = true;
    }

    unsigned int endTime1 = ::GetTickCount();
    if (shouldTrace(endTime1 - startTime1)) {
        LOG(WARNING) << "blpwtk2::MainMessagePump::doWork:  MainMessagePumpForUI::HandleWorkMessage took "
                     << (endTime1 - startTime1) << " ms to run";
    }

    --d_nestLevel;

    if (d_isInsideModalLoop && Statics::isRendererMainThreadMode()) {
        v8::Isolate *isolate = v8::Isolate::GetCurrent();

        if (isolate) {
            int microtasksScopeDepth =
                v8::MicrotasksScope::GetCurrentDepth(isolate);

            if (microtasksScopeDepth) {
                auto policy = isolate->GetMicrotasksPolicy();
                isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
                isolate->RunMicrotasks();
                isolate->SetMicrotasksPolicy(policy);
            }
        }
    }
}

void MainMessagePump::modalLoop(bool enabled)
{
    if (d_isInsideModalLoop != enabled) {
        d_isInsideModalLoop = enabled;

        if (enabled) {
            d_scopedNestedTaskAllower =
                std::make_unique<base::MessageLoopCurrent::ScopedNestableTaskAllower>();

            SetTimer(d_window,
                     reinterpret_cast<WPARAM>(this),
                     d_minTimer,
                     nullptr);
        }
        else {
            d_scopedNestedTaskAllower.release();
            KillTimer(d_window, reinterpret_cast<WPARAM>(this));
        }
    }
}

void MainMessagePump::resetWorkState()
{
    DWORD scheduleTime = ::InterlockedExchange(&d_scheduleTime, 0);
    DCHECK_NE(scheduleTime, 0U);

    // The MessagePumpForUI::HandleWorkMessage function relies on
    // MessagePumpForUI::ProcessPumpReplacementMessage to clear the work_state_
    // flag.  Because MessagePumpForUI::ProcessPumpReplacementMessage function
    // is no longer called, we manually reset the work_state_ flag by calling
    // MessagePumpForUI::ResetWorkState().
    MessagePumpForUI::ResetWorkState();
}

// static
MainMessagePump* MainMessagePump::current()
{
    MainMessagePump* pump = g_lazy_tls.Pointer()->Get();
    DCHECK(pump != nullptr);
    return pump;
}

// CREATORS
MainMessagePump::MainMessagePump()
    : base::MessagePumpForUI()
    , d_window(0)
    , d_isInsideModalLoop(false)
    , d_isInsideMainLoop(0)
    , d_isPumped(0)
    , d_needRepost(0)
    , d_scheduleTime(0)
    , d_allowIdleWork(true)
    , d_windowProcedureHook(0)
    , d_messageFilter(0)
    , d_minTimer(USER_TIMER_MINIMUM)
    , d_maxTimer(USER_TIMER_MAXIMUM)
    , d_maxPumpCountInsideModalLoop(1)
    , d_traceThreshold(0)
    , d_nestLevel(0)
{
    d_window = ::CreateWindowW(getClassName(),   // lpClassName
                               0,                // lpWindowName
                               0,                // dwStyle
                               CW_DEFAULT,       // x
                               CW_DEFAULT,       // y
                               CW_DEFAULT,       // nWidth
                               CW_DEFAULT,       // nHeight
                               HWND_MESSAGE,     // hwndParent
                               0,                // hMenu
                               0,                // hInstance
                               0);               // lpParam

    // Disable processing of non-Chrome message from within MessagePumpForUI.
    // This will allow for accurate measurement of window message processing time
    // by the embedder.
    should_process_pump_replacement_ = false;

    // Timer messages can be easily starved when the browser main thread is
    // flooded with input or posted messages.  We set a maximum allowable time
    // to wait for the timer message to fire before resorting to schedule a pump
    // from within the message filter hook.
    d_maxTimer = 4 * d_minTimer;

    // Even with a reduced maximum wait time before scheduling a pump, the large
    // number of window messages in the main thread can starve our pump message.
    // When the program counter is inside of the modal loop, this starvation can
    // be observed when resizing the window and it manifests itself as a very
    // noticable lag.  To compensate for this starvation, we perform multiple
    // flushes under a very specific condition: the renderer is running inside
    // of the browser main thread and the program counter is inside an OS modal
    // loop.
    d_maxPumpCountInsideModalLoop = 16;

    DCHECK(!g_lazy_tls.Pointer()->Get());
    g_lazy_tls.Pointer()->Set(this);

    // Active a default pump scheduler
    activateScheduler(0);

    // Install hooks into BBWindowHook so we can configure the tunables from
    // JavaScript.

    blink::BBWindowHooks::PumpConfigHooks hooks;
    hooks.listSchedulers = base::BindRepeating(&MainMessagePump::listSchedulers, base::Unretained(this));
    hooks.listSchedulerTunables = base::BindRepeating(&MainMessagePump::listSchedulerTunables, base::Unretained(this));
    hooks.activateScheduler = base::BindRepeating(&MainMessagePump::activateScheduler, base::Unretained(this));
    hooks.setSchedulerTunable = base::BindRepeating(&MainMessagePump::setSchedulerTunable, base::Unretained(this));

    blink::BBWindowHooks::InstallPumpConfigHooks(hooks);
}

MainMessagePump::~MainMessagePump()
{
    ::DestroyWindow(d_window);
    g_lazy_tls.Pointer()->Set(nullptr);
}

void MainMessagePump::init()
{
    // Setup some Windows hooks.  These hooks are used to detect when we enter
    // a modal loop.
    d_messageFilter = SetWindowsHookEx(WH_MSGFILTER,
                                       messageFilter,
                                       0,
                                       GetCurrentThreadId());

    d_windowProcedureHook = SetWindowsHookEx(WH_CALLWNDPROC,
                                             windowProcedureHook,
                                             0,
                                             GetCurrentThreadId());

    d_runLoop.reset(new base::RunLoop());
    d_runLoop->BeforeRun();
    base::MessagePump::Delegate* pump_delegate = d_runLoop->GetPumpDelegate();
    if(!pump_delegate) {
        LOG(ERROR) << "Could not get message pump delegate";
        return;
    }
    PushRunState(&d_runState, pump_delegate);
}

void MainMessagePump::flush()
{
    // We repeatedly flush the event loop up to 255 times.  We set an
    // upper bound on the number of times the flush occurs because it's
    // possible for a task to recursively schedule another task on the
    // same thread.  If this pattern repeats indefinitely, the flush
    // operation would never end and we would be stuck in an infinite
    // loop.

    for (int i=0; i<255; ++i) {
        // Store the atomic<bool> into a local variable. This helps us to see a
        // snapshot of the variable during debugging in case another thread
        // changes the value.
        bool work_scheduled = work_scheduled_;
        if (work_scheduled) {
            // This call to schedulePump() is not strictly required but it
            // helps to keep the data members in the expected state.   The
            // side effect of calling schedulePump() is the setting of the
            // d_isPumped flag to 1.  doWork() throws an assertion if this
            // flag is not set to 1.
            schedulePump();

            doWork();
        }
        else {
            break;
        }
    }
}

void MainMessagePump::cleanup()
{
    flush();

    PopRunState();
    d_runLoop->AfterRun();
    d_runLoop.reset();

    UnhookWindowsHookEx(d_windowProcedureHook);
    UnhookWindowsHookEx(d_messageFilter);
}

bool MainMessagePump::preHandleMessage(const MSG& msg)
{
    DCHECK(d_runLoop);

    // Keep note on when the program counter is between a preHandleMessage and
    // postHandleMessage.  We use this information in ScheduleWork() to determine
    // if we should schedule the pump right away or wait for postHandleMessage
    // to do it.
    int wasInsideMainLoop = ::InterlockedExchange(&d_isInsideMainLoop, 1);
    DCHECK_EQ(0, wasInsideMainLoop);

    return (!!CallMsgFilter(const_cast<MSG*>(&msg), base::kMessageFilterCode));
}

void MainMessagePump::postHandleMessage(const MSG& msg)
{
    DCHECK(d_runLoop);

    int wasInsideMainLoop = ::InterlockedExchange(&d_isInsideMainLoop, 0);
    DCHECK_EQ(1, wasInsideMainLoop);

    // There is no Windows hook that notifies us when exiting a modal dialog
    // loop.  However, when postHandleMessage is called, we can assume that we
    // are back in the application's main loop, so turn off the modal loop flag
    // if it was set.
    if (d_isInsideModalLoop) {
        DebugWithTime("EXITING MODAL LOOP\n");
        modalLoop(false);
    }

    bool work_scheduled = work_scheduled_;
    LONG wasPumped = d_isPumped;

    if (work_scheduled && 0 == wasPumped) {
        bool allowNormalWork = false;
        d_scheduler->isReadyToWork(&allowNormalWork, &d_allowIdleWork);

        // Normal work must be allowed if idle work is allowed.  The scheduler
        // is at fault if it ever allows idle work but not normal work.
        DCHECK(!d_allowIdleWork || allowNormalWork);

        if (allowNormalWork) {
            schedulePump();
        }
    }
}

void MainMessagePump::setTraceThreshold(unsigned int timeoutMS)
{
    d_traceThreshold = timeoutMS;
    LOG(INFO) << "blpwtk2::MainMessagePump::setTraceThreshold: Set traceThreshold to "
              << timeoutMS
              << " ms";
}

std::vector<std::string> MainMessagePump::listSchedulers()
{
    std::vector<std::string> list;

    list.push_back(QueuePumpScheduler::name());
    list.push_back(TimerPumpScheduler::name());

    return list;
}

std::vector<std::string> MainMessagePump::listSchedulerTunables()
{
    DCHECK(d_scheduler);
    return d_scheduler->listTunables();
}

int MainMessagePump::activateScheduler(int index)
{
    int status = 0;

    switch (index) {
    case 0:
      d_scheduler = std::make_unique<QueuePumpScheduler>();
      break;

    case 1:
      d_scheduler = std::make_unique<TimerPumpScheduler>();
      break;

    default:
      LOG(ERROR) << "blpwtk2::MainMessagePump::activateScheduler: index out of bound: " << index;
      status = -1;
      break;
    }

    return status;
}

int MainMessagePump::setSchedulerTunable(unsigned index, int value)
{
    DCHECK(d_scheduler);
    return d_scheduler->setTunable(index, value);
}

void MainMessagePump::ScheduleWork()
{
    work_scheduled_ = true;

    // Record the time when the MessageLoop becomes non-empty.  We need this
    // information when the UI thread is operating inside a modal loop to
    // determine the best time to schedule a pump.  Even though we need this
    // only for modal loop, we always record the time because it is possible
    // for ScheduleWork() to be called right before the UI thread enters the
    // modal loop.
    //
    // Note that ScheduleWork can be called from another thread and so we
    // must record the schedule time before scheduling the pump.  Doing it in
    // reverse order may cause the pump message to be processed by the main
    // thread before the schedule time is set by the current thread.
    ::InterlockedExchange(&d_scheduleTime, ::GetTickCount());

    LONG isInsideMainLoop = d_isInsideMainLoop;
    if (0 == isInsideMainLoop) {
        // We can guage the idleness of the Windows message queue by peeking at
        // it.  Given that the peek operation is not very cheap, we only do it
        // in postHandleMessage().  For all other times, we assume a non-idle
        // state.
        d_allowIdleWork = false;
        schedulePump();
    }
}

                        // --------------------------------
                        // class MainMessagePump::Scheduler
                        // --------------------------------

MainMessagePump::Scheduler::~Scheduler()
{
}


}  // close namespace blpwtk2

// vim: ts=4 et

