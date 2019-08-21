/*
 * Copyright (C) 2018 Bloomberg Finance L.P.
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

#include <blpwtk2_devtoolsmanagerdelegateimpl.h>
#include <blpwtk2_contentclient.h>

#include <stdint.h>

#include <vector>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_content_client.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_server_socket.h"
#include "ui/base/resource/resource_bundle.h"

namespace blpwtk2 {

namespace {

const int kBackLog = 10;
base::subtle::Atomic32 g_last_used_port;

                        // ============================
                        // class TCPServerSocketFactory
                        // ============================

class TCPServerSocketFactory : public content::DevToolsSocketFactory
{
    std::string d_address;
    uint16_t d_port;

    // content::DevToolsSocketFactory overrides.
    std::unique_ptr<net::ServerSocket> CreateForHttpServer() override;
    std::unique_ptr<net::ServerSocket> CreateForTethering(std::string *out_name) override;

    DISALLOW_COPY_AND_ASSIGN(TCPServerSocketFactory);

  public:
    TCPServerSocketFactory(const std::string& address, uint16_t port);
};

                        // ----------------------------
                        // class TCPServerSocketFactory
                        // ----------------------------

TCPServerSocketFactory::TCPServerSocketFactory(const std::string& address,
                                               uint16_t           port)
    : d_address(address)
    , d_port(port)
{
}

// content::DevToolsSocketFactory.
std::unique_ptr<net::ServerSocket> TCPServerSocketFactory::CreateForHttpServer()
{
    std::unique_ptr<net::ServerSocket> socket(
            new net::TCPServerSocket(nullptr, net::NetLogSource()));
    if (socket->ListenWithAddressAndPort(d_address, d_port, kBackLog) != net::OK) {
        return std::unique_ptr<net::ServerSocket>();
    }

    net::IPEndPoint endpoint;
    if (socket->GetLocalAddress(&endpoint) == net::OK) {
        base::subtle::NoBarrier_Store(&g_last_used_port, endpoint.port());
    }

    return socket;
}

std::unique_ptr<net::ServerSocket> TCPServerSocketFactory::CreateForTethering(
        std::string* out_name)
{
    return nullptr;
}

std::unique_ptr<content::DevToolsSocketFactory> CreateSocketFactory()
{
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();

    // See if the user specified a port on the command line (useful for
    // automation). If not, use an ephemeral port by specifying 0.
    uint16_t port = 0;
    if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
        int temp_port;
        std::string port_str =
            command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
        if (base::StringToInt(port_str, &temp_port) &&
                temp_port >= 0 && temp_port < 65535) {
            port = static_cast<uint16_t>(temp_port);
        }
        else {
            DLOG(WARNING) << "Invalid http debugger port number " << temp_port;
        }
    }
    return std::unique_ptr<content::DevToolsSocketFactory>(
            new TCPServerSocketFactory("127.0.0.1", port));
}
} //  namespace

                        // ---------------------------------
                        // class DevToolsManagerDelegateImpl
                        // ---------------------------------

// static
int DevToolsManagerDelegateImpl::GetHttpHandlerPort()
{
    return base::subtle::NoBarrier_Load(&g_last_used_port);
}

// static
void DevToolsManagerDelegateImpl::StartHttpHandler(
    content::BrowserContext *browser_context)
{
    content::DevToolsAgentHost::StartRemoteDebuggingServer(
            CreateSocketFactory(),
            browser_context->GetPath(),
            base::FilePath());
}

// static
void DevToolsManagerDelegateImpl::StopHttpHandler()
{
    content::DevToolsAgentHost::StopRemoteDebuggingServer();
}

// CREATORS
DevToolsManagerDelegateImpl::DevToolsManagerDelegateImpl()
{
    content::DevToolsAgentHost::AddObserver(this);
}

DevToolsManagerDelegateImpl::~DevToolsManagerDelegateImpl()
{
    content::DevToolsAgentHost::RemoveObserver(this);
}

// DevToolsManagerDelegate overrides

bool DevToolsManagerDelegateImpl::HasBundledFrontendResources()
{
  return true;
}

void DevToolsManagerDelegateImpl::DevToolsAgentHostAttached(content::DevToolsAgentHost* agent_host)
{
    content::WebContents* web_contents = agent_host->GetWebContents();
    if (!web_contents) {
        return;
    }

    web_contents->DevToolsAgentHostAttached();
}

void DevToolsManagerDelegateImpl::DevToolsAgentHostDetached(content::DevToolsAgentHost* agent_host)
{
    content::WebContents* web_contents = agent_host->GetWebContents();
    if (!web_contents) {
        return;
    }

    web_contents->DevToolsAgentHostDetached();
}

}  // close namespace blpwtk2

