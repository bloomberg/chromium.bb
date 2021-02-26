// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/connectivity_diagnostics/network_diagnostics_resource_provider.h"

#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"

namespace chromeos {
namespace network_diagnostics {

namespace {

constexpr webui::LocalizedString kLocalizedStrings[] = {
    // Network Diagnostics Strings
    {"NetworkDiagnosticsLanConnectivity",
     IDS_NETWORK_DIAGNOSTICS_LAN_CONNECTIVITY},
    {"NetworkDiagnosticsSignalStrength",
     IDS_NETWORK_DIAGNOSTICS_SIGNAL_STRENGTH},
    {"NetworkDiagnosticsGatewayCanBePinged",
     IDS_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED},
    {"NetworkDiagnosticsHasSecureWiFiConnection",
     IDS_NETWORK_DIAGNOSTICS_HAS_SECURE_WIFI_CONNECTION},
    {"NetworkDiagnosticsDnsResolverPresent",
     IDS_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PRESENT},
    {"NetworkDiagnosticsDnsLatency", IDS_NETWORK_DIAGNOSTICS_DNS_LATENCY},
    {"NetworkDiagnosticsDnsResolution", IDS_NETWORK_DIAGNOSTICS_DNS_RESOLUTION},
    {"NetworkDiagnosticsHttpFirewall", IDS_NETWORK_DIAGNOSTICS_HTTP_FIREWALL},
    {"NetworkDiagnosticsHttpsFirewall", IDS_NETWORK_DIAGNOSTICS_HTTPS_FIREWALL},
    {"NetworkDiagnosticsHttpsLatency", IDS_NETWORK_DIAGNOSTICS_HTTPS_LATENCY},
    {"NetworkDiagnosticsPassed", IDS_NETWORK_DIAGNOSTICS_PASSED},
    {"NetworkDiagnosticsFailed", IDS_NETWORK_DIAGNOSTICS_FAILED},
    {"NetworkDiagnosticsNotRun", IDS_NETWORK_DIAGNOSTICS_NOT_RUN},
    {"NetworkDiagnosticsRunning", IDS_NETWORK_DIAGNOSTICS_RUNNING},
    {"NetworkDiagnosticsResultPlaceholder",
     IDS_NETWORK_DIAGNOSTICS_RESULT_PLACEHOLDER},
    {"NetworkDiagnosticsRun", IDS_NETWORK_DIAGNOSTICS_RUN},
    {"SignalStrengthProblem_NotFound",
     IDS_NETWORK_DIAGNOSTICS_SIGNAL_STRENGTH_PROBLEM_NOT_FOUND},
    {"SignalStrengthProblem_Weak",
     IDS_NETWORK_DIAGNOSTICS_SIGNAL_STRENGTH_PROBLEM_WEAK},
    {"GatewayPingProblem_Unreachable",
     IDS_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED_PROBLEM_UNREACHABLE},
    {"GatewayPingProblem_NoDefaultPing",
     IDS_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED_PROBLEM_PING_DEFAULT_FAILED},
    {"GatewayPingProblem_DefaultLatency",
     IDS_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED_PROBLEM_DEFAULT_ABOVE_LATENCY},
    {"GatewayPingProblem_NoNonDefaultPing",
     IDS_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED_PROBLEM_PING_NON_DEFAULT_FAILED},
    {"GatewayPingProblem_NonDefaultLatency",
     IDS_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED_PROBLEM_NON_DEFAULT_ABOVE_LATENCY},
    {"SecureWifiProblem_None",
     IDS_NETWORK_DIAGNOSTICS_SECURE_WIFI_PROBLEM_NOT_SECURE},
    {"SecureWifiProblem_8021x",
     IDS_NETWORK_DIAGNOSTICS_SECURE_WIFI_PROBLEM_WEP_8021x},
    {"SecureWifiProblem_PSK",
     IDS_NETWORK_DIAGNOSTICS_SECURE_WIFI_PROBLEM_WEP_PSK},
    {"SecureWifiProblem_Unknown",
     IDS_NETWORK_DIAGNOSTICS_SECURE_WIFI_PROBLEM_UNKNOWN},
    {"DnsResolverProblem_NoNameServers",
     IDS_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PROBLEM_NO_NAME_SERVERS},
    {"DnsResolverProblem_MalformedNameServers",
     IDS_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PROBLEM_MALFORMED_NAME_SERVERS},
    {"DnsResolverProblem_EmptyNameServers",
     IDS_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PROBLEM_EMPTY_NAME_SERVERS},
    {"DnsLatencyProblem_FailedResolveHosts",
     IDS_NETWORK_DIAGNOSTICS_DNS_LATENCY_PROBLEM_FAILED_TO_RESOLVE_ALL_HOSTS},
    {"DnsLatencyProblem_LatencySlightlyAbove",
     IDS_NETWORK_DIAGNOSTICS_DNS_LATENCY_PROBLEM_SLIGHTLY_ABOVE_THRESHOLD},
    {"DnsLatencyProblem_LatencySignificantlyAbove",
     IDS_NETWORK_DIAGNOSTICS_DNS_LATENCY_PROBLEM_SIGNIFICANTLY_ABOVE_THRESHOLD},
    {"DnsResolutionProblem_FailedResolve",
     IDS_NETWORK_DIAGNOSTICS_DNS_RESOLUTION_PROBLEM_FAILED_TO_RESOLVE_HOST},
    {"FirewallProblem_DnsResolutionFailureRate",
     IDS_NETWORK_DIAGNOSTICS_FIREWALL_PROBLEM_DNS_RESOLUTION_FAILURE_RATE},
    {"FirewallProblem_FirewallDetected",
     IDS_NETWORK_DIAGNOSTICS_FIREWALL_PROBLEM_FIREWALL_DETECTED},
    {"FirewallProblem_FirewallSuspected",
     IDS_NETWORK_DIAGNOSTICS_FIREWALL_PROBLEM_FIREWALL_SUSPECTED},
    {"HttpsLatencyProblem_FailedDnsResolution",
     IDS_NETWORK_DIAGNOSTICS_HTTPS_LATENCY_PROBLEM_FAILED_DNS_RESOLUTIONS},
    {"HttpsLatencyProblem_FailedHttpsRequests",
     IDS_NETWORK_DIAGNOSTICS_HTTPS_LATENCY_PROBLEM_FAILED_HTTPS_REQUESTS},
    {"HttpsLatencyProblem_HighLatency",
     IDS_NETWORK_DIAGNOSTICS_HTTPS_LATENCY_PROBLEM_HIGH_LATENCY},
    {"HttpsLatencyProblem_VeryHighLatency",
     IDS_NETWORK_DIAGNOSTICS_HTTPS_LATENCY_PROBLEM_VERY_HIGH_LATENCY},
};

struct WebUiResource {
  const char* name;
  int id;
};

constexpr WebUiResource kResources[] = {
    {"test_canceled.png",
     IDR_CR_COMPONENTS_CHROMEOS_NETWORK_DIAGNOSTICS_CANCELED_ICON},
    {"test_failed.png",
     IDR_CR_COMPONENTS_CHROMEOS_NETWORK_DIAGNOSTICS_FAILED_ICON},
    {"test_not_run.png",
     IDR_CR_COMPONENTS_CHROMEOS_NETWORK_DIAGNOSTICS_NOT_RUN_ICON},
    {"test_passed.png",
     IDR_CR_COMPONENTS_CHROMEOS_NETWORK_DIAGNOSTICS_PASSED_ICON},
    {"test_warning.png",
     IDR_CR_COMPONENTS_CHROMEOS_NETWORK_DIAGNOSTICS_WARNING_ICON},
};

}  // namespace

void AddResources(content::WebUIDataSource* html_source) {
  for (const auto& str : kLocalizedStrings)
    html_source->AddLocalizedString(str.name, str.id);

  for (const auto& resource : kResources)
    html_source->AddResourcePath(resource.name, resource.id);
}

}  // namespace network_diagnostics
}  // namespace chromeos
