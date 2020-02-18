// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_ERROR_HANDLER_H_
#define CHROME_BROWSER_SSL_SSL_ERROR_HANDLER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/common_name_mismatch_handler.h"
#include "chrome/browser/ssl/ssl_error_assistant.pb.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"
#include "components/ssl_errors/error_classification.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

class CommonNameMismatchHandler;
class Profile;
struct DynamicInterstitialInfo;

namespace base {
class Clock;
class TimeDelta;
}

namespace content {
class WebContents;
}

namespace network_time {
class NetworkTimeTracker;
}

extern const base::Feature kMITMSoftwareInterstitial;
extern const base::Feature kCaptivePortalInterstitial;
extern const base::Feature kCaptivePortalCertificateList;

// This class is responsible for deciding what type of interstitial to display
// for an SSL validation error and actually displaying it. The display of the
// interstitial might be delayed by a few seconds while trying to determine the
// cause of the error. During this window, the class will:
// - Check for a clock error
// - Check for a known captive portal certificate SPKI
// - Wait for a name-mismatch suggested URL
// - or Wait for a captive portal result to arrive.
//
// Based on the result of these checks, SSLErrorHandler will show a customized
// interstitial, redirect to a different suggested URL, or, if all else fails,
// show the normal SSL interstitial. When --committed--interstitials is enabled,
// it passes a constructed blocking page to the |blocking_page_ready_callback|
// that was given to HandleSSLError(), instead of showing the interstitial
// directly.
//
// This class should only be used on the UI thread because its implementation
// uses captive_portal::CaptivePortalService which can only be accessed on the
// UI thread.
class SSLErrorHandler : public content::WebContentsUserData<SSLErrorHandler>,
                        public content::WebContentsObserver,
                        public content::NotificationObserver {
 public:
  typedef base::Callback<void(content::WebContents*)> TimerStartedCallback;
  typedef base::OnceCallback<void(
      std::unique_ptr<security_interstitials::SecurityInterstitialPage>)>
      BlockingPageReadyCallback;

  ~SSLErrorHandler() override;

  // Events for UMA. Do not rename or remove values, add new values to the end.
  // Public for testing.
  // If you change this, change the values in CaptivePortalTest.java as well.
  enum UMAEvent {
    HANDLE_ALL = 0,
    SHOW_CAPTIVE_PORTAL_INTERSTITIAL_NONOVERRIDABLE = 1,
    SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE = 2,
    SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE = 3,
    SHOW_SSL_INTERSTITIAL_OVERRIDABLE = 4,
    WWW_MISMATCH_FOUND = 5,  // Deprecated in M59 by WWW_MISMATCH_FOUND_IN_SAN.
    WWW_MISMATCH_URL_AVAILABLE = 6,
    WWW_MISMATCH_URL_NOT_AVAILABLE = 7,
    SHOW_BAD_CLOCK = 8,
    CAPTIVE_PORTAL_CERT_FOUND = 9,
    WWW_MISMATCH_FOUND_IN_SAN = 10,
    SHOW_MITM_SOFTWARE_INTERSTITIAL = 11,
    OS_REPORTS_CAPTIVE_PORTAL = 12,
    SHOW_BLOCKED_INTERCEPTION_INTERSTITIAL = 13,
    SSL_ERROR_HANDLER_EVENT_COUNT
  };

  // This delegate allows unit tests to provide their own Chrome specific
  // actions.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void CheckForCaptivePortal() = 0;
    virtual bool DoesOSReportCaptivePortal() = 0;
    virtual bool GetSuggestedUrl(const std::vector<std::string>& dns_names,
                                 GURL* suggested_url) const = 0;
    virtual void CheckSuggestedUrl(
        const GURL& suggested_url,
        const CommonNameMismatchHandler::CheckUrlCallback& callback) = 0;
    virtual void NavigateToSuggestedURL(const GURL& suggested_url) = 0;
    virtual bool IsErrorOverridable() const = 0;
    virtual void ShowCaptivePortalInterstitial(const GURL& landing_url) = 0;
    virtual void ShowMITMSoftwareInterstitial(
        const std::string& mitm_software_name,
        bool is_enterprise_managed) = 0;
    virtual void ShowSSLInterstitial(const GURL& support_url) = 0;
    virtual void ShowBadClockInterstitial(
        const base::Time& now,
        ssl_errors::ClockState clock_state) = 0;
    virtual void ShowBlockedInterceptionInterstitial() = 0;
    virtual void ReportNetworkConnectivity(base::OnceClosure callback) = 0;
    virtual bool HasBlockedInterception() const = 0;
  };

  // Entry point for the class. All parameters except
  // |blocking_page_ready_callback| are the same as SSLBlockingPage constructor.
  // |blocking_page_ready_callback| is intended for committed interstitials. If
  // |blocking_page_ready_callback| is null, this function will create a
  // blocking page and call Show() on it. Otherwise, this function creates an
  // interstitial and passes it to |blocking_page_ready_callback|.
  // |blocking_page_ready_callback| is guaranteed not to be called
  // synchronously.
  static void HandleSSLError(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      const base::Callback<void(content::CertificateRequestResultType)>&
          decision_callback,
      BlockingPageReadyCallback blocking_page_ready_callback);

  // Sets the binary proto for SSL error assistant. The binary proto
  // can be downloaded by the component updater, or set by tests.
  static void SetErrorAssistantProto(
      std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
          config_proto);

  // Testing methods.
  static void ResetConfigForTesting();
  static void SetInterstitialDelayForTesting(const base::TimeDelta& delay);
  // The callback pointer must remain valid for the duration of error handling.
  static void SetInterstitialTimerStartedCallbackForTesting(
      TimerStartedCallback* callback);
  static void SetClockForTesting(base::Clock* testing_clock);
  static void SetNetworkTimeTrackerForTesting(
      network_time::NetworkTimeTracker* tracker);
  static void SetReportNetworkConnectivityCallbackForTesting(
      base::OnceClosure callback);
  static void SetEnterpriseManagedForTesting(bool enterprise_managed);
  static bool IsEnterpriseManagedFlagSetForTesting();
  static std::string GetHistogramNameForTesting();
  static int GetErrorAssistantProtoVersionIdForTesting();
  static void SetOSReportsCaptivePortalForTesting(
      bool os_reports_captive_portal);
  bool IsTimerRunningForTesting() const;

 protected:
  SSLErrorHandler(std::unique_ptr<Delegate> delegate,
                  content::WebContents* web_contents,
                  Profile* profile,
                  int cert_error,
                  const net::SSLInfo& ssl_info,
                  const GURL& request_url);

  // Called when an SSL cert error is encountered. Triggers a captive portal
  // check and fires a one shot timer to wait for a "captive portal detected"
  // result to arrive. Protected for testing.
  void StartHandlingError();

 private:
  friend class content::WebContentsUserData<SSLErrorHandler>;
  FRIEND_TEST_ALL_PREFIXES(SSLErrorHandlerTest, CalculateOptionsMask);

  void ShowCaptivePortalInterstitial(const GURL& landing_url);
  void ShowMITMSoftwareInterstitial(const std::string& mitm_software_name,
                                    bool is_enterprise_managed);
  void ShowSSLInterstitial();
  void ShowBadClockInterstitial(const base::Time& now,
                                ssl_errors::ClockState clock_state);
  void ShowDynamicInterstitial(const DynamicInterstitialInfo interstitial);
  void ShowBlockedInterceptionInterstitial();

  // Gets the result of whether the suggested URL is valid. Displays
  // common name mismatch interstitial or ssl interstitial accordingly.
  void CommonNameMismatchHandlerCallback(
      CommonNameMismatchHandler::SuggestedUrlCheckResult result,
      const GURL& suggested_url);

  // content::NotificationObserver:
  void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // content::WebContentsObserver:
  void NavigationStopped() override;

  // Deletes the SSLErrorHandler. This method is called when the page
  // load stops or when there is a new navigation.
  void DeleteSSLErrorHandler();

  void HandleCertDateInvalidError();
  void HandleCertDateInvalidErrorImpl(base::TimeTicks started_handling_error);

  bool IsOnlyCertError(net::CertStatus only_cert_error_expected) const;

  std::unique_ptr<Delegate> delegate_;
  content::WebContents* const web_contents_;
  Profile* const profile_;
  const int cert_error_;
  const net::SSLInfo ssl_info_;
  const GURL request_url_;

  content::NotificationRegistrar registrar_;
  base::OneShotTimer timer_;

  std::unique_ptr<CommonNameMismatchHandler> common_name_mismatch_handler_;

  base::WeakPtrFactory<SSLErrorHandler> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHandler);
};

#endif  // CHROME_BROWSER_SSL_SSL_ERROR_HANDLER_H_
