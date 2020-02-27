/*
 * Copyright (C) 2015 Bloomberg Finance L.P.
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

#include <blpwtk2_contentutilityclientimpl.h>
#include <chrome/utility/chrome_content_utility_client.h>
#include <content/public/utility/utility_thread.h>
#include <ipc/ipc_message_macros.h>

namespace blpwtk2 {

ContentUtilityClientImpl::ContentUtilityClientImpl()
    : chrome_utility_client_(std::make_unique<ChromeContentUtilityClient>()) {}

ContentUtilityClientImpl::~ContentUtilityClientImpl() {}

void ContentUtilityClientImpl::UtilityThreadStarted() {
  chrome_utility_client_->UtilityThreadStarted();
}
bool ContentUtilityClientImpl::OnMessageReceived(const IPC::Message& message) {
  return chrome_utility_client_->OnMessageReceived(message);
}
bool ContentUtilityClientImpl::HandleServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
    return chrome_utility_client_->HandleServiceRequest(service_name, std::move(request));
}

void ContentUtilityClientImpl::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  chrome_utility_client_->RegisterNetworkBinders(registry);
}

}  // close namespace blpwtk2

// vim: ts=4 et

