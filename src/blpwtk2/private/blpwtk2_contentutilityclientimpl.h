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

#ifndef INCLUDED_BLPWTK2_CONTENTUTILITYCLIENTIMPL_H
#define INCLUDED_BLPWTK2_CONTENTUTILITYCLIENTIMPL_H

#include <content/public/utility/content_utility_client.h>
#include <memory>

class ChromeContentUtilityClient;

namespace blpwtk2 {

// This interface allows us to add hooks to the "utility" portion of the
// content module.  This is created during the startup process.
class ContentUtilityClientImpl : public content::ContentUtilityClient {
  public:
    ContentUtilityClientImpl();
    ~ContentUtilityClientImpl() final;

    // content::ContentUtilityClient:
    void UtilityThreadStarted() override;
    bool OnMessageReceived(const IPC::Message& message) override;
    bool HandleServiceRequest(
        const std::string& service_name,
        service_manager::mojom::ServiceRequest request) override;
    void RegisterNetworkBinders(
        service_manager::BinderRegistry* registry) override;

  private:
    std::unique_ptr<ChromeContentUtilityClient> chrome_utility_client_;

    DISALLOW_COPY_AND_ASSIGN(ContentUtilityClientImpl);
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_CONTENTUTILITYCLIENTIMPL_H

// vim: ts=4 et

