// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/capabilities.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/strings/pattern.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(Switches, Empty) {
  Switches switches;
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  switches.AppendToCommandLine(&cmd);
  ASSERT_EQ(0u, cmd.GetSwitches().size());
  ASSERT_EQ("", switches.ToString());
}

TEST(Switches, NoValue) {
  Switches switches;
  switches.SetSwitch("hello");

  ASSERT_TRUE(switches.HasSwitch("hello"));
  ASSERT_EQ("", switches.GetSwitchValue("hello"));

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  switches.AppendToCommandLine(&cmd);
  ASSERT_TRUE(cmd.HasSwitch("hello"));
  ASSERT_EQ(FILE_PATH_LITERAL(""), cmd.GetSwitchValueNative("hello"));
  ASSERT_EQ("--hello", switches.ToString());
}

TEST(Switches, Value) {
  Switches switches;
  switches.SetSwitch("hello", "there");

  ASSERT_TRUE(switches.HasSwitch("hello"));
  ASSERT_EQ("there", switches.GetSwitchValue("hello"));

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  switches.AppendToCommandLine(&cmd);
  ASSERT_TRUE(cmd.HasSwitch("hello"));
  ASSERT_EQ(FILE_PATH_LITERAL("there"), cmd.GetSwitchValueNative("hello"));
  ASSERT_EQ("--hello=there", switches.ToString());
}

TEST(Switches, FromOther) {
  Switches switches;
  switches.SetSwitch("a", "1");
  switches.SetSwitch("b", "1");

  Switches switches2;
  switches2.SetSwitch("b", "2");
  switches2.SetSwitch("c", "2");

  switches.SetFromSwitches(switches2);
  ASSERT_EQ("--a=1 --b=2 --c=2", switches.ToString());
}

TEST(Switches, Remove) {
  Switches switches;
  switches.SetSwitch("a", "1");
  switches.RemoveSwitch("a");
  ASSERT_FALSE(switches.HasSwitch("a"));
}

TEST(Switches, Quoting) {
  Switches switches;
  switches.SetSwitch("hello", "a  b");
  switches.SetSwitch("hello2", "  '\"  ");

  ASSERT_EQ("--hello=\"a  b\" --hello2=\"  '\\\"  \"", switches.ToString());
}

TEST(Switches, Multiple) {
  Switches switches;
  switches.SetSwitch("switch");
  switches.SetSwitch("hello", "there");

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  switches.AppendToCommandLine(&cmd);
  ASSERT_TRUE(cmd.HasSwitch("switch"));
  ASSERT_TRUE(cmd.HasSwitch("hello"));
  ASSERT_EQ(FILE_PATH_LITERAL("there"), cmd.GetSwitchValueNative("hello"));
  ASSERT_EQ("--hello=there --switch", switches.ToString());
}

TEST(Switches, Unparsed) {
  Switches switches;
  switches.SetUnparsedSwitch("a");
  switches.SetUnparsedSwitch("--b");
  switches.SetUnparsedSwitch("--c=1");
  switches.SetUnparsedSwitch("d=1");
  switches.SetUnparsedSwitch("-e=--1=1");

  ASSERT_EQ("---e=--1=1 --a --b --c=1 --d=1", switches.ToString());
}

TEST(ParseCapabilities, UnknownCapabilityLegacy) {
  // In legacy mode, unknown capabilities are ignored.
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.GetDict().Set("foo", "bar");
  Status status = capabilities.Parse(caps, false);
  ASSERT_TRUE(status.IsOk());
}

TEST(ParseCapabilities, UnknownCapabilityW3c) {
  // In W3C mode, unknown capabilities results in error.
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.GetDict().Set("foo", "bar");
  Status status = capabilities.Parse(caps);
  ASSERT_EQ(status.code(), kInvalidArgument);
}

TEST(ParseCapabilities, WithAndroidPackage) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.GetDict().SetByDottedPath("goog:chromeOptions.androidPackage", "abc");
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.IsAndroid());
  ASSERT_EQ("abc", capabilities.android_package);
}

TEST(ParseCapabilities, EmptyAndroidPackage) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.GetDict().SetByDottedPath("goog:chromeOptions.androidPackage",
                                 std::string());
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, IllegalAndroidPackage) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.GetDict().SetByDottedPath("goog:chromeOptions.androidPackage", 123);
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, LogPath) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.GetDict().SetByDottedPath("goog:chromeOptions.logPath",
                                 "path/to/logfile");
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_STREQ("path/to/logfile", capabilities.log_path.c_str());
}

TEST(ParseCapabilities, Args) {
  Capabilities capabilities;
  base::Value::ListStorage args;
  args.emplace_back("arg1");
  args.emplace_back("arg2=invalid");
  args.emplace_back("arg2=val");
  args.emplace_back("enable-blink-features=val1");
  args.emplace_back("enable-blink-features=val2,");
  args.emplace_back("--enable-blink-features=val3");
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "args"}, base::Value(args));

  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());

  ASSERT_EQ(3u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("arg1"));
  ASSERT_TRUE(capabilities.switches.HasSwitch("arg2"));
  ASSERT_EQ("", capabilities.switches.GetSwitchValue("arg1"));
  ASSERT_EQ("val", capabilities.switches.GetSwitchValue("arg2"));
  ASSERT_EQ("val1,val2,val3",
            capabilities.switches.GetSwitchValue("enable-blink-features"));
}

TEST(ParseCapabilities, Prefs) {
  Capabilities capabilities;
  base::DictionaryValue prefs;
  prefs.GetDict().Set("key1", "value1");
  prefs.GetDict().SetByDottedPath("key2.k", "value2");
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "prefs"}, prefs.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(*capabilities.prefs == prefs);
}

TEST(ParseCapabilities, LocalState) {
  Capabilities capabilities;
  base::DictionaryValue local_state;
  local_state.GetDict().Set("s1", "v1");
  local_state.GetDict().SetByDottedPath("s2.s", "v2");
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "localState"}, local_state.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(*capabilities.local_state == local_state);
}

TEST(ParseCapabilities, Extensions) {
  Capabilities capabilities;
  base::Value::ListStorage extensions;
  extensions.emplace_back("ext1");
  extensions.emplace_back("ext2");
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "extensions"}, base::Value(extensions));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, capabilities.extensions.size());
  ASSERT_EQ("ext1", capabilities.extensions[0]);
  ASSERT_EQ("ext2", capabilities.extensions[1]);
}

TEST(ParseCapabilities, UnrecognizedProxyType) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.GetDict().Set("proxyType", "unknown proxy type");
  base::DictionaryValue caps;
  caps.GetDict().Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, IllegalProxyType) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.GetDict().Set("proxyType", 123);
  base::DictionaryValue caps;
  caps.GetDict().Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, DirectProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.GetDict().Set("proxyType", "direct");
  base::DictionaryValue caps;
  caps.GetDict().Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("no-proxy-server"));
}

TEST(ParseCapabilities, SystemProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.GetDict().Set("proxyType", "system");
  base::DictionaryValue caps;
  caps.GetDict().Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(0u, capabilities.switches.GetSize());
}

TEST(ParseCapabilities, PacProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.GetDict().Set("proxyType", "pac");
  proxy.GetDict().Set("proxyAutoconfigUrl", "test.wpad");
  base::DictionaryValue caps;
  caps.GetDict().Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_EQ("test.wpad", capabilities.switches.GetSwitchValue("proxy-pac-url"));
}

TEST(ParseCapabilities, MissingProxyAutoconfigUrl) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.GetDict().Set("proxyType", "pac");
  proxy.GetDict().Set("httpProxy", "http://localhost:8001");
  base::DictionaryValue caps;
  caps.GetDict().Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, AutodetectProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.GetDict().Set("proxyType", "autodetect");
  base::DictionaryValue caps;
  caps.GetDict().Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("proxy-auto-detect"));
}

TEST(ParseCapabilities, ManualProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.GetDict().Set("proxyType", "manual");
  proxy.GetDict().Set("ftpProxy", "localhost:9001");
  proxy.GetDict().Set("httpProxy", "localhost:8001");
  proxy.GetDict().Set("sslProxy", "localhost:10001");
  proxy.GetDict().Set("socksProxy", "localhost:12345");
  proxy.GetDict().Set("socksVersion", 5);
  std::unique_ptr<base::ListValue> bypass = std::make_unique<base::ListValue>();
  bypass->Append("google.com");
  bypass->Append("youtube.com");
  proxy.SetList("noProxy", std::move(bypass));
  base::DictionaryValue caps;
  caps.GetDict().Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, capabilities.switches.GetSize());
  ASSERT_EQ(
      "ftp=localhost:9001;http=localhost:8001;https=localhost:10001;"
      "socks=socks5://localhost:12345",
      capabilities.switches.GetSwitchValue("proxy-server"));
  ASSERT_EQ(
      "google.com,youtube.com",
      capabilities.switches.GetSwitchValue("proxy-bypass-list"));
}

TEST(ParseCapabilities, IgnoreNullValueForManualProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.GetDict().Set("proxyType", "manual");
  proxy.GetDict().Set("ftpProxy", "localhost:9001");
  proxy.GetDict().Set("sslProxy", base::Value());
  proxy.GetDict().Set("noProxy", base::Value());
  base::DictionaryValue caps;
  caps.GetDict().Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("proxy-server"));
  ASSERT_EQ(
      "ftp=localhost:9001",
      capabilities.switches.GetSwitchValue("proxy-server"));
}

TEST(ParseCapabilities, MissingSocksVersion) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.GetDict().Set("proxyType", "manual");
  proxy.GetDict().Set("socksProxy", "localhost:6000");
  base::DictionaryValue caps;
  caps.GetDict().Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, BadSocksVersion) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.GetDict().Set("proxyType", "manual");
  proxy.GetDict().Set("socksProxy", "localhost:6000");
  proxy.GetDict().Set("socksVersion", 256);
  base::DictionaryValue caps;
  caps.GetDict().Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, AcceptInsecureCertsDisabledByDefault) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_FALSE(capabilities.accept_insecure_certs);
}

TEST(ParseCapabilities, EnableAcceptInsecureCerts) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.GetDict().Set("acceptInsecureCerts", true);
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.accept_insecure_certs);
}

TEST(ParseCapabilities, LoggingPrefsOk) {
  Capabilities capabilities;
  base::DictionaryValue logging_prefs;
  logging_prefs.GetDict().Set("Network", "INFO");
  base::DictionaryValue caps;
  caps.GetDict().Set("goog:loggingPrefs", std::move(logging_prefs));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.logging_prefs.size());
  ASSERT_EQ(Log::kInfo, capabilities.logging_prefs["Network"]);
}

TEST(ParseCapabilities, LoggingPrefsNotDict) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.GetDict().Set("goog:loggingPrefs", "INFO");
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, PerfLoggingPrefsInspectorDomainStatus) {
  Capabilities capabilities;
  // Perf log must be enabled if performance log preferences are specified.
  base::DictionaryValue logging_prefs;
  logging_prefs.GetDict().Set(WebDriverLog::kPerformanceType, "INFO");
  base::DictionaryValue desired_caps;
  desired_caps.GetDict().Set("goog:loggingPrefs", std::move(logging_prefs));
  ASSERT_EQ(PerfLoggingPrefs::InspectorDomainStatus::kDefaultEnabled,
            capabilities.perf_logging_prefs.network);
  ASSERT_EQ(PerfLoggingPrefs::InspectorDomainStatus::kDefaultEnabled,
            capabilities.perf_logging_prefs.page);
  base::DictionaryValue perf_logging_prefs;
  perf_logging_prefs.GetDict().Set("enableNetwork", true);
  perf_logging_prefs.GetDict().Set("enablePage", false);
  desired_caps.SetPath({"goog:chromeOptions", "perfLoggingPrefs"},
                       std::move(perf_logging_prefs));
  Status status = capabilities.Parse(desired_caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(PerfLoggingPrefs::InspectorDomainStatus::kExplicitlyEnabled,
            capabilities.perf_logging_prefs.network);
  ASSERT_EQ(PerfLoggingPrefs::InspectorDomainStatus::kExplicitlyDisabled,
            capabilities.perf_logging_prefs.page);
}

TEST(ParseCapabilities, PerfLoggingPrefsTracing) {
  Capabilities capabilities;
  // Perf log must be enabled if performance log preferences are specified.
  base::DictionaryValue logging_prefs;
  logging_prefs.GetDict().Set(WebDriverLog::kPerformanceType, "INFO");
  base::DictionaryValue desired_caps;
  desired_caps.GetDict().Set("goog:loggingPrefs", std::move(logging_prefs));
  ASSERT_EQ("", capabilities.perf_logging_prefs.trace_categories);
  base::DictionaryValue perf_logging_prefs;
  perf_logging_prefs.GetDict().Set("traceCategories",
                                   "benchmark,blink.console");
  perf_logging_prefs.GetDict().Set("bufferUsageReportingInterval", 1234);
  desired_caps.SetPath({"goog:chromeOptions", "perfLoggingPrefs"},
                       std::move(perf_logging_prefs));
  Status status = capabilities.Parse(desired_caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ("benchmark,blink.console",
            capabilities.perf_logging_prefs.trace_categories);
  ASSERT_EQ(1234,
            capabilities.perf_logging_prefs.buffer_usage_reporting_interval);
}

TEST(ParseCapabilities, PerfLoggingPrefsInvalidInterval) {
  Capabilities capabilities;
  // Perf log must be enabled if performance log preferences are specified.
  base::DictionaryValue logging_prefs;
  logging_prefs.GetDict().Set(WebDriverLog::kPerformanceType, "INFO");
  base::DictionaryValue desired_caps;
  desired_caps.GetDict().Set("goog:loggingPrefs", std::move(logging_prefs));
  base::DictionaryValue perf_logging_prefs;
  // A bufferUsageReportingInterval interval <= 0 will cause DevTools errors.
  perf_logging_prefs.GetDict().Set("bufferUsageReportingInterval", 0);
  desired_caps.SetPath({"goog:chromeOptions", "perfLoggingPrefs"},
                       std::move(perf_logging_prefs));
  Status status = capabilities.Parse(desired_caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, PerfLoggingPrefsNotDict) {
  Capabilities capabilities;
  // Perf log must be enabled if performance log preferences are specified.
  base::DictionaryValue logging_prefs;
  logging_prefs.GetDict().Set(WebDriverLog::kPerformanceType, "INFO");
  base::DictionaryValue desired_caps;
  desired_caps.GetDict().Set("goog:loggingPrefs", std::move(logging_prefs));
  desired_caps.GetDict().SetByDottedPath("goog:chromeOptions.perfLoggingPrefs",
                                         "traceCategories");
  Status status = capabilities.Parse(desired_caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, PerfLoggingPrefsNoPerfLogLevel) {
  Capabilities capabilities;
  base::DictionaryValue desired_caps;
  base::DictionaryValue perf_logging_prefs;
  perf_logging_prefs.GetDict().Set("enableNetwork", true);
  desired_caps.SetPath({"goog:chromeOptions", "perfLoggingPrefs"},
                       std::move(perf_logging_prefs));
  // Should fail because perf log must be enabled if perf log prefs specified.
  Status status = capabilities.Parse(desired_caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, PerfLoggingPrefsPerfLogOff) {
  Capabilities capabilities;
  base::DictionaryValue logging_prefs;
  // Disable performance log by setting logging level to OFF.
  logging_prefs.GetDict().Set(WebDriverLog::kPerformanceType, "OFF");
  base::DictionaryValue desired_caps;
  desired_caps.GetDict().Set("goog:loggingPrefs", std::move(logging_prefs));
  base::DictionaryValue perf_logging_prefs;
  perf_logging_prefs.GetDict().Set("enableNetwork", true);
  desired_caps.SetPath({"goog:chromeOptions", "perfLoggingPrefs"},
                       std::move(perf_logging_prefs));
  // Should fail because perf log must be enabled if perf log prefs specified.
  Status status = capabilities.Parse(desired_caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, ExcludeSwitches) {
  Capabilities capabilities;
  base::Value::ListStorage exclude_switches;
  exclude_switches.emplace_back("switch1");
  exclude_switches.emplace_back("switch2");
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "excludeSwitches"},
               base::Value(exclude_switches));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, capabilities.exclude_switches.size());
  const std::set<std::string>& switches = capabilities.exclude_switches;
  ASSERT_TRUE(base::Contains(switches, "switch1"));
  ASSERT_TRUE(base::Contains(switches, "switch2"));
}

TEST(ParseCapabilities, UseRemoteBrowserHostName) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.GetDict().SetByDottedPath("goog:chromeOptions.debuggerAddress",
                                 "abc:123");
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.IsRemoteBrowser());
  ASSERT_EQ("abc", capabilities.debugger_address.host());
  ASSERT_EQ(123, capabilities.debugger_address.port());
}

TEST(ParseCapabilities, UseRemoteBrowserIpv4) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.GetDict().SetByDottedPath("goog:chromeOptions.debuggerAddress",
                                 "127.0.0.1:456");
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.IsRemoteBrowser());
  ASSERT_EQ("127.0.0.1", capabilities.debugger_address.host());
  ASSERT_EQ(456, capabilities.debugger_address.port());
}

TEST(ParseCapabilities, UseRemoteBrowserIpv6) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.GetDict().SetByDottedPath("goog:chromeOptions.debuggerAddress",
                                 "[fe80::f2ef:86ff:fe69:cafe]:789");
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.IsRemoteBrowser());
  ASSERT_EQ("[fe80::f2ef:86ff:fe69:cafe]",
            capabilities.debugger_address.host());
  ASSERT_EQ(789, capabilities.debugger_address.port());
}

TEST(ParseCapabilities, MobileEmulationUserAgent) {
  Capabilities capabilities;
  base::DictionaryValue mobile_emulation;
  mobile_emulation.GetDict().Set("userAgent", "Agent Smith");
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "mobileEmulation"},
               std::move(mobile_emulation));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());

  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("user-agent"));
  ASSERT_EQ("Agent Smith", capabilities.switches.GetSwitchValue("user-agent"));
}

TEST(ParseCapabilities, MobileEmulationDeviceMetrics) {
  Capabilities capabilities;
  base::DictionaryValue mobile_emulation;
  mobile_emulation.GetDict().SetByDottedPath("deviceMetrics.width", 360);
  mobile_emulation.GetDict().SetByDottedPath("deviceMetrics.height", 640);
  mobile_emulation.GetDict().SetByDottedPath("deviceMetrics.pixelRatio", 3.0);
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "mobileEmulation"},
               std::move(mobile_emulation));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());

  ASSERT_EQ(360, capabilities.device_metrics->width);
  ASSERT_EQ(640, capabilities.device_metrics->height);
  ASSERT_EQ(3.0, capabilities.device_metrics->device_scale_factor);
}

TEST(ParseCapabilities, MobileEmulationDeviceName) {
  Capabilities capabilities;
  base::DictionaryValue mobile_emulation;
  mobile_emulation.GetDict().Set("deviceName", "Nexus 5");
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "mobileEmulation"},
               std::move(mobile_emulation));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());

  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("user-agent"));
  ASSERT_TRUE(base::MatchPattern(
      capabilities.switches.GetSwitchValue("user-agent"),
      "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/*.*.*.* Mobile "
      "Safari/537.36"));

  ASSERT_EQ(360, capabilities.device_metrics->width);
  ASSERT_EQ(640, capabilities.device_metrics->height);
  ASSERT_EQ(3.0, capabilities.device_metrics->device_scale_factor);
}

TEST(ParseCapabilities, MobileEmulationNotDict) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.GetDict().SetByDottedPath("goog:chromeOptions.mobileEmulation",
                                 "Google Nexus 5");
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, MobileEmulationDeviceMetricsNotDict) {
  Capabilities capabilities;
  base::DictionaryValue mobile_emulation;
  mobile_emulation.GetDict().Set("deviceMetrics", 360);
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "mobileEmulation"},
               std::move(mobile_emulation));
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, MobileEmulationDeviceMetricsNotNumbers) {
  Capabilities capabilities;
  base::DictionaryValue mobile_emulation;
  mobile_emulation.GetDict().SetByDottedPath("deviceMetrics.width", "360");
  mobile_emulation.GetDict().SetByDottedPath("deviceMetrics.height", "640");
  mobile_emulation.GetDict().SetByDottedPath("deviceMetrics.pixelRatio", "3.0");
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "mobileEmulation"},
               std::move(mobile_emulation));
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, MobileEmulationBadDict) {
  Capabilities capabilities;
  base::DictionaryValue mobile_emulation;
  mobile_emulation.GetDict().Set("deviceName", "Google Nexus 5");
  mobile_emulation.GetDict().SetByDottedPath("deviceMetrics.width", 360);
  mobile_emulation.GetDict().SetByDottedPath("deviceMetrics.height", 640);
  mobile_emulation.GetDict().SetByDottedPath("deviceMetrics.pixelRatio", 3.0);
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "mobileEmulation"},
               std::move(mobile_emulation));
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, VirtualAuthenticatorsBool) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.GetDict().Set("webauthn:virtualAuthenticators", true);
  EXPECT_TRUE(capabilities.Parse(caps).IsOk());

  caps.GetDict().Set("webauthn:virtualAuthenticators", false);
  EXPECT_TRUE(capabilities.Parse(caps).IsOk());
}

TEST(ParseCapabilities, VirtualAuthenticatorsNotBool) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.GetDict().Set("webauthn:virtualAuthenticators", "not a bool");
  EXPECT_FALSE(capabilities.Parse(caps).IsOk());
}

TEST(ParseCapabilities, VirtualAuthenticatorsLargeBlobBool) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.GetDict().Set("webauthn:extension:largeBlob", true);
  EXPECT_TRUE(capabilities.Parse(caps).IsOk());

  caps.GetDict().Set("webauthn:extension:largeBlob", false);
  EXPECT_TRUE(capabilities.Parse(caps).IsOk());
}

TEST(ParseCapabilities, VirtualAuthenticatorsLargeBlobNotBool) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.GetDict().Set("webauthn:extension:largeBlob", "not a bool");
  EXPECT_FALSE(capabilities.Parse(caps).IsOk());
}
