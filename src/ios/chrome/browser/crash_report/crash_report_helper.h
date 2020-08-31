// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORT_HELPER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORT_HELPER_H_

@class NSString;

class BreadcrumbManager;
class BreadcrumbManagerKeyedService;

namespace web {
class WebState;
}  // namespace web

class WebStateList;

namespace breakpad {

// Monitors the urls loaded by |web_state| to allow crash reports to contain the
// current loading url.
void MonitorURLsForWebState(web::WebState* web_state);

// Stop monitoring the urls loaded by |web_state|.
void StopMonitoringURLsForWebState(web::WebState* web_state);

// Monitors the urls loaded in |web_state_list| to allow crash reports to
// contain the currently loaded urls.
void MonitorURLsForWebStateList(WebStateList* web_state_list);

// Stop monitoring the urls loaded in the |web_state_list|.
void StopMonitoringURLsForWebStateList(WebStateList* web_state_list);

// Adds the state monitor to |web_state_list|. WebStateLists that are not
// monitored via this function are still monitored through WebStateObserver
// methods, but calling this function is mandatory to keep the monitoring of
// deleted tabs consistent.
void MonitorTabStateForWebStateList(WebStateList* web_state_list);

// Stop the state monitor of |web_state_list|.
void StopMonitoringTabStateForWebStateList(WebStateList* web_state_list);

// Clear any state about the urls loaded in the given WebStateList; this should
// be called when the WebStateList is deactivated.
void ClearStateForWebStateList(WebStateList* web_state_list);

// Starts listening for breadcrumbs logged to |breadcrumb_manager|. Collected
// breadcrumbs will be attached to crash reports.
void MonitorBreadcrumbManager(BreadcrumbManager* breadcrumb_manager);

// Stops listening for breadcrumbs logged to |breadcrumb_manager|.
void StopMonitoringBreadcrumbManager(BreadcrumbManager* breadcrumb_manager);

// Starts listening for breadcrumbs logged to |breadcrumb_manager_service|.
// Collected breadcrumbs will be attached to crash reports.
void MonitorBreadcrumbManagerService(
    BreadcrumbManagerKeyedService* breadcrumb_manager_service);

// Stops listening for breadcrumbs logged to |breadcrumb_manager_service|.
void StopMonitoringBreadcrumbManagerService(
    BreadcrumbManagerKeyedService* breadcrumb_manager_service);

}  // namespace breakpad

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORT_HELPER_H_
