// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ERROR_REPORTING_CHROME_JS_ERROR_REPORT_PROCESSOR_H_
#define CHROME_BROWSER_ERROR_REPORTING_CHROME_JS_ERROR_REPORT_PROCESSOR_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/crash/content/browser/error_reporting/js_error_report_processor.h"

namespace content {
class BrowserContext;
}
namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network
class GURL;
struct JavaScriptErrorReport;

// Chrome's implementation of the JavaScript error reporter.
class ChromeJsErrorReportProcessor : public JsErrorReportProcessor {
 public:
  // Creates a ChromeJsErrorReportProcessor and sets it as the processor that
  // will be returned from JsErrorReportProcessor::Get(). This will only create
  // the processor if appropriate.
  static void Create();

  void SendErrorReport(JavaScriptErrorReport error_report,
                       base::OnceClosure completion_callback,
                       content::BrowserContext* browser_context) override;

  void set_clock_for_testing(base::Clock* clock) { clock_ = clock; }

  // Access to the recent_error_reports map allows tests to confirm we are not
  // growing this map without bound.
  const base::flat_map<std::string, base::Time>&
  get_recent_error_reports_for_testing() const {
    return recent_error_reports_;
  }

 protected:
  // Non-tests should call ChromeJsErrorReportProcessor::Create() instead.
  ChromeJsErrorReportProcessor();
  ~ChromeJsErrorReportProcessor() override;

  // Testing hook -- returns the URL we will send the error reports to. By
  // default, returns the real endpoint.
  virtual std::string GetCrashEndpoint();

  // Testing hook -- returns the URL we will send the error reports to if
  // JavaScriptErrorReport::send_to_production_servers is false. By
  // default, returns the real staging endpoint.
  virtual std::string GetCrashEndpointStaging();

  // Determines the version of the OS we are on. Virtual so that tests can
  // override.
  virtual void GetOsVersion(int32_t& os_major_version,
                            int32_t& os_minor_version,
                            int32_t& os_bugfix_version);

 private:
  struct PlatformInfo;

  void OnRequestComplete(std::unique_ptr<network::SimpleURLLoader> url_loader,
                         base::ScopedClosureRunner callback_runner,
                         std::unique_ptr<std::string> response_body);

  base::Optional<JavaScriptErrorReport> CheckConsentAndRedact(
      JavaScriptErrorReport error_report);

  PlatformInfo GetPlatformInfo();

  void SendReport(const GURL& url,
                  const std::string& body,
                  base::ScopedClosureRunner callback_runner,
                  network::SharedURLLoaderFactory* loader_factory);

  void OnConsentCheckCompleted(
      base::ScopedClosureRunner callback_runner,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      base::TimeDelta browser_process_uptime,
      base::Optional<JavaScriptErrorReport> error_report);

  // To avoid spamming the error collection system, do not send duplicate
  // error reports more than once per hour. Otherwise, if we get an error
  // each time the user types another character in a search box (for
  // instance), we would get flooded.
  // This function both determines if we should send the error report and also
  // updates the map to indicate that we did send it. It assumes we will send
  // the report if it sets |should_send| to true.
  void CheckAndUpdateRecentErrorReports(
      const JavaScriptErrorReport& error_report,
      bool* should_send);

  // For JavaScript error reports, a mapping of message+product+line+column to
  // the last time we sent an error message for that
  // message+product+line+column.
  base::flat_map<std::string, base::Time> recent_error_reports_;

  // To avoid recent_error_reports_ growing without bound, we clean it out every
  // once in while. This is the last time we cleaned it out.
  base::Time last_recent_error_reports_cleaning_;

  // Clock for dependency injection. Not owned.
  base::Clock* clock_;
};

#endif  // CHROME_BROWSER_ERROR_REPORTING_CHROME_JS_ERROR_REPORT_PROCESSOR_H_
