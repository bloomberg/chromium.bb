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

#ifndef INCLUDED_BLPWTK2_MESSAGEPUMPFORUI_H
#define INCLUDED_BLPWTK2_MESSAGEPUMPFORUI_H

#include <blpwtk2_config.h>

#include <base/message_loop/message_pump_win.h>
#include <base/message_loop/message_loop.h>

namespace base {
class RunLoop;
}  // close namespace base

namespace blpwtk2 {

                        // =====================
                        // class MainMessagePump
                        // =====================

// This message pump extends MessagePumpForUI and contains methods to
// facilitate integration with an application's main message loop.
class MainMessagePump final : public base::MessagePumpForUI {
    // DATA
    NativeView d_window;
    std::unique_ptr<base::RunLoop> d_runLoop;
    std::unique_ptr<base::MessageLoop::ScopedNestableTaskAllower> d_scopedNestedTaskAllower;
    RunState d_runState;
    bool d_isInsideModalLoop;
    LONG d_isInsideMainLoop;
    LONG d_isPumped;
    LONG d_needRepost;
    DWORD d_scheduleTime;
    bool d_skipIdleWork;
    HHOOK d_windowProcedureHook;
    HHOOK d_messageFilter;
    unsigned int d_minTimer;
    unsigned int d_maxTimer;
    unsigned int d_maxPumpCountInsideModalLoop;
    unsigned int d_traceThreshold;
    int d_nestLevel;

    // ACCESSORS
    static const wchar_t* getClassName();

    // MANIPULATORS
    static LRESULT CALLBACK windowProcedure(NativeView window_handle,
                                            UINT message,
                                            WPARAM wparam,
                                            LPARAM lparam);

    static LRESULT CALLBACK messageFilter(int code,
                                          WPARAM wParam,
                                          LPARAM lParam);

    static LRESULT CALLBACK windowProcedureHook(int code,
                                                WPARAM wParam,
                                                LPARAM lParam);

    void schedulePumpIfNecessary();
    void schedulePump();
    void doWork();
    void modalLoop(bool enabled);
    void resetWorkState();
    bool shouldTrace(unsigned int elapsedTime);

    DISALLOW_COPY_AND_ASSIGN(MainMessagePump);

  public:
    // STATIC CREATORS
    static MainMessagePump* current();

    // CLASS METHODS
    static void OnDebugBreak();
        // Notify this pump that we're entering a "modal" loop due to being
        // paused at a JavaScript breakpoint.

    static void OnDebugResume();
        // Notify this pump that we're exiting a "modal" loop due to resuming
        // from being paused at a JavaScript breakpoint.

    // CREATORS
    MainMessagePump();
    ~MainMessagePump() override;

    void init();
        // This must be called after a MessageLoop is installed on the current
        // thread.

    void flush();
    void cleanup();
    bool preHandleMessage(const MSG& msg);
    void postHandleMessage(const MSG& msg);
    void setTraceThreshold(unsigned int timeoutMS);

    // base::MessagePumpForUI
    void ScheduleWork() override;
};

inline
bool MainMessagePump::shouldTrace(unsigned int elapsedTime)
{
    return d_traceThreshold != 0 && elapsedTime >= d_traceThreshold;
}

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_MESSAGEPUMPFORUI_H

// vim: ts=4 et

