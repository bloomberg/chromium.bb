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

#include <blpwtk2_contentmaindelegateimpl.h>

#include <blpwtk2_contentbrowserclientimpl.h>
#include <blpwtk2_contentrendererclientimpl.h>
#include <blpwtk2_contentutilityclientimpl.h>
#include <blpwtk2_products.h>
#include <blpwtk2_statics.h>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/path_service.h>
#include <content/public/common/content_switches.h>
#include <content/public/common/user_agent.h>
#include <ui/base/resource/resource_bundle.h>
#include <ui/base/resource/resource_bundle_win.h>
#include <ui/base/ui_base_switches.h>

extern HANDLE g_instDLL;

namespace blpwtk2 {

static void InitLogging()
{
    logging::LoggingSettings settings;
    if (!logging::GetWtk2LogMessageHandler()) {
        settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
        logging::SetLogItems(true, true, true, true);
    }
    else {
        settings.logging_dest = logging::LOG_NONE;
    }
    logging::InitLogging(settings);
}

                        // -----------------------------
                        // class ContentMainDelegateImpl
                        // -----------------------------

ContentMainDelegateImpl::ContentMainDelegateImpl(bool isSubProcess)
    : d_isSubProcess(isSubProcess)
{
}

ContentMainDelegateImpl::~ContentMainDelegateImpl()
{
}

void ContentMainDelegateImpl::appendCommandLineSwitch(const char* switchString)
{
    if (0 == std::strncmp(switchString, "--", 2))
        switchString += 2;
    d_commandLineSwitches.push_back(switchString);
}

// ContentMainDelegate implementation
bool ContentMainDelegateImpl::BasicStartupComplete(int* exit_code)
{
    base::CommandLine* commandLine = base::CommandLine::ForCurrentProcess();

    // Add all the command-line switches provided by the application.
    for (size_t i = 0; i < d_commandLineSwitches.size(); ++i) {
        const std::string& switchString = d_commandLineSwitches[i];
        size_t eqPos = switchString.find('=');
        if (std::string::npos == eqPos) {
            if (!commandLine->HasSwitch(switchString)) {
                commandLine->AppendSwitch(switchString);
            }
        }
        else {
            commandLine->AppendSwitchASCII(switchString.substr(0, eqPos),
                                           switchString.substr(eqPos+1));
        }
    }

    // point to our renderer
    if (!commandLine->HasSwitch(switches::kBrowserSubprocessPath)) {
        base::FilePath subprocess;
        bool success = base::PathService::Get(base::DIR_EXE, &subprocess);
        DCHECK(success);
        subprocess = subprocess.AppendASCII(BLPWTK2_SUBPROCESS_EXE_NAME);
        commandLine->AppendSwitchNative(switches::kBrowserSubprocessPath,
                                        subprocess.value().c_str());
    }

    // Even if the app has disabled the in-process renderer, we must make sure
    // it is not disabled if --single-process is on the command-line.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
        Statics::isInProcessRendererEnabled = true;

    InitLogging();
    SetContentClient(&d_contentClient);

    return false;
}

void ContentMainDelegateImpl::PreSandboxStartup()
{
    const base::CommandLine* commandLine = base::CommandLine::ForCurrentProcess();
    std::string processType = commandLine->GetSwitchValueASCII(switches::kProcessType);

    ui::SetResourcesDataDLL((HINSTANCE)g_instDLL);
    ui::ResourceBundle::InitSharedInstance();
    ui::ResourceBundle::GetSharedInstance().AddDLLResources();
    if (!d_isSubProcess || processType == switches::kRendererProcess) {
        // Load the devtools pak file in the renderer process as well, because
        // there are blink devtools resources that we need.
        base::FilePath pak_file;
        base::FilePath pak_dir;
        base::PathService::Get(base::DIR_MODULE, &pak_dir);
        pak_file = pak_dir.AppendASCII(BLPWTK2_PAK_NAME);
        if (base::PathExists(pak_file)) {
            Statics::hasDevTools = true;
            ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
                pak_file,
                ui::SCALE_FACTOR_NONE);
        }
    }
}

content::ContentBrowserClient*
ContentMainDelegateImpl::CreateContentBrowserClient()
{
    CHECK(!d_contentBrowserClient.get());
    d_contentBrowserClient.reset(new ContentBrowserClientImpl());
    return d_contentBrowserClient.get();
}

content::ContentRendererClient*
ContentMainDelegateImpl::CreateContentRendererClient()
{
    CHECK(!d_contentRendererClient.get());
    d_contentRendererClient.reset(new ContentRendererClientImpl());
    return d_contentRendererClient.get();
}

content::ContentUtilityClient*
ContentMainDelegateImpl::CreateContentUtilityClient()
{
    CHECK(!d_contentUtilityClient.get());
    d_contentUtilityClient.reset(new ContentUtilityClientImpl());
    return d_contentUtilityClient.get();
}

ContentBrowserClientImpl*
ContentMainDelegateImpl::GetContentBrowserClientImpl() {
  return static_cast<ContentBrowserClientImpl*>(
      d_contentBrowserClient.get());
}

}  // close namespace blpwtk2

// vim: ts=4 et

