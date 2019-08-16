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

#include <blpwtk2_browsermainrunner.h>

#include <blpwtk2_contentmaindelegateimpl.h>  // for ContentClient TODO: fix this
#include <blpwtk2_statics.h>
#include <blpwtk2_viewsdelegateimpl.h>
#include <blpwtk2_devtoolsmanagerdelegateimpl.h>

#include <base/logging.h>  // for DCHECK
#include <base/message_loop/message_loop.h>
#include <base/message_loop/message_loop_current.h>
#include <base/strings/string_number_conversions.h>
#include <base/task/thread_pool/thread_pool.h>
#include <chrome/browser/printing/print_job_manager.h>
#include <content/browser/scheduler/browser_task_executor.h>
#include <content/public/browser/browser_main_runner.h>
#include <content/public/common/content_switches.h>
#include <net/base/net_errors.h>
#include <net/socket/tcp_server_socket.h>
#include <ui/views/widget/desktop_aura/desktop_screen.h>
#include <ui/display/screen.h>
#include <base/threading/thread_restrictions.h>
#include <content/browser/startup_helper.h>

namespace blpwtk2 {

                        // -----------------------
                        // class BrowserMainRunner
                        // -----------------------

BrowserMainRunner::BrowserMainRunner(
        const sandbox::SandboxInterfaceInfo& sandboxInfo)
    : d_mainParams(*base::CommandLine::ForCurrentProcess())
    , d_sandboxInfo(sandboxInfo)
{
    Statics::initBrowserMainThread();

    d_mainParams.sandbox_info = &d_sandboxInfo;
    content::BrowserTaskExecutor::Create();
    base::ThreadPoolInstance::Create("Browser");
    d_impl = content::BrowserMainRunner::Create();
    int rc = d_impl->Initialize(d_mainParams);
    DCHECK(-1 == rc);  // it returns -1 for success!!

    Statics::browserMainTaskRunner = base::MessageLoopCurrent::Get()->task_runner();

    display::Screen::SetScreenInstance(views::CreateDesktopScreen());
    d_viewsDelegate.reset(new ViewsDelegateImpl());
    content::StartBrowserThreadPool();
}

BrowserMainRunner::~BrowserMainRunner()
{
    Statics::browserMainTaskRunner.reset();
    d_impl->Shutdown();
}

int BrowserMainRunner::run()
{
    return d_impl->Run();
}

}  // close namespace blpwtk2

// vim: ts=4 et

