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

#include <blpwtk2_inprocessrenderer.h>

#include <blpwtk2_statics.h>

#include <base/task/single_thread_task_executor.h>
#include <content/common/in_process_child_thread_params.h>
#include <content/public/renderer/render_thread.h>
#include <content/renderer/render_process_impl.h>
#include <content/public/child/dwrite_font_proxy_init_win.h>
#include <mojo/public/cpp/bindings/sync_call_restrictions.h>
#include "third_party/blink/public/platform/platform.h"
#include <third_party/blink/public/platform/web_runtime_features.h>
#include <third_party/blink/public/platform/scheduler/web_thread_scheduler.h>
#include <ui/gfx/win/direct_write.h>
#include <ui/display/win/dpi.h>

namespace blpwtk2 {

static void InitDirectWrite()
{
    // This code is adapted from RendererMainPlatformDelegate::PlatformInitialize,
    // which is used for out-of-process renderers, but is not used for in-process
    // renderers.  So, we need to do it here.
    mojo::SyncCallRestrictions::ForceSyncCallAllowed();
    content::InitializeDWriteFontProxy();
}

                        // =============================
                        // class InProcessRendererThread
                        // =============================

class InProcessRendererThread : public base::Thread {
    // DATA
    scoped_refptr<base::SingleThreadTaskRunner> d_browserIOTaskRunner;
    mojo::OutgoingInvitation* d_broker_client_invitation;
    int d_mojoHandle;
    const int32_t d_renderer_client_id;

    // Called just prior to starting the message loop
    void Init() override;

    // Called just after the message loop ends
    void CleanUp() override;

    DISALLOW_COPY_AND_ASSIGN(InProcessRendererThread);

public:
    InProcessRendererThread(
            const scoped_refptr<base::SingleThreadTaskRunner>& browserIOTaskRunner,
            mojo::OutgoingInvitation*                          broker_client_invitation,
            int                                                mojoHandle,
            const int32_t                                      renderer_client_id);
    ~InProcessRendererThread() final;
};

                        // -----------------------------
                        // class InProcessRendererThread
                        // -----------------------------

void InProcessRendererThread::Init()
{
    std::unique_ptr<blink::scheduler::WebThreadScheduler>
        main_thread_scheduler =
            blink::scheduler::WebThreadScheduler::CreateMainThreadScheduler();
    InitDirectWrite();
    content::RenderThread::InitInProcessRenderer(
        content::InProcessChildThreadParams(d_browserIOTaskRunner,
                                            d_broker_client_invitation,
                                            d_mojoHandle),
                                            std::move(main_thread_scheduler),
                                            d_renderer_client_id);

    // No longer need to hold on to this reference.
    d_browserIOTaskRunner = nullptr;
}


void InProcessRendererThread::CleanUp()
{
    content::RenderThread::CleanUpInProcessRenderer();
}


InProcessRendererThread::InProcessRendererThread(
        const scoped_refptr<base::SingleThreadTaskRunner>& browserIOTaskRunner,
        mojo::OutgoingInvitation*                          broker_client_invitation,
        int                                                mojoHandle,
        const int32_t                                      renderer_client_id)
: base::Thread("BlpInProcRenderer")
, d_browserIOTaskRunner(browserIOTaskRunner)
, d_broker_client_invitation(broker_client_invitation)
, d_mojoHandle(mojoHandle)
, d_renderer_client_id(renderer_client_id)
{
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::UI;
    StartWithOptions(options);
}

InProcessRendererThread::~InProcessRendererThread()
{
    Stop();
}

                        // ------------------------
                        // struct InProcessRenderer
                        // ------------------------

static InProcessRendererThread *g_inProcessRendererThread;

// static
void InProcessRenderer::init(
        bool                                               isHost,
        const scoped_refptr<base::SingleThreadTaskRunner>& browserIOTaskRunner,
        mojo::OutgoingInvitation* broker_client_invitation,
        int                                                mojoHandle,
        const int32_t                                      renderer_client_id)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!g_inProcessRendererThread);

    blink::Platform::InitializeBlink();
    if (Statics::isOriginalThreadMode()) {
        DCHECK(Statics::isOriginalThreadMode());
        g_inProcessRendererThread =
            new InProcessRendererThread(browserIOTaskRunner,
                                        broker_client_invitation,
                                        mojoHandle,
                                        renderer_client_id);
    }
    else {
      std::unique_ptr<blink::scheduler::WebThreadScheduler>
          main_thread_scheduler =
              blink::scheduler::WebThreadScheduler::CreateMainThreadScheduler();
      if (!isHost) {
        InitDirectWrite();
      }
      content::RenderThread::InitInProcessRenderer(
          content::InProcessChildThreadParams(browserIOTaskRunner,
                                              broker_client_invitation,
                                              mojoHandle, true),
          std::move(main_thread_scheduler),
          renderer_client_id);
    }
}

// static
void InProcessRenderer::cleanup()
{
    DCHECK(Statics::isInApplicationMainThread());

    if (Statics::isOriginalThreadMode()) {
        DCHECK(g_inProcessRendererThread);
        delete g_inProcessRendererThread;
        g_inProcessRendererThread = 0;
    }
    else {
        DCHECK(!g_inProcessRendererThread);
        content::RenderThread::CleanUpInProcessRenderer();
    }
}

// static
scoped_refptr<base::SingleThreadTaskRunner> InProcessRenderer::ioTaskRunner()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(Statics::isRendererMainThreadMode());
    return content::RenderThread::IOTaskRunner();
}

}  // close namespace blpwtk2

// vim: ts=4 et

