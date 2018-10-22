// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CERTIFICATE_ERROR_REPORT_H_
#define CHROME_BROWSER_SSL_CERTIFICATE_ERROR_REPORT_H_

#include <memory>
#include <string>

#include "chrome/browser/ssl/cert_logger.pb.h"
#include "components/version_info/version_info.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"

namespace base {
class Time;
}  // namespace base

namespace network_time {
class NetworkTimeTracker;
}  // namespace network_time

namespace net {
class CertVerifyResult;
class SSLInfo;
class X509Certificate;
}  // namespace net

// This class builds and serializes reports for invalid SSL certificate
// chains, intended to be sent with CertificateErrorReporter.
class CertificateErrorReport {
 public:
  // Describes the type of interstitial that the user was shown for the
  // error that this report represents. Gets mapped to
  // |CertLoggerInterstitialInfo::InterstitialReason|.
  enum InterstitialReason {
    INTERSTITIAL_SSL,
    INTERSTITIAL_CAPTIVE_PORTAL,
    INTERSTITIAL_CLOCK,
    INTERSTITIAL_SUPERFISH,
    INTERSTITIAL_MITM_SOFTWARE,
  };

  // Whether the user clicked through the interstitial or not.
  enum ProceedDecision { USER_PROCEEDED, USER_DID_NOT_PROCEED };

  // Whether the user was shown an option to click through the
  // interstitial.
  enum Overridable { INTERSTITIAL_OVERRIDABLE, INTERSTITIAL_NOT_OVERRIDABLE };

  // Constructs an empty report.
  CertificateErrorReport();

  // Constructs a report for the given |hostname| using the SSL
  // properties in |ssl_info|.
  CertificateErrorReport(const std::string& hostname,
                         const net::SSLInfo& ssl_info);

  // Constructs a dual verification trial report for the given |hostname|, the
  // cert and chain sent by the server, the result from the primary verifier,
  // and the result from the trial verifier.
  // TODO(mattm): remove this when the trial is done. (https://crbug.com/649026)
  CertificateErrorReport(const std::string& hostname,
                         const net::X509Certificate& unverified_cert,
                         const net::CertVerifier::Config& config,
                         const net::CertVerifyResult& primary_result,
                         const net::CertVerifyResult& trial_result);

  ~CertificateErrorReport();

  // Initializes an empty report by parsing the given serialized
  // report. |serialized_report| should be a serialized
  // CertLoggerRequest protobuf. Returns true if the report could be
  // successfully parsed and false otherwise.
  bool InitializeFromString(const std::string& serialized_report);

  // Serializes the report. The output will be a serialized
  // CertLoggerRequest protobuf. Returns true if the serialization was
  // successful and false otherwise.
  bool Serialize(std::string* output) const;

  void SetInterstitialInfo(const InterstitialReason& interstitial_reason,
                           const ProceedDecision& proceed_decision,
                           const Overridable& overridable,
                           const base::Time& interstitial_time);

  void AddNetworkTimeInfo(
      const network_time::NetworkTimeTracker* network_time_tracker);

  void AddChromeChannel(version_info::Channel channel);

  void SetIsEnterpriseManaged(bool is_enterprise_managed);

  // Sets is_retry_upload field of the protobuf to |is_retry_upload|.
  void SetIsRetryUpload(bool is_retry_upload);

  // Gets the hostname to which this report corresponds.
  const std::string& hostname() const;

  // Gets the Chrome channel attached to this report.
  chrome_browser_ssl::CertLoggerRequest::ChromeChannel chrome_channel() const;

  // Returns true if the device that issued the report is a managed device.
  bool is_enterprise_managed() const;

  // Returns true if the report has been retried.
  bool is_retry_upload() const;

 private:
  CertificateErrorReport(const std::string& hostname,
                         const net::X509Certificate& cert,
                         const net::X509Certificate* unverified_cert,
                         bool is_issued_by_known_root,
                         net::CertStatus cert_status);

  std::unique_ptr<chrome_browser_ssl::CertLoggerRequest> cert_report_;
};

#endif  // CHROME_BROWSER_SSL_CERTIFICATE_ERROR_REPORT_H_
