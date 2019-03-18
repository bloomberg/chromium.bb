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

#include <blpwtk2_browserthread.h>

#include <blpwtk2_browsermainrunner.h>
#include <blpwtk2_statics.h>

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/logging.h>  // for DCHECK
#include <base/no_destructor.h>
#include <base/single_thread_task_runner.h>
#include <base/synchronization/waitable_event.h>

#include <mutex>

namespace blpwtk2 {

static base::WaitableEvent* s_initWaitEvent = 0;
std::mutex g_quit_closure_mutex;

// Null until/unless the default main message loop is running.
base::NoDestructor<base::OnceClosure> g_quit_main_message_loop;

                            // -------------------
                            // class BrowserThread
                            // -------------------

void BrowserThread::ThreadMain()
{
    // Ensure that any singletons created by chromium will be destructed in the
    // browser-main thread, instead of the application's main thread.  This is
    // shadowing the AtExitManager created by ContentMainRunner.  The fact that
    // the constructor waits for 's_initWaitEvent' immediately after creating
    // the thread, and the fact that the main thread joins this thread before
    // shutting down ContentMainRunner, makes this shadowing thread-safe.
    // Note that this means singletons created in the in-process renderer
    // thread (the application's main thread) will be destructed in the
    // browser-main thread.  This should be safe because this would happen
    // while the renderer thread is waiting for the browser-main thread to
    // join.  However, we may get assertion failures in the future if any of
    // the renderer components start asserting that they are destroyed in the
    // same thread where they are created.  We should be able to safely remove
    // those assertions.
    base::ShadowingAtExitManager atExitManager;

    d_mainRunner = new BrowserMainRunner(d_sandboxInfo);

    // Allow the main thread to continue executing, after the
    // ShadowingAtExitManager has been fully constructed and installed, and
    // after the browser threads have been created.  This let's the main thread
    // assume it can start posting tasks immediately after constructing the
    // BrowserThread object.
    DCHECK(s_initWaitEvent);
    s_initWaitEvent->Signal();

    int rc = d_mainRunner->run();
    DCHECK(0 == rc);

    delete d_mainRunner;
}

BrowserThread::BrowserThread(const sandbox::SandboxInterfaceInfo& sandboxInfo)
    : d_sandboxInfo(sandboxInfo)
{
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    s_initWaitEvent = &event;
    base::PlatformThread::Create(0, this, &d_threadHandle);
    s_initWaitEvent->Wait();
    s_initWaitEvent = 0;
}

BrowserThread::~BrowserThread()
{
    // called from main thread
    std::lock_guard<std::mutex> guard(g_quit_closure_mutex);
    task_runner()->PostTask(FROM_HERE, std::move(*g_quit_main_message_loop));
    base::PlatformThread::Join(d_threadHandle);
}

void BrowserThread::sync()
{
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal,
                   base::Unretained(&event)));
    event.Wait();
}

BrowserMainRunner *BrowserThread::mainRunner() const
{
    return d_mainRunner;
}

scoped_refptr<base::SingleThreadTaskRunner> BrowserThread::task_runner() const
{
    DCHECK(Statics::browserMainTaskRunner);
    return Statics::browserMainTaskRunner;
}

void BrowserThread::SetMainMessageLoopQuitClosure(base::OnceClosure quit_closure)
{
    // called from browser main thread
    std::lock_guard<std::mutex> guard(g_quit_closure_mutex);
    *g_quit_main_message_loop = std::move(quit_closure);
}

}  // close namespace blpwtk2

// vim: ts=4 et

