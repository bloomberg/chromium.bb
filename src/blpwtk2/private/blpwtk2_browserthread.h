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

#ifndef INCLUDED_BLPWTK2_BROWSERTHREAD_H
#define INCLUDED_BLPWTK2_BROWSERTHREAD_H

#include <blpwtk2_config.h>

#include <base/memory/scoped_refptr.h>
#include <base/threading/platform_thread.h>
#include <sandbox/win/src/sandbox.h>

namespace base {
class SingleThreadTaskRunner;
}  // close namespace base

namespace blpwtk2 {

class BrowserMainRunner;

                            // ===================
                            // class BrowserThread
                            // ===================

// This class is only used if we are using 'ThreadMode::RENDERER_MAIN'.  See
// blpwtk2_toolkit.h for an explanation about the threads.
//
// This class starts a secondary thread and instantiates the BrowserMainRunner
// in that thread.  All browser-ui tasks (creation of WebContents, manipulation
// of the WebContentsView (i.e. the HWND of the WebContents) should be posted
// to this thread.
class BrowserThread : private base::PlatformThread::Delegate
{
    // DATA
    sandbox::SandboxInterfaceInfo d_sandboxInfo;
    BrowserMainRunner* d_mainRunner;
    base::PlatformThreadHandle d_threadHandle;

    void ThreadMain() override;

  public:
    explicit BrowserThread(const sandbox::SandboxInterfaceInfo& sandboxInfo);
    ~BrowserThread() final;

    void sync();

    BrowserMainRunner *mainRunner() const;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner() const;

    static void SetMainMessageLoopQuitClosure(base::OnceClosure quit_closure);
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_BROWSERTHREAD_H

// vim: ts=4 et

