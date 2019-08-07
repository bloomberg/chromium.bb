// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/clear_site_data_throttle.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/resource_type.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_response_info.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// Appending '2' to avoid naming conflicts with 'clear_site_data_handler.h' in
// jumbo builds. This file will be removed later as mentioned in the header.
const char kNameForLogging2[] = "ClearSiteDataThrottle";

const char kClearSiteDataHeader2[] = "Clear-Site-Data";

// Datatypes.
const char kDatatypeWildcard2[] = "\"*\"";
const char kDatatypeCookies2[] = "\"cookies\"";
const char kDatatypeStorage2[] = "\"storage\"";
const char kDatatypeCache2[] = "\"cache\"";

// Pretty-printed log output.
const char kConsoleMessageTemplate2[] = "Clear-Site-Data header on '%s': %s";
const char kConsoleMessageCleared2[] = "Cleared data types: %s.";
const char kConsoleMessageDatatypeSeparator2[] = ", ";

bool AreExperimentalFeaturesEnabled2() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
}

bool IsNavigationRequest(net::URLRequest* request) {
  ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  return info && IsResourceTypeFrame(info->GetResourceType());
}

// Represents the parameters as a single number to be recorded in a histogram.
int ParametersMask2(bool clear_cookies, bool clear_storage, bool clear_cache) {
  return static_cast<int>(clear_cookies) * (1 << 0) +
         static_cast<int>(clear_storage) * (1 << 1) +
         static_cast<int>(clear_cache) * (1 << 2);
}

// A helper function to pass an IO thread callback to a method called on
// the UI thread.
void JumpFromUIToIOThread(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO}, std::move(callback));
}

BrowserContext* GetBrowserContext(
    int process_id,
    const ResourceRequestInfo::WebContentsGetter& web_contents_getter) {
  // TODO(dullweber): Could we always use RenderProcessHost?
  WebContents* web_contents = web_contents_getter.Run();
  if (web_contents)
    return web_contents->GetBrowserContext();

  RenderProcessHost* process_host = RenderProcessHostImpl::FromID(process_id);
  if (process_host)
    return process_host->GetBrowserContext();

  return nullptr;
}

// Outputs a single |formatted_message| on the UI thread.
void OutputFormattedMessage2(WebContents* web_contents,
                             blink::mojom::ConsoleMessageLevel level,
                             const std::string& formatted_text) {
  if (web_contents)
    web_contents->GetMainFrame()->AddMessageToConsole(level, formatted_text);
}

// Outputs |messages| to the console of WebContents retrieved from
// |web_contents_getter|. Must be run on the UI thread.
void OutputMessagesOnUIThread(
    const ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    const std::vector<ClearSiteDataThrottle::ConsoleMessagesDelegate::Message>&
        messages,
    const ClearSiteDataThrottle::ConsoleMessagesDelegate::
        OutputFormattedMessageFunction& output_formatted_message_function) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = web_contents_getter.Run();

  for (const auto& message : messages) {
    // Prefix each message with |kConsoleMessageTemplate2|.
    output_formatted_message_function.Run(
        web_contents, message.level,
        base::StringPrintf(kConsoleMessageTemplate2, message.url.spec().c_str(),
                           message.text.c_str()));
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ConsoleMessagesDelegate

ClearSiteDataThrottle::ConsoleMessagesDelegate::ConsoleMessagesDelegate()
    : output_formatted_message_function_(
          base::BindRepeating(&OutputFormattedMessage2)) {}

ClearSiteDataThrottle::ConsoleMessagesDelegate::~ConsoleMessagesDelegate() {}

void ClearSiteDataThrottle::ConsoleMessagesDelegate::AddMessage(
    const GURL& url,
    const std::string& text,
    blink::mojom::ConsoleMessageLevel level) {
  messages_.push_back({url, text, level});
}

void ClearSiteDataThrottle::ConsoleMessagesDelegate::OutputMessages(
    const ResourceRequestInfo::WebContentsGetter& web_contents_getter) {
  if (messages_.empty())
    return;

  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&OutputMessagesOnUIThread, web_contents_getter,
                     std::move(messages_), output_formatted_message_function_));

  messages_.clear();
}

void ClearSiteDataThrottle::ConsoleMessagesDelegate::
    SetOutputFormattedMessageFunctionForTesting(
        const OutputFormattedMessageFunction& function) {
  output_formatted_message_function_ = function;
}

////////////////////////////////////////////////////////////////////////////////
// ClearSiteDataThrottle

// static
std::unique_ptr<ResourceThrottle>
ClearSiteDataThrottle::MaybeCreateThrottleForRequest(net::URLRequest* request) {
  // The throttle has no purpose if the request has no ResourceRequestInfo,
  // because we won't be able to determine whose data should be deleted.
  if (!ResourceRequestInfo::ForRequest(request))
    return nullptr;

  return base::WrapUnique(new ClearSiteDataThrottle(
      request, std::make_unique<ConsoleMessagesDelegate>()));
}

ClearSiteDataThrottle::~ClearSiteDataThrottle() {
  // Output the cached console messages. For navigations, we output console
  // messages when the request is finished rather than in real time, since in
  // the case of navigations swapping RenderFrameHost would cause the outputs
  // to disappear.
  if (IsNavigationRequest(request_))
    OutputConsoleMessages();
}

const char* ClearSiteDataThrottle::GetNameForLogging() {
  return kNameForLogging2;
}

void ClearSiteDataThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  *defer = HandleHeader();

  // For subresource requests, console messages are output on every redirect.
  // If the redirect is deferred, wait until it is resumed.
  if (!IsNavigationRequest(request_) && !*defer)
    OutputConsoleMessages();
}

void ClearSiteDataThrottle::WillProcessResponse(bool* defer) {
  *defer = HandleHeader();

  // For subresource requests, console messages are output on every redirect.
  // If the redirect is deferred, wait until it is resumed.
  if (!IsNavigationRequest(request_) && !*defer)
    OutputConsoleMessages();
}

// static
bool ClearSiteDataThrottle::ParseHeaderForTesting(
    const std::string& header,
    bool* clear_cookies,
    bool* clear_storage,
    bool* clear_cache,
    ConsoleMessagesDelegate* delegate,
    const GURL& current_url) {
  return ClearSiteDataThrottle::ParseHeader(
      header, clear_cookies, clear_storage, clear_cache, delegate, current_url);
}

ClearSiteDataThrottle::ClearSiteDataThrottle(
    net::URLRequest* request,
    std::unique_ptr<ConsoleMessagesDelegate> delegate)
    : request_(request),
      delegate_(std::move(delegate)),
      weak_ptr_factory_(this) {
  DCHECK(request_);
  DCHECK(delegate_);
}

const GURL& ClearSiteDataThrottle::GetCurrentURL() const {
  return request_->url();
}

const net::HttpResponseHeaders* ClearSiteDataThrottle::GetResponseHeaders()
    const {
  return request_->response_headers();
}

bool ClearSiteDataThrottle::HandleHeader() {
  const net::HttpResponseHeaders* headers = GetResponseHeaders();

  std::string header_value;
  if (!headers ||
      !headers->GetNormalizedHeader(kClearSiteDataHeader2, &header_value)) {
    return false;
  }

  // Only accept the header on secure non-unique origins.
  if (!IsOriginSecure(GetCurrentURL())) {
    delegate_->AddMessage(GetCurrentURL(),
                          "Not supported for insecure origins.",
                          blink::mojom::ConsoleMessageLevel::kError);
    return false;
  }

  url::Origin origin = url::Origin::Create(GetCurrentURL());
  if (origin.opaque()) {
    delegate_->AddMessage(GetCurrentURL(), "Not supported for unique origins.",
                          blink::mojom::ConsoleMessageLevel::kError);
    return false;
  }

  // The LOAD_DO_NOT_SAVE_COOKIES flag prohibits the request from doing any
  // modification to cookies. Clear-Site-Data applies this restriction to other
  // data types as well.
  // TODO(msramek): Consider showing a blocked icon via
  // TabSpecificContentSettings and reporting the action in the "Blocked"
  // section of the cookies dialog in OIB.
  if (request_->load_flags() & net::LOAD_DO_NOT_SAVE_COOKIES) {
    delegate_->AddMessage(
        GetCurrentURL(),
        "The request's credentials mode prohibits modifying cookies "
        "and other local data.",
        blink::mojom::ConsoleMessageLevel::kError);
    return false;
  }

  bool clear_cookies;
  bool clear_storage;
  bool clear_cache;

  if (!ClearSiteDataThrottle::ParseHeader(header_value, &clear_cookies,
                                          &clear_storage, &clear_cache,
                                          delegate_.get(), GetCurrentURL())) {
    return false;
  }

  // If the header is valid, clear the data for this browser context and origin.
  clearing_started_ = base::TimeTicks::Now();

  // Record the call parameters.
  UMA_HISTOGRAM_ENUMERATION(
      "Navigation.ClearSiteData.Parameters",
      ParametersMask2(clear_cookies, clear_storage, clear_cache), (1 << 3));

  base::WeakPtr<ClearSiteDataThrottle> weak_ptr =
      weak_ptr_factory_.GetWeakPtr();

  // Immediately bind the weak pointer to the current thread (IO). This will
  // make a potential misuse on the UI thread DCHECK immediately rather than
  // later when it's correctly used on the IO thread again.
  weak_ptr.get();

  ExecuteClearingTask(
      origin, clear_cookies, clear_storage, clear_cache,
      base::BindOnce(&ClearSiteDataThrottle::TaskFinished, weak_ptr));

  return true;
}

// static
bool ClearSiteDataThrottle::ParseHeader(const std::string& header,
                                        bool* clear_cookies,
                                        bool* clear_storage,
                                        bool* clear_cache,
                                        ConsoleMessagesDelegate* delegate,
                                        const GURL& current_url) {
  if (!base::IsStringASCII(header)) {
    delegate->AddMessage(current_url, "Must only contain ASCII characters.",
                         blink::mojom::ConsoleMessageLevel::kError);
    return false;
  }

  *clear_cookies = false;
  *clear_storage = false;
  *clear_cache = false;

  std::vector<std::string> input_types = base::SplitString(
      header, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::string output_types;

  for (unsigned i = 0; i < input_types.size(); i++) {
    bool* data_type = nullptr;

    if (AreExperimentalFeaturesEnabled2() &&
        input_types[i] == kDatatypeWildcard2) {
      input_types.push_back(kDatatypeCookies2);
      input_types.push_back(kDatatypeStorage2);
      input_types.push_back(kDatatypeCache2);
      continue;
    } else if (input_types[i] == kDatatypeCookies2) {
      data_type = clear_cookies;
    } else if (input_types[i] == kDatatypeStorage2) {
      data_type = clear_storage;
    } else if (input_types[i] == kDatatypeCache2) {
      data_type = clear_cache;
    } else {
      delegate->AddMessage(
          current_url,
          base::StringPrintf("Unrecognized type: %s.", input_types[i].c_str()),
          blink::mojom::ConsoleMessageLevel::kError);
      continue;
    }

    DCHECK(data_type);

    if (*data_type)
      continue;

    *data_type = true;
    if (!output_types.empty())
      output_types += kConsoleMessageDatatypeSeparator2;
    output_types += input_types[i];
  }

  if (!*clear_cookies && !*clear_storage && !*clear_cache) {
    delegate->AddMessage(current_url, "No recognized types specified.",
                         blink::mojom::ConsoleMessageLevel::kError);
    return false;
  }

  // Pretty-print which types are to be cleared.
  // TODO(crbug.com/798760): Remove the disclaimer about cookies.
  std::string console_output =
      base::StringPrintf(kConsoleMessageCleared2, output_types.c_str());
  if (*clear_cookies) {
    console_output +=
        " Clearing channel IDs and HTTP authentication cache is currently not"
        " supported, as it breaks active network connections.";
  }
  delegate->AddMessage(current_url, console_output,
                       blink::mojom::ConsoleMessageLevel::kInfo);

  return true;
}

void ClearSiteDataThrottle::ExecuteClearingTask(const url::Origin& origin,
                                                bool clear_cookies,
                                                bool clear_storage,
                                                bool clear_cache,
                                                base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto* request_info = ResourceRequestInfo::ForRequest(request_);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ClearSiteData,
          base::BindRepeating(&GetBrowserContext, request_info->GetChildID(),
                              request_info->GetWebContentsGetterForRequest()),
          origin, clear_cookies, clear_storage, clear_cache,
          true /*avoid_closing_connections*/,
          base::BindOnce(&JumpFromUIToIOThread, std::move(callback))));
}

void ClearSiteDataThrottle::TaskFinished() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!clearing_started_.is_null());

  UMA_HISTOGRAM_CUSTOM_TIMES("Navigation.ClearSiteData.Duration",
                             base::TimeTicks::Now() - clearing_started_,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromSeconds(1), 50);

  // For subresource requests, console messages are output immediately.
  if (!IsNavigationRequest(request_))
    OutputConsoleMessages();

  Resume();
}

void ClearSiteDataThrottle::OutputConsoleMessages() {
  ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request_);
  if (info)
    delegate_->OutputMessages(info->GetWebContentsGetterForRequest());
}

}  // namespace content
