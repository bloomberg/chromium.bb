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

#ifndef INCLUDED_BLPWTK2_TOOLKITIMPL_H
#define INCLUDED_BLPWTK2_TOOLKITIMPL_H

#include <blpwtk2_config.h>

#include <blpwtk2_contentmaindelegateimpl.h>
#include <blpwtk2_toolkit.h>



// patch section: embedder ipc
#include <blpwtk2_processhostdelegate.h>


// patch section: multi-heap tracer
#include <blpwtk2_embedderheaptracer.h>
#include <blpwtk2_embedderheaptracershim.h>



#include <mojo/public/cpp/bindings/sync_call_restrictions.h>
#include <sandbox/win/src/sandbox_types.h>
#include "base/metrics/field_trial.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace base {
class FilePath;
class Thread;
class MessageLoop;
}  // close namespace base

namespace content {
class ContentMainRunner;
}  // close namespace content

namespace gin {
class IsolateHolder;
}  // close namespace gin

namespace blpwtk2 {

class MainMessagePump;
class BrowserThread;
class BrowserMainRunner;
class ProcessHostImpl;
class Profile;
class StringRef;
<<<<<<< HEAD
class GpuDataLogger;

||||||| merged common ancestors
=======
class LogMessageThrottler;
>>>>>>> origin/blpwtk2/log-message-handler

                        // =================
                        // class ToolkitImpl
                        // =================

// This is the implementation of the Toolkit.  This class is responsible for
// setting up the threads (based on the application's selection thread mode),
// see blpwtk_toolkit.h for an explanation about the threads.
// There is only ever one instance of ToolkitImpl.  It is created when
// ToolkitFactory::create is called, and it is deleted when Toolkit::destroy()
// is called.  Note that chromium threads are only started when the first
// WebView is created.
class ToolkitImpl : public Toolkit {
    // DATA
    ContentMainDelegateImpl d_mainDelegate;
    std::unique_ptr<content::ContentMainRunner> d_mainRunner;
    MainMessagePump *d_messagePump;
    std::unique_ptr<base::FieldTrialList> field_trial_list;

    std::unique_ptr<BrowserThread> d_browserThread;
        // Only used for the RENDERER_MAIN thread mode and when an external
        // host process is unavailable.  The BrowserThread internally creates
        // an instance of BrowserMainRunner, which runs on a new thread.

    std::unique_ptr<BrowserMainRunner> d_browserMainRunner;
        // Only used for the ORIGINAL thread mode.  This is needed to run the
        // browser code in the application thread.

    std::unique_ptr<base::MessageLoop> d_renderMainMessageLoop;



    // patch section: embedder ipc
    std::unique_ptr<gin::IsolateHolder> d_isolateHolder;
        // Only used for ORIGINAL thread mode and when the toolkit is created with
        // browserV8Enabled flag


    // patch section: gpu
    scoped_refptr<GpuDataLogger> d_gpuDataLogger;
        // GPU data manager observer to log the gpu process messages
        // note: verbosity of the log depends on chromium gpu debugging switches


    // patch section: multi-heap tracer
    std::unordered_map<int, std::unique_ptr<EmbedderHeapTracerShim>> d_heapTracers;
        // Registered heap tracers.

    std::shared_ptr<LogMessageThrottler> d_logMessageThrottler;

    ~ToolkitImpl() override;
        // Shutdown all threads and delete the toolkit.  To ensure the same
        // allocator that was used to create the instance is also used to
        // delete the instance, the embedder is required to call the destroy()
        // method instead of directly deleting the instance.

    // MANIPULATORS
    void initializeContent(const sandbox::SandboxInterfaceInfo& sandboxInfo);
        // Start the ContentMainRunner in the application thread.  The content
        // main runner requires 'sandboxInfo' during its initialization.

    void startMessageLoop(const sandbox::SandboxInterfaceInfo& sandboxInfo);
        // Setup and start the message loop in the application thread.  If the
        // toolkit is operating as the browser, it launches a
        // BrowerMainRunner.  The browser main runner requires 'sandboxInfo'
        // during its initialization.

    void attachGPUDataLogObserver();
        // attach the GPU Data logger to the GPU Data manager as observer
    void detachGPUDataLogObserver();
        // remove the GPU Data logger attached the GPU Data manager as observer

    std::string createProcessHost(
            const sandbox::SandboxInterfaceInfo& sandboxInfo,
            bool                                 isolated,
            const std::string&                   profileDir);
        // Creates the bootstrap process host.  This method creates a thread
        // in which it launches a BrowserMainRunner.  The browser main runner
        // requires 'sandboxInfo' during its initialization.  This method
        // returns the host channel that a render process can use to connect
        // to it.

  public:
    static ToolkitImpl *instance();
        // There can only be one instance of a toolkit in a process.  This
        // method returns the currently loaded instance.

    // CREATORS
    explicit ToolkitImpl(const std::string&              dictionaryPath,
                         const std::string&              hostChannel,
                         const std::vector<std::string>& cmdLineSwitches,
                         bool                            isolated,
                         bool                            browserV8Enabled,
                         const std::string&              profileDir,
                         std::shared_ptr<LogMessageThrottler> logMessageThrottler);

    // blpwtk2::Toolkit overrides
    bool hasDevTools() override;
    void destroy() override;
    String createHostChannel(int              pid,
                             bool             isolated,
                             const StringRef& dataDir) override;
    Profile *getProfile(int pid, bool launchDevToolsServer) override;
    bool preHandleMessage(const NativeMsg *msg) override;
    void postHandleMessage(const NativeMsg *msg) override;
    void addOriginToTrustworthyList(const StringRef& originString) override;
    void setWebViewHostObserver(WebViewHostObserver* observer) override;
    void setTraceThreshold(unsigned int timeoutMS) override;



    // patch section: embedder ipc
    void opaqueMessageToRendererAsync(int pid, const StringRef &message) override;
    void setIPCDelegate(ProcessHostDelegate *delegate) override;


    // patch section: expose v8 platform


    // patch section: multi-heap tracer
    int addV8HeapTracer(EmbedderHeapTracer *tracer) override;
    void removeV8HeapTracer(int embedder_id) override;



};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_TOOLKITIMPL_H

// vim: ts=4 et

