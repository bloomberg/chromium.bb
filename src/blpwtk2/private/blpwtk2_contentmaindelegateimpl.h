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

#ifndef INCLUDED_BLPWTK2_CONTENTMAINDELEGATEIMPL_H
#define INCLUDED_BLPWTK2_CONTENTMAINDELEGATEIMPL_H

#include <blpwtk2_config.h>
#include <blpwtk2_contentclient.h>

#include <content/public/app/content_main_delegate.h>
#include <chrome/common/chrome_content_client.h>
#include <memory>

namespace blpwtk2 {
class ContentBrowserClientImpl;
class RendererInfoMap;

                        // =============================
                        // class ContentMainDelegateImpl
                        // =============================

// This is our implementation of the content::ContentMainDelegate interface.
// This allows us to hook into the "main" of the content module (the
// content::ContentMainRunner class).
class ContentMainDelegateImpl : public content::ContentMainDelegate {
    // DATA
    std::vector<std::string> d_commandLineSwitches;
    ContentClient d_contentClient;
    std::unique_ptr<content::ContentBrowserClient> d_contentBrowserClient;
    std::unique_ptr<content::ContentRendererClient> d_contentRendererClient;
    std::unique_ptr<content::ContentUtilityClient> d_contentUtilityClient;
    bool d_isSubProcess;

    DISALLOW_COPY_AND_ASSIGN(ContentMainDelegateImpl);

  public:
    // CREATORS
    explicit ContentMainDelegateImpl(bool isSubProcess);
    ~ContentMainDelegateImpl() final;

    // MANIPULATORS
    void appendCommandLineSwitch(const char *switchString);

    // ContentMainDelegate implementation
    bool BasicStartupComplete(int *exit_code) override;
    void PreSandboxStartup() override;
    content::ContentBrowserClient *CreateContentBrowserClient() override;
    content::ContentRendererClient *CreateContentRendererClient() override;
    content::ContentUtilityClient *CreateContentUtilityClient() override;

    // Return nullptr if ContentBrowserClient does not exist
    ContentBrowserClientImpl* GetContentBrowserClientImpl();
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_CONTENTMAINDELEGATEIMPL_H

// vim: ts=4 et

