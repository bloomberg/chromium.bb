/*
 * Copyright (C) 2019 Bloomberg Finance L.P.
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

#ifndef INCLUDED_BLPWTK2_RENDERERIOTHREAD_H
#define INCLUDED_BLPWTK2_RENDERERIOTHREAD_H

#include <blpwtk2_config.h>

#include <base/synchronization/waitable_event.h>
#include <base/threading/platform_thread.h>
#include <mojo/public/cpp/system/wait_set.h>
#include <base/callback.h>

#include <mutex>

namespace blpwtk2 {

// This class facilitates a proxy thread between the UI thread and the
// delivery of a sync mojo call.
//
// On Windows, Mojo core uses kernel32.dll functions to send/receive messages
// and wait for completion.  This always works well for async messages and
// conditionally for sync message.  The condition where it doesn't work well
// for sync message is when the sender of the message owns a window created
// by using user32.dll and this window is sent a message while it's issuing
// a sync call.
//
// Consider the following case:
//
//   Thread 1 (T1) owns a child Window 1 (W1)
//   Thread 2 (T2) owns a toplevel Window 2 (W2)
//   Thread 3 (T3) owns a toplevel Window 3 (W3)
//
//   Initial condition:
//     - W1 has focus and it's the child of W2
//     - T2 is in the middle of the event loop
//
//  1. The user moves focus to W3
//      a. Windows sends WM_ACTIVATE on W2 to deactivate it
//      b. Windows sends WM_KILLFOCUS on W1
//  2. T2 sends a MojoIPC message to T1
//  3. T2 uses kernel32 to wait for T1
//     T2 is blocked on T1
//  4. T1 receives the WM_KILLFOCUS. The GetMessage() waits for
//     the WM_ACTIVATE to be processed by T2 before it can
//     process the WM_KILLFOCUS.
//     T1 is blocked on T2
//
//  We have T2 blocked on T1 in step 3 and
//          T1 blocked on T2 in step 4, which results in a deadlock.
//
//  If we make T2 receive and process the WM_ACTIVATE, it will unblock
//  T1 to process the MojoIPC message, which will then unblock T2. Since
//  Mojo core uses kernel32.dll functions to wait for completion, it is
//  unaware of Windows messages and so it doesn't process unqueued messages
//  while it waits.
//
//  We can solve this by introducing another thread that doesn't own any
//  windows:
//
//   Thread 1 (T1) owns a child Window 1 (W1)
//   Thread 2 (T2) owns a toplevel Window 2 (W2)
//   Thread 3 (T3) owns a toplevel Window 3 (W3)
//   Thread 4 (T4) does not own any windows
//
//   Initial condition:
//     - W1 has focus and it's the child of W2
//     - T2 is in the middle of the event loop
//
//  1. The user moves focus to W3
//      a. Windows sends WM_ACTIVATE on W2 to deactivate it
//      b. Windows sends WM_KILLFOCUS on W1
//  2. T2 sends a MojoIPC message to T1
//  3. T2 sends a sync Windows message to T4  (***)
//     T2 is blocked on T4
//  4. T4 uses kernel32 to wait for T1
//     T4 is blocked on T1
//  5. T1 receives the WM_KILLFOCUS. The GetMessage() waits for
//     the WM_ACTIVATE to be processed by T2 before it can
//     process the WM_KILLFOCUS.
//     T1 is blocked on T2
//
//  ----------
//
//  6. T2 receives the WM_ACTIVATE in the middle of the
//     SendMessage call
//  7. T1 is unblocked and processes WM_KILLFOCUS
//  8. T1 processes the MojoIPC message and unblocks T4
//  9. T4 is unblocked by T1 (by the completion of Mojo) and
//     unblocks T2
//
//  We are no longer in the deadlock.  This proxy thread is only necessary
//  when T2 owns a window.  In the standard browser environment, T2 (which
//  is the renderer) does not own a window.  For blpwtk2, the embedder can
//  create windows from thread T2 and this is how this problem arises.

class RendererIOThread : private base::PlatformThread::Delegate {
  public:
    explicit RendererIOThread();
    ~RendererIOThread() final;

    static LRESULT CALLBACK windowProcedure(NativeView window_handle,
                                            UINT message,
                                            WPARAM wparam,
                                            LPARAM lparam);

    mojo::UIWaitProxy *proxy();
    void Wait(mojo::WaitSet* wait_set,
              base::WaitableEvent** ready_event,
              size_t* num_ready_handles,
              mojo::Handle* ready_handles,
              MojoResult* ready_results,
              MojoHandleSignalsState* signals_states);

  private:
    void ThreadMain() override;

    base::PlatformThreadHandle thread_handle_;
    mojo::UIWaitProxy proxy_;
    NativeView window_ = 0;
    base::WaitableEvent *init_wait_event_ = 0;

    mojo::WaitSet* wait_set_ = 0;
    base::WaitableEvent** ready_event_ = 0;
    size_t* num_ready_handles_ = 0;
    mojo::Handle* ready_handles_ = 0;
    MojoResult* ready_results_ = 0;
    MojoHandleSignalsState* signals_states_ = 0;

    DISALLOW_COPY_AND_ASSIGN(RendererIOThread);
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RENDERERIOTHREAD_H

// vim: ts=4 et

