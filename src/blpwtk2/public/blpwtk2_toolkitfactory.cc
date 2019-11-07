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

#include <blpwtk2_toolkitfactory.h>

#include <blpwtk2_products.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_stringref.h>
#include <blpwtk2_toolkitcreateparams.h>
#include <blpwtk2_toolkitimpl.h>

#include <base/command_line.h>
#include <base/environment.h>
#include <base/files/file_path.h>
#include <base/logging.h>  // for DCHECK
#include <base/strings/string16.h>
#include <base/strings/utf_string_conversions.h>
#include <base/win/wrapped_window_proc.h>
#include "components/printing/renderer/print_render_frame_helper.h"
#include <content/child/font_warmup_win.h>
#include <content/public/app/content_main_runner.h>
#include <content/renderer/render_frame_impl.h>
#include <content/renderer/render_widget.h>
#include <net/http/http_network_session.h>
#include <net/socket/client_socket_pool_manager.h>
#include <printing/print_settings.h>
#include <ui/views/corewm/tooltip_win.h>

namespace blpwtk2 {
static bool g_created = false;

static void setMaxSocketsPerProxy(int count)
{
    DCHECK(1 <= count);
    DCHECK(99 >= count);

    const net::HttpNetworkSession::SocketPoolType POOL =
        net::HttpNetworkSession::NORMAL_SOCKET_POOL;

    // The max per group can never exceed the max per proxy.  Use the default
    // max per group, unless count is less than the default.

    int prevMaxPerProxy =
        net::ClientSocketPoolManager::max_sockets_per_proxy_server(POOL);
    int newMaxPerGroup = std::min(count,
                                  (int)net::kDefaultMaxSocketsPerGroupNormal);

    if (newMaxPerGroup > prevMaxPerProxy) {
        net::ClientSocketPoolManager::set_max_sockets_per_proxy_server(
            POOL,
            count);
        net::ClientSocketPoolManager::set_max_sockets_per_group(
            POOL,
            newMaxPerGroup);
    }
    else {
        net::ClientSocketPoolManager::set_max_sockets_per_group(
            POOL,
            newMaxPerGroup);
        net::ClientSocketPoolManager::set_max_sockets_per_proxy_server(
            POOL,
            count);
    }
}

						// ---------------------
						// struct ToolkitFactory
						// ---------------------

// static
Toolkit* ToolkitFactory::create(const ToolkitCreateParams& params)
{
    DCHECK(!g_created);
    DCHECK(!ToolkitImpl::instance());

    Statics::initApplicationMainThread();
    Statics::threadMode = params.threadMode();
    Statics::inProcessResourceLoader = params.inProcessResourceLoader();
    Statics::isInProcessRendererEnabled = params.isInProcessRendererEnabled();
    Statics::channelErrorHandler = params.channelErrorHandler();
    Statics::inProcessResizeOptimizationDisabled = params.isInProcessResizeOptimizationDisabled();
    Statics::toolkitDelegate = params.delegate();

    // If this process is the host, then set the environment variable that
    // subprocesses will use to determine which SubProcessMain module should
    // be loaded.
    if (params.hostChannel().isEmpty()) {
        char subProcessModuleEnvVar[64];
        sprintf_s(subProcessModuleEnvVar, sizeof(subProcessModuleEnvVar),
                  "BLPWTK2_SUBPROCESS_%d_%d_%d_%d_%d",
                  CHROMIUM_VERSION_MAJOR,
                  CHROMIUM_VERSION_MINOR,
                  CHROMIUM_VERSION_BUILD,
                  CHROMIUM_VERSION_PATCH,
                  BB_PATCH_NUMBER);
        std::string subProcessModule = params.subProcessModule().toStdString();
        if (subProcessModule.empty()) {
            subProcessModule = BLPWTK2_DLL_NAME;
        }
        std::unique_ptr<base::Environment> env(base::Environment::Create());
        env->SetVar(subProcessModuleEnvVar, subProcessModule);
	}

    base::win::SetWinProcExceptionFilter(params.winProcExceptionFilter());



    // patch section: custom CRT handler


    // patch section: search highlight


    // patch section: tooltip



    if (params.isMaxSocketsPerProxySet()) {
        setMaxSocketsPerProxy(params.maxSocketsPerProxy());
    }

    std::vector<std::string> commandLineSwitches;

    for (size_t i = 0; i < params.numCommandLineSwitches(); ++i) {
        StringRef switchRef = params.commandLineSwitchAt(i);
        std::string switchString(switchRef.data(), switchRef.length());
        commandLineSwitches.push_back(switchString);
    }

    std::string dictionaryPath(params.dictionaryPath().data(),
                               params.dictionaryPath().length());
    std::string hostChannel(params.hostChannel().data(),
                            params.hostChannel().length());
    std::string profileDirectory(params.profileDirectory().data(),
                                 params.profileDirectory().length());

    std::string html(params.headerFooterHTMLContent().data(),
                     params.headerFooterHTMLContent().length());
    printing::PrintSettings::SetDefaultPrinterSettings(
        base::UTF8ToUTF16(html), params.isPrintBackgroundGraphicsEnabled());

    ToolkitImpl* toolkit = new ToolkitImpl(dictionaryPath,
                                           hostChannel,
                                           commandLineSwitches,
                                           params.isIsolatedProfile(),
                                           profileDirectory);

    g_created = true;
    return toolkit;
}

}  // close namespace blpwtk2

// vim: ts=4 et

