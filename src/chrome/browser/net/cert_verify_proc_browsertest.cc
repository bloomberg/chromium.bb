// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

// Base class for tests that want to record a net log. The subclass should
// implement the VerifyNetLog method which will be called after the test body
// completes.
class NetLogPlatformBrowserTestBase : public PlatformBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    net_log_path_ = tmp_dir_.GetPath().AppendASCII("netlog.json");
    command_line->AppendSwitchPath(network::switches::kLogNetLog,
                                   net_log_path_);
  }

  void TearDownInProcessBrowserTestFixture() override {
    // When using the --log-net-log command line param, the net log is
    // finalized during the destruction of the network service, which is
    // started before this method is called, but completes asynchronously.
    //
    // Try for up to 5 seconds to read the netlog file.
    constexpr base::TimeDelta kMaxWaitTime = base::TimeDelta::FromSeconds(5);
    constexpr base::TimeDelta kWaitInterval =
        base::TimeDelta::FromMilliseconds(50);
    int tries_left = kMaxWaitTime / kWaitInterval;

    base::Optional<base::Value> parsed_net_log;
    while (true) {
      std::string file_contents;
      ASSERT_TRUE(base::ReadFileToString(net_log_path_, &file_contents));

      parsed_net_log = base::JSONReader::Read(file_contents);
      if (parsed_net_log)
        break;

      if (--tries_left <= 0)
        break;
      // The netlog file did not parse as valid JSON. Probably the Network
      // Service is still shutting down. Wait a bit and try again.
      base::PlatformThread::Sleep(kWaitInterval);
    }
    ASSERT_TRUE(parsed_net_log);

    VerifyNetLog(&parsed_net_log.value());

    PlatformBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  // Subclasses should override this to implement the test verification
  // conditions. It will be called after the test fixture has been torn down.
  virtual void VerifyNetLog(base::Value* parsed_net_log) = 0;

 protected:
  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 private:
  base::FilePath net_log_path_;
  base::ScopedTempDir tmp_dir_;
};

// This is an integration test to ensure that CertVerifyProc netlog events
// continue to be logged once cert verification is moved out of the network
// service process. (See crbug.com/1015134 and crbug.com/1040681.)
class CertVerifyProcNetLogBrowserTest : public NetLogPlatformBrowserTestBase {
 public:
  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_.ServeFilesFromSourceDirectory("chrome/test/data/");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  }

  void VerifyNetLog(base::Value* parsed_net_log) override {
    base::DictionaryValue* main;
    ASSERT_TRUE(parsed_net_log->GetAsDictionary(&main));

    base::Value* events = main->FindListKey("events");
    ASSERT_TRUE(events);

    bool found_cert_verify_proc_event = false;
    for (const auto& event : events->GetList()) {
      base::Optional<int> event_type = event.FindIntKey("type");
      ASSERT_TRUE(event_type.has_value());
      if (event_type ==
          static_cast<int>(net::NetLogEventType::CERT_VERIFY_PROC)) {
        base::Optional<int> phase = event.FindIntKey("phase");
        if (!phase.has_value() ||
            *phase != static_cast<int>(net::NetLogEventPhase::BEGIN)) {
          continue;
        }
        const base::Value* params = event.FindDictKey("params");
        if (!params)
          continue;
        const std::string* host = params->FindStringKey("host");
        if (host && *host == kTestHost) {
          found_cert_verify_proc_event = true;
          break;
        }
      }
    }

    EXPECT_TRUE(found_cert_verify_proc_event);
  }

  const std::string kTestHost = "netlog-example.a.test";

 protected:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(CertVerifyProcNetLogBrowserTest, Test) {
  ASSERT_TRUE(https_server_.Start());

  // Request using a unique host name to ensure that the cert verification wont
  // use a cached result for 127.0.0.1 that happened before the test starts
  // logging.
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server_.GetURL(kTestHost, "/ssl/google.html")));

  // Technically there is no guarantee that if the cert verifier is running out
  // of process that the netlog mojo messages will be delivered before the cert
  // verification mojo result. See:
  // https://chromium.googlesource.com/chromium/src/+/master/docs/mojo_ipc_conversion.md#Ordering-Considerations
  // Hopefully this won't be flaky.
  base::RunLoop().RunUntilIdle();
  content::FlushNetworkServiceInstanceForTesting();
}
