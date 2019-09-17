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

#include <blpwtk2_toolkitimpl.h>

#include <blpwtk2_browsercontextimpl.h>
#include <blpwtk2_browsermainrunner.h>
#include <blpwtk2_contentbrowserclientimpl.h>

#include <blpwtk2_browserthread.h>
#include <blpwtk2_channelinfo.h>
#include <blpwtk2_desktopstreamsregistry.h>
#include <blpwtk2_inprocessrenderer.h>
#include <blpwtk2_mainmessagepump.h>
#include <blpwtk2_processhostimpl.h>
#include <blpwtk2_products.h>
#include <blpwtk2_profileimpl.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_stringref.h>
#include <blpwtk2_webviewcreateparams.h>
#include <blpwtk2_webviewimpl.h>
#include <blpwtk2_webviewproxy.h>
#include <blpwtk2_utility.h>

#include "base/base_switches.h"
#include "base/feature_list.h"
#include <base/command_line.h>
#include <base/message_loop/message_loop.h>
#include <base/path_service.h>
#include <base/process/memory.h>
#include <base/synchronization/waitable_event.h>
#include <base/task/post_task.h>
#include <base/task/task_traits.h>
#include <base/threading/thread_restrictions.h>
#include <base/strings/utf_string_conversions.h>
#include <chrome/common/chrome_paths.h>
#include <content/app/mojo/mojo_init.h>
#include <content/public/app/content_main.h>
#include <content/public/app/content_main_runner.h>
#include <content/public/app/sandbox_helper_win.h>  // for InitializeSandboxInfo
#include <content/public/browser/browser_task_traits.h>
#include <content/public/browser/browser_thread.h>
#include <content/public/browser/render_process_host.h>
#include <content/public/common/content_switches.h>
#include <content/common/in_process_child_thread_params.h>
#include <content/renderer/render_thread_impl.h>
#include <content/public/renderer/render_thread.h>
#include <content/browser/browser_main_loop.h>
#include <sandbox/win/src/win_utils.h>
#include <services/service_manager/public/cpp/service_executable/switches.h>
#include <services/service_manager/sandbox/switches.h>
#include <third_party/blink/public/platform/web_security_origin.h>
//#include <third_party/blink/public/web/blink.h>
#include <third_party/blink/public/web/web_security_policy.h>
#include <third_party/blink/public/web/web_script_controller.h>
#include <third_party/icu/source/common/unicode/locid.h>
#include <ui/base/ime/init/input_method_initializer.h>
#include <ui/base/l10n/l10n_util.h>

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <mutex>
#include <thread>
#include <tuple>
#include <utility>

#include <windows.h>

// patch section: embedder ipc


// patch section: multi-heap tracer



namespace blpwtk2 {

static ToolkitImpl *g_instance;
constexpr size_t EXIT_TIME_OUT_MS{1000};
constexpr size_t TERMINATE_LOG_WAIT_TIME_MS{200};

                        // ===============
                        // class ScopeExitGuard
                        // ===============
// ScopeExitGuard is used to prevent deadlocks / race conditions.
// At the given timeout if a ScopeExitGuard is not out of scope,
// the current process will be terminated.
// On the other hand,  nothing will happen except logging an info when
// a ScopeExitGuard is out of scope normally.
class ScopeExitGuard {
public:
  ScopeExitGuard(size_t timeout_ms) {
    LOG(INFO) << "Monitoring renderer exit process...";
    monitor_thread_ = std::thread([this, timeout_ms]() {
      std::mutex mtx;
      std::unique_lock<std::mutex> lk(mtx);
      while (cv_.wait_for(lk, std::chrono::milliseconds(timeout_ms)) !=
                 std::cv_status::timeout &&
             !exit_normally_) {
      }
      if (!exit_normally_) {
        // Try to log the error message before terminating the process
        LOG(ERROR) << "Renderer terminated because of exit timeout";
        std::this_thread::sleep_for(
            std::chrono::milliseconds{TERMINATE_LOG_WAIT_TIME_MS});
        base::Process::TerminateCurrentProcessImmediately(0);
      }
    });
  }

  ~ScopeExitGuard() {
    exit_normally_ = true;
    cv_.notify_one();
    monitor_thread_.join();
    LOG(INFO) << "Renderer exits normally";
  }

private:
  std::atomic<bool> exit_normally_{false};
  std::condition_variable cv_;
  std::thread monitor_thread_;
};

static void createLoopbackHostChannel(
        std::string                                       *hostChannel,
        bool                                               isolated,
        const std::string&                                 profileDir,
        const scoped_refptr<base::SingleThreadTaskRunner>& runner)
{
    DCHECK(Statics::isInBrowserMainThread());
    DCHECK(hostChannel);
    *hostChannel = ProcessHostImpl::createHostChannel(
            base::GetCurrentProcId(),
            isolated,
            profileDir,
            runner);
}

static void setupSandbox(sandbox::SandboxInterfaceInfo *info,
                         std::vector<std::string>      *switches,
                         bool                           isHost)
{
    memset(info, 0, sizeof(*info));

    if (isHost) {
        // Create a new broker service and harden the integrity level policy
        // of this process.  The hardening is done by disallowing processes
        // with lower integity level from read/execute operations on this
        // process' security token, should a lower integrity process somehow
        // get a hold of this process' security token.
        //
        // The documentation of InitializeSandboxInfo() states that this
        // function, along with the sandbox library, must be statically linked
        // into the executable.  This requirement comes from the way sandbox
        // interception is implemented.  The broker process computes the
        // offsets of the system-call functions (from ntdll.dll) from its
        // address space and installs trampolines to override the same
        // functions in the target processes using the same offset values.
        // This can only work if the offset is recorded from the process image
        // and the installation of the trampolines also happens on a process
        // image.  If the offset was recorded from a loadable module, the
        // base address would have an additional offset which cannot be
        // corrected before installing the trampolines.  This is why the
        // sandbox library needs to be statically linked with the executables.
        //
        // We workaround this limitation by importing the function pointers
        // from the target process instead of computing it based on the broker
        // process.  This modification is made into the sandbox library.
        content::InitializeSandboxInfo(info);
    }
    else {
        // If host channel is set, we must be in RENDERER_MAIN thread mode.
        CHECK(Statics::isRendererMainThreadMode());

        // If another process is our host, then explicitly disable sandbox
        // in *this* process.
        // Since both 'broker_services' and 'target_services' are be null in
        // our SandboxInterfaceInfo, we don't want chromium to touch it.  This
        // flag prevents chromium from trying to use these services.
        switches->push_back(service_manager::switches::kNoSandbox);
    }
}

static void applySwitchesToContentMainDelegate(
        ContentMainDelegateImpl         *delegate,
        const std::vector<std::string>&  switches)
{
    // Apply the command line switches from the embedder
    for (auto cmdLineSwitch : switches) {
        delegate->appendCommandLineSwitch(cmdLineSwitch.c_str());
    }
}

static void getSwitchesFromHostChannel(std::vector<std::string> *switches,
                                       const ChannelInfo&        channelInfo)
{
    // If host channel is set, we must not be in ORIGINAL thread mode.
    CHECK(!Statics::isOriginalThreadMode());

    // Create an arglist from the switches in the channel info.
    for (auto cmdLineSwitch : channelInfo.switches()) {
        if (cmdLineSwitch.second.empty()) {
            switches->push_back(cmdLineSwitch.first);
        }
        else {
            switches->push_back(
                cmdLineSwitch.first + "=" + cmdLineSwitch.second);
        }
    }
}

static void setupDictionaryFiles(const std::string& path)
{
    // Set the path to the dictionary files
    if (!path.empty()) {
        LOG(INFO) << "Setting dictionary path: " << path;
        base::ThreadRestrictions::ScopedAllowIO allowIO;
        base::PathService::Override(chrome::DIR_APP_DICTIONARIES,
                              base::FilePath::FromUTF8Unsafe(path));
    }
}

void setDefaultLocaleIfWindowsLocaleIsNotSupported()
{
    // 1. Determine the user's locale
    WCHAR localeNameW[LOCALE_NAME_MAX_LENGTH] = {};
    char  localeName[LOCALE_NAME_MAX_LENGTH] = {};

    GetLocaleInfoEx(
        LOCALE_NAME_USER_DEFAULT,
        LOCALE_SNAME,
        localeNameW,
        LOCALE_NAME_MAX_LENGTH);

    wcstombs(&localeName[0], &localeNameW[0], LOCALE_NAME_MAX_LENGTH);

    // 2. Get the available locales from Chromium
    const auto& availableLocales = l10n_util::GetAvailableLocales();

    // 3. If the user's locale is not available, fall back to en-US.
    if (std::find(availableLocales.begin(),
                  availableLocales.end(),
                  localeName) == availableLocales.end()) {
        LOG(INFO)
            << "Locale '" << localeName << "' not available, using en-US";

        UErrorCode error_code = U_ZERO_ERROR;
        icu::Locale::setDefault(icu::Locale::getUS(), error_code);
    }
}

static std::unique_ptr<base::MessagePump> messagePumpForUIFactory()
{
    if (Statics::isInApplicationMainThread()) {
		// Create an instance of MainMessagePump.  This pump is designed to
		// co-exist with the embedder's message loop on the same thread.
        return std::unique_ptr<base::MessagePump>(new MainMessagePump());
    }

    return std::unique_ptr<base::MessagePump>(new base::MessagePumpForUI());
}

static void startRenderer(
    bool isHost,
    const ChannelInfo& channelInfo,
    mojo::OutgoingInvitation* broker_client_invitation) {
  // Run the render thread in the current thread.  Normally, Content calls
  // back to its embedder via the ContentBrowserClient to ask it to
  // initialize the renderer.  In this case, Content doesn't even have the
  // correct Mojo service request token nor the controller handle, since it
  // was not available and not provided to Content when initializeContent()
  // was called.  For that reason, we start the renderer here instead of in
  // the callback.

  scoped_refptr<base::SingleThreadTaskRunner> ioTaskRunner;

  // If the host is running in this process, let the renderer use the
  // host's IO thread.  This way, the renderer needn't create a ChildIO
  // thread.
  if (isHost) {
    ioTaskRunner = base::CreateSingleThreadTaskRunnerWithTraits(
        {content::BrowserThread::IO});
  }

  LOG(INFO) << "Initializing InProcessRenderer";
  InProcessRenderer::init(isHost, ioTaskRunner, broker_client_invitation,
                          channelInfo.getMojoServiceToken(),
                          channelInfo.getMojoControllerHandle());
}

static size_t GetSwitchPrefixLength(const base::string16& string)
{
    const wchar_t* const kSwitchPrefixes[] = {L"--", L"-", L"/"};
    size_t switch_prefix_count = base::size(kSwitchPrefixes);

    for (size_t i = 0; i < switch_prefix_count; ++i) {
        base::string16 prefix(kSwitchPrefixes[i]);
        if (string.compare(0, prefix.length(), prefix) == 0) {
            return prefix.length();
        }
    }
    return 0;
}

static bool IsSwitch(const base::string16&  string,
                     base::string16        *switch_string,
                     base::string16        *switch_value)
{
    const wchar_t kSwitchValueSeparator[] = FILE_PATH_LITERAL("=");

    switch_string->clear();
    switch_value->clear();

    size_t prefix_length = GetSwitchPrefixLength(string);
    if (prefix_length == 0 || prefix_length == string.length()) {
        return false;
    }

    const size_t equals_position = string.find(kSwitchValueSeparator);
    *switch_string = string.substr(0, equals_position);
    if (equals_position != base::string16::npos) {
        *switch_value = string.substr(equals_position + 1);
    }

    return true;
}

static void appendCommandLine(const std::vector<std::string>& argv)
{
    const wchar_t kSwitchTerminator[] = FILE_PATH_LITERAL("--");

    bool parseSwitches = true;
    base::CommandLine* commandLine = base::CommandLine::ForCurrentProcess();

    for (const auto& arg_utf8 : argv) {
        // Convert the UTF-8 encoded argument to UTF-16
        std::string temp = "--" + arg_utf8;
        base::string16 arg;
        base::UTF8ToUTF16(temp.data(), temp.size(), &arg);

        // Trim whitespaces from the argument
        base::TrimWhitespace(arg, base::TRIM_ALL, &arg);
        parseSwitches &= (arg != kSwitchTerminator);

        base::string16 switch_string, switch_value;
        if (parseSwitches && IsSwitch(arg, &switch_string, &switch_value)) {
            commandLine->AppendSwitchNative(
                    base::UTF16ToASCII(switch_string), switch_value);
        }
        else {
            commandLine->AppendArgNative(arg);
        }
    }
}

                        // -----------------
                        // class ToolkitImpl
                        // -----------------

void ToolkitImpl::initializeContent(const sandbox::SandboxInterfaceInfo& sandboxInfo)
{
    // Create a ContentMainRunner
    d_mainRunner.reset(content::ContentMainRunner::Create());
    content::ContentMainParams mainParams(&d_mainDelegate);

    // We needn't worry about passing a pointer to an object on the stack
    // because ContentMainRunnerImpl::Initialize makes a copy of sandbox_info.
    sandbox::SandboxInterfaceInfo sandboxInfoCopy = sandboxInfo;
    mainParams.sandbox_info = &sandboxInfoCopy;

    // Initialize Content
	base::EnableTerminationOnOutOfMemory();
	int rc = d_mainRunner->Initialize(mainParams);
    CHECK(-1 == rc);  // it returns -1 for success!!
}

void ToolkitImpl::startMessageLoop(const sandbox::SandboxInterfaceInfo& sandboxInfo)
{
    // Install the above message pump as the default message pump for newly
    // created UI-type message loops.  This association is one way:
    //     MessageLoop ==> MessagePump
    //
    // This allows the message loop to notify the pump when it gets some new
    // tasks so the pump knows to wake it up at some point.
    //
    // Note that the MessageLoop itself doesn't care about UI or IO tasks;
    // it's only responsible for queuing up tasks and flushing them when the
    // associated pump signals it.  The pump, however, does have a notion of
    // being UI-based or IO-based.
    //
    // The MessagePumpForUI uses constructs from user32.dll to drive it while
    // the MessagePumpForIO uses constructs from kernel32.dll to drive it.
    // The latter implementation (MessagePumpForIO) is much simpler and faster
    // than the former (MessagePumpForUI) but some cases require the use of
    // the MessagePumpForUI.  One particular use case that requires
    // MessagePumpForUI is single-threaded COM apartments.  When COM is
    // initialized on a thread, it can be set to STA (single-threaded) mode or
    // MTA (multi-threaded) mode.  In single threaded mode, the COM library
    // provides synchronization of the objects in the its apartment and it
    // uses window messages (user32.dll) to facilitate the synchronization.
    // Since each thread can have at most one event loop, STA forces other
    // code running on the same thread to also use window messages for its
    // synchronization.
    //
    // To allow the embedder to use COM in STA mode and to also drive the
    // message loop using window messages, we use MessagePumpForUI when we
    // are operating in RendererMain mode (renderer running in the main
    // thread).  In the original thread mode (where the browser runs in the
    // main thread), we also need to use MessagePumpForUI since the browser
    // owns the created windows and they pretty much rely entirely on window
    // messages for synchronization.
    base::MessagePump::OverrideMessagePumpForUIFactory(&messagePumpForUIFactory);

    if (Statics::isRendererMainThreadMode()) {
        // If the renderer is to run in the application thread, we create a
        // instance of UI message loop.  This will use the main message pump
        // that was installed above.  Once a message loop is created, it
        // places a reference to itself in TLS.  It can be looked up by
        // calling MessageLoop::current().
        d_renderMainMessageLoop = std::make_unique<base::MessageLoop>(base::MessageLoop::TYPE_UI);
    }
    else {
        DCHECK(Statics::isOriginalThreadMode());

        // If the browser is to run in the application thread, we simply
        // create an instance of BrowserMainRunner.  It will create a
        // message loop on the current thread.
        d_browserMainRunner.reset(new BrowserMainRunner(sandboxInfo));
    }

    // Initialize the main message pump.  This effectively installs the hooks
    // into the windows message queue and registers the current message loop
    // to the message pump.  This association is also one way:
    //     MessageLoop <== MessagePump
    //
    // This allows the pump to tell the message loop to flush out its tasks.
    // This association is not established when the MessageLoop is created
    // because it is possible for the message pump to be temporarily switch
    // over to poke a nested message loop to make it flush out its tasks. This
    // is not a norm but some scenarios (such as sync IPC calls) do require
    // nested message loops.
    LOG(INFO) << "Initializing MainMessagePump";
    d_messagePump = MainMessagePump::current();
    d_messagePump->init();
}

std::string ToolkitImpl::createProcessHost(
        const sandbox::SandboxInterfaceInfo&               sandboxInfo,
        bool                                               isolated,
        const std::string&                                 profileDir)
{
    std::string hostChannel;

    // Disable sync call restriction
    mojo::SyncCallRestrictions::ForceSyncCallAllowed();

    // If this process is the host and the main thread is being used by the
    // renderer, we need to create another thread to run the process host.
    d_browserThread.reset(new BrowserThread(sandboxInfo));

    // Normally we let the embedder call createHostChannel() to create a
    // process host.  Since the browser code is running in this process, there
    // is no need for the embedder to tell us to establish a loop-back
    // connection.  We'll just create the process host in this process on the
    // newly spawned browser thread.
    d_browserThread->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&createLoopbackHostChannel,
                   &hostChannel,
                   isolated,
                   profileDir,
                   d_browserThread->task_runner()));

    // Wait for process host to come alive.
    LOG(INFO) << "Waiting for ProcessHost on the browser thread";
    d_browserThread->sync();
    CHECK(!hostChannel.empty());
    LOG(INFO) << "ProcessHost on the browser thread has been initialized";

    return hostChannel;
}

ToolkitImpl *ToolkitImpl::instance()
{
    return g_instance;
}

static bool
startsWith(const std::string& hay, const std::string& needle)
{
    return hay.compare(0, needle.size(), needle) == 0;
}

ToolkitImpl::ToolkitImpl(const std::string&              dictionaryPath,
                         const std::string&              hostChannel,
                         const std::vector<std::string>& cmdLineSwitches,
                         bool                            isolated,
                         const std::string&              profileDir)
    : d_mainDelegate(false)
{
    ChannelInfo channelInfo;
    std::string currentHostChannel = hostChannel;
    std::vector<std::string> args = cmdLineSwitches;
    bool isHost = currentHostChannel.empty();

    DCHECK(!g_instance);
    g_instance = this;
    content::InitializeMojo();
    base::CommandLine::Init(0, nullptr);

    // Setup sandbox
    sandbox::SandboxInterfaceInfo sandboxInfo;
    setupSandbox(&sandboxInfo, &args, isHost);

    // Setup path to dictionary files.
    setupDictionaryFiles(dictionaryPath);

    // Create a process host if we necessary.
    if (isHost && Statics::isRendererMainThreadMode()) {
        // Apply command line switches to content.
        applySwitchesToContentMainDelegate(&d_mainDelegate, args);

        // Initialize content.
        initializeContent(sandboxInfo);

        DCHECK(currentHostChannel.empty());
        currentHostChannel = createProcessHost(sandboxInfo,
                                               isolated,
                                               profileDir);

        // The renderer running on the main thread and the browser running
        // on the browser thread already share most of the command line
        // arguments.  The only useful piece of information that we can
        // extract out of 'currentHostChannel' is the Mojo service request
        // token and the controller handler.

        // Deserialize channel info.
        bool check = channelInfo.deserialize(currentHostChannel);
        DCHECK(check);

        // Apply command line switches from channel info.
        getSwitchesFromHostChannel(&args, channelInfo);

        std::vector<std::string> filteredArgs;

        for (const auto& arg : args) {
            if (startsWith(arg, service_manager::switches::kServiceRequestAttachmentName)) {
                continue;
            }

            filteredArgs.push_back(arg);
        }

        appendCommandLine(filteredArgs);
    }
    else {
        if (!isHost) {
            // Deserialize channel info.
            bool check = channelInfo.deserialize(currentHostChannel);
            DCHECK(check);

            // Apply command line switches from channel info.
            getSwitchesFromHostChannel(&args, channelInfo);
        }

        // Apply command line switches to content.
        applySwitchesToContentMainDelegate(&d_mainDelegate, args);

        // Initialize content.
        initializeContent(sandboxInfo);
    }

    if (!isHost) {
        const base::CommandLine& command_line =
            *base::CommandLine::ForCurrentProcess();
        std::string process_type =
            command_line.GetSwitchValueASCII(switches::kProcessType);
        // Chromium 62 checks field trials. Without the following initialization, there will be a failure with the following calls:
 	    // base::FieldTrialList::GetInitiallyActiveFieldTrials(...) Line 719
        // from: variations::ChildProcessFieldTrialSyncer::InitFieldTrialObserving(...) Line 34
 	    // from: content::ChildThreadImpl::Init(...) Line 618

        // The following code is adapted from InitializeFieldTrialAndFeatureList(..) in content_main_runner.cc
        //
        // Initialize statistical testing infrastructure.  We set the entropy
        // provider to nullptr to disallow non-browser processes from creating
        // their own one-time randomized trials; they should be created in the
        // browser process.
        field_trial_list.reset(new base::FieldTrialList(nullptr));

        // Run this logic on all child processes. Zygotes will run this at a
        // later point in time when the command line has been updated.
        base::FieldTrialList::CreateTrialsFromCommandLine(
            command_line, switches::kFieldTrialHandle, -1);

        std::unique_ptr<base::FeatureList> feature_list(
            new base::FeatureList);
        base::FieldTrialList::CreateFeaturesFromCommandLine(
            command_line, switches::kEnableFeatures,
        switches::kDisableFeatures, feature_list.get());
        base::FeatureList::SetInstance(std::move(feature_list));    
    }
    // Start pumping the message loop.
    startMessageLoop(sandboxInfo);

    if (Statics::isRendererMainThreadMode()) {
        // Initialize the renderer.
        DCHECK(!currentHostChannel.empty());
        ContentBrowserClientImpl* pBrowserClientImpl = d_mainDelegate.GetContentBrowserClientImpl();
        startRenderer(isHost, channelInfo, pBrowserClientImpl ? pBrowserClientImpl->GetClientInvitation() : nullptr);
    }

    ui::InitializeInputMethod();
    setDefaultLocaleIfWindowsLocaleIsNotSupported();
}

ToolkitImpl::~ToolkitImpl()
{
    LOG(INFO) << "Shutting down threads...";
    ScopeExitGuard exit_guard{EXIT_TIME_OUT_MS};

    if (Statics::isRendererMainThreadMode()) {
        if (d_browserThread.get()) {
            d_browserThread->task_runner()->PostTask(
                FROM_HERE,
                base::Bind(&ProcessHostImpl::releaseAll));

            // Make sure any tasks posted to the browser-main thread have been
            // handled.
            d_browserThread->sync();
        }
    }
    else {
        DCHECK(Statics::isOriginalThreadMode());
        ProcessHostImpl::releaseAll();
    }

    DCHECK(nullptr == ProfileImpl::anyInstance());

    d_messagePump->flush();

    if (Statics::isInProcessRendererEnabled)
        InProcessRenderer::cleanup();

    d_messagePump->cleanup();

    if (Statics::isRendererMainThreadMode()) {
        d_renderMainMessageLoop.reset();
        d_browserThread.reset();
    }
    else {
        DCHECK(Statics::isOriginalThreadMode());
        d_browserMainRunner.reset();
    }

    d_mainRunner->Shutdown();
    d_mainRunner.reset();

    DCHECK(g_instance);
    g_instance = nullptr;
}

bool ToolkitImpl::hasDevTools()
{
    DCHECK(Statics::isInApplicationMainThread());
    return Statics::hasDevTools;
}

void ToolkitImpl::destroy()
{
    DCHECK(Statics::isInApplicationMainThread());
    delete this;
}

String ToolkitImpl::createHostChannel(int              pid,
                                      bool             isolated,
                                      const StringRef& dataDir)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(GetCurrentProcessId() != static_cast<unsigned>(pid));

    if (Statics::isOriginalThreadMode()) {
        std::string hostChannel =
            ProcessHostImpl::createHostChannel(
                    static_cast<base::ProcessId>(pid),
                    isolated,
                    std::string(dataDir.data(), dataDir.size()),
                    d_browserThread->task_runner());
        return String(hostChannel.data(), hostChannel.size());
    }

    DCHECK(Statics::isRendererMainThreadMode());

    Profile *profile = ProfileImpl::anyInstance();
    DCHECK(profile);

    return profile->createHostChannel(
            pid, isolated, std::string(dataDir.data(), dataDir.size()));
}

Profile *ToolkitImpl::getProfile(int pid, bool launchDevtoolsServer)
{
    // TODO(imran): Return the browser context in ORIGINAL thread mode
    DCHECK(Statics::isRendererMainThreadMode());
    return new ProfileImpl(d_messagePump, pid, launchDevtoolsServer);
}

bool ToolkitImpl::preHandleMessage(const NativeMsg *msg)
{
    DCHECK(Statics::isInApplicationMainThread());
    return d_messagePump->preHandleMessage(*msg);
}

void ToolkitImpl::postHandleMessage(const NativeMsg *msg)
{
    DCHECK(Statics::isInApplicationMainThread());
    return d_messagePump->postHandleMessage(*msg);
}

void ToolkitImpl::addOriginToTrustworthyList(const StringRef& originString)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(Statics::isRendererMainThreadMode());
    blink::WebSecurityPolicy::AddOriginToTrustworthySafelist(toWebString(originString));
}

void ToolkitImpl::setWebViewHostObserver(WebViewHostObserver* observer)
{
    if (Statics::isInBrowserMainThread()) {
        Statics::webViewHostObserver = observer;
    }
    else if (d_browserThread) {
        d_browserThread->task_runner()->PostTask(
                FROM_HERE,
                base::Bind(&ToolkitImpl::setWebViewHostObserver,
                           base::Unretained(this),
                           observer));
    }
}

void ToolkitImpl::setTraceThreshold(unsigned int timeoutMS)
{
    d_messagePump->setTraceThreshold(timeoutMS);
}



// patch section: embedder ipc


// patch section: expose v8 platform


// patch section: multi-heap tracer



}  // close namespace blpwtk2

// vim: ts=4 et

