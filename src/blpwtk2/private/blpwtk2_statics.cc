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

#include <blpwtk2_statics.h>

#include <base/logging.h>  // for DCHECK
#include <base/memory/scoped_refptr.h>
#include <base/single_thread_task_runner.h>

namespace blpwtk2 {

ThreadMode Statics::threadMode = ThreadMode::ORIGINAL;
base::PlatformThreadId Statics::applicationMainThreadId = base::kInvalidThreadId;
base::PlatformThreadId Statics::browserMainThreadId = base::kInvalidThreadId;
ResourceLoader* Statics::inProcessResourceLoader = 0;
scoped_refptr<base::SingleThreadTaskRunner> Statics::browserMainTaskRunner;
ToolkitCreateParams::ChannelErrorHandler Statics::channelErrorHandler = 0;
bool Statics::hasDevTools = false;
bool Statics::isInProcessRendererEnabled = true;
bool Statics::inProcessResizeOptimizationDisabled = false;
WebViewHostObserver* Statics::webViewHostObserver = 0;
ToolkitDelegate *Statics::toolkitDelegate = nullptr;

static int lastRoutingId = 0;

void Statics::initApplicationMainThread()
{
    DCHECK(applicationMainThreadId == base::kInvalidThreadId);
    applicationMainThreadId = base::PlatformThread::CurrentId();
}

void Statics::initBrowserMainThread()
{
    DCHECK(browserMainThreadId == base::kInvalidThreadId);
    browserMainThreadId = base::PlatformThread::CurrentId();
}

int Statics::getUniqueRoutingId()
{
    DCHECK(isInApplicationMainThread());
    return ++lastRoutingId;
}

}  // close namespace blpwtk2

// vim: ts=4 et

