// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/dbus/proxy_resolution_service_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromeos {

// Helper for calling ProxyResolutionServiceProvider's |ResolveProxyInternal()|
// method. Unlike the unit-tests which mock the network setup, this uses the
// default dependencies from the running browser.
class ProxyResolutionServiceProviderTestWrapper {
 public:
  ProxyResolutionServiceProviderTestWrapper() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  ~ProxyResolutionServiceProviderTestWrapper() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  // Calls ResolveProxyInternal() and returns its result synchronously as a
  // single string (which may be prefixed by "ERROR: " if it is an error message
  // as opposed to a proxy result).
  std::string ResolveProxyAndWait(const std::string& url) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    base::RunLoop run_loop;

    std::string result;
    impl_.ResolveProxyInternal(
        url,
        base::BindOnce(
            &ProxyResolutionServiceProviderTestWrapper::OnResolveProxyComplete,
            &result, run_loop.QuitClosure()));

    run_loop.Run();

    return result;
  }

 private:
  static void OnResolveProxyComplete(std::string* result,
                                     base::RepeatingClosure quit_closure,
                                     const std::string& error,
                                     const std::string& pac_string) {
    if (!error.empty()) {
      *result = "ERROR: " + error;
    } else {
      *result = pac_string;
    }

    std::move(quit_closure).Run();
  }

  ProxyResolutionServiceProvider impl_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolutionServiceProviderTestWrapper);
};

// Base test fixture that exposes a way to invoke ProxyResolutionServiceProvider
// synchronously from the UI thread.
class ProxyResolutionServiceProviderBaseBrowserTest
    : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    proxy_service_ =
        std::make_unique<ProxyResolutionServiceProviderTestWrapper>();
  }

  void TearDownOnMainThread() override {
    proxy_service_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  std::string ResolveProxyAndWait(const std::string& source_url) {
    return proxy_service_->ResolveProxyAndWait(source_url);
  }

 private:
  std::unique_ptr<ProxyResolutionServiceProviderTestWrapper> proxy_service_;
};

// Fixture that launches the browser with --proxy-server="https://proxy.test".
class ProxyResolutionServiceProviderManualProxyBrowserTest
    : public ProxyResolutionServiceProviderBaseBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kProxyServer,
                                    "https://proxy.test");
  }
};

// Tests that the D-Bus proxy resolver returns the correct result when using
// --proxy-server flag. These resolutions will happen synchronously at the //net
// layer.
IN_PROC_BROWSER_TEST_F(ProxyResolutionServiceProviderManualProxyBrowserTest,
                       ResolveProxy) {
  EXPECT_EQ("HTTPS proxy.test:443",
            ResolveProxyAndWait("http://www.google.com"));
}

// Simple PAC script that returns the same two proxies for all requests.
const char kPacData[] =
    "function FindProxyForURL(url, host) {\n"
    "  return 'PROXY foo1; PROXY foo2';\n"
    "}\n";

// Fixture that launches the browser with --proxy-pac-url="data:...".
class ProxyResolutionServiceProviderPacBrowserTest
    : public ProxyResolutionServiceProviderBaseBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kProxyPacUrl, GetPacUrl());
  }

 private:
  // Encode the PAC script as a data: URL.
  static std::string GetPacUrl() {
    std::string b64_encoded;
    base::Base64Encode(kPacData, &b64_encoded);
    return "data:application/x-javascript-config;base64," + b64_encoded;
  }
};

// Tests that the D-Bus proxy resolver returns the correct result when using
// --proxy-pac-url flag. These resolutions will happen asynchronously at the
// //net layer, as they need to query a PAC script.
IN_PROC_BROWSER_TEST_F(ProxyResolutionServiceProviderPacBrowserTest,
                       ResolveProxy) {
  EXPECT_EQ("PROXY foo1:80;PROXY foo2:80",
            ResolveProxyAndWait("http://www.google.com"));
}

}  // namespace chromeos
