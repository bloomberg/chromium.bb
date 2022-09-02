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

#include <blpwtk2_devtoolsfrontendhostdelegateimpl.h>

#include <blpwtk2_urlrequestcontextgetterimpl.h>
#include <blpwtk2_statics.h>

#include <base/containers/span.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/json/string_escape.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/utf_string_conversions.h>
#include <base/values.h>
#include <content/public/browser/browser_context.h>
#include <content/public/browser/browser_task_traits.h>
#include <content/public/browser/browser_thread.h>
#include <content/public/browser/devtools_agent_host.h>
#include <content/public/browser/render_frame_host.h>
#include <content/public/browser/web_contents.h>
#include <content/public/browser/storage_partition.h>
#include <ipc/ipc_channel.h>
#include <net/base/io_buffer.h>
#include <net/base/net_errors.h>
#include <net/http/http_response_headers.h>
#include <net/url_request/url_fetcher.h>
#include <net/url_request/url_fetcher_response_writer.h>

#include "base/base64.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace blpwtk2 {

namespace {

base::DictionaryValue BuildObjectForResponse(const net::HttpResponseHeaders* rh,
                                             bool success,
                                             int net_error) {
  base::DictionaryValue response = base::DictionaryValue();
  int responseCode = 200;
  if (rh) {
    responseCode = rh->response_code();
  } else if (!success) {
    // In case of no headers, assume file:// URL and failed to load
    responseCode = 404;
  }
  response.SetInteger("statusCode", responseCode);
  response.SetInteger("netError", net_error);
  response.SetString("netErrorName", net::ErrorToString(net_error));

  auto headers = std::make_unique<base::DictionaryValue>();
  size_t iterator = 0;
  std::string name;
  std::string value;
  // TODO(caseq): this probably needs to handle duplicate header names
  // correctly by folding them.
  while (rh && rh->EnumerateHeaderLines(&iterator, &name, &value))
    headers->SetString(name, value);

  response.Set("headers", std::move(headers));
  return response;
}

}  // namespace


// This constant should be in sync with
// the constant at shell_devtools_bindings.h.
// This was copied from shell_devtools_frontend.cc
const size_t kMaxMessageChunkSize = IPC::Channel::kMaximumMessageSize / 4;


class DevToolsFrontendHostDelegateImpl::NetworkResourceLoader
    : public network::SimpleURLLoaderStreamConsumer {
 public:
  NetworkResourceLoader(int stream_id,
                        int request_id,
                        DevToolsFrontendHostDelegateImpl* bindings,
                        std::unique_ptr<network::SimpleURLLoader> loader,
                        network::mojom::URLLoaderFactory* url_loader_factory)
      : stream_id_(stream_id),
        request_id_(request_id),
        bindings_(bindings),
        loader_(std::move(loader)) {
    loader_->SetOnResponseStartedCallback(base::BindOnce(
        &NetworkResourceLoader::OnResponseStarted, base::Unretained(this)));
    loader_->DownloadAsStream(url_loader_factory, this);
  }
  NetworkResourceLoader(const NetworkResourceLoader&) = delete;
  NetworkResourceLoader& operator=(const NetworkResourceLoader&) = delete;

 private:
  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head) {
    response_headers_ = response_head.headers;
  }

  void OnDataReceived(base::StringPiece chunk,
                      base::OnceClosure resume) override {
    base::Value chunkValue;

    bool encoded = !base::IsStringUTF8(chunk);
    if (encoded) {
      std::string encoded_string;
      base::Base64Encode(chunk, &encoded_string);
      chunkValue = base::Value(std::move(encoded_string));
    } else {
      chunkValue = base::Value(chunk);
    }
    base::Value id(stream_id_);
    base::Value encodedValue(encoded);

    bindings_->CallClientFunction("DevToolsAPI", "streamWrite", std::move(id),
                                  std::move(chunkValue),
                                  std::move(encodedValue));
    std::move(resume).Run();
  }

  void OnComplete(bool success) override {
    auto response = BuildObjectForResponse(response_headers_.get(), success,
                                           loader_->NetError());
    bindings_->SendMessageAck(request_id_, std::move(response));
    bindings_->loaders_.erase(bindings_->loaders_.find(this));
  }

  void OnRetry(base::OnceClosure start_retry) override { NOTREACHED(); }

  const int stream_id_;
  const int request_id_;
  DevToolsFrontendHostDelegateImpl* const bindings_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
};


DevToolsFrontendHostDelegateImpl::DevToolsFrontendHostDelegateImpl(
    content::WebContents* inspectorContents,
    content::WebContents* inspectedContents)
: WebContentsObserver(inspectorContents)
, d_inspectedContents(inspectedContents)
, d_inspectElementPointPending(false)
, d_weakFactory(this)
{
}

DevToolsFrontendHostDelegateImpl::~DevToolsFrontendHostDelegateImpl()
{
    for (const auto& pair : d_pendingRequests)
        delete pair.first;

    // Delete the temporary directory that we created in the constructor.
    // allow IO during deletion of temporary directory
    if (d_requestContextGetter.get()) {
        base::ThreadRestrictions::ScopedAllowIO allowIO;
        DCHECK(base::PathExists(d_requestContextGetter->path()));
        base::DeletePathRecursively(d_requestContextGetter->path());
        d_requestContextGetter = nullptr;
    }
}

void DevToolsFrontendHostDelegateImpl::inspectElementAt(const POINT& point)
{
    if (agent_host_) {
        agent_host_->InspectElement(d_inspectedContents->GetMainFrame(), point.x, point.y);
        return;
    }
    d_inspectElementPointPending = true;
    d_inspectElementPoint = point;
}

// ======== WebContentsObserver overrides ============

void DevToolsFrontendHostDelegateImpl::RenderFrameCreated(
        content::RenderFrameHost* renderFrameHost)
{
    if (!d_frontendHost) {
      d_frontendHost = content::DevToolsFrontendHost::Create(
          web_contents()->GetMainFrame(),
          base::BindRepeating(
              &DevToolsFrontendHostDelegateImpl::HandleMessageFromDevToolsFrontend,
              base::Unretained(this)));
    }
}

void DevToolsFrontendHostDelegateImpl::PrimaryMainDocumentElementAvailable()
{
    agent_host_ = content::DevToolsAgentHost::GetOrCreateFor(d_inspectedContents);
    agent_host_->AttachClient(this);
    if (d_inspectElementPointPending) {
        d_inspectElementPointPending = false;
        agent_host_->InspectElement(d_inspectedContents->GetMainFrame(), d_inspectElementPoint.x, d_inspectElementPoint.y);
    }
}

void DevToolsFrontendHostDelegateImpl::WebContentsDestroyed()
{
    if (agent_host_) {
        agent_host_->DetachClient(this);
        agent_host_ = nullptr;
    }
}

void DevToolsFrontendHostDelegateImpl::HandleMessageFromDevToolsFrontend(
    base::Value message)
{
  // This implementation was copied from shell_devtools_bindings.cc

  if (!message.is_dict())
    return;
  const std::string* method = message.FindStringKey("method");
  if (!method)
    return;

  int request_id = message.FindIntKey("id").value_or(0);
  base::Value* params_value = message.FindListKey("params");

  // Since we've received message by value, we can take the list.
  base::Value::ListStorage params;
  if (params_value) {
    params = std::move(*params_value).TakeListDeprecated();
  }

  if (*method == "dispatchProtocolMessage" && params.size() == 1) {
    const std::string* protocol_message = params[0].GetIfString();
    if (!agent_host_ || !protocol_message)
      return;
    agent_host_->DispatchProtocolMessage(
        this, base::as_bytes(base::make_span(*protocol_message)));
  } else if (*method == "loadCompleted") {
    CallClientFunction("DevToolsAPI", "setUseSoftMenu", base::Value(true));
  } else if (*method == "loadNetworkResource" && params.size() == 3) {
    // TODO(pfeldman): handle some of the embedder messages in content.
    const std::string* url = params[0].GetIfString();
    const std::string* headers = params[1].GetIfString();
    absl::optional<const int> stream_id = params[2].GetIfInt();
    if (!url || !headers || !stream_id.has_value()) {
      return;
    }

    GURL gurl(*url);
    if (!gurl.is_valid()) {
      base::Value response(base::Value::Type::DICTIONARY);
      response.SetIntKey("statusCode", 404);
      response.SetBoolKey("urlValid", false);
      SendMessageAck(request_id, std::move(response));
      return;
    }

    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation(
            "devtools_handle_front_end_messages", R"(
            semantics {
              sender: "Developer Tools"
              description:
                "When user opens Developer Tools, the browser may fetch "
                "additional resources from the network to enrich the debugging "
                "experience (e.g. source map resources)."
              trigger: "User opens Developer Tools to debug a web page."
              data: "Any resources requested by Developer Tools."
              destination: OTHER
            }
            policy {
              cookies_allowed: YES
              cookies_store: "user"
              setting:
                "It's not possible to disable this feature from settings."
              chrome_policy {
                DeveloperToolsAvailability {
                  policy_options {mode: MANDATORY}
                  DeveloperToolsAvailability: 2
                }
              }
            })");

    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = gurl;
    // TODO(caseq): this preserves behavior of URLFetcher-based implementation.
    // We really need to pass proper first party origin from the front-end.
    resource_request->site_for_cookies = net::SiteForCookies::FromUrl(gurl);
    resource_request->headers.AddHeadersFromString(*headers);

    auto* partition =
        d_inspectedContents->GetMainFrame()->GetStoragePartition();
    auto factory = partition->GetURLLoaderFactoryForBrowserProcess();

    auto simple_url_loader = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    auto resource_loader = std::make_unique<NetworkResourceLoader>(
        *stream_id, request_id, this, std::move(simple_url_loader),
        factory.get());
    loaders_.insert(std::move(resource_loader));
    return;
  } else if (*method == "getPreferences") {
    SendMessageAck(request_id, std::move(preferences_));
    return;
  } else if (*method == "setPreference") {
    if (params.size() < 2)
      return;
    const std::string* name = params[0].GetIfString();

    // We're just setting params[1] as a value anyways, so just make sure it's
    // the type we want, but don't worry about getting it.
    if (!name || !params[1].is_string())
      return;

    preferences_.SetKey(*name, std::move(params[1]));
  } else if (*method == "removePreference") {
    const std::string* name = params[0].GetIfString();
    if (!name)
      return;
    preferences_.RemoveKey(*name);
  } else if (*method == "requestFileSystems") {
    CallClientFunction("DevToolsAPI", "fileSystemsLoaded",
                       base::Value(base::Value::Type::LIST));
  } else if (*method == "reattach") {
    if (!agent_host_)
      return;
    agent_host_->DetachClient(this);
    agent_host_->AttachClient(this);
  } else if (*method == "registerExtensionsAPI") {
    if (params.size() < 2)
      return;
    const std::string* origin = params[0].GetIfString();
    const std::string* script = params[1].GetIfString();
    if (!origin || !script)
      return;
    extensions_api_[*origin + "/"] = *script;
  } else {
    return;
  }

  if (request_id)
    SendMessageAck(request_id, {});
}

void DevToolsFrontendHostDelegateImpl::DispatchProtocolMessage(
        content::DevToolsAgentHost* agent_host,
        base::span<const uint8_t> message)
{
    // This implementation was copied from shell_devtools_bindings.cc

    base::StringPiece str_message(reinterpret_cast<const char*>(message.data()),
                                message.size());
    if (str_message.length() < kMaxMessageChunkSize) {
        CallClientFunction("DevToolsAPI", "dispatchMessage",
                base::Value(std::string(str_message)));
    } else {
        size_t total_size = str_message.length();
        for (size_t pos = 0; pos < str_message.length();
                pos += kMaxMessageChunkSize) {
            base::StringPiece str_message_chunk =
                str_message.substr(pos, kMaxMessageChunkSize);

            CallClientFunction(
                    "DevToolsAPI", "dispatchMessageChunk",
                    base::Value(std::string(str_message_chunk)),
                    base::Value(base::NumberToString(pos ? 0 : total_size)));
        }
    }
}

void DevToolsFrontendHostDelegateImpl::AgentHostClosed(
        content::DevToolsAgentHost* agentHost)
{
    // TODO: notify blpwtk2 clients?
}

void DevToolsFrontendHostDelegateImpl::CallClientFunction(
    const std::string& object_name,
    const std::string& method_name,
    base::Value arg1,
    base::Value arg2,
    base::Value arg3,
    base::OnceCallback<void(base::Value)> cb) {

  // This implementation was copied from shell_devtools_bindings.cc

  std::string javascript;

  web_contents()->GetMainFrame()->AllowInjectingJavaScript();

  base::Value::List arguments;
  if (!arg1.is_none()) {
    arguments.Append(std::move(arg1));
    if (!arg2.is_none()) {
      arguments.Append(std::move(arg2));
      if (!arg3.is_none()) {
        arguments.Append(std::move(arg3));
      }
    }
  }
  web_contents()->GetMainFrame()->ExecuteJavaScriptMethod(
      base::ASCIIToUTF16(object_name), base::ASCIIToUTF16(method_name),
      std::move(arguments), std::move(cb));
}

void DevToolsFrontendHostDelegateImpl::SendMessageAck(int request_id, base::Value arg) {
  // This implementation was copied from shell_devtools_bindings.cc

  CallClientFunction("DevToolsAPI", "embedderMessageAck",
                     base::Value(request_id), std::move(arg));
}


}  // close namespace blpwtk2

// vim: ts=4 et

